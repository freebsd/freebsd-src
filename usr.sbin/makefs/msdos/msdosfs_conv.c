/*-
 * SPDX-License-Identifier: BSD-4-Clause
 *
 * Copyright (C) 1994, 1995, 1997 Wolfgang Solfrank.
 * Copyright (C) 1994, 1995, 1997 TooLs GmbH.
 * All rights reserved.
 * Original code by Paul Popelka (paulp@uts.amdahl.com) (see below).
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by TooLs GmbH.
 * 4. The name of TooLs GmbH may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TOOLS GMBH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*-
 * Written by Paul Popelka (paulp@uts.amdahl.com)
 *
 * You can do anything you want with this software, just don't say you wrote
 * it, and don't remove this notice.
 *
 * This software is provided "as is".
 *
 * The author supplies this software to be publicly redistributed on the
 * understanding that the author is not responsible for the correct
 * functioning of this software in any circumstances and is not liable for
 * any damages caused by this software.
 *
 * October 1992
 */

#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/endian.h>

#include <dirent.h>
#include <stdio.h>
#include <string.h>

#include <fs/msdosfs/bpb.h>
#include "msdos/direntry.h"
#include <fs/msdosfs/msdosfsmount.h>

#include "makefs.h"
#include "msdos.h"

static int char8ucs2str(const uint8_t *in, int n, uint16_t *out, int m);
static void ucs2pad(uint16_t *buf, int len, int size);
static int char8match(uint16_t *w1, uint16_t *w2, int n);

static const u_char unix2dos[256] = {
	0,    0,    0,    0,    0,    0,    0,    0,	/* 00-07 */
	0,    0,    0,    0,    0,    0,    0,    0,	/* 08-0f */
	0,    0,    0,    0,    0,    0,    0,    0,	/* 10-17 */
	0,    0,    0,    0,    0,    0,    0,    0,	/* 18-1f */
	0,    '!',  0,    '#',  '$',  '%',  '&',  '\'',	/* 20-27 */
	'(',  ')',  0,    '+',  0,    '-',  0,    0,	/* 28-2f */
	'0',  '1',  '2',  '3',  '4',  '5',  '6',  '7',	/* 30-37 */
	'8',  '9',  0,    0,    0,    0,    0,    0,	/* 38-3f */
	'@',  'A',  'B',  'C',  'D',  'E',  'F',  'G',	/* 40-47 */
	'H',  'I',  'J',  'K',  'L',  'M',  'N',  'O',	/* 48-4f */
	'P',  'Q',  'R',  'S',  'T',  'U',  'V',  'W',	/* 50-57 */
	'X',  'Y',  'Z',  0,    0,    0,    '^',  '_',	/* 58-5f */
	'`',  'A',  'B',  'C',  'D',  'E',  'F',  'G',	/* 60-67 */
	'H',  'I',  'J',  'K',  'L',  'M',  'N',  'O',	/* 68-6f */
	'P',  'Q',  'R',  'S',  'T',  'U',  'V',  'W',	/* 70-77 */
	'X',  'Y',  'Z',  '{',  0,    '}',  '~',  0,	/* 78-7f */
	0,    0,    0,    0,    0,    0,    0,    0,	/* 80-87 */
	0,    0,    0,    0,    0,    0,    0,    0,	/* 88-8f */
	0,    0,    0,    0,    0,    0,    0,    0,	/* 90-97 */
	0,    0,    0,    0,    0,    0,    0,    0,	/* 98-9f */
	0,    0xad, 0xbd, 0x9c, 0xcf, 0xbe, 0xdd, 0xf5,	/* a0-a7 */
	0xf9, 0xb8, 0xa6, 0xae, 0xaa, 0xf0, 0xa9, 0xee,	/* a8-af */
	0xf8, 0xf1, 0xfd, 0xfc, 0xef, 0xe6, 0xf4, 0xfa,	/* b0-b7 */
	0xf7, 0xfb, 0xa7, 0xaf, 0xac, 0xab, 0xf3, 0xa8,	/* b8-bf */
	0xb7, 0xb5, 0xb6, 0xc7, 0x8e, 0x8f, 0x92, 0x80,	/* c0-c7 */
	0xd4, 0x90, 0xd2, 0xd3, 0xde, 0xd6, 0xd7, 0xd8,	/* c8-cf */
	0xd1, 0xa5, 0xe3, 0xe0, 0xe2, 0xe5, 0x99, 0x9e,	/* d0-d7 */
	0x9d, 0xeb, 0xe9, 0xea, 0x9a, 0xed, 0xe8, 0xe1,	/* d8-df */
	0xb7, 0xb5, 0xb6, 0xc7, 0x8e, 0x8f, 0x92, 0x80,	/* e0-e7 */
	0xd4, 0x90, 0xd2, 0xd3, 0xde, 0xd6, 0xd7, 0xd8,	/* e8-ef */
	0xd1, 0xa5, 0xe3, 0xe0, 0xe2, 0xe5, 0x99, 0xf6,	/* f0-f7 */
	0x9d, 0xeb, 0xe9, 0xea, 0x9a, 0xed, 0xe8, 0x98,	/* f8-ff */
};

