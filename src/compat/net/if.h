#pragma once
/*
 * compat/net/if.h -- mingw-w64 has no <net/if.h>.
 *
 * Only src/system/os_constants.hpp includes it (for the IFF_* interface flags);
 * values are the Linux/Android ones and no interface query is implemented here.
 */
#if defined(_WIN32) && !defined(SANDVIK_COMPAT_NET_IF)
#define SANDVIK_COMPAT_NET_IF 1

#ifndef IFF_UP
#define IFF_UP 0x1
#endif
#ifndef IFF_BROADCAST
#define IFF_BROADCAST 0x2
#endif
#ifndef IFF_DEBUG
#define IFF_DEBUG 0x4
#endif
#ifndef IFF_LOOPBACK
#define IFF_LOOPBACK 0x8
#endif
#ifndef IFF_POINTOPOINT
#define IFF_POINTOPOINT 0x10
#endif
#ifndef IFF_NOTRAILERS
#define IFF_NOTRAILERS 0x20
#endif
#ifndef IFF_RUNNING
#define IFF_RUNNING 0x40
#endif
#ifndef IFF_NOARP
#define IFF_NOARP 0x80
#endif
#ifndef IFF_PROMISC
#define IFF_PROMISC 0x100
#endif
#ifndef IFF_ALLMULTI
#define IFF_ALLMULTI 0x200
#endif
#ifndef IFF_MASTER
#define IFF_MASTER 0x400
#endif
#ifndef IFF_SLAVE
#define IFF_SLAVE 0x800
#endif
#ifndef IFF_MULTICAST
#define IFF_MULTICAST 0x1000
#endif
#ifndef IFF_PORTSEL
#define IFF_PORTSEL 0x2000
#endif
#ifndef IFF_AUTOMEDIA
#define IFF_AUTOMEDIA 0x4000
#endif
#ifndef IFF_DYNAMIC
#define IFF_DYNAMIC 0x8000
#endif

#endif /* _WIN32 && !SANDVIK_COMPAT_NET_IF */
