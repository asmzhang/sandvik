/*
 * posix_win32_poll.cpp -- Windows poll() for the Android natives.
 *
 * Kept apart from posix_win32.cpp because it needs winsock: winsock declares
 * listen(SOCKET, int), which clashes with the POSIX listen() shim defined
 * there.
 *
 * libcore.io.Linux.poll() receives the Linux POLL* values published by
 * OsConstants, marshalled through libcore.io.StructPollfd; WSAPoll expects the
 * winsock values, so both directions are translated. The file descriptors come
 * from libcore.io.Linux.open(), i.e. they are CRT fds, hence the
 * _get_osfhandle() conversion.
 *
 * Only the function's linkage is shared with compat/sys/poll.h: this file does
 * NOT include it, because winsock2.h defines its own struct pollfd (SOCKET fd,
 * i.e. a different layout). The local struct below restates the POSIX/Linux
 * layout that Linux.cpp marshals, and the static_assert keeps the two in sync.
 *
 * Nothing in this file is compiled on POSIX hosts.
 */

#if defined(_WIN32)

#include <cerrno>
#include <vector>

#include <winsock2.h>
#include <windows.h>

#include <io.h>

// The layout libcore.io.StructPollfd marshals into on Linux (see
// compat/sys/poll.h, which Linux.cpp includes).
struct sandvik_pollfd {
	int fd;
	short events;
	short revents;
};

static_assert(sizeof(struct sandvik_pollfd) == 8, "pollfd layout must match compat/sys/poll.h");

namespace {

constexpr short kPollIn = 0x001;
constexpr short kPollPri = 0x002;
constexpr short kPollOut = 0x004;
constexpr short kPollErr = 0x008;
constexpr short kPollHup = 0x010;
constexpr short kPollNval = 0x020;

void ensureWinsock() {
	static const int started = []() {
		WSADATA data;
		return WSAStartup(MAKEWORD(2, 2), &data);
	}();
	(void)started;
}

short eventsToWinsock(short events) {
	short result = 0;
	if ((events & kPollIn) != 0) {
		result |= POLLRDNORM;
	}
	if ((events & kPollPri) != 0) {
		result |= POLLRDBAND;
	}
	if ((events & kPollOut) != 0) {
		result |= POLLWRNORM;
	}
	if ((events & kPollErr) != 0) {
		result |= POLLERR;
	}
	if ((events & kPollHup) != 0) {
		result |= POLLHUP;
	}
	if ((events & kPollNval) != 0) {
		result |= POLLNVAL;
	}
	return result;
}

short eventsFromWinsock(short revents) {
	short result = 0;
	if ((revents & POLLRDNORM) != 0) {
		result |= kPollIn;
	}
	if ((revents & POLLRDBAND) != 0) {
		result |= kPollPri;
	}
	if ((revents & POLLWRNORM) != 0) {
		result |= kPollOut;
	}
	if ((revents & POLLERR) != 0) {
		result |= kPollErr;
	}
	if ((revents & POLLHUP) != 0) {
		result |= kPollHup;
	}
	if ((revents & POLLNVAL) != 0) {
		result |= kPollNval;
	}
	return result;
}

}  // namespace

extern "C" {

int poll(struct sandvik_pollfd* fds, unsigned long nfds, int timeout) {
	if (fds == nullptr && nfds != 0) {
		errno = EINVAL;
		return -1;
	}

	ensureWinsock();

	std::vector<WSAPOLLFD> sockets(nfds);
	for (unsigned long i = 0; i < nfds; ++i) {
		sockets[i].fd = static_cast<SOCKET>(_get_osfhandle(fds[i].fd));
		sockets[i].events = eventsToWinsock(fds[i].events);
		sockets[i].revents = 0;
	}

	const int ready = WSAPoll(sockets.data(), static_cast<ULONG>(nfds), timeout);
	if (ready == SOCKET_ERROR) {
		errno = EINVAL;
		return -1;
	}
	for (unsigned long i = 0; i < nfds; ++i) {
		fds[i].revents = eventsFromWinsock(sockets[i].revents);
	}
	return ready;
}

}  // extern "C"

#endif /* _WIN32 */
