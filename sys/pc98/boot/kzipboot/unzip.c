/*
 * unzip.c -- decompress files in gzip or pkzip format.
 * Copyright (C) 1992-1993 Jean-loup Gailly
 *
 * Adapted for Linux booting by Hannu Savolainen 1993
 * Adapted for FreeBSD booting by Serge Vakulenko
 *
 * This is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License, see the file COPYING.
 *
 * The code in this file is derived from the file funzip.c written
 * and put in the public domain by Mark Adler.
 */

/*
 * This version can extract files in gzip or pkzip format.
 * For the latter, only the first entry is extracted, and it has to be
 * either deflated or stored.
 */

#include "gzip.h"

#include <sys/types.h>
#include <sys/inflate.h>

/* PKZIP header definitions */
#define LOCSIG  0x04034b50L     /* four-byte lead-in (lsb first) */
#define LOCFLG  6               /* offset of bit flag */
#define  CRPFLG 1               /*  bit for encrypted entry */
#define  EXTFLG 8               /*  bit for extended local header */
#define LOCHOW  8               /* offset of compression method */
#define LOCTIM  10              /* file mod time (for decryption) */
#define LOCCRC  14              /* offset of crc */
#define LOCSIZ  18              /* offset of compressed size */
#define LOCLEN  22              /* offset of uncompressed length */
#define LOCFIL  26              /* offset of file name field length */
#define LOCEXT  28              /* offset of extra field length */
#define LOCHDR  30              /* size of local header, including sig */
#define EXTHDR  16              /* size of extended local header, inc sig */

int pkzip;                      /* set for a pkzip file */
int extended;                   /* set if extended local header */

/* Macros for getting two-byte and four-byte header values */
#define SH(p) ((ushort)(uchar)((p)[0]) | ((ushort)(uchar)((p)[1]) << 8))
#define LG(p) ((ulong)(SH(p)) | ((ulong)(SH((p)+2)) << 16))

/*
 * Check zip file and advance inptr to the start of the compressed data.
 * Get ofname from the local header if necessary.
 */
void check_zipfile()
{
	uchar *h = inbuf + inptr;       /* first local header */

	/* Check validity of local header, and skip name and extra fields */
	inptr += LOCHDR + SH(h + LOCFIL) + SH(h + LOCEXT);

	if (inptr > insize || LG(h) != LOCSIG)
		error("input not a zip");

	method = h[LOCHOW];
	if (method != STORED && method != DEFLATED)
		error("first entry not deflated or stored--can't extract");

	/* If entry encrypted, decrypt and validate encryption header */
	if (h[LOCFLG] & CRPFLG)
		error("encrypted file");

	/* Save flags for unzip() */
	extended = (h[LOCFLG] & EXTFLG) != 0;
	pkzip = 1;
}

int
Flush (void *nu, u_char *buf, u_long cnt)
{
	outcnt = cnt;
	flush_window();
	return 0;
}

int
NextByte (void *nu)
{
	return ((int) get_byte ());
}

struct inflate infl; /* put it into the BSS */

/*
 * Unzip in to out.  This routine works on both gzip and pkzip files.
 *
 * IN assertions: the buffer inbuf contains already the beginning of
 * the compressed data, from offsets inptr to insize-1 included.
 * The magic header has already been checked. The output buffer is cleared.
 */

void unzip()
{
	ulong orig_crc = 0;     /* original crc */
	ulong orig_len = 0;     /* original uncompressed length */
	uchar buf[EXTHDR];      /* extended local header */
	int n, res;

	crc = 0xffffffffL;      /* initialize crc */

	if (pkzip && !extended) { /* crc and length at the end otherwise */
		orig_crc = LG(inbuf + LOCCRC);
		orig_len = LG(inbuf + LOCLEN);
	}

	if (method != DEFLATED)
		error("internal error, invalid method");
	infl.gz_input = NextByte;
	infl.gz_output = Flush;
	infl.gz_slide = window;
	res = inflate (&infl);
	if (res == 3)
		error("out of memory");
	else if (res != 0)
		error("invalid compressed format");

	/* Get the crc and original length */
	if (!pkzip) {
		/* crc32 (see algorithm.doc)
		 * uncompressed input size modulo 2^32
		 */
		for (n = 0; n < 8; n++)
			buf[n] = get_byte();    /* may cause an error if EOF */
		orig_crc = LG(buf);
		orig_len = LG(buf+4);

	} else if (extended) {          /* If extended header, check it */
		/* signature - 4bytes: 0x50 0x4b 0x07 0x08
		 * CRC-32 value
		 * compressed size 4-bytes
		 * uncompressed size 4-bytes
		 */
		for (n = 0; n < EXTHDR; n++)
			buf[n] = get_byte();    /* may cause an error if EOF */
		orig_crc = LG(buf+4);
		orig_len = LG(buf+12);
	}

	/* Validate decompression */
	if (orig_crc != (crc ^ 0xffffffffL))
		error("crc error");
	if (orig_len != output_ptr)
		error("length error");

	/* Check if there are more entries in a pkzip file */
	if (pkzip && inptr+4 < insize && LG(inbuf+inptr) == LOCSIG)
		error("zip file has more than one entry");
}
