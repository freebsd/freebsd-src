/*
 * gzip.h -- common declarations for all gzip modules
 * Copyright (C) 1992-1993 Jean-loup Gailly.
 * Adapted for FreeBSD boot unpacker by Serge Vakulenko.
 * This is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License, see the file COPYING.
 */

typedef unsigned char  uchar;
typedef unsigned long  ulong;

#define NULL 0

#define STORED          0               /* Compression methods */
#define COMPRESSED      1
#define PACKED          2
#define DEFLATED        8               /* methods 3 to 7 reserved */

#define INBUFSIZ        0x8000          /* input buffer size */

#define OUTBUFSIZ       16384           /* output buffer size */
#define OUTBUF_EXTRA    2048            /* required by unlzw() */

#define GZIP_MAGIC      "\037\213"      /* gzip files, 1F 8B */
#define OLD_GZIP_MAGIC  "\037\236"      /* gzip 0.5 = freeze 1.x */
#define PKZIP_MAGIC     "PK\003\004"    /* pkzip files */
#define PACK_MAGIC      "\037\036"      /* packed files */
#define LZW_MAGIC       "\037\235"      /* lzw files, 1F 9D */

/* gzip flag byte */
#define ASCII_FLAG      0x01            /* file probably ascii text */
#define CONTINUATION    0x02            /* cont. of multi-part gzip file */
#define EXTRA_FIELD     0x04            /* extra field present */
#define ORIG_NAME       0x08            /* original file name present */
#define COMMENT         0x10            /* file comment present */
#define ENCRYPTED       0x20            /* file is encrypted */
#define RESERVED        0xC0            /* reserved */

/* window size--must be a power of two, and */
/* at least 32K for zip's deflate method */
#define WSIZE           0x8000

extern int method;      /* compression method */

extern uchar *inbuf;   /* input buffer */
extern uchar *outbuf;  /* output buffer */
extern uchar *window;  /* Sliding window and suffix table (unlzw) */

extern unsigned insize; /* valid bytes in inbuf */
extern unsigned inptr;  /* index of next byte to be processed in inbuf */
extern unsigned outcnt; /* bytes in output buffer */

extern int pkzip;               /* set for a pkzip file */
extern int extended;            /* set if extended local header */
extern ulong crc;               /* shift register contents */
extern ulong output_ptr;        /* total output bytes */

extern void unzip (void);
extern void check_zipfile (void);
extern void updcrc (uchar *s, unsigned n);
extern void clear_bufs (void);
extern void fill_inbuf (void);
extern void flush_window (void);
extern void error (char *m);

static inline uchar get_byte ()
{
	if (inptr >= insize)
		fill_inbuf ();
	return (inbuf[inptr++]);
}

static inline void put_char (uchar c)
{
	window[outcnt++] = c;
	if (outcnt == WSIZE)
		flush_window();
}
