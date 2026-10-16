#pragma once
/*
 * compat/sys/un.h -- mingw-w64 has no <sys/un.h>.
 *
 * src/system/os_constants.hpp includes it but names nothing from it (no
 * sockaddr_un), and the only AF_UNIX user -- src/zygote.cpp -- is excluded from
 * the Windows build, so an empty shim is enough to keep the include set intact.
 */
#if defined(_WIN32) && !defined(SANDVIK_COMPAT_SYS_UN)
#define SANDVIK_COMPAT_SYS_UN 1

#endif /* _WIN32 && !SANDVIK_COMPAT_SYS_UN */
