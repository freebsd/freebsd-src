/*-
 * Copyright (c) 2001 Doug Rabson
 * Copyright (c) 2006 Marcel Moolenaar
 * All rights reserved.
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
__FBSDID("$FreeBSD: src/sys/boot/efi/libefi/efifs.c,v 1.10.6.1 2008/11/25 02:59:29 kensmith Exp $");

#include <sys/param.h>
#include <sys/time.h>
#include <stddef.h>
#include <stdarg.h>

#include <bootstrap.h>

#include <efi.h>
#include <efilib.h>
#include <efiprot.h>

/* Perform I/O in blocks of size EFI_BLOCK_SIZE. */
#define	EFI_BLOCK_SIZE	(1024 * 1024)

union fileinfo {
	EFI_FILE_INFO info;
	char bytes[sizeof(EFI_FILE_INFO) + 508];
};

static EFI_GUID sfs_guid = SIMPLE_FILE_SYSTEM_PROTOCOL;
static EFI_GUID fs_guid = EFI_FILE_SYSTEM_INFO_ID;
static EFI_GUID fi_guid = EFI_FILE_INFO_ID;

static int
efifs_open(const char *upath, struct open_file *f)
{
	struct devdesc *dev = f->f_devdata;
	EFI_FILE_IO_INTERFACE *fsif;
	EFI_FILE *file, *root;
	EFI_HANDLE h;
	EFI_STATUS status;
	CHAR16 *cp, *path;

	if (f->f_dev != &efifs_dev || dev->d_unit < 0)
		return (EINVAL);

	h = efi_find_handle(f->f_dev, dev->d_unit);
	if (h == NULL)
		return (EINVAL);

	status = BS->HandleProtocol(h, &sfs_guid, (VOID **)&fsif);
	if (EFI_ERROR(status))
		return (efi_status_to_errno(status));

	/* Get the root directory. */
	status = fsif->OpenVolume(fsif, &root);
	if (EFI_ERROR(status))
		return (efi_status_to_errno(status));

	while (*upath == '/')
		upath++;

	/* Special case: opening the root directory. */
	if (*upath == '\0') {
		f->f_fsdata = root;
		return (0);
	}

	path = malloc((strlen(upath) + 1) * sizeof(CHAR16));
	if (path == NULL) {
		root->Close(root);
		return (ENOMEM);
	}

	cp = path;
	while (*upath != '\0') {
		if (*upath == '/') {
			*cp = '\\';
			while (upath[1] == '/')
				upath++;
		} else
			*cp = *upath;
		upath++;
		cp++;
	}
	*cp = 0;

	/* Open the file. */
	status = root->Open(root, &file, path,
	    EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE, 0);
	if (status == EFI_ACCESS_DENIED || status == EFI_WRITE_PROTECTED)
		status = root->Open(root, &file, path, EFI_FILE_MODE_READ, 0);
	free(path);
	root->Close(root);
	if (EFI_ERROR(status))
		return (efi_status_to_errno(status));

	f->f_fsdata = file;
	return (0);
}

static int
efifs_close(struct open_file *f)
{
	EFI_FILE *file = f->f_fsdata;

	if (file == NULL)
		return (EBADF);

	file->Close(file);
	f->f_fsdata = NULL;
	return (0);
}

static int
efifs_read(struct open_file *f, void *buf, size_t size, size_t *resid)
{
	EFI_FILE *file = f->f_fsdata;
	EFI_STATUS status;
	UINTN sz = size;
	char *bufp;

	if (file == NULL)
		return (EBADF);

	bufp = buf;
	while (size > 0) {
		sz = size;
		if (sz > EFI_BLOCK_SIZE)
			sz = EFI_BLOCK_SIZE;
		status = file->Read(file, &sz, bufp);
		if (EFI_ERROR(status))
			return (efi_status_to_errno(status));
		if (sz == 0)
			break;
		size -= sz;
		bufp += sz;
	}
	if (resid)
		*resid = size;
	return (0);
}

