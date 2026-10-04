#!/bin/sh

#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2026 Maksym Sobolyev <sobomax@sippysoft.com>
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#

# Hunt for "panic: flush_newblk_dep: Bad newblk" (PR 297976).
#
# flush_pagedep_deps() drops the soft updates lock to obtain the vnode of a
# newly created directory (MKDIR_BODY).  If the mkdir dependency completes
# in that window, the directory's allocdirect is retired and a lookup of its
# first block by block number can return a stale allocindir left behind by a
# previous owner of the same physical block that was relocated by
# ffs_reallocblks() and freed.
#
# Three kinds of workers on a small SU (no journal) file system:
# - "wal" writers grow two interleaved files past UFS_NDADDR so that
#   clusters referenced from an indirect block keep getting relocated,
#   freeing the old blocks while their dependencies are retained.
# - "dir" workers fill a directory, unlink the trailing entries so the next
#   insert sets IN_ENDOFF, then mkdir subdirectories.  ffs_vput_pair()
#   then truncates the parent and calls ffs_syncvnode(MNT_WAIT), which
#   is the path into flush_pagedep_deps() / flush_newblk_dep().
# - "sync" workers fsync() the parents and their subdirectories to
#   complete the mkdir dependencies while a flush is in progress.
#
# Several details are needed for a new directory to land on such a block:
# directory blocks are allocated from the metadata area of the cylinder
# group, so it is filled with placeholder directories first; fragments
# are as large as blocks so that a directory block takes over a whole
# freed block; each block the writers add must be on disk, with its
# dependencies complete, before it is relocated, while the writers'
# inodes stay unwritten: the writers put every new block on disk with a
# partial O_DIRECT write before completing it, and fsync() of a small
# helper file writes the cylinder group map; and the writers' inodes are
# isolated in their own inode blocks so that no other fsync() writes them.
# Keep the load light: dirty buffer pressure or dependency cleanups sync
# every inode and retire the stale dependencies.
#
# The writers used to put their blocks on disk with fdatasync(), but that
# writes the inode whenever the size or a block pointer changed once the
# i_flag/i_flags mixup in ffs_syncvnode() is fixed, which retires the
# stale dependencies at once.  WALSYNC=fdatasync selects that old method,
# which only works on kernels with the mixup; the default is "direct".
#
# Tunables: NWAL, NDIR, NSYNC, NFILES, NSUB, WALSIZE (bytes),
# WALDELAY (us between 32k writes), WALSYNC (direct or fdatasync),
# WALFSYNC (fsync the writers every N writes, 0 off), DIRDELAY (us between
# mkdirs), NPAD (directories used to fill the metadata area of the
# cylinder group), RUNTIME.
# Set dtrace=1 to count how often the affected functions were entered and
# how often a new directory reused a block with a stale dependency.  The
# latter is the precondition for the panic and can be observed on a fixed
# kernel.
#
# Panics within a few minutes on 14.3 without the fix.

. ../default.cfg
[ `id -u` -ne 0 ] && echo "Must be root!" && exit 1

dir=/tmp
odir=`pwd`
cd $dir
sed '1,/^EOF/d' < $odir/$0 > $dir/mkdir_blkreuse.c
mycc -o mkdir_blkreuse -Wall -Wextra -O0 -g mkdir_blkreuse.c || exit 1
rm -f mkdir_blkreuse.c
cd $odir

runtime=`echo ${RUNTIME:-10m} | sed -e 's/m$/*60/' -e 's/s$//' | bc`
export WALSYNC=${WALSYNC:-direct}
case $WALSYNC in
direct|fdatasync)
	;;
*)
	echo "WALSYNC must be direct or fdatasync" >&2
	exit 1
	;;
esac
s=0
set -e
mount | grep "on $mntpoint " | grep -q /dev/md && umount -f $mntpoint
[ -c /dev/md$mdstart ] &&  mdconfig -d -u $mdstart
mdconfig -a -t swap -s 1g -u $mdstart
# Fragment size equal to the block size makes every directory block a full
# block allocation, so a new directory's block 0 can take over exactly the
# block number a relocated cluster gave back, instead of a fragment carved
# from a partially used block.  SU defers inode frees; use the highest
# inode density to limit "out of inodes".
newfs -U -b 32768 -f 32768 -i 32768 md$mdstart > /dev/null
mount -o noatime /dev/md$mdstart $mntpoint
set +e

