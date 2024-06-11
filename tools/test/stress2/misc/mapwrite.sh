#!/bin/sh

# File corruption scenario

# Test program by Rob Norris <rob norris klarasystems com>
# Test program obtained from: https://gist.github.com/robn/9804c60cd0275086d26893d73e7af35c
# https://github.com/openzfs/zfs/issues/15654

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

set -u
prog=$(basename "$0" .sh)
cat > /tmp/$prog.c <<EOF
/*
 * Some kind of clone-related crasher. Not sure if legit or just outdated
 * assertion.
 *
 * Creates clone, maps it, writes from map back into itself.
 *
 * Compile a recent (2.2+) ZFS with --enable-debug.
 *
 * cc -o mapwrite mapwrite.c
 *
 * echo 1 > /sys/modules/zfs/parameters/zfs_bclone_enabled
 * zpool create tank ...
 * cd /tank
 * mapwrite
 *
 * [    7.666305] VERIFY(arc_released(db->db_buf)) failed
 * [    7.666443] PANIC at dbuf.c:2150:dbuf_redirty()
 * [    7.666489] Showing stack for process 608
 * [    7.666534] CPU: 1 PID: 608 Comm: mapwrite Tainted: P           O      5.10.170 #3
 * [    7.666610] Call Trace:
 * [    7.666646]  dump_stack+0x57/0x6e
 * [    7.666717]  spl_panic+0xd3/0xfb [spl]
 * [    7.667113]  ? zfs_btree_find+0x16a/0x300 [zfs]
 * [    7.667278]  ? range_tree_find_impl+0x55/0xa0 [zfs]
 * [    7.667333]  ? _cond_resched+0x1a/0x50
 * [    7.667371]  ? __kmalloc_node+0x14a/0x2b0
 * [    7.667415]  ? spl_kmem_alloc_impl+0xb0/0xd0 [spl]
 * [    7.667555]  ? __list_add+0x12/0x30 [zfs]
 * [    7.667681]  spl_assert+0x17/0x20 [zfs]
 * [    7.667807]  dbuf_redirty+0xad/0xb0 [zfs]
 * [    7.667963]  dbuf_dirty+0xe76/0x1310 [zfs]
 * [    7.668011]  ? mutex_lock+0xe/0x30
 * [    7.668133]  ? dbuf_noread+0x112/0x240 [zfs]
 * [    7.668271]  dmu_write_uio_dnode+0x101/0x1b0 [zfs]
 * [    7.668411]  dmu_write_uio_dbuf+0x4a/0x70 [zfs]
 * [    7.668555]  zfs_write+0x500/0xc80 [zfs]
 * [    7.668610]  ? page_add_file_rmap+0xe/0xb0
 * [    7.668740]  zpl_iter_write+0xe4/0x130 [zfs]
 * [    7.668803]  new_sync_write+0x119/0x1b0
 * [    7.668843]  vfs_write+0x1ce/0x260
 * [    7.668880]  __x64_sys_pwrite64+0x91/0xc0
 * [    7.668918]  do_syscall_64+0x30/0x40
 * [    7.668957]  entry_SYSCALL_64_after_hwframe+0x61/0xc6
 */

#define	_GNU_SOURCE

#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/mman.h>

#define	DATASIZE	(1024*1024)
char data[DATASIZE];

#define NDATA		(512)

#define FILE_NAME	"file"
#define CLONE_NAME	"clone"

static int
_create_file(void)
{
	memset(data, 0x5a, DATASIZE);

	int fd;
	if ((fd = open(FILE_NAME, O_RDWR | O_CREAT | O_APPEND,
	    S_IRUSR | S_IWUSR)) < 0) {
		perror("open '" FILE_NAME "'");
		abort();
	}

	for (int i = 0; i < NDATA; i++) {
		int nwr = write(fd, data, DATASIZE);
		if (nwr < 0) {
			perror("write");
			abort();
		}
		if (nwr < DATASIZE) {
			fprintf(stderr, "short write\n");
			abort();
		}
	}

	if (lseek(fd, 0, SEEK_SET) < 0) {
		perror("lseek");
		abort();
	}

	sync();

	return (fd);
}

static int
_clone_file(int sfd)
{
	int dfd;
	if ((dfd = open(CLONE_NAME, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR)) < 0) {
		perror("open '" CLONE_NAME "'");
		abort();
	}

	if (copy_file_range(sfd, 0, dfd, 0, DATASIZE * NDATA, 0) < 0) {
		perror("copy_file_range");
		abort();
	}

	return (dfd);
}

static void *
_map_file(int fd)
{
	void *p = mmap(NULL, DATASIZE*NDATA, PROT_READ, MAP_SHARED, fd, 0);
	if (p == MAP_FAILED) {
		perror("mmap");
		abort();
	}

	return (p);
}

static void
_map_write(void *p, int fd)
{
	if (pwrite(fd, p, DATASIZE, 0) < 0) {
		perror("pwrite");
		abort();
	}
}

int
main(void)
{
	int sfd = _create_file();
	int dfd = _clone_file(sfd);
	void *p = _map_file(dfd);
	_map_write(p, dfd);
	return (0);
}
EOF
mycc -o /tmp/$prog -Wall -Wextra -O2 /tmp/$prog.c || exit 1

mount | grep -q "on $mntpoint " && umount -f $mntpoint
mdconfig -l | grep -q md$mdstart && mdconfig -d -u $mdstart
mdconfig -s 5g -u $mdstart

newfs -n $newfs_flags /dev/md$mdstart > /dev/null
mount /dev/md$mdstart $mntpoint

mycc -o /tmp/swap -Wall -Wextra -O0 ../tools/swap.c || exit 1
timeout -k 90 60 /tmp/swap -d 100 &
for i in `jot 10`; do
	capacity=`swapinfo | tail -1 | sed 's/.* //; s/%//'`
	[ $capacity -gt 1 ] && break
	sleep 2	# Wait for swapping
done

cd $mntpoint
/tmp/$prog; s=$?
pkill swap
wait
cmp $mntpoint/file $mntpoint/clone || { echo Fail; s=1; }
cd -

umount $mntpoint
mdconfig -d -u $mdstart
rm /tmp/$prog /tmp/$prog.c
exit $s
