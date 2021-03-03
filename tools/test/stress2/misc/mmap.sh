#!/bin/sh

# Test scenario by Michiel Boland <michiel@boland.org>

# panic: pmap_remove_all: page 0xc491f000 is fictitious

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

odir=`pwd`
cd /tmp
sed '1,/^EOF/d' < $odir/$0 > mmap.c
mycc -o mmap -Wall mmap.c
rm -f mmap.c

./mmap
rm -f ./mmap
exit

EOF
#include <sys/types.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

static const off_t map_address = 0xa0000;
static const size_t map_size = 0x1000;

static int testit(int fd)
{
        void *p;
        int rv;

        p = mmap(NULL, 2 * map_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd,
                  map_address);
        if (p == MAP_FAILED) {
                perror("mmap");
                return -1;
        }
        rv = *(char *) p;
        if (munmap(p, map_size) == -1) {
                perror("munmap");
                return -1;
        }
        return rv;
}

int main(void)
{
        int fd, rv;

        fd = open("/dev/mem", O_RDWR);
        if (fd == -1) {
                perror("open");
                return 1;
        }
        rv = testit(fd);
        close(fd);
        return rv;
}

