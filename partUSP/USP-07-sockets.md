# USP 7 — Sockets — TCP & UDP

> Sockets are how Unix programs talk over the network. Same API works for TCP, UDP, Unix domain sockets.

## The 5-call life of a TCP server

```c
int s = socket(AF_INET, SOCK_STREAM, 0);
struct sockaddr_in a = { .sin_family = AF_INET, .sin_port = htons(8080), .sin_addr.s_addr = INADDR_ANY };
bind(s, (struct sockaddr*)&a, sizeof(a));
listen(s, 64);
int c = accept(s, NULL, NULL);
read(c, buf, ...); write(c, ...);
close(c);
```

Five system calls:
1. **socket** — create endpoint.
2. **bind** — assign address.
3. **listen** — mark as a server (backlog of pending connections).
4. **accept** — block until a client connects; returns a new fd for that connection.
5. **read/write/close** — talk and disconnect.

## The 3-call life of a TCP client

```c
int s = socket(AF_INET, SOCK_STREAM, 0);
struct sockaddr_in a = { .sin_family = AF_INET, .sin_port = htons(8080) };
inet_pton(AF_INET, "1.2.3.4", &a.sin_addr);
connect(s, (struct sockaddr*)&a, sizeof(a));
write(s, ...); read(s, ...); close(s);
```

`connect` performs TCP's three-way handshake (SYN, SYN-ACK, ACK) and returns when established (or fails).

## Address structures

```c
struct sockaddr_in {                 // IPv4
    sa_family_t  sin_family;         // AF_INET
    in_port_t    sin_port;            // big-endian
    struct in_addr sin_addr;
};
struct sockaddr_in6 {                // IPv6
    sa_family_t     sin6_family;
    in_port_t       sin6_port;
    uint32_t        sin6_flowinfo;
    struct in6_addr sin6_addr;
    uint32_t        sin6_scope_id;
};
struct sockaddr_un {                 // Unix domain
    sa_family_t sun_family;          // AF_UNIX
    char        sun_path[108];
};
```

All cast to `struct sockaddr *` for the syscall API. **Network byte order** (big-endian) for ports/IPs — use `htons`, `htonl`, `ntohs`, `ntohl`.

## Address families & types

- `AF_INET` / `AF_INET6` — IPv4 / IPv6.
- `AF_UNIX` — local IPC over a filesystem path.
- `AF_PACKET` — raw L2 (Ethernet frames). For sniffers, custom protocols.

Socket types:
- `SOCK_STREAM` — TCP, reliable, ordered, byte stream.
- `SOCK_DGRAM` — UDP, datagrams, unreliable, may be reordered.
- `SOCK_RAW` — raw IP. Need root.
- `SOCK_SEQPACKET` — message-oriented but reliable. Used by some IPC protocols.

## TCP vs UDP

TCP:
- Reliable, ordered, congestion-controlled.
- Stateful — connection setup/teardown.
- Bytes streaming with no message boundaries.

UDP:
- Unreliable, unordered.
- Stateless — just send packets.
- Use cases: DNS, VoIP, video streaming, time sync, game state, anything with own retransmit logic.

## A minimal echo server

```c
int s = socket(AF_INET, SOCK_STREAM, 0);
int yes = 1;
setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));   // reuse port immediately
bind(...); listen(s, 64);
for (;;) {
    int c = accept(s, NULL, NULL);
    if (c < 0) { perror("accept"); continue; }
    char buf[4096];
    ssize_t n;
    while ((n = read(c, buf, sizeof(buf))) > 0) {
        if (write(c, buf, n) < 0) break;
    }
    close(c);
}
```

This handles **one client at a time**. To handle many: fork per connection, thread per connection, or `epoll`. We'll do epoll next chapter.

## A minimal UDP echo server

```c
int s = socket(AF_INET, SOCK_DGRAM, 0);
bind(...);
for (;;) {
    char buf[1500]; struct sockaddr_in peer; socklen_t plen = sizeof(peer);
    ssize_t n = recvfrom(s, buf, sizeof(buf), 0, (struct sockaddr*)&peer, &plen);
    sendto(s, buf, n, 0, (struct sockaddr*)&peer, plen);
}
```

