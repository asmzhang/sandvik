#pragma once
/*
 * compat/sys/poll.h -- mingw-w64 has no <sys/poll.h>.
 *
 * android/12.0.0/natives/libcore/io/Linux.cpp includes <sys/poll.h> and calls
 * poll(); the POLL* values come from compat/poll.h (Linux values, also
 * republished to Java through OsConstants). The struct mirrors the Linux
 * layout, which is what libcore.io.StructPollfd marshals into.
 *
 * The declaration below must match the definition in src/compat/posix_win32.cpp.
 */
#include <poll.h>
#include <sys/types.h>

#if defined(_WIN32) && !defined(SANDVIK_COMPAT_SYS_POLL)
#define SANDVIK_COMPAT_SYS_POLL 1

struct pollfd {
	int fd;
	short events;
	short revents;
};

#ifdef __cplusplus
extern "C" {
#endif

int poll(struct pollfd* fds, unsigned long nfds, int timeout);

#ifdef __cplusplus
}
#endif

#endif /* _WIN32 && !SANDVIK_COMPAT_SYS_POLL */