if [ $dtrace ]; then
	# flush_pagedep_deps() and flush_newblk_dep() are inlined;
	# get_parent_vp() is only called from flush_pagedep_deps().
	# The second clause detects the precondition for the panic: block 0
	# of a new directory is allocated while the newblk hash still holds a
	# dependency of a previous owner of that block, an allocindir
	# (D_ALLOCINDIR == 7) or an allocdirect (D_ALLOCDIRECT == 5) of
	# another inode.  A non-zero count means an unfixed kernel would
	# have panicked.
	cat > /tmp/mkdir_blkreuse.d <<'EOD'
fbt::softdep_setup_mkdir:entry,
fbt::get_parent_vp:entry,
fbt::ffs_reallocblks:entry
{
	@[probefunc] = count();
}

fbt::softdep_setup_allocdirect:entry
/args[1] == 0 && args[3] == 0 && (args[0]->i_mode & 0170000) == 0040000/
{
	this->ino = args[0]->i_number;
	this->blk = args[2];
	this->sd = args[0]->i_ump->um_softdep;
	this->nb = this->sd->sd_newblkhash[this->blk &
	    this->sd->sd_newblkhashsize].lh_first;
	this->hit = 0;
	this->hit += this->nb != NULL && this->nb->nb_newblkno == this->blk &&
	    (this->nb->nb_list.wk_type == 7 ||
	    (this->nb->nb_list.wk_type == 5 &&
	    ((struct allocdirect *)this->nb)->ad_inodedep->id_ino !=
	    this->ino)) ? 1 : 0;
	this->nb = this->nb != NULL ? this->nb->nb_hash.le_next : NULL;
	this->hit += this->nb != NULL && this->nb->nb_newblkno == this->blk &&
	    (this->nb->nb_list.wk_type == 7 ||
	    (this->nb->nb_list.wk_type == 5 &&
	    ((struct allocdirect *)this->nb)->ad_inodedep->id_ino !=
	    this->ino)) ? 1 : 0;
	this->nb = this->nb != NULL ? this->nb->nb_hash.le_next : NULL;
	this->hit += this->nb != NULL && this->nb->nb_newblkno == this->blk &&
	    (this->nb->nb_list.wk_type == 7 ||
	    (this->nb->nb_list.wk_type == 5 &&
	    ((struct allocdirect *)this->nb)->ad_inodedep->id_ino !=
	    this->ino)) ? 1 : 0;
	this->nb = this->nb != NULL ? this->nb->nb_hash.le_next : NULL;
	this->hit += this->nb != NULL && this->nb->nb_newblkno == this->blk &&
	    (this->nb->nb_list.wk_type == 7 ||
	    (this->nb->nb_list.wk_type == 5 &&
	    ((struct allocdirect *)this->nb)->ad_inodedep->id_ino !=
	    this->ino)) ? 1 : 0;
	@["new directory block 0 allocated"] = count();
}

fbt::softdep_setup_allocdirect:entry
/this->hit != 0/
{
	@["new directory block 0 with stale dependency (would panic)"] =
	    count();
}
EOD
	dtrace -s /tmp/mkdir_blkreuse.d &
	dpid=$!
	sleep 2
fi

/tmp/mkdir_blkreuse $mntpoint $runtime || s=1

if [ $dtrace ]; then
	kill -s TERM $dpid
	wait $dpid
fi

for i in `jot 6`; do
	mount | grep -q "on $mntpoint " || break
	umount $mntpoint && break || sleep 10
	[ $i -eq 6 ] &&
	    { echo FATAL; fstat -mf $mntpoint; exit 1; }
done
checkfs /dev/md$mdstart || s=2
mdconfig -d -u $mdstart
rm -f /tmp/mkdir_blkreuse /tmp/mkdir_blkreuse.d
exit $s
EOF
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define CHUNK 32768
#define INOPAD 128	/* UFS2 inodes per 32k block */

static const char *path;
static int nwal, ndir, nsync, nfiles, nsub, walfsync;
static enum { WAL_DIRECT, WAL_FDATASYNC } walsync;
static const char *walsyncname[] = { "direct", "fdatasync" };
static long walsize;
static useconds_t waldelay, dirdelay;
static volatile sig_atomic_t done;

static int
getenvint(const char *name, int def)
{
	char *v;

	if ((v = getenv(name)) == NULL || *v == '\0')
		return (def);
	return (atoi(v));
}

