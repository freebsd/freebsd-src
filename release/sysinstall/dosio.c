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

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <ctype.h>
#include <string.h>
#include "dosio.h"

#define SECSIZ  512             /* sector size */
#define SSHIFT    9             /* SECSIZ shift */
#define DEPSEC   16             /* directory entries per sector */
#define DSHIFT    4             /* DEPSEC shift */
#define NFATS     2             /* number of FATs */
#define DENMSZ    8             /* DE name size */
#define DEXTSZ    3             /* DE extension size */
#define DENXSZ   11             /* DE name + extension size */
#define LOCLUS    2             /* lowest cluster number */

/* DOS "BIOS Parameter Block" */
typedef struct {
   u_char secsiz[2];            /* sector size */
   u_char spc;                  /* sectors per cluster */
   u_char ressec[2];            /* reserved sectors */
   u_char fats;                 /* FATs */
   u_char dirents[2];           /* root directory entries */
   u_char secs[2];              /* total sectors */
   u_char media;                /* media descriptor */
   u_char spf[2];               /* sectors per FAT */
   u_char spt[2];               /* sectors per track */
   u_char heads[2];             /* drive heads */
   u_char hidsec[4];            /* hidden sectors */
   u_char lsecs[4];             /* huge sectors */
} DOS_BPB;

/* Fixed portion of DOS boot sector */
typedef struct {
   u_char jmp[3];               /* usually 80x86 'jmp' opcode */
   u_char oem[8];               /* OEM name and version */
   DOS_BPB bpb;                 /* BPB */
   u_char drive;                /* drive number */
   u_char reserved;             /* reserved */
   u_char extsig;               /* extended boot signature */
   u_char volid[4];             /* volume ID */
   u_char label[11];            /* volume label */
   u_char fstype[8];            /* file system type */
} DOS_BS;

/* Supply missing "." and ".." root directory entries */
static DOS_DE dot[2] = {
   {".       ", "   ", FA_DIR, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
      {0, 0}, {0x21, 0}, {0, 0}, {0, 0, 0, 0} },
   {"..      ", "   ", FA_DIR, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
      {0, 0}, {0x21, 0}, {0, 0}, {0, 0, 0, 0} }
};

/* I/O error handler address */
int (*dos_ioerr)(int op) = NULL;

/* The usual conversion macros to avoid multiplication and division */
#define bytsec(n)      ((n) >> (SSHIFT))
#define secbyt(s)      ((u_long)(s) << (SSHIFT))
#define entsec(e)      ((e) >> (DSHIFT))
#define bytblk(fs, n)  ((n) >> (fs)->bshift)
#define blkbyt(fs, b)  ((u_long)(b) << (fs)->bshift)
#define blksec(fs, b)  ((u_long)(b) << ((fs)->bshift - (SSHIFT)))

/* Convert cluster number to offset within filesystem */
#define blkoff(fs, b)  secbyt((fs)->lsndta) + blkbyt(fs, (b) - (LOCLUS))

/* Convert cluster number to logical sector number */
#define blklsn(fs, b)  ((fs)->lsndta + blksec(fs, (b) - (LOCLUS)))

/* Convert cluster number to offset within FAT */
#define fatoff(fat12, c)  ((u_long)(c) + ((fat12) ? (c) >> 1 : (c)))

/* Does cluster number reference a valid data cluster? */
#define okclus(fs, c)  ((c) >= (LOCLUS) && (c) <= (fs)->xclus)

/* Return on error */
#define RETERR(err) {  \
   errno = err;        \
   return -1;          \
}

static int dosunmount(DOS_FS *fs);
static int dosstat(DOS_FS *fs, DOS_DE *de, struct stat *sb);
static int parsebs(DOS_FS *fs, DOS_BS *bs);
static int namede(DOS_FS *fs, const char *path, DOS_DE **dep);
static DOS_DE *lookup(DOS_FS *fs, unsigned c, const u_char *nx, int *err);
static off_t fsize(DOS_FS *fs, DOS_DE *de);
static u_short fatget(DOS_FS *fs, u_short c);
static int fatend(int fat12, u_short c);
static int fatcnt(DOS_FS *fs, u_short c);
static int ioread(int fd, off_t offset, void *buf, size_t nbytes);
static int ioget(int fd, u_long lsec, void *buf, u_int nsec);
static u_char *nxname(const char *name, char **endptr);
static int sepchar(int c);
static int wildchar(int c);
static int doschar(int c);

/*
 * Mount DOS filesystem
 */
