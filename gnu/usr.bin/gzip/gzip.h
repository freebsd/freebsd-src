/* gzip.h -- common declarations for all gzip modules
 * Copyright (C) 1992-1993 Jean-loup Gailly.
 * This is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License, see the file COPYING.
 */

#if defined(__STDC__) || defined(PROTO)
#  define OF(args)  args
#else
#  define OF(args)  ()
#endif

#ifdef __STDC__
   typedef void *voidp;
#else
   typedef char *voidp;
#endif

/* I don't like nested includes, but the string and io functions are used
 * too often
 */
#include <stdio.h>
#if !defined(NO_STRING_H) || defined(STDC_HEADERS)
#  include <string.h>
#  if !defined(STDC_HEADERS) && !defined(NO_MEMORY_H) && !defined(__GNUC__)
#    include <memory.h>
#  endif
#  define memzero(s, n)     memset ((voidp)(s), 0, (n))
#else
#  include <strings.h>
#  define strchr            index 
#  define strrchr           rindex
#  define memcpy(d, s, n)   bcopy((s), (d), (n)) 
#  define memcmp(s1, s2, n) bcmp((s1), (s2), (n)) 
#  define memzero(s, n)     bzero((s), (n))
#endif

#ifndef RETSIGTYPE
#  define RETSIGTYPE void
#endif

#define local static

typedef unsigned char  uch;
typedef unsigned short ush;
typedef unsigned long  ulg;

/* Return codes from gzip */
#define OK      0
#define ERROR   1
#define WARNING 2

/* Compression methods (see algorithm.doc) */
#define STORED      0
#define COMPRESSED  1
#define PACKED      2
#define LZHED       3
/* methods 4 to 7 reserved */
#define DEFLATED    8
#define MAX_METHODS 9
extern int method;         /* compression method */

/* To save memory for 16 bit systems, some arrays are overlaid between
 * the various modules:
 * deflate:  prev+head   window      d_buf  l_buf  outbuf
 * unlzw:    tab_prefix  tab_suffix  stack  inbuf  outbuf
 * inflate:              window             inbuf
 * unpack:               window             inbuf  prefix_len
 * unlzh:    left+right  window      c_table inbuf c_len
 * For compression, input is done in window[]. For decompression, output
 * is done in window except for unlzw.
 */

#ifndef	INBUFSIZ
#  ifdef SMALL_MEM
#    define INBUFSIZ  0x2000  /* input buffer size */
#  else
#    define INBUFSIZ  0x8000  /* input buffer size */
#  endif
#endif
#define INBUF_EXTRA  64     /* required by unlzw() */

#ifndef	OUTBUFSIZ
#  ifdef SMALL_MEM
#    define OUTBUFSIZ   8192  /* output buffer size */
#  else
#    define OUTBUFSIZ  16384  /* output buffer size */
#  endif
#endif
#define OUTBUF_EXTRA 2048   /* required by unlzw() */

#ifndef DIST_BUFSIZE
#  ifdef SMALL_MEM
#    define DIST_BUFSIZE 0x2000 /* buffer for distances, see trees.c */
#  else
#    define DIST_BUFSIZE 0x8000 /* buffer for distances, see trees.c */
#  endif
#endif

#ifdef DYN_ALLOC
#  define EXTERN(type, array)  extern type * near array
#  define DECLARE(type, array, size)  type * near array
#  define ALLOC(type, array, size) { \
      array = (type*)fcalloc((size_t)(((size)+1L)/2), 2*sizeof(type)); \
      if (array == NULL) error("insufficient memory"); \
   }
#  define FREE(array) {if (array != NULL) fcfree(array), array=NULL;}
#else
#  define EXTERN(type, array)  extern type array[]
#  define DECLARE(type, array, size)  type array[size]
#  define ALLOC(type, array, size)
#  define FREE(array)
#endif

EXTERN(uch, inbuf);          /* input buffer */
EXTERN(uch, outbuf);         /* output buffer */
EXTERN(ush, d_buf);          /* buffer for distances, see trees.c */
EXTERN(uch, window);         /* Sliding window and suffix table (unlzw) */
#define tab_suffix window
#ifndef MAXSEG_64K
#  define tab_prefix prev    /* hash link (see deflate.c) */
#  define head (prev+WSIZE)  /* hash head (see deflate.c) */
   EXTERN(ush, tab_prefix);  /* prefix code (see unlzw.c) */