static void
handler(int sig __unused)
{

	done = 1;
}

/*
 * Inodes are handed out by a rotor, so surround a file with enough
 * placeholder inodes that its inode block holds nothing anybody else will
 * fsync().  An fsync() of an unrelated inode in the same block would
 * write the writer's inode and retire the stale dependencies.
 */
static int
padded_create(const char *dir, const char *name, int *seq)
{
	char pad[MAXPATHLEN];
	int fd, i;

	for (i = 0; i < 2 * INOPAD + 1; i++) {
		if (i == INOPAD) {
			snprintf(pad, sizeof(pad), "%s/%s", dir, name);
			if ((fd = open(pad, O_RDWR | O_CREAT | O_TRUNC,
			    0644)) == -1)
				err(1, "open(%s)", pad);
			continue;
		}
		snprintf(pad, sizeof(pad), "%s/pad.%d", dir, (*seq)++);
		if (close(open(pad, O_CREAT | O_WRONLY, 0644)) == -1)
			err(1, "creat(%s)", pad);
	}
	return (fd);
}

/*
 * Create the writer's files before any other worker starts allocating
 * inodes, so that the padding is not interleaved with their inodes.
 */
static void
wal_setup(int idx, int *fd, int *fdcg)
{
	char dir[MAXPATHLEN];
	int seq;

	snprintf(dir, sizeof(dir), "%s/w%d", path, idx);
	if (mkdir(dir, 0755) == -1 && errno != EEXIST)
		err(1, "mkdir(%s)", dir);
	seq = 0;
	fd[0] = padded_create(dir, "wal", &seq);
	*fdcg = padded_create(dir, "cg", &seq);
	fd[1] = padded_create(dir, "db", &seq);
}

/*
 * Directory blocks are placed in the metadata area at the start of the
 * cylinder group, file data after it.  Fill the metadata area with
 * placeholder directories so that the directories created later have to
 * take the lowest free block of the data area, where the relocated
 * clusters of the writers leave their holes.
 */
static void
metapad(int npad)
{
	char dir[MAXPATHLEN];
	int i;

	snprintf(dir, sizeof(dir), "%s/m", path);
	if (mkdir(dir, 0755) == -1 && errno != EEXIST)
		err(1, "mkdir(%s)", dir);
	for (i = 0; i < npad; i++) {
		snprintf(dir, sizeof(dir), "%s/m/%d", path, i);
		if (mkdir(dir, 0755) == -1 && errno != EEXIST)
			err(1, "mkdir(%s)", dir);
	}
}

/*
 * On an O_DIRECT descriptor, ffs_write() writes a partial block out right
 * away with bawrite(), without the inode or the indirect block, while a
 * write that completes a block goes to cluster_write() and so to
 * ffs_reallocblks().  Write the first half of the block alone to get it on
 * disk, then the second half to put it into the cluster.
 */
static int
wal_direct(int fd, char *buf, off_t off)
{
	if (pwrite(fd, buf, CHUNK / 2, off) != CHUNK / 2 ||
	    pwrite(fd, buf + CHUNK / 2, CHUNK / 2, off + CHUNK / 2) !=
	    CHUNK / 2)
		return (-1);
	return (0);
}

/*
 * Slowly grow two files in lockstep, like a database and its WAL, so that
 * their blocks are interleaved and ffs_reallocblks() has to relocate the
 * clusters referenced from the indirect blocks, freeing the old blocks for
 * reuse.
 *
 * A relocated block only leaves its old allocindir behind if that
 * dependency had already completed, and the leftover is only retained
 * while the indirect block pointer in the inode is not yet on disk.
 * Each new block is put on disk by wal_direct() before a later write
 * extends the cluster and has it relocated, and fsync() of a small third
 * file that just allocated a block in the same cylinder group writes that
 * map synchronously.  Together they complete the dependencies of the new
 * blocks without writing the writers' inodes, which is exactly the state
 * needed.  The syncer then writes the indirect block, freeing the old
 * blocks in a burst, and the inode pointer only becomes durable at the
 * following flush of the device vnode.  With WALSYNC=fdatasync,
 * fdatasync() puts the blocks on disk instead, which leaves the inode
 * alone only on kernels where ffs_syncvnode() tested IN_SIZEMOD and
 * IN_IBLKDATA in i_flags.  Never fsync() the two files unless asked to
 * with WALFSYNC.
 */
