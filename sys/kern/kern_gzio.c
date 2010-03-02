/*
 * $Id: kern_gzio.c,v 1.6 2008-10-18 22:54:45 lbazinet Exp $
 *
 * core_gzip.c -- gzip routines used in compressing user process cores
 *
 * This file is derived from src/lib/libz/gzio.c in FreeBSD.
 */

/* gzio.c -- IO on .gz files
 * Copyright (C) 1995-1998 Jean-loup Gailly.
 * For conditions of distribution and use, see copyright notice in zlib.h
 *
 */

/* @(#) $FreeBSD$ */

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/vnode.h>
#include <sys/syslog.h>
#include <sys/endian.h>
#include <net/zutil.h>
#include <sys/libkern.h>

#include <sys/vnode.h>
#include <sys/mount.h>

#define GZ_HEADER_LEN	10

#ifndef Z_BUFSIZE
#  ifdef MAXSEG_64K
#    define Z_BUFSIZE 4096 /* minimize memory usage for 16-bit DOS */
#  else
#    define Z_BUFSIZE 16384
#  endif
#endif
#ifndef Z_PRINTF_BUFSIZE
#  define Z_PRINTF_BUFSIZE 4096
#endif

#define ALLOC(size) malloc(size, M_TEMP, M_WAITOK | M_ZERO)
#define TRYFREE(p) {if (p) free(p, M_TEMP);}

static int gz_magic[2] = {0x1f, 0x8b}; /* gzip magic header */

/* gzip flag byte */
#define ASCII_FLAG   0x01 /* bit 0 set: file probably ascii text */
#define HEAD_CRC     0x02 /* bit 1 set: header CRC present */
#define EXTRA_FIELD  0x04 /* bit 2 set: extra field present */
#define ORIG_NAME    0x08 /* bit 3 set: original file name present */
#define COMMENT      0x10 /* bit 4 set: file comment present */
#define RESERVED     0xE0 /* bits 5..7: reserved */

typedef struct gz_stream {
    z_stream stream;
    int      z_err;   /* error code for last stream operation */
    int      z_eof;   /* set if end of input file */
    struct vnode *file; /* vnode pointer of .gz file */
    Byte     *inbuf;  /* input buffer */
    Byte     *outbuf; /* output buffer */
    uLong    crc;     /* crc32 of uncompressed data */
    char     *msg;    /* error message */
    char     *path;   /* path name for debugging only */
    int      transparent; /* 1 if input file is not a .gz file */
    char     mode;    /* 'w' or 'r' */
    long     startpos; /* start of compressed data in file (header skipped) */
    off_t    outoff;  /* current offset in output file */
    int	     flags;
} gz_stream;


local int do_flush        OF((gzFile file, int flush));
local int    destroy      OF((gz_stream *s));
local void   putU32      OF((gz_stream *file, uint32_t x));
local void *gz_alloc      OF((void *notused, u_int items, u_int size));
local void gz_free        OF((void *notused, void *ptr));

/* ===========================================================================
     Opens a gzip (.gz) file for reading or writing. The mode parameter
   is as in fopen ("rb" or "wb"). The file is given either by file descriptor
   or path name (if fd == -1).
     gz_open return NULL if the file could not be opened or if there was
   insufficient memory to allocate the (de)compression state; errno
   can be checked to distinguish the two cases (if errno is zero, the
   zlib error is Z_MEM_ERROR).
*/
gzFile gz_open (path, mode, vp)
    const char *path;
    const char *mode;
    struct vnode *vp;
{
    int err;
    int level = Z_DEFAULT_COMPRESSION; /* compression level */
    int strategy = Z_DEFAULT_STRATEGY; /* compression strategy */
    const char *p = mode;
    gz_stream *s;
    char fmode[80]; /* copy of mode, without the compression level */
    char *m = fmode;
    int resid;
    int error;
    char buf[GZ_HEADER_LEN + 1];

    if (!path || !mode) return Z_NULL;

    s = (gz_stream *)ALLOC(sizeof(gz_stream));
    if (!s) return Z_NULL;

    s->stream.zalloc = (alloc_func)gz_alloc;
    s->stream.zfree = (free_func)gz_free;
    s->stream.opaque = (voidpf)0;
    s->stream.next_in = s->inbuf = Z_NULL;
    s->stream.next_out = s->outbuf = Z_NULL;
    s->stream.avail_in = s->stream.avail_out = 0;
    s->file = NULL;
    s->z_err = Z_OK;
    s->z_eof = 0;
    s->crc = 0;
    s->msg = NULL;
    s->transparent = 0;
    s->outoff = 0;
    s->flags = 0;

