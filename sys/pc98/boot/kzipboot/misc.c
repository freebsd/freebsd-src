/*
 * misc.c
 *
 * This is a collection of several routines from gzip-1.0.3
 * adapted for Linux.
 *
 * Ported to 386bsd by Serge Vakulenko
 */

#include "gzip.h"

unsigned outcnt;
unsigned insize;
unsigned inptr;

extern const char input_data[];
extern const int input_len;

int input_ptr;

int method;

char *output_data;
ulong output_ptr;

void makecrc (void);
void putstr (char *c);
void *memcpy (void *to, const void *from, unsigned len);
int memcmp (const void *arg1, const void *arg2, unsigned len);

ulong crc;                              /* shift register contents */
ulong crc_32_tab[256];                  /* crc table, defined below */

/*
 * Run a set of bytes through the crc shift register.  If s is a NULL
 * pointer, then initialize the crc shift register contents instead.
 * Return the current crc in either case.
 */
void updcrc(s, n)
uchar *s;                       /* pointer to bytes to pump through */
unsigned n;                     /* number of bytes in s[] */
{
	while (n--)
		crc = crc_32_tab[(uchar)crc ^ (*s++)] ^ (crc >> 8);
}

/*
 * Clear input and output buffers
 */
void clear_bufs()
{
	outcnt = 0;
	insize = inptr = 0;
}

/*
 * Fill the input buffer. This is called only when the buffer is empty
 * and at least one byte is really needed.
 */
void fill_inbuf ()
{
	int len, i;

	/* Read as much as possible */
	insize = 0;
	do {
		len = INBUFSIZ - insize;
		if (len > input_len - input_ptr + 1)
			len = input_len-input_ptr+1;
		if (len <= 0)
			break;
		for (i=0; i<len; i++)
			inbuf[insize+i] = input_data[input_ptr+i];
		insize += len;
		input_ptr += len;
	} while (insize < INBUFSIZ);
	if (insize == 0)
		error("unable to fill buffer");
	inptr = 0;
}

/*
 * Write the output window window[0..outcnt-1] and update crc and bytes_out.
 * (Used for the decompressed data only.)
 */
void flush_window()
{
	if (outcnt == 0) return;
	updcrc(window, outcnt);

	memcpy(&output_data[output_ptr], (char *)window, outcnt);

	output_ptr += outcnt;
	outcnt = 0;
}

/*
 * Code to compute the CRC-32 table. Borrowed from
 * gzip-1.0.3/makecrc.c.
 * Not copyrighted 1990 Mark Adler
 */
void makecrc(void)
{
	ulong c;        /* crc shift register */
	ulong e;        /* polynomial exclusive-or pattern */
	int i;          /* counter for all possible eight bit values */
	int k;          /* byte being shifted into crc apparatus */

	/* terms of polynomial defining this crc (except x^32): */
	static const uchar poly[] = { 0,1,2,4,5,7,8,10,11,12,16,22,23,26, };

	/* Make exclusive-or pattern from polynomial */
	e = 0;
	for (i = 0; i < sizeof(poly)/sizeof(*poly); i++)
		e |= 1L << (31 - poly[i]);

	crc_32_tab[0] = 0;

	for (i = 1; i < 256; i++) {
		c = 0;
		for (k = i | 256; k != 1; k >>= 1) {
			c = c & 1 ? (c >> 1) ^ e : c >> 1;
			if (k & 1)
				c ^= e;
		}
		crc_32_tab[i] = c;
	}
}

/*
 * Check the magic number of the input file and update ofname if an
 * original name was given and to_stdout is not set.
 * Set inptr to the offset of the next byte to be processed.
 */
static void get_method()
{
	uchar flags;
	char magic[2];  /* magic header */

	magic[0] = get_byte();
	magic[1] = get_byte();

	method = -1;            /* unknown yet */
	extended = pkzip = 0;
	/* assume multiple members in gzip file except for record oriented I/O */

	if (memcmp(magic, GZIP_MAGIC, 2) == 0
	    || memcmp(magic, OLD_GZIP_MAGIC, 2) == 0) {
		method = get_byte();
		flags  = get_byte();
		if (flags & ENCRYPTED)
			error("Input is encrypted");
		if (flags & CONTINUATION)
			error("Multi part input");
		if (flags & RESERVED)
			error("Input has invalid flags");

		(void) get_byte();      /* Get timestamp */
		(void) get_byte();
		(void) get_byte();
		(void) get_byte();

		(void) get_byte();      /* Ignore extra flags for the moment */
		(void) get_byte();      /* Ignore OS type for the moment */

		if (flags & EXTRA_FIELD) {
			unsigned len = get_byte();
			len |= get_byte() << 8;
			while (len--)
				(void) get_byte();
		}

		/* Discard file comment if any */
		if (flags & COMMENT)
			while (get_byte())
				continue;

	} else if (memcmp(magic, PKZIP_MAGIC, 2) == 0 && inptr == 2
	    && memcmp(inbuf, PKZIP_MAGIC, 4) == 0) {
		/*
		 * To simplify the code, we support a zip file when alone only.
		 * We are thus guaranteed that the entire local header fits in inbuf.
		 */
		inptr = 0;
		check_zipfile();

	} else if (memcmp(magic, PACK_MAGIC, 2) == 0)
		error("packed input");
	else if (memcmp(magic, LZW_MAGIC, 2) == 0)
		error("compressed input");
	if (method == -1)
		error("Corrupted input");
}

void
decompress_kernel (void *dest)
{
	output_data = dest;
	output_ptr = 0;

	input_ptr = 0;

	clear_bufs ();
	makecrc ();
	get_method ();
	unzip ();
}
