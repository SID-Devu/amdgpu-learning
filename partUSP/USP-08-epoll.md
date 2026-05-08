# USP 8 — I/O multiplexing: select, poll, epoll, io_uring

> Handle 10,000 connections in one thread. The reason async servers exist.

## The problem

A simple TCP server `accept`s, then `read`s. While reading from one client, the next client waits. **One client at a time.**

Solutions:
1. **Process per connection** — `fork` for each. Apache prefork. Heavy.
2. **Thread per connection** — `pthread_create`. Lighter; still limited by # threads.
3. **One thread, many fds, async** — multiplexing. The way modern servers (nginx, Node, Redis) work.

We focus on (3).

## select — the original (1980s)

```c
fd_set rfds; FD_ZERO(&rfds); FD_SET(fd, &rfds);
struct timeval tv = { .tv_sec = 5 };
int n = select(fd + 1, &rfds, NULL, NULL, &tv);
if (n > 0 && FD_ISSET(fd, &rfds)) /* readable */;
```

- Bitmap-based. Limited to FD_SETSIZE (typically 1024).
- O(n) on every call (kernel scans all bits).
- Awful for many fds.

Use only for compatibility on ancient systems.

## poll — improvement

```c
struct pollfd pfd[N];
pfd[0] = (struct pollfd){ .fd = sockfd, .events = POLLIN };
int n = poll(pfd, 1, 5000);   // 5s timeout in ms
if (pfd[0].revents & POLLIN) /* readable */;
```

- Array-based. No FD_SETSIZE limit.
- Still O(n) — kernel scans the whole array each call.
- Decent up to a few hundred fds.

## epoll — Linux's scalable answer

```c
int ep = epoll_create1(0);
struct epoll_event ev = { .events = EPOLLIN, .data.fd = sockfd };
epoll_ctl(ep, EPOLL_CTL_ADD, sockfd, &ev);

struct epoll_event events[64];
int n = epoll_wait(ep, events, 64, /*timeout_ms=*/-1);
for (int i = 0; i < n; i++) {
    int fd = events[i].data.fd;
    if (events[i].events & EPOLLIN) /* readable */;
}
```

- O(1) — kernel keeps a ready-list internally.
- No FD limit (well, ulimit -n).
- Edge-triggered or level-triggered modes.

### Level-triggered (LT, default)
"Tell me as long as fd is readable." Easy to use; close to poll's semantics.

### Edge-triggered (ET, `EPOLLET`)
"Tell me only when fd **becomes** readable." Faster; harder. You **must** drain — read until EAGAIN. Forgetting causes hangs.

```c
ev.events = EPOLLIN | EPOLLET;
```

Modern high-perf code uses ET + non-blocking sockets.

### Non-blocking sockets

```c
int flags = fcntl(fd, F_GETFL); fcntl(fd, F_SETFL, flags | O_NONBLOCK);
```

Now `read`/`write` return -1 with `errno=EAGAIN` instead of blocking. Required for ET; also good with LT for safety.

## A real epoll server skeleton

```c
int s = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int));
bind(s, ...); listen(s, 128);

int ep = epoll_create1(EPOLL_CLOEXEC);
epoll_ctl(ep, EPOLL_CTL_ADD, s, &(struct epoll_event){.events=EPOLLIN, .data.fd=s});

for (;;) {
    struct epoll_event evs[64];
    int n = epoll_wait(ep, evs, 64, -1);
    for (int i = 0; i < n; i++) {
        int fd = evs[i].data.fd;
        if (fd == s) {
            for (;;) {
                int c = accept4(s, NULL, NULL, SOCK_NONBLOCK | SOCK_CLOEXEC);
                if (c < 0) { if (errno == EAGAIN) break; perror("accept"); break; }
                struct epoll_event cev = {.events = EPOLLIN | EPOLLET, .data.fd = c};
                epoll_ctl(ep, EPOLL_CTL_ADD, c, &cev);
            }
        } else {
            handle_data(fd, ep);  // read non-blocking; on EAGAIN, return; on EOF, close + EPOLL_CTL_DEL
        }
    }
}
```