    s->path = (char*)ALLOC(strlen(path)+1);
    if (s->path == NULL) {
        return destroy(s), (gzFile)Z_NULL;
    }
    strcpy(s->path, path); /* do this early for debugging */

    s->mode = '\0';
    do {
        if (*p == 'r') s->mode = 'r';
        if (*p == 'w' || *p == 'a') s->mode = 'w';
        if (*p >= '0' && *p <= '9') {
	    level = *p - '0';
	} else if (*p == 'f') {
	  strategy = Z_FILTERED;
	} else if (*p == 'h') {
	  strategy = Z_HUFFMAN_ONLY;
	} else {
	    *m++ = *p; /* copy the mode */
	}
    } while (*p++ && m != fmode + sizeof(fmode));

    if (s->mode != 'w') {
        log(LOG_ERR, "gz_open: mode is not w (%c)\n", s->mode);
        return destroy(s), (gzFile)Z_NULL;
    }
    
    err = deflateInit2(&(s->stream), level,
                       Z_DEFLATED, -MAX_WBITS, DEF_MEM_LEVEL, strategy);
    /* windowBits is passed < 0 to suppress zlib header */

    s->stream.next_out = s->outbuf = (Byte*)ALLOC(Z_BUFSIZE);
    if (err != Z_OK || s->outbuf == Z_NULL) {
        return destroy(s), (gzFile)Z_NULL;
    }

    s->stream.avail_out = Z_BUFSIZE;
    s->file = vp;

    /* Write a very simple .gz header:
     */
    snprintf(buf, sizeof(buf), "%c%c%c%c%c%c%c%c%c%c", gz_magic[0],
             gz_magic[1], Z_DEFLATED, 0 /*flags*/, 0,0,0,0 /*time*/,
             0 /*xflags*/, OS_CODE);

    if ((error = vn_rdwr(UIO_WRITE, s->file, buf, GZ_HEADER_LEN, s->outoff,
                         UIO_SYSSPACE, IO_NODELOCKED|IO_UNIT, curproc->p_ucred,
                         NOCRED, &resid, curthread))) {
        s->outoff += GZ_HEADER_LEN - resid;
        return destroy(s), (gzFile)Z_NULL;
    }
    s->outoff += GZ_HEADER_LEN;
    s->startpos = 10L;
    
    return (gzFile)s;
}


 /* ===========================================================================
 * Cleanup then free the given gz_stream. Return a zlib error code.
   Try freeing in the reverse order of allocations.
 */
local int destroy (s)
    gz_stream *s;
{
    int err = Z_OK;

    if (!s) return Z_STREAM_ERROR;

    TRYFREE(s->msg);

    if (s->stream.state != NULL) {
	if (s->mode == 'w') {
	    err = deflateEnd(&(s->stream));
	}
    }
    if (s->z_err < 0) err = s->z_err;

    TRYFREE(s->inbuf);
    TRYFREE(s->outbuf);
    TRYFREE(s->path);
    TRYFREE(s);
    return err;
}


/* ===========================================================================
     Writes the given number of uncompressed bytes into the compressed file.
   gzwrite returns the number of bytes actually written (0 in case of error).
*/
int ZEXPORT gzwrite (file, buf, len)
    gzFile file;
    const voidp buf;
    unsigned len;
{
    gz_stream *s = (gz_stream*)file;
    off_t curoff;
    size_t resid;
    int error;
    int vfslocked;

    if (s == NULL || s->mode != 'w') return Z_STREAM_ERROR;

    s->stream.next_in = (Bytef*)buf;
    s->stream.avail_in = len;

    curoff = s->outoff;
    while (s->stream.avail_in != 0) {

        if (s->stream.avail_out == 0) {

            s->stream.next_out = s->outbuf;
            vfslocked = VFS_LOCK_GIANT(s->file->v_mount);
            error = vn_rdwr_inchunks(UIO_WRITE, s->file, s->outbuf, Z_BUFSIZE,
                        curoff, UIO_SYSSPACE, IO_NODELOCKED|IO_UNIT,
                        curproc->p_ucred, NOCRED, &resid, curthread);
            VFS_UNLOCK_GIANT(vfslocked);
            if (error) {
                log(LOG_ERR, "gzwrite: vn_rdwr return %d\n", error);
                curoff += Z_BUFSIZE - resid;
                s->z_err = Z_ERRNO;
                break;
            }
            curoff += Z_BUFSIZE;
            s->stream.avail_out = Z_BUFSIZE;
        }
        s->z_err = deflate(&(s->stream), Z_NO_FLUSH);
        if (s->z_err != Z_OK) {
            log(LOG_ERR,
                "gzwrite: deflate returned error %d\n", s->z_err);
            break;
        }
    }

    s->crc = ~crc32_raw(buf, len, ~s->crc);
    s->outoff = curoff;

    return (int)(len - s->stream.avail_in);
}