#else
#  define tab_prefix0 prev
#  define head tab_prefix1
   EXTERN(ush, tab_prefix0); /* prefix for even codes */
   EXTERN(ush, tab_prefix1); /* prefix for odd  codes */
#endif

extern unsigned insize; /* valid bytes in inbuf */
extern unsigned inptr;  /* index of next byte to be processed in inbuf */
extern unsigned outcnt; /* bytes in output buffer */

extern long bytes_in;   /* number of input bytes */
extern long bytes_out;  /* number of output bytes */
extern long header_bytes;/* number of bytes in gzip header */

#define isize bytes_in
/* for compatibility with old zip sources (to be cleaned) */

extern int  ifd;        /* input file descriptor */
extern int  ofd;        /* output file descriptor */
extern char ifname[];   /* input file name or "stdin" */
extern char ofname[];   /* output file name or "stdout" */
extern char *progname;  /* program name */

extern long time_stamp; /* original time stamp (modification time) */
extern long ifile_size; /* input file size, -1 for devices (debug only) */

typedef int file_t;     /* Do not use stdio */
#define NO_FILE  (-1)   /* in memory compression */


#define	PACK_MAGIC     "\037\036" /* Magic header for packed files */
#define	GZIP_MAGIC     "\037\213" /* Magic header for gzip files, 1F 8B */
#define	OLD_GZIP_MAGIC "\037\236" /* Magic header for gzip 0.5 = freeze 1.x */
#define	LZH_MAGIC      "\037\240" /* Magic header for SCO LZH Compress files*/
#define PKZIP_MAGIC    "\120\113\003\004" /* Magic header for pkzip files */

/* gzip flag byte */
#define ASCII_FLAG   0x01 /* bit 0 set: file probably ascii text */
#define CONTINUATION 0x02 /* bit 1 set: continuation of multi-part gzip file */
#define EXTRA_FIELD  0x04 /* bit 2 set: extra field present */
#define ORIG_NAME    0x08 /* bit 3 set: original file name present */
#define COMMENT      0x10 /* bit 4 set: file comment present */
#define ENCRYPTED    0x20 /* bit 5 set: file is encrypted */
#define RESERVED     0xC0 /* bit 6,7:   reserved */

/* internal file attribute */
#define UNKNOWN 0xffff
#define BINARY  0
#define ASCII   1

#ifndef WSIZE
#  define WSIZE 0x8000     /* window size--must be a power of two, and */
#endif                     /*  at least 32K for zip's deflate method */

#define MIN_MATCH  3
#define MAX_MATCH  258
/* The minimum and maximum match lengths */

#define MIN_LOOKAHEAD (MAX_MATCH+MIN_MATCH+1)
/* Minimum amount of lookahead, except at the end of the input file.
 * See deflate.c for comments about the MIN_MATCH+1.
 */

#define MAX_DIST  (WSIZE-MIN_LOOKAHEAD)
/* In order to simplify the code, particularly on 16 bit machines, match
 * distances are limited to MAX_DIST instead of WSIZE.
 */

extern int decrypt;        /* flag to turn on decryption */
extern int exit_code;      /* program exit code */
extern int verbose;        /* be verbose (-v) */
extern int quiet;          /* be quiet (-q) */
extern int level;          /* compression level */
extern int test;           /* check .z file integrity */
extern int to_stdout;      /* output to stdout (-c) */
extern int save_orig_name; /* set if original name must be saved */

#define get_byte()  (inptr < insize ? inbuf[inptr++] : fill_inbuf(0))
#define try_byte()  (inptr < insize ? inbuf[inptr++] : fill_inbuf(1))

/* put_byte is used for the compressed output, put_ubyte for the
 * uncompressed output. However unlzw() uses window for its
 * suffix table instead of its output buffer, so it does not use put_ubyte
 * (to be cleaned up).
 */