static const u_char u2l[256] = {
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, /* 00-07 */
	0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, /* 08-0f */
	0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, /* 10-17 */
	0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, /* 18-1f */
	' ',  '!',  '"',  '#',  '$',  '%',  '&', '\'', /* 20-27 */
	'(',  ')',  '*',  '+',  ',',  '-',  '.',  '/', /* 28-2f */
	'0',  '1',  '2',  '3',  '4',  '5',  '6',  '7', /* 30-37 */
	'8',  '9',  ':',  ';',  '<',  '=',  '>',  '?', /* 38-3f */
	'@',  'a',  'b',  'c',  'd',  'e',  'f',  'g', /* 40-47 */
	'h',  'i',  'j',  'k',  'l',  'm',  'n',  'o', /* 48-4f */
	'p',  'q',  'r',  's',  't',  'u',  'v',  'w', /* 50-57 */
	'x',  'y',  'z',  '[', '\\',  ']',  '^',  '_', /* 58-5f */
	'`',  'a',  'b',  'c',  'd',  'e',  'f',  'g', /* 60-67 */
	'h',  'i',  'j',  'k',  'l',  'm',  'n',  'o', /* 68-6f */
	'p',  'q',  'r',  's',  't',  'u',  'v',  'w', /* 70-77 */
	'x',  'y',  'z',  '{',  '|',  '}',  '~', 0x7f, /* 78-7f */
	0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, /* 80-87 */
	0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f, /* 88-8f */
	0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, /* 90-97 */
	0x98, 0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f, /* 98-9f */
	0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, /* a0-a7 */
	0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf, /* a8-af */
	0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, /* b0-b7 */
	0xb8, 0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf, /* b8-bf */
	0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, /* c0-c7 */
	0xe8, 0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee, 0xef, /* c8-cf */
	0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xd7, /* d0-d7 */
	0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xdf, /* d8-df */
	0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, /* e0-e7 */
	0xe8, 0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee, 0xef, /* e8-ef */
	0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, /* f0-f7 */
	0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff, /* f8-ff */
};

/*
 * Determine the number of slots necessary for Win95 names
 */
int
winSlotCnt(const u_char *un, size_t unlen)
{
	const u_char *cp;

	/*
	 * Drop trailing blanks and dots
	 */
	for (cp = un + unlen; unlen > 0; unlen--)
		if (*--cp != ' ' && *cp != '.')
			break;

	return howmany(unlen, WIN_CHARS);
}

/*
 * Compare our filename to the one in the Win95 entry
 * Returns the checksum or -1 if no match
 */
