#pragma once
/*
 * compat/poll.h -- mingw-w64 has no <poll.h>.
 *
 * src/system/os_constants.hpp publishes these to Java through OsConstants; the
 * values are the Linux/Android ones (winsock defines no POLL* names, so there
 * is nothing to clash with). No file in the Windows build calls poll().
 */
#if defined(_WIN32) && !defined(SANDVIK_COMPAT_POLL)
#define SANDVIK_COMPAT_POLL 1

#ifndef POLLIN
#define POLLIN 0x001
#endif
#ifndef POLLPRI
#define POLLPRI 0x002
#endif
#ifndef POLLOUT
#define POLLOUT 0x004
#endif
#ifndef POLLERR
#define POLLERR 0x008
#endif
#ifndef POLLHUP
#define POLLHUP 0x010
#endif
#ifndef POLLNVAL
#define POLLNVAL 0x020
#endif
#ifndef POLLRDNORM
#define POLLRDNORM 0x040
#endif
#ifndef POLLRDBAND
#define POLLRDBAND 0x080
#endif
#ifndef POLLWRNORM
#define POLLWRNORM 0x100
#endif
#ifndef POLLWRBAND
#define POLLWRBAND 0x200
#endif

#endif /* _WIN32 && !SANDVIK_COMPAT_POLL */
