#include <vvpkg/vfile.h>
#include <vvpkg/c_file_funcs.h>
#include "manifest_parser.h"

#include <sqxx/sqxx.hpp>
#include <rapidjson/filereadstream.h>
#include <folly/MoveWrapper.h>

#if defined(_WIN32)
#include <direct.h>
#else
#include <sys/stat.h>
#define _mkdir(fn) ::mkdir(fn, 0755)
#endif
#include <system_error>

static void mkdirp(char const* fn)
{
	errno = 0;
	_mkdir(fn);
	if (errno == EEXIST)
		errno = 0;
	else if (errno != 0)
		throw std::system_error(errno, std::system_category());
}

namespace vvpkg
{

struct vfile::impl
{
	impl(char const* db, bool readonly)
	    : conn(db,
	           readonly ? sqxx::OPEN_READONLY
	                    : sqxx::OPEN_CREATE | sqxx::OPEN_READWRITE)
	{
		if (not readonly)
		{
			conn.run(R"(
			create table if not exists cblocks (
			    id     blob primary key,
			    offset integer not null,
			    size   integer not null
			) without rowid)");
			conn.run(R"(
			create unique index if not exists size_idx
			on cblocks(offset)
			)");
		}

		conn.run(R"(
		create temporary table newfile (
		    id blob not null
		))");
	}

	sqxx::connection conn;
};

vfile::vfile(std::string path, char const* mode) : db_path_(std::move(path))
{
	mkdirp(db_path_.data());
	auto n = db_path_.size();
	if (not db_path_.empty() and db_path_.back() != '/')
		db_path_.push_back('/');
	db_path_.append("vvpkg.db");
	path_ = db_path_;
	path_.remove_suffix(path_.size() - n);

	bool readonly = mode == stdex::string_view("r");
	if (not readonly and mode != stdex::string_view("r+"))
		throw std::runtime_error{ R"(mode may be "r" or "r+")" };
	impl_.reset(new impl(db_path_.data(), readonly));
}

vfile::vfile(vfile&&) noexcept(
    std::is_nothrow_move_constructible<std::string>::value) = default;
vfile& vfile::operator=(vfile&&) noexcept(
    std::is_nothrow_move_assignable<std::string>::value) = default;
vfile::~vfile() = default;

void vfile::merge(std::vector<msg_digest> const& missing, bundle const& bs,
                  stdex::signature<void(int64_t, char const*, size_t)> emit)
{
	static auto feed =
	    impl_->conn.prepare("insert into cblocks values(?, ?, ?)");
	static auto getsize = impl_->conn.prepare(
	    "select offset + size from cblocks order by offset desc limit 1");

	auto it = begin(bs.blocks());
	auto ed = end(bs.blocks());

	impl_->conn.run("begin");

	getsize.run();
	auto offset = getsize ? getsize.val<int64_t>(0) : 0;
	getsize.reset();

	for (auto&& id : missing)
	{
		it = std::find_if(it, ed, [&](decltype(*it) binfo) {
			return std::get<1>(binfo) == id;
		});

		if (it == ed)
			break;

		auto blk = bs.block_at(it);
		emit(offset, blk.data(), blk.size());

		feed.bind(0, sqxx::blob(id.data(), hash::digest_size), false);
		feed.bind(1, offset);
		offset += int(blk.size());
		feed.bind(2, int(blk.size()));
		feed.run();
		feed.reset();
		feed.clear_bindings();
	}

	impl_->conn.run("commit");
}

auto vfile::list(std::string commitid)
    -> std::function<std::pair<int64_t, int64_t>()>
{
	auto segments = impl_->conn.prepare(R"(
	with pieces
	     as (select offset        as off_start,
			offset + size as off_end,
			newfile.rowid as j
		   from newfile join cblocks using(id)
		  order by newfile.rowid),
	     segments(off_start, off_end, j, k)
	     as (select off_start,
			off_end,
			j,
			j as k
		   from pieces
		  where j = 1
		 union all
		 select a.off_start,
			a.off_end,
			a.j,
			case
			  when r.off_end = a.off_start
			  then r.k
			  else a.j
			end k
		   from pieces a join segments r
		     on a.j = r.j + 1)
	select min(off_start),
	       max(off_end)
	  from segments
	 group by k
	)");

	auto prefix = stdex::string_view(db_path_).substr(0, path_.size() + 1);
	commitid.insert(0, prefix.data(), prefix.size());
	commitid.append(".json");

	static auto staging =
	    impl_->conn.prepare("insert into newfile values(?)");
	char buf[4096];
	manifest_parser<rapidjson::FileReadStream> manifest(
	    xfopen(commitid.data(), "rb"), buf, sizeof(buf));
	manifest.parse([&](char const* p, size_t sz) {
		auto blockid = hashlib::unhexlify<hash::digest_size>(
		    stdex::string_view(p, sz));
		staging.bind(0, sqxx::blob(blockid.data(), hash::digest_size),
		             false);
		staging.run();
		staging.reset();
		staging.clear_bindings();
	});

	segments.run();

	return [ q = folly::makeMoveWrapper(segments), this ]() mutable
	{
		if (q->done())
		{
			impl_->conn.run("delete from newfile");
			return std::make_pair(int64_t(), int64_t());
		}

		auto off_start = q->val<int64_t>(0);
		auto off_end = q->val<int64_t>(1);
		q->next_row();
		return std::make_pair(off_start, off_end);
	};
}

}
