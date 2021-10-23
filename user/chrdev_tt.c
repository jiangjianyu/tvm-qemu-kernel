#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <getopt.h>
#include <errno.h>
#include <emmintrin.h>

#include <sys/mman.h>
#define MAX_BUF_SIZE 256

static void print_usage(const char *prog)
{
    fprintf(stdout,"Usage: %s [-hdwr]\n",prog);
    fprintf(stdout,"\t-d --device\t\t\t\t: device to use.\n");
    fprintf(stdout,"\t-w --write\t\t\t\t: write a new mesage .\n");
    fprintf(stdout,"\t-r --read\t\t\t\t: read the message manage by char device.\n");
    fprintf(stdout,"\t-h --help\t\t\t\t: print this message\n");
    return;
}

static const struct option lopts[] = {
    { "device", required_argument, 0, 'd' },
    { "write", required_argument, 0, 'w' },
    { "read", no_argument, 0, 'r' },
    { "help", no_argument, 0, 'h' },
    { 0, 0, 0, 0 },
};

#define DEV_NAME "/dev/tvm-vta-0"

int main(int argc, char* argv[])
{
    struct stat st_dev;
    const char* device = DEV_NAME;
    char user_msg[MAX_BUF_SIZE];
    int option_index = 0;
    int c = -1;
    int msg_len = 0;
    int ret, fd = 0;

    if (stat(device, &st_dev) == -1)
    {
        fprintf(stderr,"stat: %s\n", strerror(errno));
        return -1;
    }

    fd = open(device, O_RDWR);
    if (fd < 0){
        fprintf(stderr,"open: %s\n", strerror(errno));
        return -1;
    }

    // ioctl(fd, 1);

    int* address = mmap (NULL, 4096 * 100, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

    if (address == MAP_FAILED) {
        fprintf(stderr, "error in mmap\n");
        return 0;
    }

    for (int i = 0;i < 100;i++) {
        address[4096 / sizeof(int) * i] = (int)100;
    }

    ioctl(fd, 3);

    munmap(address, 4096 * 100);

    close(fd);
    return 0;
}