static void
wal(int idx, int *fd, int fdcg)
{
	char buf[CHUNK];
	long off;
	int i;

	memset(buf, idx + 1, sizeof(buf));
	if (walsync == WAL_DIRECT)
		for (i = 0; i < 2; i++)
			if (fcntl(fd[i], F_SETFL,
			    fcntl(fd[i], F_GETFL) | O_DIRECT) == -1)
				err(1, "fcntl(O_DIRECT)");
	while (done == 0) {
		for (i = 0; i < 2; i++)
			if (ftruncate(fd[i], 0) == -1)
				err(1, "ftruncate");
		for (off = 0; off < walsize && done == 0; off += CHUNK) {
			for (i = 0; i < 2; i++) {
				if (walsync == WAL_DIRECT &&
				    wal_direct(fd[i], buf, off) == 0)
					continue;
				if (walsync != WAL_DIRECT &&
				    write(fd[i], buf, sizeof(buf)) ==
				    sizeof(buf))
					continue;
				if (errno == ENOSPC) {
					sleep(1);
					break;
				}
				err(1, "write");
			}
			if ((off / CHUNK) % 8 == 0 && ftruncate(fdcg, 0) == -1)
				err(1, "ftruncate");
			if (pwrite(fdcg, buf, sizeof(buf),
			    ((off / CHUNK) % 8) * CHUNK) != sizeof(buf) &&
			    errno != ENOSPC)
				err(1, "pwrite");
			fsync(fdcg);
			for (i = 0; i < 2; i++) {
				if (walfsync != 0 &&
				    (off / CHUNK) % walfsync == 0)
					fsync(fd[i]);
				else if (walsync == WAL_FDATASYNC)
					fdatasync(fd[i]);
			}
			usleep(waldelay);
		}
	}
	_exit(0);
}

/*
 * Fill a directory, remove the entries at the end so the next insert leaves
 * unused blocks after the last entry (IN_ENDOFF), then create new
 * directories in it.  ffs_vput_pair() truncates and syncs the parent.
 */
static void
dirwork(int idx)
{
	char parent[MAXPATHLEN], name[MAXPATHLEN];
	int fd, i, keep;

	snprintf(parent, sizeof(parent), "%s/p%d", path, idx);
	if (mkdir(parent, 0755) == -1 && errno != EEXIST)
		err(1, "mkdir(%s)", parent);
	keep = nfiles / 4;
	while (done == 0) {
		for (i = 0; i < nfiles; i++) {
			snprintf(name, sizeof(name),
			    "%s/f%08d-%0200d", parent, i, idx);
			if ((fd = open(name, O_CREAT | O_WRONLY, 0644)) ==
			    -1) {
				if (errno == ENOSPC)
					break;
				err(1, "creat(%s)", name);
			}
			close(fd);
		}
		for (i = nfiles - 1; i >= keep; i--) {
			snprintf(name, sizeof(name),
			    "%s/f%08d-%0200d", parent, i, idx);
			if (unlink(name) == -1 && errno != ENOENT)
				err(1, "unlink(%s)", name);
		}
		for (i = 0; i < nsub; i++) {
			snprintf(name, sizeof(name), "%s/d%d", parent, i);
			if (mkdir(name, 0755) == -1) {
				if (errno == ENOSPC)
					break;
				if (errno != EEXIST)
					err(1, "mkdir(%s)", name);
			}
			/*
			 * A trickle, not a flood: running out of inodes or
			 * dependencies triggers cleanups that sync every
			 * inode, retiring the stale dependencies.
			 */
			usleep(dirdelay);
		}
		for (i = 0; i < nsub; i++) {
			snprintf(name, sizeof(name), "%s/d%d", parent, i);
			if (rmdir(name) == -1 && errno != ENOENT)
				err(1, "rmdir(%s)", name);
		}
		for (i = keep - 1; i >= 0; i--) {
			snprintf(name, sizeof(name),
			    "%s/f%08d-%0200d", parent, i, idx);
			if (unlink(name) == -1 && errno != ENOENT)
				err(1, "unlink(%s)", name);
		}
	}
	_exit(0);
}

/*
 * Push the mkdir dependencies to completion while the dir workers are
 * flushing their parents.
 */
