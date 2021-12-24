#include <vvpkg/vvpkg.h>
#include <vvpkg/fd_funcs.h>

#include <lip.h>

#include "demo_helpers.h"

void dump_blocks(char const* filename);

using namespace stdex::literals;

int main(int argc, char* argv[])
{
	if (argc < 2)
	{
		std::cerr << "usage: ./dump_blocks <PATH>\n";
		exit(2);
	}

	try
	{
		dump_blocks(argv[1]);
	}
	catch (std::exception& e)
	{
		std::cerr << "ERROR: " << e.what() << std::endl;
		exit(1);
	}
}

struct blockHash {
public:
	vvpkg::msg_digest hash;
	int64_t start, end;
	blockHash(vvpkg::msg_digest hash, int64_t start, int64_t end)
		: hash{hash}, start{start}, end{end} {};
	int64_t size() const {
		return end - start;
	}
};

struct lipFile {
public:
	std::string name;
	int64_t start, end;
	std::vector<vvpkg::msg_digest> hshList;
	lipFile(std::string name, int64_t start, int64_t end)
		: name{name}, start{start}, end{end}, hshList{} {};
	void addHash(vvpkg::msg_digest hsh) {
		hshList.push_back(hsh);
	}
	bool operator <(const lipFile &other) const {
		return start < other.start;
	}
	int64_t size() const {
		return end - start;
	}
	vvpkg::msg_digest get_hash() const {
		char* hashes = new char[hshList.size() * hshList[0].size()];
		for (size_t i = 0; i < hshList.size(); i++) {
			std::copy(hshList[i].begin(), hshList[i].end(), hashes + i * hshList[0].size());
		}
		auto hsh = rax::deuceclient::hash(hashes, hshList.size() * hshList[0].size()).digest();
		delete []hashes;
		return hsh;
	}
};

std::ostream& operator<<(std::ostream& os, const std::vector<lipFile>& lfv) {
    os << "{";
    char buf[2 * vvpkg::hash::digest_size];
    for (int i = 0; i < lfv.size() - 1; i++) {
        hashlib::detail::hexlify_to(lfv[i].get_hash(), buf);
        os << "\"" << lfv[i].name << "\": \"" << buf << "\", ";
    }
    os << "\"" << lfv[lfv.size() - 1].name << "\": \"" << buf << "\"}";
    return os;
}

vvpkg::msg_digest get_hash(readOnlyFileHandle *fh, int64_t start, int64_t end) {
	char* buffer = new char[end - start];
	fh->Seek(start, File::Location::BEGIN);
	fh->Read(buffer, end - start);
	auto hsh = rax::deuceclient::hash(buffer, end - start).digest();
	delete []buffer;
	return hsh;
}

void make_hashes(char const* filename, std::vector<blockHash> bhl) {
	auto fh = new readOnlyFileHandle(filename);
	std::vector<lipFile> l;
	lip::LIP lipf(fh);
	auto index = lipf.getIndex();
	index->resetItr();
	decltype(index->getNext()) ptr;
	char fname[FILENAME_MAX];
	while ((ptr = index->getNext()) != nullptr) {
		if (!ptr->isDirectory()) {
			index->getName(ptr, fname);
			l.emplace_back(fname, ptr->begin.offset, ptr->end.offset);
		}
	}
	std::sort(l.begin(), l.end());
	auto bhlit = bhl.begin();
	auto lit = l.begin();
	while (lit != l.end()) {
		if (lit->end <= bhlit->start) {
			// std::cout << "File ended before block " << lit->name << std::endl;
			// std::cout << "Next File" << std::endl;
			lit++;
		} else if (lit->start <= bhlit->start && lit->end < bhlit->end) {
			// std::cout << "File ended during block " << lit->name << std::endl;
			// std::cout << "Hash block start to file end" << std::endl;
			lit->addHash(get_hash(fh, bhlit->start, lit->end));
			lit++;
		} else if (bhlit->start < lit->start && lit->end < bhlit->end) {
			// std::cout << "File fully inside block " << lit->name << std::endl;
			// std::cout << "Hash full file" << std::endl;
			lit->addHash(get_hash(fh, lit->start, lit->end));
			lit++;
		} else if (lit->start <= bhlit->start && bhlit->end <= lit->end) {
			// std::cout << "Block fully inside file " << lit->name << std::endl;
			// std::cout << "Hash full block (already present)" << std::endl;
			lit->addHash(bhlit->hash);
			bhlit++;
		} else if (bhlit->start < lit->start && bhlit->end <= lit->end) {
			// std::cout << "Block ended during file " << lit->name << std::endl;
			// std::cout << "Hash file start to block end" << std::endl;
			lit->addHash(get_hash(fh, lit->start, bhlit->end));
			bhlit++;
		} else if (bhlit->end <= lit->start) {
			// std::cout << "Block ended before file " << lit->name << std::endl;
			// std::cout << "Next Block" << std::endl;
			bhlit++;
		} else {
			std::cerr << "Invalid Condition (" << lit->start << ", " << lit->end;
			std::cerr << ")(" << bhlit->start << ", " << bhlit->end << ")" << std::endl;
			exit(1);
		}
	}
	std::cout << l << std::endl;
}

void dump_blocks(char const* filename)
{
	int fd = vvpkg::xopen_for_read(filename);
	defer(vvpkg::xclose(fd));
	vvpkg::managed_bundle<vvpkg::rabin_boundary> bs(10 * 1024 * 1024);
	std::vector<blockHash> bhl;

	int64_t file_size = 0;
	bool bundle_is_full;
	do {
		bundle_is_full = bs.consume(vvpkg::from_descriptor(fd));
		if (bs.empty())
			break;
		int64_t offset = file_size;

		for (auto&& t : bs.blocks()) {
			size_t end_of_block;
			vvpkg::msg_digest blockid;
			std::tie(end_of_block, blockid) = t;

			// char buf[2 * vvpkg::hash::digest_size];
			// hashlib::detail::hexlify_to(blockid, buf);
			bhl.emplace_back(blockid, offset, file_size + int64_t(end_of_block));

			offset = file_size + int64_t(end_of_block);
		}
		file_size += int64_t(bs.size());
		bs.clear();
	} while (bundle_is_full);

	make_hashes(filename, bhl);
}
