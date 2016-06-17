/*
 * util.c -  Miscellaneous support
 *
 * Copyright (C) 1997,1999 Martin von Löwis
 * Copyright (C) 1997 Régis Duchesne
 * Copyright (C) 2001 Anton Altaparmakov (AIA)
 *
 * The utf8 routines are copied from Python wstrop module.
 */

#include "ntfstypes.h"
#include "struct.h"
#include "util.h"
#include <linux/string.h>
#include <linux/errno.h>
#include <asm/div64.h>		/* For do_div(). */
#include "support.h"

/*
 * Converts a single wide character to a sequence of utf8 bytes.
 * The character is represented in host byte order.
 * Returns the number of bytes, or 0 on error.
 */
static int to_utf8(ntfs_u16 c, unsigned char *buf)
{
	if (c == 0)
		return 0; /* No support for embedded 0 runes. */
	if (c < 0x80) {
		if (buf)
			buf[0] = (unsigned char)c;
		return 1;
	}
	if (c < 0x800) {
		if (buf) {
			buf[0] = 0xc0 | (c >> 6);
			buf[1] = 0x80 | (c & 0x3f);
		}
		return 2;
	}
	/* c < 0x10000 */
	if (buf) {
		buf[0] = 0xe0 | (c >> 12);
		buf[1] = 0x80 | ((c >> 6) & 0x3f);
		buf[2] = 0x80 | (c & 0x3f);
	}
	return 3;
}

/*
 * Decodes a sequence of utf8 bytes into a single wide character.
 * The character is returned in host byte order.
 * Returns the number of bytes consumed, or 0 on error.
 */
static int from_utf8(const unsigned char *str, ntfs_u16 *c)
{
	int l = 0, i;

	if (*str < 0x80) {
		*c = *str;
		return 1;
	}
	if (*str < 0xc0)	/* Lead byte must not be 10xxxxxx. */
		return 0;	/* Is c0 a possible lead byte? */
	if (*str < 0xe0) {		/* 110xxxxx */
		*c = *str & 0x1f;
		l = 2;
	} else if (*str < 0xf0) {	/* 1110xxxx */
		*c = *str & 0xf;
		l = 3;
	} else if (*str < 0xf8) {   	/* 11110xxx */
		*c = *str & 7;
		l = 4;
	} else /* We don't support characters above 0xFFFF in NTFS. */
		return 0;
	for (i = 1; i < l; i++) {
		/* All other bytes must be 10xxxxxx. */
		if ((str[i] & 0xc0) != 0x80)
			return 0;
		*c <<= 6;
		*c |= str[i] & 0x3f;
	}
	return l;
}

/*
 * Converts wide string to UTF-8. Expects two in- and two out-parameters.
 * Returns 0 on success, or error code. 
 * The caller has to free the result string.
 */
static int ntfs_dupuni2utf8(ntfs_u16 *in, int in_len, char **out, int *out_len)
{
	int i, tmp;
	int len8;
	unsigned char *result;

	ntfs_debug(DEBUG_NAME1, "converting l = %d\n", in_len);
	/* Count the length of the resulting UTF-8. */
	for (i = len8 = 0; i < in_len; i++) {
		tmp = to_utf8(NTFS_GETU16(in + i), 0);
		if (!tmp)
			/* Invalid character. */
			return -EILSEQ;
		len8 += tmp;
	}
	*out = result = ntfs_malloc(len8 + 1); /* allow for zero-termination */
	if (!result)
		return -ENOMEM;
	result[len8] = '\0';
	*out_len = len8;
	for (i = len8 = 0; i < in_len; i++)
		len8 += to_utf8(NTFS_GETU16(in + i), result + len8);
	ntfs_debug(DEBUG_NAME1, "result %p:%s\n", result, result);
	return 0;
}

/*
 * Converts an UTF-8 sequence to a wide string. Same conventions as the
 * previous function.
 */