int
dos_mount(DOS_FS *fs, const char *devname)
{
   char buf[SECSIZ];
   int err;

   memset(fs, 0, sizeof(DOS_FS));
   if ((fs->fd = open(devname, O_RDONLY)) == -1)
      RETERR(errno);
   if (!(err = ioget(fs->fd, 0, buf, 1)) &&
       !(err = parsebs(fs, (DOS_BS *)buf)))
      if (!(fs->fat = malloc(secbyt(fs->spf))))
         err = errno;
      else
         err = ioget(fs->fd, fs->lsnfat, fs->fat, fs->spf);
   if (err) {
      dosunmount(fs);
      RETERR(err);
   }
   fs->bsize = secbyt(fs->spc);
   fs->bshift = ffs(fs->bsize) - 1;
   return 0;
}

/*
 * Unmount mounted filesystem
 */
int
dos_unmount(DOS_FS *fs)
{
   int err;

   if (fs->links)
      RETERR(EBUSY);
   if ((err = dosunmount(fs)))
      RETERR(err);
   return 0;
}

/*
 * Common code shared by dos_mount() and dos_unmount()
 */
static
int dosunmount(DOS_FS *fs)
{
   if (fs->fat)
      free(fs->fat);
   return close(fs->fd) ? errno : 0;
}

/*
 * Determine free data space in filesystem (in bytes)
 */
u_long
dos_free(DOS_FS *fs)
{
   unsigned n, c;

   n = 0;
   for (c = LOCLUS; c <= fs->xclus; c++)
      if (!fatget(fs, c))
         n++;
   return blkbyt(fs, n);
}

/*
 * Close open file
 */
int
dos_close(void *v)
{
    DOS_FILE *f = v;

    f->fs->links--;
    free(f);
    return 0;
}

/*
 * Reposition with file
 */
fpos_t
dos_seek(void *v, fpos_t offset, int whence)
{
   off_t off;
   u_long size;
   DOS_FILE *f = v;

   size = cv4(f->de.size);
   switch (whence) {
   case SEEK_SET:
      off = 0;
      break;
   case SEEK_CUR:
      off = f->offset;
      break;
   case SEEK_END:
      off = size;
      break;
   default:
      RETERR(EINVAL);
   }
   off += offset;
   if (off < 0 || off > size)
      RETERR(EINVAL);
   f->offset = off;
   f->c = 0;
   return 0;
}

/*
 * Read from file
 */
int
dos_read(void *v, char *buf, int nbytes)
{
   off_t size;
   u_long off, cnt, n;
   unsigned clus, c;
   int err;
   DOS_FILE *f = v;

   if ((size = fsize(f->fs, &f->de)) == -1)
      RETERR(EBADFS);
   if (nbytes > (n = size - f->offset))
      nbytes = n;
   off = f->offset;
   if ((clus = cv2(f->de.clus))) 
      off &= f->fs->bsize - 1;
   c = f->c;
   cnt = nbytes;
   while (cnt) {
      n = 0;
      if (!c) {
         if ((c = clus))
            n = bytblk(f->fs, f->offset);
      } else if (!off)
         n++;
      while (n--) {
         c = fatget(f->fs, c);
         if (!okclus(f->fs, c))
            RETERR(EBADFS);
      }
      if (!clus || (n = f->fs->bsize - off) > cnt)
         n = cnt;
      if ((err = ioread(f->fs->fd, (c ? blkoff(f->fs, c) : 
                                        secbyt(f->fs->lsndir)) + off,
                        buf, n)))
         RETERR(err);
      f->offset += n;
      f->c = c;
      off = 0;
      buf += n;
      cnt -= n;
   }
   return nbytes;
}

/*
 * Get file status 
 */
int
dos_stat(DOS_FS *fs, const char *path, struct stat *sb)
{
   DOS_DE *de;
   int err;

   if ((err = namede(fs, path, &de)) || (err = dosstat(fs, de, sb)))
      RETERR(err);
   return 0;
}

/*
 * Get file status of open file
 */
int
dos_fstat(DOS_FILE *f, struct stat *sb)
{
   int err;

   if ((err = dosstat(f->fs, &f->de, sb)))
      RETERR(err);
   return 0;
}

/*
 * File status primitive
 */
static int
dosstat(DOS_FS *fs, DOS_DE *de, struct stat *sb)
{

   memset(sb, 0, sizeof(struct stat));
   sb->st_mode = (de->attr & FA_DIR) ? S_IFDIR | 0777 : S_IFREG | 0666;
   if (de->attr & FA_RDONLY)
      sb->st_mode &= ~0222;
   if (de->attr & FA_HIDDEN)
      sb->st_mode &= ~0007;
   if (de->attr & FA_SYSTEM)
      sb->st_mode &= ~0077;
   sb->st_nlink = 1;
   dos_cvtime(&sb->st_atime, cv2(de->date), cv2(de->time));
   sb->st_mtime = sb->st_atime;
   sb->st_ctime = sb->st_atime;
   if ((sb->st_size = fsize(fs, de)) == -1)
      return EBADFS;
   if (!(de->attr & FA_DIR) || cv2(de->clus))
      sb->st_blocks = bytblk(fs, sb->st_size + fs->bsize - 1);
   sb->st_blksize = fs->bsize;
   return 0;
}

