/*-
 * Copyright 2021 Toomas Soome <tsoome@me.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <stand.h>
#include <sys/queue.h>

/*
 * While setting "currdev" environment variable, alse "mount" the
 * new root file system. This is done to hold disk device open
 * in between file accesses, and thus preserve block cache for
 * this device. Additionally, this allows us to optimize filesystem
 * access by sharing filesystem metadata (like superblock).
 */

typedef STAILQ_HEAD(mnt_info_list, mnt_info) mnt_info_list_t;

typedef struct mnt_info {
	STAILQ_ENTRY(mnt_info)	mnt_link;	/* link in mount list */
	const struct fs_ops	*mnt_fs;
	char			*mnt_dev;
	char			*mnt_path;
	unsigned		mnt_refcount;
	void			*mnt_data;	/* Private state */
} mnt_info_t;

/* list of mounted filesystems. */
static mnt_info_list_t mnt_list = STAILQ_HEAD_INITIALIZER(mnt_list);

static void
free_mnt(mnt_info_t *mnt)
{
	free(mnt->mnt_dev);
	free(mnt->mnt_path);
	free(mnt);
}

static int
add_mnt_info(struct fs_ops *fs, const char *dev, const char *path, void *data)
{
	mnt_info_t *mnt;

	mnt = malloc(sizeof(*mnt));
	if (mnt == NULL)
		return (ENOMEM);

	mnt->mnt_fs = fs;
	mnt->mnt_dev = strdup(dev);
	mnt->mnt_path = strdup(path);
	mnt->mnt_data = data;
	mnt->mnt_refcount = 1;

	if (mnt->mnt_dev == NULL || mnt->mnt_path == NULL) {
		free_mnt(mnt);
		return (ENOMEM);
	}
	STAILQ_INSERT_TAIL(&mnt_list, mnt, mnt_link);
	return (0);
}

static void
delete_mnt_info(mnt_info_t *mnt)
{
	STAILQ_REMOVE(&mnt_list, mnt, mnt_info, mnt_link);
	free_mnt(mnt);
}

int
mount(const char *dev, const char *path, int flags __unused, void *data)
{
	mnt_info_t *mnt;
	int rc = -1;

	/* Is it already mounted? */
	STAILQ_FOREACH(mnt, &mnt_list, mnt_link) {
		if (strcmp(dev, mnt->mnt_dev) == 0 &&
		    strcmp(path, mnt->mnt_path) == 0) {
			mnt->mnt_refcount++;
			return (0);
		}
	}

	for (int i = 0; file_system[i] != NULL; i++) {
		struct fs_ops *fs;

		fs = file_system[i];
		if (fs->fo_mount == NULL)
			continue;

		if (fs->fo_mount(dev, path, &data) != 0)
			continue;

		rc = add_mnt_info(fs, dev, path, data);
		if (rc != 0 && mnt->mnt_fs->fo_unmount != NULL) {
			printf("failed to mount %s: %s\n", dev,
			    strerror(rc));
			(void)mnt->mnt_fs->fo_unmount(dev, data);
		}
		break;
	}


	/*
	 * if rc is -1, it means we have no file system with fo_mount()
	 * callback, or all fo_mount() calls failed. As long as we
	 * have missing fo_mount() callbacks, we allow mount() to return 0.
	 */
	if (rc == -1)
		rc = 0;

	return (rc);
}

int
unmount(const char *dev, int flags __unused)
{
	mnt_info_t *mnt;
	int rv;

	rv = 0;
	STAILQ_FOREACH(mnt, &mnt_list, mnt_link) {
		if (strcmp(dev, mnt->mnt_dev) == 0) {
			if (mnt->mnt_refcount > 1) {
				mnt->mnt_refcount--;
				break;
			}

			if (mnt->mnt_fs->fo_unmount != NULL)
				rv = mnt->mnt_fs->fo_unmount(dev,
				    mnt->mnt_data);
			delete_mnt_info(mnt);
			break;
		}
	}

	if (rv != 0)
		printf("failed to unmount %s: %d\n", dev, rv);
	return (0);
}
