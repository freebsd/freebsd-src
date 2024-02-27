#!/bin/sh

# Test program obtained from Kyle Evans <kevans@FreeBSD.org>

# Demonstrate UFS SU file corruption

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

set -u
prog=$(basename "$0" .sh)
log=/tmp/$prog.log
rm -f $log
cat > /tmp/$prog.c <<EOF
#include <sys/mman.h>
#include <sys/stat.h>

#include <assert.h>
#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

#define	FILE	"file"

int
main(void)
{
	struct stat sb;
	ssize_t wsz;
	size_t bufsz;
	void *buf, *obuf;
	int mfd, fd;
	int done = 0;

	mfd = open(FILE, O_RDONLY);
	assert(mfd >= 0);

	assert(fstat(mfd, &sb) == 0);
	bufsz = sb.st_size;
	buf = obuf = mmap(NULL, bufsz, PROT_READ, MAP_SHARED, mfd, 0);
	assert(buf != MAP_FAILED);

	/* O_RDWR */
	fd = open(FILE, O_RDWR);
	if (fd < 0)
		err(1, "open");
	assert(fd >= 0);

again:
	while (bufsz > 0) {
		wsz = write(fd, buf, bufsz);
		if (wsz < 0)
			err(1, "write");
		else if (wsz == 0)
			fprintf(stderr, "Huh?\n");
		bufsz -= wsz;
		buf += wsz;
	}

	bufsz = sb.st_size;
	buf = obuf;

	if (++done < 2)
		goto again;

	close(fd);
	munmap(obuf, sb.st_size);
	close(mfd);
	return (0);
}
EOF
mycc -o /tmp/$prog -Wall -Wextra -O0 /tmp/$prog.c || exit 1

cat > /tmp/$prog.serial.c <<EOF
/* Fill a file with sequential numbers */
#include <sys/param.h>
#include <sys/mman.h>
#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int
main(int argc, char *argv[])
{
	size_t i, size;
	long ix, *lp;
	int fd;
	char *file;

	if (argc != 3) {
		fprintf(stderr, "Usage: %s <file> <file length in bytes>\n", argv[0]);
		exit(1);
	}
	file = argv[1];
	size = atol(argv[2]);

	if ((fd = open(file, O_RDWR | O_CREAT | O_TRUNC, 0600)) < 0)
		err(1, "%s", file);

	if (lseek(fd, size - 1, SEEK_SET) == -1)
		err(1, "lseek error");

	/* write a dummy byte at the last location */
	if (write(fd, "\0", 1) != 1)
		err(1, "write error");

	if ((lp = mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0)) == MAP_FAILED)
		err(1, "mmap()");

	for (i = 0, ix = 0; i < size; i += sizeof(long), ix++)
		lp[ix] = ix;

	if (munmap(lp, size) == -1)
		err(1, "munmap");
	close(fd);
}
EOF
mycc -o /tmp/$prog.serial -Wall -Wextra -O0 /tmp/$prog.serial.c || exit 1

mount | grep -q "on $mntpoint " && umount -f $mntpoint
mdconfig -l | grep -q md$mdstart && mdconfig -d -u $mdstart
mdconfig -s 5g -u $mdstart

newfs -n $newfs_flags /dev/md$mdstart > /dev/null
mount /dev/md$mdstart $mntpoint

here=`pwd`
cd $mntpoint

size=875998990
pagesize=`sysctl -n hw.pagesize`
tail=$((size % pagesize))
/tmp/$prog.serial file $size

cat file file > file.post
mv file file.orig
md5=`md5 < file.post`

cp /usr/bin/sort /tmp/$prog.sort
counter=1
n=$((`sysctl -n hw.ncpu`))
[ $n -gt 10 ] && n=10
s=0
start=`date +%s`
while [ $((`date +%s` - start)) -lt 300 ]; do
	st=`date +%s`
	cp file.orig file
	for i in `jot $n`; do
		timeout -k 70s 1m /tmp/$prog.sort /dev/zero &
	done
	sleep $n
	/tmp/$prog
	while pkill $prog.sort; do sleep .2; done
	wait
	m=`md5 < file`
	if [ $md5 != $m ]; then
		echo "Failed @ iteration $counter"
		ls -l
		od -t x8 file      > /var/tmp/$prog.file1
		od -t x8 file.post > /var/tmp/$prog.file2
		diff /var/tmp/$prog.file1 /var/tmp/$prog.file2 > $log
		head -10 $log
		rm /var/tmp/$prog.file1 /var/tmp/$prog.file2
		s=1
		break
	fi
	echo "`date +%T` Loop #$counter, elapsed $((`date +%s` - st)) seconds."
	counter=$((counter + 1))
done
cd $here

umount $mntpoint
mdconfig -d -u $mdstart
rm /tmp/$prog /tmp/$prog.c /tmp/$prog.sort
[ $s -eq 0 ] &&
	printf "OK   File size is %9d, tail is %4d bytes. (%3d loops)\n" $size $tail $counter ||
	printf "FAIL File size is %9d, tail is %4d bytes. (%3d loops)\n" $size $tail $counter
exit $s
