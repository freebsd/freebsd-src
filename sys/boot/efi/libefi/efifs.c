/*-
 * Copyright (c) 2001 Doug Rabson
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
 *
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/time.h>
#include <stddef.h>
#include <stand.h>
#include <stdarg.h>

#include <efi.h>
#include <efilib.h>
#include "efiboot.h"

static int
efifs_open(const char *upath, struct open_file *f)
{
	struct efi_devdesc *dev = f->f_devdata;
	static EFI_GUID sfsid = SIMPLE_FILE_SYSTEM_PROTOCOL;
	EFI_FILE_IO_INTERFACE *sfs;
	EFI_FILE *root;
	EFI_FILE *file;
	EFI_STATUS status;
	CHAR16 *cp;
	CHAR16 *path;

	if (!dev->d_handle)
		return ENOENT;

	status = BS->HandleProtocol(dev->d_handle, &sfsid, (VOID **)&sfs);
	if (EFI_ERROR(status))
		return ENOENT;

	/*
	 * Find the root directory.
	 */
	status = sfs->OpenVolume(sfs, &root);

	/*
	 * Convert path to CHAR16, skipping leading separators.
	 */
	while (*upath == '/')
		upath++;
	if (!*upath) {
		/* Opening the root directory, */
		f->f_fsdata = root;
		return 0;
	}
	cp = path = malloc((strlen(upath) + 1) * sizeof(CHAR16));
	if (path == NULL)
		return ENOMEM;
	while (*upath) {
		if (*upath == '/')
			*cp = '\\';
		else
			*cp = *upath;
		upath++;
		cp++;
	}
	*cp++ = 0;

	/*
	 * Try to open it.
	 */
	status = root->Open(root, &file, path, EFI_FILE_MODE_READ, 0);
	free(path);
	if (EFI_ERROR(status)) {
		root->Close(root);
		return ENOENT;
	}

	root->Close(root);
	f->f_fsdata = file;
	return 0;
}

static int
efifs_close(struct open_file *f)
{
	EFI_FILE *file = f->f_fsdata;

	file->Close(file);
	return 0;
}

static int
efifs_read(struct open_file *f, void *buf, size_t size, size_t *resid)
{
	EFI_FILE *file = f->f_fsdata;
	EFI_STATUS status;
	UINTN sz = size;
	char *bufp;

	bufp = buf;
	while (size > 0) {
		sz = size;
		if (sz > 8192)
			sz = 8192;
		status = file->Read(file, &sz, bufp);
		twiddle();
		if (EFI_ERROR(status))
			return EIO;
		if (sz == 0)
			break;
		size -= sz;
		bufp += sz;
	}
	*resid = size;
	return 0;
}

static off_t
efifs_seek(struct open_file *f, off_t offset, int where)
{
	EFI_FILE *file = f->f_fsdata;
	EFI_STATUS status;
	UINT64 base;
	UINTN sz;
	static EFI_GUID infoid = EFI_FILE_INFO_ID;
	EFI_FILE_INFO info;

	switch (where) {
	case SEEK_SET:
		base = 0;
		break;

	case SEEK_CUR:
		status = file->GetPosition(file, &base);
		if (EFI_ERROR(status))
			return -1;
		break;

	case SEEK_END:
		sz = sizeof(info);
		status = file->GetInfo(file, &infoid, &sz, &info);
		if (EFI_ERROR(status))
			return -1;
		base = info.FileSize;
		break;
	}

	status = file->SetPosition(file, base + offset);
	if (EFI_ERROR(status))
		return -1;
	file->GetPosition(file, &base);

	return base;
}

