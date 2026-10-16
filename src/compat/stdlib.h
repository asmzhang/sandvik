#pragma once
/*
 * compat/stdlib.h — mingw-w64 provides no realpath().
 *
 * libcore.io.UnixFileSystem uses the POSIX form (including the
 * "resolved == nullptr allocates" contract); map it onto _fullpath().
 * Everything else is forwarded to the toolchain's <stdlib.h> unchanged.
 */
#include_next <stdlib.h>

#if defined(_WIN32) && !defined(SANDVIK_COMPAT_REALPATH)
#define SANDVIK_COMPAT_REALPATH 1

#ifndef _MAX_PATH
#define _MAX_PATH 260
#endif

/*
 * C++ has nullptr, C (e.g. ext/libminizip's miniz.c) does not: use a portable
 * spelling so this shim can be included from both languages.
 */
#if defined(__cplusplus)
#define SANDVIK_COMPAT_NULL nullptr
#else
#define SANDVIK_COMPAT_NULL NULL
#endif

static inline char* realpath(const char* path, char* resolved) {
	if (path == SANDVIK_COMPAT_NULL) {
		return SANDVIK_COMPAT_NULL;
	}
	if (resolved != SANDVIK_COMPAT_NULL) {
		return _fullpath(resolved, path, _MAX_PATH);
	}
	/* Same contract as POSIX: caller frees the returned buffer. */
	return _fullpath(SANDVIK_COMPAT_NULL, path, 0);
}
#endif /* _WIN32 && !SANDVIK_COMPAT_REALPATH */