int
winChkName(const u_char *un, size_t unlen, struct winentry *wep, int chksum)
{
	uint16_t wn[WIN_MAXLEN], *p;
	uint16_t buf[WIN_CHARS];
	int i, len;

	/*
	 * First compare checksums
	 */
	if (wep->weCnt & WIN_LAST)
		chksum = wep->weChksum;
	else if (chksum != wep->weChksum)
		chksum = -1;
	if (chksum == -1)
		return -1;

	/*
	 * Offset of this entry
	 */
	i = ((wep->weCnt & WIN_CNT) - 1) * WIN_CHARS;

	/*
	 * Translate UNIX name to ucs-2
	 */
	len = char8ucs2str(un, unlen, wn, WIN_MAXLEN);
	ucs2pad(wn, len, WIN_MAXLEN);

	if (i >= len + 1)
		return -1;
	if ((wep->weCnt & WIN_LAST) && (len - i > WIN_CHARS))
		return -1;

	/*
	 * Fetch name segment from directory entry
	 */
	p = &buf[0];
	memcpy(p, wep->wePart1, sizeof(wep->wePart1));
	p += sizeof(wep->wePart1) / sizeof(*p);
	memcpy(p, wep->wePart2, sizeof(wep->wePart2));
	p += sizeof(wep->wePart2) / sizeof(*p);
	memcpy(p, wep->wePart3, sizeof(wep->wePart3));

	/*
	 * And compare name segment
	 */
	if (!(char8match(&wn[i], buf, WIN_CHARS)))
		return -1;

	return chksum;
}

/*
 * Compute the checksum of a DOS filename for Win95 use
 */
uint8_t
winChksum(uint8_t *name)
{
	int i;
	uint8_t s;

	for (s = 0, i = 11; --i >= 0; s += *name++)
		s = (s << 7) | (s >> 1);
	return s;
}

/*
 * Create a Win95 long name directory entry
 * Note: assumes that the filename is valid,
 *	 i.e. doesn't consist solely of blanks and dots
 */
int
unix2winfn(const u_char *un, size_t unlen, struct winentry *wep, int cnt,
    int chksum)
{
	uint16_t wn[WIN_MAXLEN], *p;
	int i, len;
	const u_char *cp;

	/*
	 * Drop trailing blanks and dots
	 */
	for (cp = un + unlen; unlen > 0; unlen--)
		if (*--cp != ' ' && *cp != '.')
			break;

	/*
	 * Offset of this entry
	 */
	i = (cnt - 1) * WIN_CHARS;

	/*
	 * Translate UNIX name to ucs-2
	 */
	len = char8ucs2str(un, unlen, wn, WIN_MAXLEN);
	ucs2pad(wn, len, WIN_MAXLEN);

	/*
	 * Initialize winentry to some useful default
	 */
	memset(wep, 0xff, sizeof(*wep));
	wep->weCnt = cnt;
	wep->weAttributes = ATTR_WIN95;
	wep->weReserved1 = 0;
	wep->weChksum = chksum;
	wep->weReserved2 = 0;

	/*
	 * Store name segment into directory entry
	 */
	p = &wn[i];
	memcpy(wep->wePart1, p, sizeof(wep->wePart1));
	p += sizeof(wep->wePart1) / sizeof(*p);
	memcpy(wep->wePart2, p, sizeof(wep->wePart2));
	p += sizeof(wep->wePart2) / sizeof(*p);
	memcpy(wep->wePart3, p, sizeof(wep->wePart3));

	if (len > i + WIN_CHARS)
		return 1;

	wep->weCnt |= WIN_LAST;
	return 0;
}

/*
 * Convert a unix filename to a DOS filename according to Win95 rules.
 * If applicable and gen is not 0, it is inserted into the converted
 * filename as a generation number.
 * Returns
 *	0 if name couldn't be converted
 *	1 if the converted name is the same as the original
 *	  (no long filename entry necessary for Win95)
 *	2 if conversion was successful
 *	3 if conversion was successful and generation number was inserted
 */