/*
 * Convert from DOS date and time
 */
void
dos_cvtime(time_t *timer, u_short ddate, u_short dtime)
{
   struct tm tm;

   memset(&tm, 0, sizeof(tm));
   tm.tm_sec =  (dtime & 0x1f) << 1;
   tm.tm_min =  dtime >> 5 & 0x3f;
   tm.tm_hour = dtime >> 11;
   tm.tm_mday = ddate & 0x1f;
   tm.tm_mon = (ddate >> 5 & 0xf) - 1;
   tm.tm_year = 80 + (ddate >> 9);
   *timer = mktime(&tm);
}

/*
 * Open DOS file
 */
FILE *
dos_open(DOS_FS *fs, const char *path)
{
   DOS_DE *de;
   DOS_FILE *f;
   u_long size;
   u_int clus;
   int err;
   FILE *fp;

   if ((err = namede(fs, path, &de)))
       return NULL;
   clus = cv2(de->clus);
   size = cv4(de->size);
   if ((clus && (!okclus(fs, clus) || (!(de->attr & FA_DIR) && !size))) || 
       (!clus && !(de->attr & FA_DIR) && size))
       return NULL;
   f = (DOS_FILE *)malloc(sizeof(DOS_FILE));
   memset(f, 0, sizeof(DOS_FILE));
   f->fs = fs;
   fs->links++;
   f->de = *de;
   fp = funopen(f, dos_read, NULL, dos_seek, dos_close);
   return fp;
}

/*
 * Parse DOS boot sector
 */
static int
parsebs(DOS_FS *fs, DOS_BS *bs)
{
   u_long sc;

   if ((bs->jmp[0] != 0xe9 && (bs->jmp[0] != 0xeb || bs->jmp[2] != 0x90)) ||
       bs->bpb.media < 0xf0 ||
       cv2(bs->bpb.secsiz) != SECSIZ ||
       !bs->bpb.spc || (bs->bpb.spc ^ (bs->bpb.spc - 1)) < bs->bpb.spc)
      return EINVAL;
   fs->spf = cv2(bs->bpb.spf);
   fs->dirents = cv2(bs->bpb.dirents);
   fs->spc = bs->bpb.spc;
   sc = cv2(bs->bpb.secs);
   if (!sc && bs->extsig == 0x29)
      sc = cv4(bs->bpb.lsecs);
   if (!sc || bs->bpb.fats != NFATS || bs->bpb.spc > 64)
      return EINVAL;
   if (!fs->dirents || fs->dirents & (DEPSEC - 1))
      return EINVAL;
   fs->lsnfat = cv2(bs->bpb.ressec);
   fs->lsndir = fs->lsnfat + (u_long)fs->spf * NFATS;
   fs->lsndta = fs->lsndir + entsec(fs->dirents);
   if (fs->lsndta > sc || !(sc = (sc - fs->lsndta) / fs->spc) || sc >= 0xfff6)
      return EINVAL;
   fs->fat12 = sc < 0xff6;
   fs->xclus = sc + 1;
   if (fs->spf < bytsec(fatoff(fs->fat12, fs->xclus) + SECSIZ))
      return EINVAL;
   if (bs->extsig == 0x29)
      fs->volid = cv4(bs->volid);
   return 0;
}

/*
 * Return directory entry from path
 */
static int
namede(DOS_FS *fs, const char *path, DOS_DE **dep)
{
   DOS_DE *de;
   u_char *nx;
   int err;

   err = 0;
   de = dot;
   if (*path == '/')
      path++;
   while (*path) {
      if (!(nx = nxname(path, (char **)&path)))
         return EINVAL;
      if (!(de->attr & FA_DIR))
         return ENOTDIR;
      if (!(de = lookup(fs, cv2(de->clus), nx, &err)))
         return err ? err : ENOENT;
      if (*path == '/')
         path++;
   }
   *dep = de;
   return 0;
}

/*
 * Lookup path segment
 */
static DOS_DE *
lookup(DOS_FS *fs, unsigned c, const u_char *nx, int *err)
{
   static DOS_DE dir[DEPSEC];
   u_long lsec;
   u_int nsec;
   int s, e;

   if (!c) 
      for (e = 0; e < 2; e++)
         if (!memcmp(dot + e, nx, DENXSZ))
            return dot + e;
   nsec = !c ? entsec(fs->dirents) : fs->spc;
   lsec = 0;
   do {
      if (!c && !lsec)
         lsec = fs->lsndir;
      else if okclus(fs, c)
         lsec = blklsn(fs, c);
      else {
         *err = EBADFS;
         return NULL;
      }
      for (s = 0; s < nsec; s++) {
         if ((e = ioget(fs->fd, lsec + s, dir, 1))) {
            *err = e;
            return NULL;
         }
         for (e = 0; e < DEPSEC; e++) {
            if (!*dir[e].name)
               return NULL;
            if (*dir[e].name == 0xe5 || dir[e].attr & FA_LABEL)
               continue;
            if (!memcmp(dir + e, nx, DENXSZ))
               return dir + e;
         }
      }
   } while (c && !fatend(fs->fat12, c = fatget(fs, c)));
   return NULL;
}

