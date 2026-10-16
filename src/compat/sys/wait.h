#pragma once
/*
 * compat/sys/wait.h -- mingw-w64 has no <sys/wait.h>.
 *
 * Only src/system/os_constants.hpp includes it (for the waitpid() flags), so
 * this shim declares the Linux/Android flag values and nothing else.
 */
#if defined(_WIN32) && !defined(SANDVIK_COMPAT_SYS_WAIT)
#define SANDVIK_COMPAT_SYS_WAIT 1

#ifndef WNOHANG
#define WNOHANG 1
#endif
#ifndef WUNTRACED
#define WUNTRACED 2
#endif
#ifndef WSTOPPED
#define WSTOPPED 2
#endif
#ifndef WEXITED
#define WEXITED 4
#endif
#ifndef WCONTINUED
#define WCONTINUED 8
#endif
#ifndef WNOWAIT
#define WNOWAIT 0x01000000
#endif

#endif /* _WIN32 && !SANDVIK_COMPAT_SYS_WAIT */