int
unix2dosfn(const u_char *un, u_char dn[12], size_t unlen, u_int gen)
{
	int i, j, l;
	int conv = 1;
	const u_char *cp, *dp, *dp1;
	u_char gentext[6], *wcp;
	int shortlen;

	/*
	 * Fill the dos filename string with blanks. These are DOS's pad
	 * characters.
	 */
	for (i = 0; i < 11; i++)
		dn[i] = ' ';
	dn[11] = 0;

	/*
	 * The filenames "." and ".." are handled specially, since they
	 * don't follow dos filename rules.
	 */
	if (un[0] == '.' && unlen == 1) {
		dn[0] = '.';
		return gen <= 1;
	}
	if (un[0] == '.' && un[1] == '.' && unlen == 2) {
		dn[0] = '.';
		dn[1] = '.';
		return gen <= 1;
	}

	/*
	 * Filenames with only blanks and dots are not allowed!
	 */
	for (cp = un, i = unlen; --i >= 0; cp++)
		if (*cp != ' ' && *cp != '.')
			break;
	if (i < 0)
		return 0;

	/*
	 * Now find the extension
	 * Note: dot as first char doesn't start extension
	 *	 and trailing dots and blanks are ignored
	 */
	dp = dp1 = 0;
	for (cp = un + 1, i = unlen - 1; --i >= 0;) {
		switch (*cp++) {
		case '.':
			if (!dp1)
				dp1 = cp;
			break;
		case ' ':
			break;
		default:
			if (dp1)
				dp = dp1;
			dp1 = 0;
			break;
		}
	}

	/*
	 * Now convert it
	 */
	if (dp) {
		if (dp1)
			l = dp1 - dp;
		else
			l = unlen - (dp - un);
		for (i = 0, j = 8; i < l && j < 11; i++, j++) {
			if (dp[i] != (dn[j] = unix2dos[dp[i]])
			    && conv != 3)
				conv = 2;
			if (!dn[j]) {
				conv = 3;
				dn[j--] = ' ';
			}
		}
		if (i < l)
			conv = 3;
		dp--;
	} else {
		for (dp = cp; *--dp == ' ' || *dp == '.';);
		dp++;
	}

	shortlen = (dp - un) <= 8;

	/*
	 * Now convert the rest of the name
	 */
	for (i = j = 0; un < dp && j < 8; i++, j++, un++) {
		if ((*un == ' ') && shortlen)
			dn[j] = ' ';
		else
			dn[j] = unix2dos[*un];
		if ((*un != dn[j])
		    && conv != 3)
			conv = 2;
		if (!dn[j]) {
			conv = 3;
			dn[j--] = ' ';
		}
	}
	if (un < dp)
		conv = 3;
	/*
	 * If we didn't have any chars in filename,
	 * generate a default
	 */
	if (!j)
		dn[0] = '_';

	/*
	 * The first character cannot be E5,
	 * because that means a deleted entry
	 */
	if (dn[0] == 0xe5)
		dn[0] = SLOT_E5;

	/*
	 * If there wasn't any char dropped,
	 * there is no place for generation numbers
	 */
	if (conv != 3) {
		if (gen > 1)
			return 0;
		return conv;
	}

	/*
	 * Now insert the generation number into the filename part
	 */
	for (wcp = gentext + sizeof(gentext); wcp > gentext && gen; gen /= 10)
		*--wcp = gen % 10 + '0';
	if (gen)
		return 0;
	for (i = 8; dn[--i] == ' ';);
	i++;
	if (gentext + sizeof(gentext) - wcp + 1 > 8 - i)
		i = 8 - (gentext + sizeof(gentext) - wcp + 1);
	dn[i++] = '~';
	while (wcp < gentext + sizeof(gentext))
		dn[i++] = *wcp++;
	return 3;
}

/*
 * Convert 8bit character string into UCS-2 string
 * return total number of output chacters
 */
static int
char8ucs2str(const uint8_t *in, int n, uint16_t *out, int m)
{
	uint16_t *p;

	p = out;
	while (n > 0 && in[0] != 0) {
		if (m < 1)
			break;
		if (p)
			p[0] = htole16(in[0]);
		p += 1;
		m -= 1;
		in += 1;
		n -= 1;
	}

	return p - out;
}

static void
ucs2pad(uint16_t *buf, int len, int size)
{

	if (len < size-1)
		buf[len++] = 0x0000;
	while (len < size)
		buf[len++] = 0xffff;
}

/*
 * Compare two 8bit char conversions case-insensitive
 *
 * uses the DOS case folding table
 */
static int
char8match(uint16_t *w1, uint16_t *w2, int n)
{
	uint16_t u1, u2;

	while (n > 0) {
		u1 = le16toh(*w1);
		u2 = le16toh(*w2);
		if (u1 == 0 || u2 == 0)
			return u1 == u2;
		if (u1 > 255 || u2 > 255)
			return 0;
		u1 = u2l[u1 & 0xff];
		u2 = u2l[u2 & 0xff];
		if (u1 != u2)
			return 0;
		++w1;
		++w2;
		--n;
	}

	return 1;
}