This handles tens of thousands of connections in one thread. Add multiple worker threads, each with their own epoll fd, and use `SO_REUSEPORT` for load-balancing — modern nginx-style architecture.

## Edge-triggered drain rule

For each readable event:
```c
for (;;) {
    ssize_t n = read(fd, buf, sizeof(buf));
    if (n < 0) { if (errno == EAGAIN) break; close_connection(); break; }
    if (n == 0) { close_connection(); break; }   // EOF
    process(buf, n);
}
```

If you don't drain, you may not get another event until **more** data arrives — your existing data sits there.

## EPOLLOUT — when fd is writable

For `write`-heavy code, register `EPOLLOUT` only when you have data to send and the kernel buffer is full. When the kernel can accept more, you'll get an event.

Common trick: lazily install `EPOLLOUT` only when needed; remove it when buffer is empty (`EPOLL_CTL_MOD`).

## io_uring — the modern way

epoll tells you "fd is ready, now you do the syscall." io_uring lets you submit operations and receive completions — true async I/O for files, sockets, etc.

```c
#include <liburing.h>
struct io_uring ring;
io_uring_queue_init(8, &ring, 0);
struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);
io_uring_prep_read(sqe, fd, buf, len, off);
io_uring_submit(&ring);

struct io_uring_cqe *cqe;
io_uring_wait_cqe(&ring, &cqe);
ssize_t n = cqe->res;
io_uring_cqe_seen(&ring, cqe);
```

- Two ring buffers (submission, completion) shared with the kernel.
- Reduces syscall overhead drastically.
- Supports nearly every I/O primitive.

io_uring is the future. For now, epoll is more universal; many libraries support both.

## kqueue (BSD/macOS)

Equivalent to epoll on BSD/macOS. Slightly different API; same semantics.

## eventfd / signalfd / timerfd in epoll

- **eventfd**: send "wake up" between threads as a fd readable.
- **signalfd**: turn signals into fd events.
- **timerfd**: turn timers into fd events.

Drop them all into one epoll loop — clean, single-threaded async server.

## Common bugs

1. **Forgetting non-blocking** with edge-triggered → hang.
2. **Not draining** with ET → starvation.
3. **Adding the same fd twice** to epoll without `EPOLL_CTL_MOD` → EEXIST.
4. **Close-before-EPOLL_CTL_DEL** while another thread is in `epoll_wait` — race window. Use `EPOLLEXCLUSIVE` or careful single-threading.
5. **Leak**: epoll_ctl(ADD) on a duplicated fd; the ADD is on the underlying file, not on the descriptor. Use `EPOLL_CTL_DEL` properly.

## Try it

1. Implement an `epoll`-based echo server. Use ET + non-blocking. Test with 1000 simultaneous clients (use `ab` or write a small load tool).
2. Add `SO_REUSEPORT` and 4 worker processes. Each binds and `epoll_wait`s. Verify load is spread.
3. Use `timerfd` to fire every 1 second; integrate into your epoll loop alongside sockets.
4. Use `signalfd` for SIGINT; cleanly shut down all connections on Ctrl-C.
5. Benchmark: poll vs epoll on 10000 fds. Most are idle. Measure CPU.
6. Bonus: try `liburing` for the same echo server. Compare throughput to epoll on fast network.

## Read deeper

- TLPI chapter 63 (alternate I/O models).
- "The C10K problem" (Dan Kegel, 2002) — original analysis of the scalability problem.
- `man 7 epoll`, `man 2 epoll_ctl`, `man 2 epoll_wait`.
- io_uring docs: kernel.dk and Jens Axboe's blog.

Next → [pthreads deep](USP-09-pthreads.md).
