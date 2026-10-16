#pragma once
/*
 * compat/netinet/in.h -- mingw-w64 has no <netinet/in.h>.
 *
 * Only src/system/os_constants.hpp includes it (for IPPROTO_*); values are the
 * Linux/Android ones and nothing calls the socket API here.
 */
#if defined(_WIN32) && !defined(SANDVIK_COMPAT_NETINET_IN)
#define SANDVIK_COMPAT_NETINET_IN 1

#ifndef IPPROTO_IP
#define IPPROTO_IP 0
#endif
#ifndef IPPROTO_ICMP
#define IPPROTO_ICMP 1
#endif
#ifndef IPPROTO_TCP
#define IPPROTO_TCP 6
#endif
#ifndef IPPROTO_UDP
#define IPPROTO_UDP 17
#endif
#ifndef IPPROTO_IPV6
#define IPPROTO_IPV6 41
#endif
#ifndef IPPROTO_ICMPV6
#define IPPROTO_ICMPV6 58
#endif
#ifndef IPPROTO_RAW
#define IPPROTO_RAW 255
#endif

#endif /* _WIN32 && !SANDVIK_COMPAT_NETINET_IN */
