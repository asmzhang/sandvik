#pragma once
/*
 * compat/sys/socket.h -- mingw-w64 has no <sys/socket.h>.
 *
 * src/system/os_constants.hpp is the only includer: it republishes the AF_,
 * SOCK_, SOL_, SO_, SHUT_ and MSG_ values to Java. The numbers here are the
 * Linux/Android ones. winsock2 defines its own AF_ and SOCK_ through
 * <winsock2.h>, which this build never includes, and nothing here calls
 * socket(). Every macro is #ifndef-guarded, so a future winsock include would
 * take precedence instead of colliding.
 */
#if defined(_WIN32) && !defined(SANDVIK_COMPAT_SYS_SOCKET)
#define SANDVIK_COMPAT_SYS_SOCKET 1

#ifndef AF_UNSPEC
#define AF_UNSPEC 0
#endif
#ifndef AF_UNIX
#define AF_UNIX 1
#endif
#ifndef AF_INET
#define AF_INET 2
#endif
#ifndef AF_INET6
#define AF_INET6 10
#endif
#ifndef AF_NETLINK
#define AF_NETLINK 16
#endif
#ifndef AF_PACKET
#define AF_PACKET 17
#endif
#ifndef AF_VSOCK
#define AF_VSOCK 40
#endif

#ifndef SOCK_STREAM
#define SOCK_STREAM 1
#endif
#ifndef SOCK_DGRAM
#define SOCK_DGRAM 2
#endif
#ifndef SOCK_RAW
#define SOCK_RAW 3
#endif
#ifndef SOCK_SEQPACKET
#define SOCK_SEQPACKET 5
#endif
#ifndef SOCK_CLOEXEC
#define SOCK_CLOEXEC 02000000
#endif
#ifndef SOCK_NONBLOCK
#define SOCK_NONBLOCK 04000
#endif

#ifndef SOL_SOCKET
#define SOL_SOCKET 1
#endif
#ifndef SOL_UDP
#define SOL_UDP 17
#endif

#ifndef SO_DEBUG
#define SO_DEBUG 1
#endif
#ifndef SO_REUSEADDR
#define SO_REUSEADDR 2
#endif
#ifndef SO_TYPE
#define SO_TYPE 3
#endif
#ifndef SO_ERROR
#define SO_ERROR 4
#endif
#ifndef SO_DONTROUTE
#define SO_DONTROUTE 5
#endif
#ifndef SO_BROADCAST
#define SO_BROADCAST 6
#endif
#ifndef SO_SNDBUF
#define SO_SNDBUF 7
#endif
#ifndef SO_RCVBUF
#define SO_RCVBUF 8
#endif
#ifndef SO_KEEPALIVE
#define SO_KEEPALIVE 9
#endif
#ifndef SO_OOBINLINE
#define SO_OOBINLINE 10
#endif
#ifndef SO_LINGER
#define SO_LINGER 13
#endif
#ifndef SO_PASSCRED
#define SO_PASSCRED 16
#endif
#ifndef SO_PEERCRED
#define SO_PEERCRED 17
#endif
#ifndef SO_RCVLOWAT
#define SO_RCVLOWAT 18
#endif
#ifndef SO_SNDLOWAT
#define SO_SNDLOWAT 19
#endif
#ifndef SO_RCVTIMEO
#define SO_RCVTIMEO 20
#endif
#ifndef SO_SNDTIMEO
#define SO_SNDTIMEO 21
#endif
#ifndef SO_BINDTODEVICE
#define SO_BINDTODEVICE 25
#endif
#ifndef SO_PROTOCOL
#define SO_PROTOCOL 38
#endif
#ifndef SO_DOMAIN
#define SO_DOMAIN 39
#endif

#ifndef SHUT_RD
#define SHUT_RD 0
#endif
#ifndef SHUT_WR
#define SHUT_WR 1
#endif
#ifndef SHUT_RDWR
#define SHUT_RDWR 2
#endif

#ifndef MSG_OOB
#define MSG_OOB 1
#endif
#ifndef MSG_PEEK
#define MSG_PEEK 2
#endif
#ifndef MSG_DONTROUTE
#define MSG_DONTROUTE 4
#endif
#ifndef MSG_CTRUNC
#define MSG_CTRUNC 8
#endif
#ifndef MSG_TRUNC
#define MSG_TRUNC 0x20
#endif
#ifndef MSG_EOR
#define MSG_EOR 0x80
#endif
#ifndef MSG_WAITALL
#define MSG_WAITALL 0x100
#endif

/*
 * libcore.io.Linux.listen() is the only socket call the Android natives make
 * directly (`::listen(fd, backlog)` on a CRT fd). The declaration is skipped in
 * translation units that pulled in winsock, which declares its own
 * listen(SOCKET, int); the definition in src/compat/posix_win32.cpp reports
 * ENOSYS, because the AF_UNIX endpoints it would serve are not reachable from
 * the CRT fd layer.
 */
#if !defined(_WINSOCK2API_) && !defined(_WINSOCKAPI_)
#ifdef __cplusplus
extern "C" {
#endif

int listen(int fd, int backlog);

#ifdef __cplusplus
}
#endif
#endif /* !winsock */

#endif /* _WIN32 && !SANDVIK_COMPAT_SYS_SOCKET */
