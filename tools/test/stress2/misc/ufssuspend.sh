#!/bin/sh

# ioctl(g_ufs_suspend_handle, UFSSUSPEND, &statfsp->f_fsid) test scenario.

# Bug 230220 - UFS: the freezing ioctl (i.e.UFSSUSPEND) causes panic or EBUSY
# "panic: devfs_set_cdevpriv failed" seen.
# Test scenario by Dexuan Cui <decui microsoft com>
# Fixed by r337055.

[ `id -u` -ne 0 ] && echo "Must be root!" && exit 1
. ../default.cfg

cat > /tmp/ufssuspend.c <<EOF

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/param.h>
#include <sys/ucred.h>
#include <sys/mount.h>

#include <ufs/ffs/fs.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <paths.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <syslog.h>
#include <unistd.h>

static int g_ufs_suspend_handle = -1;
static const char *dev = "/dev";

static int
freeze(void)
{
	struct statfs *mntbuf, *statfsp;
	int mntsize;
	int error = 0;
	int i;

	g_ufs_suspend_handle = open(_PATH_UFSSUSPEND, O_RDWR);
	if (g_ufs_suspend_handle == -1) {
		printf("unable to open %s", _PATH_UFSSUSPEND);
		return (errno);
	}

	mntsize = getmntinfo(&mntbuf, MNT_NOWAIT);
	if (mntsize == 0) {
		printf("There is no mount information\n");
		return (EINVAL);
	}

	for (i = mntsize - 1; i >= 0; --i) {
		statfsp = &mntbuf[i];

		if (strncmp(statfsp->f_mntonname, dev, strlen(dev)) == 0)
			continue; /* skip to freeze '/dev' */

		if (statfsp->f_flags & MNT_RDONLY)
			continue; /* skip to freeze RDONLY partition */

		if (strncmp(statfsp->f_fstypename, "ufs", 3) != 0)
			continue; /* so far, only UFS can be frozen */

		printf("suspending fs: %s\n",  statfsp->f_mntonname);
		error = ioctl(g_ufs_suspend_handle, UFSSUSPEND, &statfsp->f_fsid);
		if (error != 0) {
			printf("error: %d\n", errno);
			error = errno;
		} else {
			printf("Successfully suspend fs: %s\n",  statfsp->f_mntonname);
		}
	}

	return (error);
}

/**
 * closing the opened handle will thaw the FS.
 */
static int
thaw(void)
{
	int error = 0;

	if (g_ufs_suspend_handle != -1) {
		error = close(g_ufs_suspend_handle);
		if (!error) {
			g_ufs_suspend_handle = -1;
			printf("Successfully thaw the fs\n");
		} else {
			error = errno;
			printf("Fail to thaw the fs: "
					"%d %s\n", errno, strerror(errno));
		}
	} else {
		printf("The fs has already been thawed\n\n");
	}

	return (error);
}

int
main(void)
{
	int error;

	error = freeze();
	printf("freeze: err=%d\n", error);

	error = thaw();
	printf("thaw: err=%d\n", error);

	return 0;
}
EOF

mycc -o /tmp/ufssuspend -Wall -Wextra -O2 -g /tmp/ufssuspend.c || exit 1
rm /tmp/ufssuspend.c

cd /tmp
./ufssuspend > /dev/null
s=$?
rm /tmp/ufssuspend
exit $s