/* ===========================================================================
     Flushes all pending output into the compressed file. The parameter
   flush is as in the deflate() function.
*/
local int do_flush (file, flush)
    gzFile file;
    int flush;
{
    uInt len;
    int done = 0;
    gz_stream *s = (gz_stream*)file;
    off_t curoff = s->outoff;
    size_t resid;
    int vfslocked = 0;
    int error;

    if (s == NULL || s->mode != 'w') return Z_STREAM_ERROR;

    if (s->stream.avail_in) {
        log(LOG_WARNING, "do_flush: avail_in non-zero on entry\n");
    } 

    s->stream.avail_in = 0; /* should be zero already anyway */

    for (;;) {
        len = Z_BUFSIZE - s->stream.avail_out;

        if (len != 0) {
            vfslocked = VFS_LOCK_GIANT(s->file->v_mount);
            error = vn_rdwr_inchunks(UIO_WRITE, s->file, s->outbuf, len, curoff,
                        UIO_SYSSPACE, IO_NODELOCKED|IO_UNIT, curproc->p_ucred,
                        NOCRED, &resid, curthread);
            VFS_UNLOCK_GIANT(vfslocked);
	    if (error) {
                s->z_err = Z_ERRNO;
                s->outoff = curoff + len - resid;
                return Z_ERRNO;
            }
            s->stream.next_out = s->outbuf;
            s->stream.avail_out = Z_BUFSIZE;
            curoff += len;
        }
        if (done) break;
        s->z_err = deflate(&(s->stream), flush);

	/* Ignore the second of two consecutive flushes: */
	if (len == 0 && s->z_err == Z_BUF_ERROR) s->z_err = Z_OK;

        /* deflate has finished flushing only when it hasn't used up
         * all the available space in the output buffer: 
         */
        done = (s->stream.avail_out != 0 || s->z_err == Z_STREAM_END);
 
        if (s->z_err != Z_OK && s->z_err != Z_STREAM_END) break;
    }
    s->outoff = curoff;

    return  s->z_err == Z_STREAM_END ? Z_OK : s->z_err;
}

int ZEXPORT gzflush (file, flush)
     gzFile file;
     int flush;
{
    gz_stream *s = (gz_stream*)file;
    int err = do_flush (file, flush);

    if (err) return err;
    return  s->z_err == Z_STREAM_END ? Z_OK : s->z_err;
}


/* ===========================================================================
   Outputs a long in LSB order to the given file
*/
local void putU32 (s, x)
    gz_stream *s;
    uint32_t x;
{
    uint32_t xx;
    off_t curoff = s->outoff;
    int resid;

#if BYTE_ORDER == BIG_ENDIAN
    xx = bswap32(x);
#else
    xx = x;
#endif
    vn_rdwr(UIO_WRITE, s->file, (caddr_t)&xx, sizeof(xx), curoff,
            UIO_SYSSPACE, IO_NODELOCKED|IO_UNIT, curproc->p_ucred,
            NOCRED, &resid, curthread);
    s->outoff += sizeof(xx) - resid;
}


/* ===========================================================================
     Flushes all pending output if necessary, closes the compressed file
   and deallocates all the (de)compression state.
*/
int ZEXPORT gzclose (file)
    gzFile file;
{
    int err;
    gz_stream *s = (gz_stream*)file;

    if (s == NULL) return Z_STREAM_ERROR;

    if (s->mode == 'w') {
        err = do_flush (file, Z_FINISH);
        if (err != Z_OK) {
            log(LOG_ERR, "gzclose: do_flush failed (err %d)\n", err);
            return destroy((gz_stream*)file);
        }
#if 0
	printf("gzclose: putting crc: %lld total: %lld\n",
	    (long long)s->crc, (long long)s->stream.total_in);
	printf("sizeof uLong = %d\n", (int)sizeof(uLong));
#endif
        putU32 (s, s->crc);
        putU32 (s, (uint32_t) s->stream.total_in);
    }
    return destroy((gz_stream*)file);
}

/*
 * Space allocation and freeing routines for use by zlib routines when called
 * from gzip modules.
 */
static void *
gz_alloc(void *notused __unused, u_int items, u_int size)
{
    void *ptr;

    MALLOC(ptr, void *, items * size, M_TEMP, M_NOWAIT | M_ZERO);
    return ptr;
}
                                     
static void
gz_free(void *opaque __unused, void *ptr)
{
    FREE(ptr, M_TEMP);
}