static int
efifs_write(struct open_file *f, void *buf, size_t size, size_t *resid)
{
	EFI_FILE *file = f->f_fsdata;
	EFI_STATUS status;
	UINTN sz = size;
	char *bufp;

	if (file == NULL)
		return (EBADF);

	bufp = buf;
	while (size > 0) {
		sz = size;
		if (sz > EFI_BLOCK_SIZE)
			sz = EFI_BLOCK_SIZE;
		status = file->Write(file, &sz, bufp);
		if (EFI_ERROR(status))
			return (efi_status_to_errno(status));
		if (sz == 0)
			break;
		size -= sz;
		bufp += sz;
	}
	if (resid)
		*resid = size;
	return (0);
}

static off_t
efifs_seek(struct open_file *f, off_t offset, int where)
{
	EFI_FILE *file = f->f_fsdata;
	EFI_STATUS status;
	UINT64 base;

	if (file == NULL)
		return (EBADF);

	switch (where) {
	case SEEK_SET:
		break;

	case SEEK_END:
		status = file->SetPosition(file, ~0ULL);
		if (EFI_ERROR(status))
			return (-1);
		/* FALLTHROUGH */

	case SEEK_CUR:
		status = file->GetPosition(file, &base);
		if (EFI_ERROR(status))
			return (-1);
		offset = (off_t)(base + offset);
		break;

	default:
		return (-1);
	}
	if (offset < 0)
		return (-1);

	status = file->SetPosition(file, (UINT64)offset);
	return (EFI_ERROR(status) ? -1 : offset);
}

static int
efifs_stat(struct open_file *f, struct stat *sb)
{
	EFI_FILE *file = f->f_fsdata;
	union fileinfo fi;
	EFI_STATUS status;
	UINTN sz;

	if (file == NULL)
		return (EBADF);

	bzero(sb, sizeof(*sb));

	sz = sizeof(fi);
	status = file->GetInfo(file, &fi_guid, &sz, &fi);
	if (EFI_ERROR(status))
		return (efi_status_to_errno(status));

	sb->st_mode = S_IRUSR | S_IRGRP | S_IROTH;
	if ((fi.info.Attribute & EFI_FILE_READ_ONLY) == 0)
		sb->st_mode |= S_IWUSR | S_IWGRP | S_IWOTH;
	if (fi.info.Attribute & EFI_FILE_DIRECTORY)
		sb->st_mode |= S_IFDIR;
	else
		sb->st_mode |= S_IFREG;
	sb->st_nlink = 1;
	sb->st_atime = efi_time(&fi.info.LastAccessTime);
	sb->st_mtime = efi_time(&fi.info.ModificationTime);
	sb->st_ctime = efi_time(&fi.info.CreateTime);
	sb->st_size = fi.info.FileSize;
	sb->st_blocks = fi.info.PhysicalSize / S_BLKSIZE;
	sb->st_blksize = S_BLKSIZE;
	sb->st_birthtime = sb->st_ctime;
	return (0);
}

static int
efifs_readdir(struct open_file *f, struct dirent *d)
{
	EFI_FILE *file = f->f_fsdata;
	union fileinfo fi;
	EFI_STATUS status;
	UINTN sz;
	int i;

	if (file == NULL)
		return (EBADF);

	sz = sizeof(fi);
	status = file->Read(file, &sz, &fi);
	if (EFI_ERROR(status))
		return (efi_status_to_errno(status));
	if (sz == 0)
		return (ENOENT);

	d->d_fileno = 0;
	d->d_reclen = sizeof(*d);
	if (fi.info.Attribute & EFI_FILE_DIRECTORY)
		d->d_type = DT_DIR;
	else
		d->d_type = DT_REG;
	for (i = 0; fi.info.FileName[i] != 0; i++)
		d->d_name[i] = fi.info.FileName[i];
	d->d_name[i] = 0;
	d->d_namlen = i;
	return (0);
}

struct fs_ops efifs_fsops = {
	.fs_name = "efifs",
	.fo_open = efifs_open,
	.fo_close = efifs_close,
	.fo_read = efifs_read,
	.fo_write = efifs_write,
	.fo_seek = efifs_seek,
	.fo_stat = efifs_stat,
	.fo_readdir = efifs_readdir
};

