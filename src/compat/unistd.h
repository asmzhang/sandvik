#pragma once
/*
 * compat/unistd.h -- POSIX identity calls mingw-w64 does not provide.
 *
 * src/vfs.hpp namespaces the host cache directory with getuid() to avoid a
 * shared, predictable path in the temp directory. Windows processes have no
 * uid, so the shims report 0: one stable namespace per machine, which is what
 * that namespacing asks for.
 *
 * Everything else is forwarded to the toolchain's <unistd.h> unchanged.
 */
#include_next <unistd.h>

#if defined(_WIN32) && !defined(SANDVIK_COMPAT_UNISTD_IDS)
#define SANDVIK_COMPAT_UNISTD_IDS 1

static inline unsigned int getuid(void) {
	return 0;
}

static inline unsigned int geteuid(void) {
	return 0;
}

static inline unsigned int getgid(void) {
	return 0;
}

static inline unsigned int getegid(void) {
	return 0;
}

/*
 * libcore.io.Linux.setpgid() joins a process group during zygote/child setup.
 * Windows has no process groups to join from the CRT layer, so the call is
 * accepted and ignored (pid/pgid are validated by the caller).
 */
#ifdef __cplusplus
extern "C" {
#endif

int setpgid(int pid, int pgid);

#ifdef __cplusplus
}
#endif

#endif /* _WIN32 && !SANDVIK_COMPAT_UNISTD_IDS */
