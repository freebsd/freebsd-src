/*-
 * Copyright (c) 2014 Andrew Turner
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
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <stdarg.h>

#include <stand.h>

#include <bootstrap.h>

#include <efi.h>
#include <efilib.h>
#include <efiprot.h>

#define	SEMIHOSTING_HACKS

static EFI_GUID sfs_guid = SIMPLE_FILE_SYSTEM_PROTOCOL;
static EFI_GUID file_info_guid = EFI_FILE_INFO_ID;

static int efifs_open(const char *path, struct open_file *f);
static int efifs_write(struct open_file *f, void *buf, size_t size,
    size_t *resid);
static int efifs_close(struct open_file *f);
static int efifs_read(struct open_file *f, void *buf, size_t size,
    size_t *resid);
static off_t efifs_seek(struct open_file *f, off_t offset, int where);
static int efifs_stat(struct open_file *f, struct stat *sb);
static int efifs_readdir(struct open_file *f, struct dirent *d);

struct fs_ops efifs_fsops = {
	"efifs",
	efifs_open,
	efifs_close,
	efifs_read,
	efifs_write,
	efifs_seek,
	efifs_stat,
	efifs_readdir
};

static CHAR16 *
path_to_filename(const char *path)
{
	CHAR16 *filename;
	size_t len;
	int i;

	len = strlen(path) + 1;
	filename = malloc(len * 2);
	if (filename == NULL)
		return (NULL);

	for (i = 0; i < len; i++) {
		if (path[i] == '/')
			filename[i] = L'\\';
		else
			filename[i] = path[i];
	}

	return filename;
}

static int
efifs_open(const char *path, struct open_file *f)
{
	EFI_FILE *dev, *file;
	EFI_STATUS status;
	struct devdesc *devdesc;
	CHAR16 *filename;

	devdesc = (struct devdesc *)(f->f_devdata);
	if (devdesc->d_opendata == NULL)
		return (EINVAL);

	if (f->f_dev != &efisfs_dev)
		return (EINVAL);

	dev = devdesc->d_opendata;

	filename = path_to_filename(path);
	if (filename == NULL)
		return (ENOMEM);

	status = dev->Open(dev, &file, filename, EFI_FILE_MODE_READ, 0);
	free(filename);
	if (EFI_ERROR(status))
		return (efi_status_to_errno(status));

	f->f_fsdata = file;

	return (0);
}

static int
efifs_close(struct open_file *f)
{
	EFI_STATUS status;
	EFI_FILE *file;

	file = f->f_fsdata;
	if (file == NULL)
		return (EINVAL);

	status = file->Close(file);
	if (EFI_ERROR(status))
		return (efi_status_to_errno(status));

	return (0);
}

static int
efifs_read(struct open_file *f, void *buf, size_t size, size_t *resid)
{
	EFI_STATUS status;
	EFI_FILE *file;
	size_t read_size;

	file = f->f_fsdata;
	if (file == NULL)
		return (EINVAL);

	read_size = size;
	status = file->Read(file, &read_size, buf);
#ifdef SEMIHOSTING_HACKS
	if (status == EFI_ABORTED) {
		/*
		 * Semihosting incorrectly returns EFI_ABORTED on EOF
		 * with nothing to read.
		 */
		*resid = size;
		return (0);
	}
#endif
	if (EFI_ERROR(status))
		return (efi_status_to_errno(status));

	if (resid != NULL)
		*resid = (size - read_size);

	return (0);
}

static int
efifs_write(struct open_file *f, void *buf, size_t size, size_t *resid)
{
	printf("efifs_write\n");

	return (EINVAL);
}

static off_t
efifs_seek(struct open_file *f, off_t offset, int where)
{
	EFI_STATUS status;
	EFI_FILE *file;
	uint64_t pos;

	file = f->f_fsdata;
	if (file == NULL)
		return (-1);

	switch(where) {
	case SEEK_SET:
		pos = 0;
		break;
	case SEEK_CUR:
		/* Read the current position first */
		status = file->GetPosition(file, &pos);
		if (EFI_ERROR(status))
			return (-1);
		break;
	case SEEK_END:
	default:
		return (-1);
	}

	pos += offset;
	status = file->SetPosition(file, pos);
	if (EFI_ERROR(status))
		return (-1);

	return (pos);
}

static int
efifs_stat(struct open_file *f, struct stat *sb)
{
	EFI_FILE_INFO *info;
	EFI_STATUS status;
	EFI_FILE *file;
	UINTN infosz;

	file = f->f_fsdata;
	if (file == NULL)
		return (EINVAL);

	infosz = sizeof(EFI_FILE_INFO);
	info = malloc(infosz);
	if (info == NULL)
		return (ENOMEM);

	status = file->GetInfo(file, &file_info_guid, &infosz, info);
	/* Resize the buffer and try again if required */
	if (status == EFI_BUFFER_TOO_SMALL) {
		EFI_FILE_INFO *tmp;

		tmp = realloc(info, infosz);
		if (info == NULL) {
			free(info);
			return (ENOMEM);
		}
		info = tmp;
		status = file->GetInfo(file, &file_info_guid, &infosz, info);
	}

	if (EFI_ERROR(status))
		return (efi_status_to_errno(status));

	bzero(sb, sizeof(*sb));

	sb->st_size = info->FileSize;
	sb->st_mode = S_IRUSR;
	if (info->FileName[0] == 0)
		sb->st_mode = S_IFDIR;
	else
		sb->st_mode |=
		    (info->Attribute & EFI_FILE_DIRECTORY) ? S_IFDIR : S_IFREG;

	return (0);
}

