#include "repository.h"
#include "rabin_boundary.h"
#include <stdex/defer.h>
#include <iostream>

#include <vvpkg/fd_funcs.h>

using namespace stdex::literals;

void commit(char const* rev, char const* fn)
{
	auto repo = vvpkg::repository(".", "r+");
	vvpkg::managed_bundle<vvpkg::rabin_boundary> bs(10 * 1024 * 1024);
	int fd = [&] {
		if (fn == "-"_sv)
			return vvpkg::xstdin_fileno();
		else
			return vvpkg::xopen_for_read(fn);
	}();
	defer(vvpkg::xclose(fd));

	std::cerr << repo.commit(rev, bs, vvpkg::from_descriptor(fd))
                  << std::endl;
}

void checkout(char const* rev, char const* fn)
{
	auto repo = vvpkg::repository(".", "r");
	int fd = [&] {
		if (fn == "-"_sv)
			return vvpkg::xstdout_fileno();
		else
			return vvpkg::xopen_for_write(fn);
	}();
	defer(vvpkg::xclose(fd));

	repo.checkout(rev, vvpkg::to_descriptor(fd));
}