static int
efifs_dev_init(void) 
{
	EFI_HANDLE *handles;
	EFI_STATUS status;
	UINTN sz;
	int err;

	sz = 0;
	status = BS->LocateHandle(ByProtocol, &sfs_guid, 0, &sz, 0);
	if (status == EFI_BUFFER_TOO_SMALL) {
		handles = (EFI_HANDLE *)malloc(sz);
		status = BS->LocateHandle(ByProtocol, &sfs_guid, 0, &sz,
		    handles);
		if (EFI_ERROR(status))
			free(handles);
	}
	if (EFI_ERROR(status))
		return (efi_status_to_errno(status));
	err = efi_register_handles(&efifs_dev, handles,
	    sz / sizeof(EFI_HANDLE));
	free(handles);
	return (err);
}

/*
 * Print information about disks
 */
static void
efifs_dev_print(int verbose)
{
	union {
		EFI_FILE_SYSTEM_INFO info;
		char buffer[1024];
	} fi;
	char line[80];
	EFI_FILE_IO_INTERFACE *fsif;
	EFI_FILE *volume;
	EFI_HANDLE h;
	EFI_STATUS status;
	UINTN sz;
	int i, unit;

	for (unit = 0, h = efi_find_handle(&efifs_dev, 0);
	    h != NULL; h = efi_find_handle(&efifs_dev, ++unit)) {
		sprintf(line, "    %s%d: ", efifs_dev.dv_name, unit);
		pager_output(line);

		status = BS->HandleProtocol(h, &sfs_guid, (VOID **)&fsif);
		if (EFI_ERROR(status))
			goto err;

		status = fsif->OpenVolume(fsif, &volume);
		if (EFI_ERROR(status))
			goto err;

		sz = sizeof(fi);
		status = volume->GetInfo(volume, &fs_guid, &sz, &fi);
		volume->Close(volume);
		if (EFI_ERROR(status))
			goto err;

		if (fi.info.ReadOnly)
			pager_output("[RO] ");
		else
			pager_output("     ");
		for (i = 0; fi.info.VolumeLabel[i] != 0; i++)
			fi.buffer[i] = fi.info.VolumeLabel[i];
		fi.buffer[i] = 0;
		if (fi.buffer[0] != 0)
			pager_output(fi.buffer);
		else
			pager_output("EFI filesystem");
		pager_output("\n");
		continue;

	err:
		sprintf(line, "[--] error %d: unable to obtain information\n",
		    efi_status_to_errno(status));
		pager_output(line);
	}
}

/*
 * Attempt to open the disk described by (dev) for use by (f).
 *
 * Note that the philosophy here is "give them exactly what
 * they ask for".  This is necessary because being too "smart"
 * about what the user might want leads to complications.
 * (eg. given no slice or partition value, with a disk that is
 *  sliced - are they after the first BSD slice, or the DOS
 *  slice before it?)
 */
static int 
efifs_dev_open(struct open_file *f, ...)
{
	va_list		args;
	struct devdesc	*dev;

	va_start(args, f);
	dev = va_arg(args, struct devdesc*);
	va_end(args);

	if (dev->d_unit < 0)
		return(ENXIO);
	return (0);
}

static int 
efifs_dev_close(struct open_file *f)
{

	return (0);
}

static int 
efifs_dev_strategy(void *devdata, int rw, daddr_t dblk, size_t size, char *buf, size_t *rsize)
{

	return (ENOSYS);
}

struct devsw efifs_dev = {
	.dv_name = "fs", 
	.dv_type = DEVT_DISK, 
	.dv_init = efifs_dev_init,
	.dv_strategy = efifs_dev_strategy, 
	.dv_open = efifs_dev_open, 
	.dv_close = efifs_dev_close, 
	.dv_ioctl = noioctl,
	.dv_print = efifs_dev_print,
	.dv_cleanup = NULL
};
