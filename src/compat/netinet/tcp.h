#pragma once
/*
 * compat/netinet/tcp.h -- mingw-w64 has no <netinet/tcp.h>.
 *
 * Only src/system/os_constants.hpp includes it (for TCP_*); Linux/Android values.
 */
#if defined(_WIN32) && !defined(SANDVIK_COMPAT_NETINET_TCP)
#define SANDVIK_COMPAT_NETINET_TCP 1

#ifndef TCP_NODELAY
#define TCP_NODELAY 1
#endif
#ifndef TCP_MAXSEG
#define TCP_MAXSEG 2
#endif
#ifndef TCP_KEEPIDLE
#define TCP_KEEPIDLE 4
#endif
#ifndef TCP_KEEPINTVL
#define TCP_KEEPINTVL 5
#endif
#ifndef TCP_KEEPCNT
#define TCP_KEEPCNT 6
#endif
#ifndef TCP_USER_TIMEOUT
#define TCP_USER_TIMEOUT 18
#endif

#endif /* _WIN32 && !SANDVIK_COMPAT_NETINET_TCP */
