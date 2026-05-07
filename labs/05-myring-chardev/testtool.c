#include <fcntl.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define MYRING_IOC_MAGIC 'r'
struct myring_state { uint32_t head, tail, capacity, _reserved; };
#define MYRING_GET_STATE _IOR(MYRING_IOC_MAGIC, 0, struct myring_state)
#define MYRING_RESET     _IO (MYRING_IOC_MAGIC, 1)

static void usage(void)
{
    fprintf(stderr,
            "usage: testtool <state|reset|read|write> [args]\n"
            "  state          : print head/tail/capacity\n"
            "  reset          : empty the ring\n"
            "  write <text>   : write text\n"
            "  read <n>       : read up to n bytes\n");
    exit(2);
}

int main(int argc, char **argv)
{
    if (argc < 2) usage();

    int fd = open("/dev/myring", O_RDWR);
    if (fd < 0) { perror("open"); return 1; }

    if (!strcmp(argv[1], "state")) {
        struct myring_state st;
        if (ioctl(fd, MYRING_GET_STATE, &st) < 0) { perror("ioctl"); return 1; }
        printf("head=%u tail=%u capacity=%u\n", st.head, st.tail, st.capacity);

    } else if (!strcmp(argv[1], "reset")) {
        if (ioctl(fd, MYRING_RESET) < 0) { perror("ioctl"); return 1; }

    } else if (!strcmp(argv[1], "write") && argc >= 3) {
        ssize_t r = write(fd, argv[2], strlen(argv[2]));
        if (r < 0) { perror("write"); return 1; }
        printf("wrote %zd bytes\n", r);

    } else if (!strcmp(argv[1], "read") && argc >= 3) {
        size_t n = strtoul(argv[2], NULL, 0);
        char *buf = malloc(n + 1);
        ssize_t r = read(fd, buf, n);
        if (r < 0) { perror("read"); return 1; }
        buf[r] = 0;
        printf("read %zd bytes: %s\n", r, buf);
        free(buf);

    } else usage();

    close(fd);
    return 0;
}