static int
efifs_stat(struct open_file *f, struct stat *sb)
{
	EFI_FILE *file = f->f_fsdata;
	EFI_STATUS status;
	char *buf;
	UINTN sz;
	static EFI_GUID infoid = EFI_FILE_INFO_ID;
	EFI_FILE_INFO *info;

	bzero(sb, sizeof(*sb));

	buf = malloc(1024);
	sz = 1024;

	status = file->GetInfo(file, &infoid, &sz, buf);
	if (EFI_ERROR(status)) {
		free(buf);
		return -1;
	}

	info = (EFI_FILE_INFO *) buf;

	if (info->Attribute & EFI_FILE_READ_ONLY)
		sb->st_mode = S_IRUSR;
	else
		sb->st_mode = S_IRUSR | S_IWUSR;
	if (info->Attribute & EFI_FILE_DIRECTORY)
		sb->st_mode |= S_IFDIR;
	sb->st_size = info->FileSize;

	free(buf);
	return 0;
}

static int
efifs_readdir(struct open_file *f, struct dirent *d)
{
	EFI_FILE *file = f->f_fsdata;
	EFI_STATUS status;
	char *buf;
	UINTN sz;
	EFI_FILE_INFO *info;
	int i;

	buf = malloc(1024);
	sz = 1024;

	status = file->Read(file, &sz, buf);
	if (EFI_ERROR(status) || sz < offsetof(EFI_FILE_INFO, FileName))
	    return ENOENT;

	info = (EFI_FILE_INFO *) buf;

	d->d_fileno = 0;
	d->d_reclen = sizeof(*d);
	if (info->Attribute & EFI_FILE_DIRECTORY)
		d->d_type = DT_DIR;
	else
		d->d_type = DT_REG;
	d->d_namlen = ((info->Size - offsetof(EFI_FILE_INFO, FileName))
		       / sizeof(CHAR16));
	for (i = 0; i < d->d_namlen; i++)
		d->d_name[i] = info->FileName[i];
	d->d_name[i] = 0;

	free(buf);
	return 0;
}

struct fs_ops efi_fsops = {
	"fs",
	efifs_open,
	efifs_close,
	efifs_read,
	null_write,
	efifs_seek,
	efifs_stat,
	efifs_readdir
};

static EFI_HANDLE *fs_handles;
UINTN fs_handle_count;;

static int
efifs_dev_init(void) 
{
	EFI_STATUS	status;
	UINTN		sz;
	static EFI_GUID sfsid = SIMPLE_FILE_SYSTEM_PROTOCOL;

	sz = 0;
	status = BS->LocateHandle(ByProtocol, &sfsid, 0, &sz, 0);
	if (status != EFI_BUFFER_TOO_SMALL)
		return ENOENT;
	fs_handles = (EFI_HANDLE *) malloc(sz);
	status = BS->LocateHandle(ByProtocol, &sfsid, 0,
				  &sz, fs_handles);
	if (EFI_ERROR(status)) {
		free(fs_handles);
		return ENOENT;
	}
	fs_handle_count = sz / sizeof(EFI_HANDLE);

	return 0;
}

/*
 * Print information about disks
 */
static void
efifs_dev_print(int verbose)
{
	int		i;
	char		line[80];

	for (i = 0; i < fs_handle_count; i++) {
		sprintf(line, "    fs%d:   EFI filesystem", i);
		pager_output(line);
		/* XXX more detail? */
		pager_output("\n");
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
	va_list			args;
	struct efi_devdesc	*dev;
	int			unit;

	va_start(args, f);
	dev = va_arg(args, struct efi_devdesc*);
	va_end(args);

	unit = dev->d_kind.efidisk.unit;
	if (unit < 0 || unit >= fs_handle_count) {
		printf("attempt to open nonexistent EFI filesystem\n");
		return(ENXIO);
	}

	dev->d_handle = fs_handles[unit];

	return 0;
}

static int 
efifs_dev_close(struct open_file *f)
{

	return 0;
}

static int 
efifs_dev_strategy(void *devdata, int rw, daddr_t dblk, size_t size, char *buf, size_t *rsize)
{
	return 0;
}

struct devsw efifs_dev = {
	"fs", 
	DEVT_DISK, 
	efifs_dev_init,
	efifs_dev_strategy, 
	efifs_dev_open, 
	efifs_dev_close, 
	noioctl,
	efifs_dev_print
};
