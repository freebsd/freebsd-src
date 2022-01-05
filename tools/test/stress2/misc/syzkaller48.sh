#!/bin/sh

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

# Fixed by:
# 628c3b307fb2 - main - cache: only let non-dir descriptors through when doing EMPTYPATH lookups

. ../default.cfg
[ `id -u` -ne 0 ] && echo "Must be root!" && exit 1

cat > /tmp/syzkaller48.c <<EOF
// Reported-by: syzbot+9aa5439dd9c708aeb1a8@syzkaller.appspotmail.com

#define _GNU_SOURCE

#include <pwd.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/endian.h>
#include <sys/syscall.h>
#include <unistd.h>

#ifndef SYS___realpathat
#define SYS___realpathat 574
#endif

uint64_t r[1] = {0xffffffffffffffff};

int main(void)
{
 int i;

  syscall(SYS_mmap, 0x20000000ul, 0x1000000ul, 7ul, 0x1012ul, -1, 0ul);
  intptr_t res = 0;
  memcpy((void*)0x200000c0, "./file0\000", 8);
  for (i = 0; i < 1000; i++) {
  res = syscall(SYS_open, 0x200000c0ul, 0x48300ul, 0ul);
  if (res != -1)
    r[0] = res;
  memcpy((void*)0x20000080, ".\000", 2);
  syscall(SYS___realpathat, r[0], 0x20000080ul, 0x200002c0ul, 0xabul, 0ul);
  close(res);
  }
  return 0;
}
EOF

set -e
mount | grep "on $mntpoint " | grep -q /dev/md && umount -f $mntpoint
[ -c /dev/md$mdstart ] &&  mdconfig -d -u $mdstart
mdconfig -a -t swap -s 5g -u $mdstart
newfs $newfs_flags -n md$mdstart > /dev/null
mount /dev/md$mdstart $mntpoint
set +e

mkdir $mntpoint/work
mycc -o $mntpoint/work/syzkaller48 -Wall -Wextra -O0 /tmp/syzkaller48.c || exit 1

while true; do
	touch $mntpoint/work/file0
	rm $mntpoint/work/file0
done &

start=`date +%s`
while [ $((`date +%s` - start)) -lt 120 ]; do
	(cd $mntpoint/work; ./syzkaller48)
done
kill $!
wait
ls -l $mntpoint/work

for i in `jot 6`; do
	mount | grep -q "on $mntpoint " || break
	umount $mntpoint && break || sleep 10
	[ $i -eq 6 ] &&
	    { echo FATAL; fstat -mf $mntpoint; exit 1; }
done
mdconfig -d -u $mdstart

rm -rf /tmp/syzkaller48.c
exit 0