#define put_byte(c) {outbuf[outcnt++]=(uch)(c); if (outcnt==OUTBUFSIZ)\
   flush_outbuf();}
#define put_ubyte(c) {window[outcnt++]=(uch)(c); if (outcnt==WSIZE)\
   flush_window();}

/* Output a 16 bit value, lsb first */
#define put_short(w) \
{ if (outcnt < OUTBUFSIZ-2) { \
    outbuf[outcnt++] = (uch) ((w) & 0xff); \
    outbuf[outcnt++] = (uch) ((ush)(w) >> 8); \
  } else { \
    put_byte((uch)((w) & 0xff)); \
    put_byte((uch)((ush)(w) >> 8)); \
  } \
}

/* Output a 32 bit value to the bit stream, lsb first */
#define put_long(n) { \
    put_short((n) & 0xffff); \
    put_short(((ulg)(n)) >> 16); \
}

#define seekable()    0  /* force sequential output */
#define translate_eol 0  /* no option -a yet */

#define tolow(c)  (isupper(c) ? (c)-'A'+'a' : (c))    /* force to lower case */

/* Macros for getting two-byte and four-byte header values */
#define SH(p) ((ush)(uch)((p)[0]) | ((ush)(uch)((p)[1]) << 8))
#define LG(p) ((ulg)(SH(p)) | ((ulg)(SH((p)+2)) << 16))

/* Diagnostic functions */
#ifdef DEBUG
#  define Assert(cond,msg) {if(!(cond)) error(msg);}
#  define Trace(x) fprintf x
#  define Tracev(x) {if (verbose) fprintf x ;}
#  define Tracevv(x) {if (verbose>1) fprintf x ;}
#  define Tracec(c,x) {if (verbose && (c)) fprintf x ;}
#  define Tracecv(c,x) {if (verbose>1 && (c)) fprintf x ;}
#else
#  define Assert(cond,msg)
#  define Trace(x)
#  define Tracev(x)
#  define Tracevv(x)
#  define Tracec(c,x)
#  define Tracecv(c,x)
#endif

#define WARN(msg) {if (!quiet) fprintf msg ; \
		   if (exit_code == OK) exit_code = WARNING;}

	/* in zip.c: */
extern int zip        OF((int in, int out));
extern int file_read  OF((char *buf,  unsigned size));

	/* in unzip.c */
extern int unzip      OF((int in, int out));
extern int check_zipfile OF((int in));

	/* in unpack.c */
extern int unpack     OF((int in, int out));

	/* in unlzh.c */
extern int unlzh      OF((int in, int out));

	/* in gzip.c */
RETSIGTYPE abort_gzip OF((void));

        /* in deflate.c */
void lm_init OF((int pack_level, ush *flags));
ulg  deflate OF((void));

        /* in trees.c */
void ct_init     OF((ush *attr, int *method));
int  ct_tally    OF((int dist, int lc));
ulg  flush_block OF((char *buf, ulg stored_len, int eof));

        /* in bits.c */
void     bi_init    OF((file_t zipfile));
void     send_bits  OF((int value, int length));
unsigned bi_reverse OF((unsigned value, int length));
void     bi_windup  OF((void));
void     copy_block OF((char *buf, unsigned len, int header));
extern   int (*read_buf) OF((char *buf, unsigned size));

	/* in util.c: */
extern int copy           OF((int in, int out));
extern ulg  updcrc        OF((uch *s, unsigned n));
extern void clear_bufs    OF((void));
extern int  fill_inbuf    OF((int eof_ok));
extern void flush_outbuf  OF((void));
extern void flush_window  OF((void));
extern void write_buf     OF((int fd, voidp buf, unsigned cnt));
extern char *strlwr       OF((char *s));
extern char *basename     OF((char *fname));
extern void make_simple_name OF((char *name));
extern char *add_envopt   OF((int *argcp, char ***argvp, char *env));
extern void error         OF((char *m));
extern void warn          OF((char *a, char *b));
extern void read_error    OF((void));
extern void write_error   OF((void));
extern void display_ratio OF((long num, long den, FILE *file));
extern voidp xmalloc      OF((unsigned int size));

	/* in inflate.c */
extern int inflate OF((void));