static int ntfs_duputf82uni(unsigned char* in, int in_len, ntfs_u16** out,
		int *out_len)
{
	int i, tmp;
	int len16;
	ntfs_u16* result;
	ntfs_u16 wtmp;

	for (i = len16 = 0; i < in_len; i += tmp, len16++) {
		tmp = from_utf8(in + i, &wtmp);
		if (!tmp)
			return -EILSEQ;
	}
	*out = result = ntfs_malloc(2 * (len16 + 1));
	if (!result)
		return -ENOMEM;
	result[len16] = 0;
	*out_len = len16;
	for (i = len16 = 0; i < in_len; i += tmp, len16++) {
		tmp = from_utf8(in + i, &wtmp);
		NTFS_PUTU16(result + len16, wtmp);
	}
	return 0;
}

/* Encodings dispatchers. */
int ntfs_encodeuni(ntfs_volume *vol, ntfs_u16 *in, int in_len, char **out,
		int *out_len)
{
	if (vol->nls_map)
		return ntfs_dupuni2map(vol, in, in_len, out, out_len);
	else
		return ntfs_dupuni2utf8(in, in_len, out, out_len);
}

int ntfs_decodeuni(ntfs_volume *vol, char *in, int in_len, ntfs_u16 **out,
		int *out_len)
{
	if (vol->nls_map)
		return ntfs_dupmap2uni(vol, in, in_len, out, out_len);
	else
		return ntfs_duputf82uni(in, in_len, out, out_len);
}

/* Same address space copies. */
void ntfs_put(ntfs_io *dest, void *src, ntfs_size_t n)
{
	ntfs_memcpy(dest->param, src, n);
	((char*)dest->param) += n;
}

void ntfs_get(void* dest, ntfs_io *src, ntfs_size_t n)
{
	ntfs_memcpy(dest, src->param, n);
	((char*)src->param) += n;
}

void *ntfs_calloc(int size)
{
	void *result = ntfs_malloc(size);
	if (result)
		ntfs_bzero(result, size);
	return result;
}

/* Copy len ascii characters from from to to. :) */
void ntfs_ascii2uni(short int *to, char *from, int len)
{
	int i;

	for (i = 0; i < len; i++)
		NTFS_PUTU16(to + i, from[i]);
	to[i] = 0;
}

/* strncmp for Unicode strings. */
int ntfs_uni_strncmp(short int* a, short int *b, int n)
{
	int i;

	for(i = 0; i < n; i++)
	{
		if (NTFS_GETU16(a + i) < NTFS_GETU16(b + i))
			return -1;
		if (NTFS_GETU16(b + i) < NTFS_GETU16(a + i))
			return 1;
		if (NTFS_GETU16(a + i) == 0)
			break;
	}
	return 0;
}

/* strncmp between Unicode and ASCII strings. */
int ntfs_ua_strncmp(short int* a, char* b, int n)
{
	int i;

	for (i = 0; i < n; i++)	{
		if(NTFS_GETU16(a + i) < b[i])
			return -1;
		if(b[i] < NTFS_GETU16(a + i))
			return 1;
		if (b[i] == 0)
			return 0;
	}
	return 0;
}

#define NTFS_TIME_OFFSET ((ntfs_time64_t)(369*365 + 89) * 24 * 3600 * 10000000)

/* Convert the NT UTC (based 1.1.1601, in hundred nanosecond units)
 * into Unix UTC (based 1.1.1970, in seconds). */
ntfs_time_t ntfs_ntutc2unixutc(ntfs_time64_t ntutc)
{
	/* Subtract the NTFS time offset, then convert to 1s intervals. */
	ntfs_time64_t t = ntutc - NTFS_TIME_OFFSET;
	do_div(t, 10000000);
	return (ntfs_time_t)t;
}

/* Convert the Unix UTC into NT UTC. */
ntfs_time64_t ntfs_unixutc2ntutc(ntfs_time_t t)
{
	/* Convert to 100ns intervals and then add the NTFS time offset. */
	return (ntfs_time64_t)t * 10000000 + NTFS_TIME_OFFSET;
}

#undef NTFS_TIME_OFFSET

/* Fill index name. */
void ntfs_indexname(char *buf, int type)
{
	char hex[] = "0123456789ABCDEF";
	int index;
	*buf++ = '$';
	*buf++ = 'I';
	for (index = 24; index > 0; index -= 4)
		if ((0xF << index) & type)
			break;
	while (index >= 0) {
		*buf++ = hex[(type >> index) & 0xF];
		index -= 4;
	}
	*buf = '\0';
}