static void
syncwork(int idx)
{
	char name[MAXPATHLEN];
	int fd, i, j;

	i = idx;
	while (done == 0) {
		i = (i + 1) % ndir;
		snprintf(name, sizeof(name), "%s/p%d", path, i);
		if ((fd = open(name, O_RDONLY | O_DIRECTORY)) != -1) {
			fsync(fd);
			close(fd);
		}
		for (j = 0; j < nsub; j += 7) {
			snprintf(name, sizeof(name), "%s/p%d/d%d", path,
			    i, j);
			if ((fd = open(name, O_RDONLY | O_DIRECTORY)) !=
			    -1) {
				fsync(fd);
				close(fd);
			}
		}
		usleep(1000);
	}
	_exit(0);
}

int
main(int argc, char *argv[])
{
	pid_t *pids;
	time_t start, runtime;
	int e, i, n, npad, status, *walcg, *walfd;

	if (argc != 3) {
		fprintf(stderr, "Usage %s <path> <runtime seconds>\n",
		    argv[0]);
		exit(1);
	}
	path = argv[1];
	runtime = atol(argv[2]);
	nwal = getenvint("NWAL", 1);
	ndir = getenvint("NDIR", 2);
	nsync = getenvint("NSYNC", 1);
	nfiles = getenvint("NFILES", 200);
	nsub = getenvint("NSUB", 50);
	walsize = getenvint("WALSIZE", 64 * 1024 * 1024);
	waldelay = getenvint("WALDELAY", 40000);
	walfsync = getenvint("WALFSYNC", 0);
	dirdelay = getenvint("DIRDELAY", 2000);
	npad = getenvint("NPAD", 3000);
	walsync = WAL_DIRECT;
	for (i = 0; i < (int)nitems(walsyncname); i++)
		if (getenv("WALSYNC") != NULL &&
		    strcmp(getenv("WALSYNC"), walsyncname[i]) == 0)
			walsync = i;
	if (nwal < 0 || ndir < 1 || nsync < 0 || nfiles < 8 || nsub < 1 ||
	    walsize < CHUNK || walfsync < 0 || npad < 0)
		errx(1, "bad tunables");
	fprintf(stderr, "wal=%d dir=%d sync=%d files=%d sub=%d walsize=%ld "
	    "waldelay=%uus walsync=%s walfsync=%d dirdelay=%uus npad=%d "
	    "runtime=%lds\n",
	    nwal, ndir, nsync, nfiles, nsub, walsize, (unsigned)waldelay,
	    walsyncname[walsync], walfsync,
	    (unsigned)dirdelay, npad, (long)runtime);

	n = nwal + ndir + nsync;
	if ((pids = calloc(n, sizeof(*pids))) == NULL ||
	    (walfd = calloc(nwal + 1, 2 * sizeof(*walfd))) == NULL ||
	    (walcg = calloc(nwal + 1, sizeof(*walcg))) == NULL)
		err(1, "calloc");
	for (i = 0; i < nwal; i++)
		wal_setup(i, &walfd[2 * i], &walcg[i]);
	metapad(npad);
	signal(SIGTERM, handler);
	signal(SIGALRM, handler);
	n = 0;
	for (i = 0; i < nwal; i++)
		if ((pids[n++] = fork()) == 0)
			wal(i, &walfd[2 * i], walcg[i]);
	for (i = 0; i < ndir; i++)
		if ((pids[n++] = fork()) == 0)
			dirwork(i);
	for (i = 0; i < nsync; i++)
		if ((pids[n++] = fork()) == 0)
			syncwork(i);
	for (i = 0; i < n; i++)
		if (pids[i] == -1)
			err(1, "fork()");

	start = time(NULL);
	while (time(NULL) - start < runtime) {
		sleep(1);
		if (waitpid(-1, &status, WNOHANG) > 0 && status != 0) {
			fprintf(stderr, "worker failed early\n");
			break;
		}
	}
	for (i = 0; i < n; i++)
		kill(pids[i], SIGTERM);
	e = 0;
	for (i = 0; i < n; i++) {
		if (waitpid(pids[i], &status, 0) == -1) {
			if (errno == ECHILD)
				continue;
			err(1, "waitpid(%d)", pids[i]);
		}
		if (status != 0) {
			if (WIFSIGNALED(status))
				fprintf(stderr, "pid %d exit signal %d\n",
				    pids[i], WTERMSIG(status));
			e++;
		}
	}

	return (e);
}
