#!/bin/sh

# Bug 275570 - self-referential nullfs mount over tmpfs in combination with MNT_UPDATE results in a hang

# Test scenario mounts:
# tmpfs on /mnt (tmpfs, local, noexec, read-only)
# .setup-done on /mnt/.setup-done (nullfs, local)

# Hang seen on a GENERIC kernel only
# UID  PID PPID C PRI NI   VSZ  RSS MWCHAN STAT TT     TIME COMMAND
#   0 4429 4417 4  59  0 14192 2832 vlp2   D+    0  0:00.06 ./nullfs33
# https://people.freebsd.org/~pho/stress/log/log0663.txt


. ../default.cfg
set -u
prog=$(basename "$0" .sh)

cat > /tmp/$prog.c <<EOF
#include <assert.h>
#include <err.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/_iovec.h>
#include <sys/file.h>
#include <sys/mount.h>

static void xmount(const char* fstype, const char* from, const char* to, int flags) {

  char errmsg[255];
  errmsg[0] = '\0';

  int capacity = 8;

  struct iovec* iov = malloc(sizeof(struct iovec) * capacity);

  iov[0].iov_base = "fstype";
  iov[0].iov_len  = sizeof("fstype");
  iov[1].iov_base = __DECONST(char*, fstype);
  iov[1].iov_len  = strlen(fstype) + 1;

  iov[2].iov_base = "fspath";
  iov[2].iov_len  = sizeof("fspath");
  iov[3].iov_base = __DECONST(char*, to);
  iov[3].iov_len  = strlen(to) + 1;

  iov[4].iov_base = "from";
  iov[4].iov_len  = sizeof("from");
  iov[5].iov_base = __DECONST(char*, from);
  iov[5].iov_len  = strlen(from) + 1;

  iov[6].iov_base = "errmsg";
  iov[6].iov_len  = sizeof("errmsg");
  iov[7].iov_base = errmsg;
  iov[7].iov_len  = sizeof(errmsg);

  int len = 8;

  if (flags & MNT_NOCOVER) {

    assert(len + 2 <= capacity);

    iov[len].iov_base = "nocover";
    iov[len].iov_len  = sizeof("nocover");
    len++;

    iov[len].iov_base = NULL;
    iov[len].iov_len  = 0;
    len++;
  }

  assert(len == capacity);

  if (nmount(iov, len, (int)flags) == -1) {
    err(EXIT_FAILURE, "nmount %s -> %s: %s", from, to, errmsg);
  }

  free(iov);
}

static void xchdir(const char* path) {
  if (chdir(path) == -1) {
    err(EXIT_FAILURE, "chdir(\"%s\")", path);
  }
}

static void xcreat(char* path, int mode) {
  int fd = open(path, O_CREAT | O_WRONLY, mode);
  if (fd == -1) {
    err(EXIT_FAILURE, "can't create %s", path);
  }
  close(fd);
}

//#define ROOT_DIR "/mnt"
#define ROOT_DIR "$mntpoint"

int main(int argc __unused, char* argv[] __unused) {

  xchdir(ROOT_DIR);

  xmount("tmpfs", "tmpfs", ".", MNT_NOEXEC);

  xcreat(".setup-done", 0444);
  xmount("nullfs", ".setup-done", ".setup-done", 0);

  xmount("tmpfs", "tmpfs", ".", MNT_RDONLY | MNT_NOEXEC | MNT_UPDATE);

  unmount(ROOT_DIR "/.setup-done", MNT_FORCE);
  unmount(ROOT_DIR, MNT_FORCE);

  return 0;
}
EOF
cc -o /tmp/$prog -Wall -Wextra -O0 /tmp/$prog.c || exit 1

cd /tmp
start=`date +%s`
while [ $((`date +%s` - start)) -lt 60 ]; do
	./$prog || break
done
cd -

mount | grep -q "on $mntpoint " && umount $mntpoint
rm -f /tmp/$prog /tmp/$prog.c
exit 0