UDP is simpler — no listen/accept, just receive and send.

## SO_REUSEADDR / SO_REUSEPORT

- `SO_REUSEADDR` — let you bind even if the port is in TIME_WAIT (recently closed connection still in kernel state).
- `SO_REUSEPORT` — multiple processes can bind to the same port (the kernel load-balances incoming connections). Modern servers use this for multi-process scaling.

## Other useful socket options

- `TCP_NODELAY` — disable Nagle's algorithm (small writes go out immediately). For latency-sensitive apps.
- `SO_KEEPALIVE` — kernel sends probes if connection is idle, detects dead peers.
- `SO_RCVBUF` / `SO_SNDBUF` — kernel buffer sizes per socket.
- `SO_LINGER` — control close behavior.

## DNS resolution — getaddrinfo

```c
struct addrinfo hints = { .ai_family = AF_UNSPEC, .ai_socktype = SOCK_STREAM };
struct addrinfo *res;
getaddrinfo("example.com", "80", &hints, &res);
for (struct addrinfo *p = res; p; p = p->ai_next) {
    int s = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
    if (s < 0) continue;
    if (connect(s, p->ai_addr, p->ai_addrlen) == 0) { /* success */ break; }
    close(s);
}
freeaddrinfo(res);
```

`getaddrinfo` does DNS, returns a linked list of candidates. **Always use this**, never raw `gethostbyname` (deprecated, not thread-safe).

## Unix domain sockets

`AF_UNIX` with a filesystem path. Faster than TCP loopback (no checksum, no protocol stack overhead). Used in:
- D-Bus,
- systemd journal,
- container runtimes,
- Wayland / X11.

Bonus: pass file descriptors between processes via `SCM_RIGHTS` ancillary messages. Foundational for socket activation, privilege separation.

## Read & write semantics

- `read` returns `0` on EOF, `-1` on error, otherwise bytes read.
- `write` may return less than requested → loop.
- `recv`/`send` are like read/write with extra flags (`MSG_PEEK`, `MSG_DONTWAIT`, `MSG_NOSIGNAL`).

Always loop on partial writes:
```c
ssize_t write_all(int fd, const void *buf, size_t n) {
    const char *p = buf; size_t left = n;
    while (left) {
        ssize_t w = write(fd, p, left);
        if (w < 0) { if (errno == EINTR) continue; return -1; }
        p += w; left -= w;
    }
    return n;
}
```

## TCP states (briefly)

```
LISTEN → SYN_RCVD → ESTABLISHED → CLOSE_WAIT or FIN_WAIT_1 → ... → TIME_WAIT → CLOSED
```

`netstat -an` or `ss -ant` shows TCP states for all sockets. **TIME_WAIT** is normal for the active closer; it's why `SO_REUSEADDR` is often needed.

## Common bugs

1. **Endianness** — forgetting `htons`/`htonl`.
2. **Address structure size** — `sizeof(struct sockaddr)` (16) vs the right `sockaddr_in` size (16). For `sockaddr_un`, varies.
3. **Buffer too small** for a packet → truncation on UDP recvfrom.
4. **Not handling EAGAIN** with non-blocking sockets.
5. **Concurrent close + read** races — careful with shared sockets.
6. **SIGPIPE on TCP write** to closed connection — set `MSG_NOSIGNAL` or ignore.

## Try it

1. Echo server (TCP). Verify with `nc 127.0.0.1 8080`.
2. Echo server (UDP). Verify with `nc -u 127.0.0.1 8080`.
3. Resolve a hostname with `getaddrinfo`; print all returned addresses.
4. Use a Unix domain socket. Pass a file descriptor from one process to another via SCM_RIGHTS.
5. Use `setsockopt(TCP_NODELAY)`. Compare round-trip latency to default.
6. Bonus: write an HTTP/0.9 fetcher (`GET /\r\n\r\n`, then read).

## Read deeper

- TLPI chapters 56–62 (sockets).
- *Unix Network Programming* by Stevens — the deepest treatment.
- `man 7 socket`, `man 7 ip`, `man 7 tcp`, `man 7 udp`, `man 7 unix`.

Next → [I/O multiplexing — select, poll, epoll, io_uring](USP-08-epoll.md).