/*
 * Return size of file in bytes
 */
static off_t
fsize(DOS_FS *fs, DOS_DE *de)
{
   u_long size;
   u_int c;
   int n;

   if (!(size = cv4(de->size)) && de->attr & FA_DIR)
      if (!(c = cv2(de->clus)))
         size = fs->dirents * sizeof(DOS_DE);
      else {
         if ((n = fatcnt(fs, c)) == -1)
            return n;
         size = blkbyt(fs, n);
      }
   return size;
}

/*
 * Return next cluster in cluster chain
 */
static u_short
fatget(DOS_FS *fs, u_short c)
{
   u_short x;

   x = cv2(fs->fat + fatoff(fs->fat12, c));
   return fs->fat12 ? c & 1 ? x >> 4 : x & 0xfff : x;
}

/*
 * Count number of clusters in chain
 */
static int
fatcnt(DOS_FS *fs, u_short c)
{
   int n;

   for (n = 0; okclus(fs, c); n++)
      c = fatget(fs, c);
   return fatend(fs->fat12, c) ? n : -1;
}

/*
 * Is cluster an end-of-chain marker?
 */
static int
fatend(int fat12, u_short c)
{
   return c > (fat12 ? 0xff7 : 0xfff7) || c == 0xfff0;
}

/*
 * Offset-based I/O primitive
 */
static int
ioread(int fd, off_t offset, void *buf, size_t nbytes)
{
   char tmp[SECSIZ];
   u_int off, n;
   int err;

   if ((off = offset & (SECSIZ - 1))) {
      offset -= off;
      if ((err = ioget(fd, bytsec(offset), tmp, 1)))
         return err;
      offset += SECSIZ;
      if ((n = SECSIZ - off) > nbytes)
         n = nbytes;
      memcpy(buf, tmp + off, n);
      buf += n;
      nbytes -= n;
   }
   n = nbytes & (SECSIZ - 1);
   if (nbytes -= n) {
      if ((err = ioget(fd, bytsec(offset), buf, bytsec(nbytes))))
         return err;
      offset += nbytes;
      buf += nbytes;
   }
   if (n) {
      if ((err = ioget(fd, bytsec(offset), tmp, 1)))
         return err;
      memcpy(buf, tmp, n);
   }
   return 0;
}

/*
 * Sector-based I/O primitive
 */
static int
ioget(int fd, u_long lsec, void *buf, u_int nsec)
{
   size_t nbytes;
   ssize_t n;

   nbytes = secbyt(nsec);
   do {
      if (lseek(fd, secbyt(lsec), SEEK_SET) == -1)
         return errno;
      n = read(fd, buf, nbytes);
   } while (n == -1 && errno == EIO && dos_ioerr && dos_ioerr(0));
   if (n != nbytes)
      return n == -1 ? errno : EIO;
   return 0;
}

/*
 * Convert name to DOS directory (name + extension) format
 */
static u_char *
nxname(const char *name, char **endptr)
{
   static u_char nx[DENXSZ];
   int i;

   memset(nx, ' ', sizeof(nx));
   for (i = 0; i < DENMSZ && doschar(*name); i++)
      nx[i] = toupper(*name++);
   if (i) {
      if (i == DENMSZ)
         while (!sepchar(*name))
            name++;
      if (*name == '.') {
         name++;
         for (i = 0; i < DEXTSZ && doschar(*name); i++)
            nx[DENMSZ + i] = toupper(*name++);
         if (i == DEXTSZ)
            while(!sepchar(*name))
               name++;
      }
   } else if (*name == '.') {
      nx[0] = *name++;
      if (*name == '.')
         nx[1] = *name++;
   }
   if ((*name && *name != '/') || *nx == ' ')
      return NULL;
   if (*nx == 0xe5)
      *nx = 5;
   *endptr = (char *)name;
   return nx;
}

/*
 * Is character a path-separator?
 */
static int
sepchar(int c)
{
   return !wildchar(c) && !doschar(c);
}

/*
 * Is character a wildcard?
 */
static int
wildchar(int c)
{
   return c == '*' || c == '?';
}

/*
 * Is character valid in a DOS name?
 */
static int
doschar(int c)
{
   return c & 0x80 || (c >= ' ' && !strchr("\"*+,./:;<=>?[\\]|", c));
}
