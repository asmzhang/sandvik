#pragma once
/*
 * compat/netdb.h -- mingw-w64 has no <netdb.h>.
 *
 * src/system/os_constants.hpp publishes the addrinfo/getnameinfo flag values to
 * Java through OsConstants; the numbers below are glibc's (the fallback used by
 * that header when a macro is undefined reports -1 instead). Nothing in the
 * Windows build performs name resolution -- winsock is never initialised -- so
 * declaring the constants is all this shim has to do.
 */
#if defined(_WIN32) && !defined(SANDVIK_COMPAT_NETDB)
#define SANDVIK_COMPAT_NETDB 1

#define AI_PASSIVE 0x0001
#define AI_CANONNAME 0x0002
#define AI_NUMERICHOST 0x0004
#define AI_V4MAPPED 0x0008
#define AI_ALL 0x0010
#define AI_ADDRCONFIG 0x0020
#define AI_NUMERICSERV 0x0400

#define NI_NOFQDN 0x0001
#define NI_NUMERICHOST 0x0002
#define NI_NAMEREQD 0x0004
#define NI_NUMERICSERV 0x0008
#define NI_DGRAM 0x0010

#define EAI_BADFLAGS -1
#define EAI_NONAME -2
#define EAI_AGAIN -3
#define EAI_FAIL -4
#define EAI_NODATA -5
#define EAI_FAMILY -6
#define EAI_SOCKTYPE -7
#define EAI_SERVICE -8
#define EAI_MEMORY -10
#define EAI_SYSTEM -11
#define EAI_OVERFLOW -12

#endif /* _WIN32 && !SANDVIK_COMPAT_NETDB */
