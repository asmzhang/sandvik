#pragma once
/*
 * compat/net/if_arp.h -- mingw-w64 has no <net/if_arp.h>.
 *
 * Only src/system/os_constants.hpp includes it (for ARPHRD_*); Linux/Android values.
 */
#if defined(_WIN32) && !defined(SANDVIK_COMPAT_NET_IF_ARP)
#define SANDVIK_COMPAT_NET_IF_ARP 1

#ifndef ARPHRD_ETHER
#define ARPHRD_ETHER 1
#endif
#ifndef ARPHRD_LOOPBACK
#define ARPHRD_LOOPBACK 772
#endif

#endif /* _WIN32 && !SANDVIK_COMPAT_NET_IF_ARP */
