#!/bin/sh

# Test scenario for Bug 261707.
# Based on Kostik's shm_super.sh
# "panic: vm_page_free_prep: freeing mapped page ..." seen.

# Test scenario suggestion by kib@

. ../default.cfg

cat > /tmp/shm3.c <<EOF
#include <sys/param.h>
#include <sys/mman.h>

#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define M(x)    ((x) * 1024 * 1024)
#define SZ      M(256)

int
main(void)
{
        off_t cnt;
        void *ptr;
        int error, shmfd;
	char buf[128];

        shmfd = shm_open(SHM_ANON, O_CREAT | O_RDWR, 0600);
        if (shmfd == -1)
                err(1, "shm_open");
        error = ftruncate(shmfd, SZ);
        if (error == -1)
                err(1, "truncate");
        memset(buf, 0, sizeof(buf));
        for (cnt = 0; cnt < SZ; cnt += sizeof(buf)) {
                error = write(shmfd, buf, sizeof(buf));
                if (error == -1)
                        err(1, "write");
                else if (error != sizeof(buf))
                        errx(1, "short write %d", (int)error);
        }
        ptr = mmap(NULL, SZ, PROT_READ | PROT_WRITE, MAP_SHARED,
            shmfd, 0);
        if (ptr == MAP_FAILED)
                err(1, "mmap");
        for (cnt = 0; cnt < SZ; cnt += PAGE_SIZE)
                *((char *)ptr + cnt) = 0;
	close(shmfd);
	sleep(30);
}
EOF
mycc -o /tmp/shm3 -Wall -Wextra -O0 -g /tmp/shm3.c || exit 1
rm /tmp/shm3.c

../testcases/swap/swap -t 3m -i 50 > /dev/null &
sleep 30
for i in `jot 50`; do
	/tmp/shm3 &
done
while pgrep -q shm3; do
	sleep 5
done
while pkill swap; do
	sleep .2	
done
wait

rm -f /tmp/shm3
exit $s
