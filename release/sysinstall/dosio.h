/*
 * Copyright (c) 1996 Robert Nordier
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef DOSIO_H
#define DOSIO_H

/* 
 * DOS file attributes
 */

#define FA_RDONLY 0x01   /* read-only */
#define FA_HIDDEN 0x02   /* hidden file */
#define FA_SYSTEM 0x04   /* system file */
#define FA_LABEL  0x08   /* volume label */
#define FA_DIR    0x10   /* directory */
#define FA_ARCH   0x20   /* archive (file modified) */

/* 
 * Error number to overload if filesystem errors are detected during
 * routine processing
 */

#define EBADFS  EINVAL

/*
 * Macros to convert DOS-format 16-bit and 32-bit quantities
 */

#define cv2(p) ((u_short)(p)[0] |         \
               ((u_short)(p)[1] << 010))

#define cv4(p) ((u_long)(p)[0] |          \
               ((u_long)(p)[1] << 010) |  \
               ((u_long)(p)[2] << 020) |  \
               ((u_long)(p)[3] << 030))

/*
 * DOS directory structure
 */

typedef struct {
   u_char name[8];              /* name */
   u_char ext[3];               /* extension */
   u_char attr;                 /* attributes */
   u_char reserved[10];         /* reserved */
   u_char time[2];              /* time */
   u_char date[2];              /* date */
   u_char clus[2];              /* starting cluster */
   u_char size[4];              /* file size */
} DOS_DE;

typedef struct {
   u_char *fat;                 /* FAT */
   u_long  volid;               /* volume id */
   u_long  lsnfat;              /* logical sector number: fat */
   u_long  lsndir;              /* logical sector number: dir */
   u_long  lsndta;              /* logical sector number: data area */
   short   fd;                  /* file descriptor */
   short   fat12;               /* 12-bit FAT entries */
   u_short spf;                 /* sectors per fat */
   u_short dirents;             /* root directory entries */
   u_short spc;                 /* sectors per cluster */
   u_short xclus;               /* maximum cluster number */
   u_short bsize;               /* cluster size in bytes */
   u_short bshift;              /* cluster conversion shift */
   u_short links;               /* active links to structure */
} DOS_FS;

typedef struct {
   DOS_FS *fs;                  /* associated filesystem */
   DOS_DE  de;                  /* directory entry */
   u_long  offset;              /* current offset */
   u_short c;                   /* last cluster read */
} DOS_FILE;

/*
 * The following variable can be set to the address of an error-handling
 * routine which will be invoked when a read() returns EIO.  The handler
 * should return 1 to retry the read, otherwise 0.
 */

extern int (*dos_ioerr)(int op);

int dos_mount(DOS_FS *fs, const char *devname);
int dos_unmount(DOS_FS *fs);
u_long dos_free(DOS_FS *fs);
FILE *dos_open(DOS_FS *fs, const char *path);
int dos_close(void *v);
fpos_t dos_seek(void *v, fpos_t offset, int whence);
int dos_read(void *v, char *buf, int nbytes);
int dos_stat(DOS_FS *fs, const char *path, struct stat *sb);
int dos_fstat(DOS_FILE *f, struct stat *sb);
void dos_cvtime(time_t *timer, u_short ddate, u_short dtime);

#endif /* !DOSIO_H */
