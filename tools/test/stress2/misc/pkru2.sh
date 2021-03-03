#/bin/sh

# Test scenario for Intel userspace protection keys feature on Skylake Xeons
# Based on tests by kib@

grep -qw PKU /var/run/dmesg.boot || exit 0
cd /tmp

cat > /tmp/pkru2a.c <<EOF
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <machine/sysarch.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static volatile char *mapping;

#define	OPKEY	1

int
main(void)
{
	time_t start;
	int error;

	start = time(NULL);
	while (time(NULL) - start < 60) {
		mapping = mmap(NULL, getpagesize(), PROT_READ | PROT_WRITE,
		    MAP_PRIVATE | MAP_ANON, -1, 0);
		if (mapping == MAP_FAILED)
			err(1, "mmap");
		error = x86_pkru_protect_range((void *)mapping,
		    getpagesize(), OPKEY, 0);
		error = x86_pkru_protect_range((void *)mapping,
		    getpagesize() * 64, OPKEY, 0);
		if (error != 0)
			err(1, "x86_pkru_protect_range");
		error = x86_pkru_set_perm(OPKEY, 0, 0);
		if (error != 0)
			err(1, "x86_pkru_set_perm");
		if (munmap((void *)mapping, getpagesize()) == -1)
			err(1, "munmap()");
	}
	return (0);
}
EOF
cc -Wall -Wextra -g -O -o pkru2a64 pkru2a.c || exit 1
cc -Wall -Wextra -g -O -o pkru2a32 -m32 pkru2a.c || exit 1
rm pkru2a.c
./pkru2a64
./pkru2a32
rm -f pkru2a64 pkru2a32

cat > /tmp/pkru2b.c <<EOF
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <machine/sysarch.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static volatile char *mapping;

#define	OPKEY	1

int
main(void)
{
	time_t start;
	int error;

	start = time(NULL);
	while (time(NULL) - start < 60) {
		mapping = mmap(NULL, getpagesize(), PROT_READ | PROT_WRITE,
		    MAP_PRIVATE | MAP_ANON, -1, 0);
		if (mapping == MAP_FAILED)
			err(1, "mmap");
		error = x86_pkru_protect_range((void *)mapping,
		    getpagesize() * 64, OPKEY, 0);
		if (error != 0)
			err(1, "x86_pkru_protect_range");
		error = x86_pkru_set_perm(OPKEY, 0, 0);
		if (error != 0)
			err(1, "x86_pkru_set_perm");
		if (munmap((void *)mapping, getpagesize()) == -1)
			err(1, "munmap()");
	}
	return (0);
}
EOF

cc -Wall -Wextra -g -O -o pkru2b64 pkru2b.c || exit 1
cc -Wall -Wextra -g -O -o pkru2b32 -m32 pkru2b.c || exit 1
rm pkru2b.c
./pkru2b64
./pkru2b32
rm -f pkru2b64 pkru2b32

exit
