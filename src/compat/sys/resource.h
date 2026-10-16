#pragma once
/*
 * compat/sys/resource.h -- mingw-w64 has no <sys/resource.h>.
 *
 * Only src/system/os_constants.hpp includes it (for RLIMIT_*), so the constants
 * below are the Linux/Android ones and no getrlimit()/setrlimit() is provided.
 */
#if defined(_WIN32) && !defined(SANDVIK_COMPAT_SYS_RESOURCE)
#define SANDVIK_COMPAT_SYS_RESOURCE 1

#ifndef RLIMIT_CPU
#define RLIMIT_CPU 0
#endif
#ifndef RLIMIT_FSIZE
#define RLIMIT_FSIZE 1
#endif
#ifndef RLIMIT_DATA
#define RLIMIT_DATA 2
#endif
#ifndef RLIMIT_STACK
#define RLIMIT_STACK 3
#endif
#ifndef RLIMIT_CORE
#define RLIMIT_CORE 4
#endif
#ifndef RLIMIT_RSS
#define RLIMIT_RSS 5
#endif
#ifndef RLIMIT_NPROC
#define RLIMIT_NPROC 6
#endif
#ifndef RLIMIT_NOFILE
#define RLIMIT_NOFILE 7
#endif
#ifndef RLIMIT_MEMLOCK
#define RLIMIT_MEMLOCK 8
#endif
#ifndef RLIMIT_AS
#define RLIMIT_AS 9
#endif

#endif /* _WIN32 && !SANDVIK_COMPAT_SYS_RESOURCE */