static int
efifs_readdir(struct open_file *f, struct dirent *d)
{
	EFI_FILE_INFO *cur_file;
	EFI_STATUS status;
	EFI_FILE *file;
	UINTN filesz;
	int i;

	file = f->f_fsdata;
	if (file == NULL)
		return (EINVAL);

	filesz = sizeof(*cur_file) + sizeof(d->d_name) * 2;
	cur_file = malloc(filesz);
	if (cur_file == NULL)
		return (ENOMEM);

	bzero(cur_file, filesz);

	status = file->Read(file, &filesz, cur_file);
	if (EFI_ERROR(status)) {
		free(cur_file);
		return (efi_status_to_errno(status));
	}

	if (filesz == 0) {
		free(cur_file);
		return (ENOENT);
	}

	bzero(d, sizeof(*d));

	for (i = 0; i < sizeof(d->d_name); i++) {
		d->d_name[i] = cur_file->FileName[i];
		if (cur_file->FileName[i] == L'\0')
			break;
	}

	free(cur_file);

	return (0);
}


static int efisfs_init(void);
static int efisfs_strategy(void *, int, daddr_t, size_t, char *, size_t *);
static int efisfs_open(struct open_file *, ...);
static int efisfs_close(struct open_file *);
static void efisfs_print(int);

struct devsw efisfs_dev = {
	.dv_name = "simplefs",
	.dv_type = DEVT_FS,
	.dv_init = efisfs_init,
	.dv_strategy = efisfs_strategy,
	.dv_open = efisfs_open,
	.dv_close = efisfs_close,
	.dv_ioctl = noioctl,
	.dv_print = efisfs_print,
	.dv_cleanup = NULL
};

static int
efisfs_init(void)
{
	EFI_FILE_IO_INTERFACE *fsio;
	EFI_HANDLE *hin, *hout;
	EFI_STATUS status;
	UINTN sz;
	u_int n, nin, nout;
	int err;

	sz = 0;
	hin = NULL;
	status = BS->LocateHandle(ByProtocol, &sfs_guid, 0, &sz, 0);
	if (status == EFI_BUFFER_TOO_SMALL) {
		hin = (EFI_HANDLE *)malloc(sz * 2);
		status = BS->LocateHandle(ByProtocol, &sfs_guid, 0, &sz,
		    hin);
		if (EFI_ERROR(status))
			free(hin);
	}
	if (EFI_ERROR(status))
		return (efi_status_to_errno(status));

	/* Filter handles to only include FreeBSD partitions. */
	nin = sz / sizeof(EFI_HANDLE);
	hout = hin + nin;
	nout = 0;

	for (n = 0; n < nin; n++) {
		status = BS->HandleProtocol(hin[n], &sfs_guid, &fsio);
		if (EFI_ERROR(status))
			continue;
		if(fsio->Revision != EFI_FILE_IO_INTERFACE_REVISION)
			continue;
		hout[nout] = hin[n];
		nout++;
	}

	err = efi_register_handles(&efisfs_dev, hout, NULL, nout);
	free(hin);
	return (err);
}

static int
efisfs_strategy(void *devdata, int rw, daddr_t blk, size_t size, char *buf,
    size_t *rsize)
{
	printf("efisfs_strategy\n");

	return (ENXIO);
}

static int
efisfs_open(struct open_file *f, ...)
{
	va_list args;
	struct devdesc *dev;
	EFI_FILE_IO_INTERFACE *fsio;
	EFI_FILE *fileio;
	EFI_HANDLE h;
	EFI_STATUS status;

	va_start(args, f);
	dev = va_arg(args, struct devdesc*);
	va_end(args);

	h = efi_find_handle(&efisfs_dev, dev->d_unit);
	if (h == NULL)
		return (EINVAL);

	status = BS->HandleProtocol(h, &sfs_guid, &fsio);
	if (EFI_ERROR(status))
		return (efi_status_to_errno(status));

	if(fsio->Revision != EFI_FILE_IO_INTERFACE_REVISION)
		return (EAGAIN);

	status = fsio->OpenVolume(fsio, &fileio);
	if (EFI_ERROR(status))
		return (efi_status_to_errno(status));

	dev->d_opendata = fileio;
	return (0);
}

static int
efisfs_close(struct open_file *f)
{
	struct devdesc *dev;

	dev = (struct devdesc *)(f->f_devdata);
	if (dev->d_opendata == NULL)
		return (EINVAL);

	dev->d_opendata = NULL;
	return (0);
}

static void
efisfs_print(int verbose)
{
	printf("efisfs_print\n");
}

