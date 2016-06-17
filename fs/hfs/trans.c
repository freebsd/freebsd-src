/*
 * linux/fs/hfs/trans.c
 *
 * Copyright (C) 1995-1997  Paul H. Hargrove
 * This file may be distributed under the terms of the GNU General Public License.
 *
 * This file contains routines for converting between the Macintosh
 * character set and various other encodings.  This includes dealing
 * with ':' vs. '/' as the path-element separator.
 *
 * Latin-1 translation based on code contributed by Holger Schemel
 * (aeglos@valinor.owl.de).
 *
 * The '8-bit', '7-bit ASCII' and '7-bit alphanumeric' encodings are
 * implementations of the three encodings recommended by Apple in the
 * document "AppleSingle/AppleDouble Formats: Developer's Note
 * (9/94)".  This document is available from Apple's Technical
 * Information Library from the World Wide Web server
 * www.info.apple.com.
 *
 * The 'CAP' encoding is an implementation of the naming scheme used
 * by the Columbia AppleTalk Package, available for anonymous FTP from
 * ????.
 *
 * "XXX" in a comment is a note to myself to consider changing something.
 *
 * In function preconditions the term "valid" applied to a pointer to
 * a structure means that the pointer is non-NULL and the structure it
 * points to has all fields initialized to consistent values.
 */

#include "hfs.h"
#include <linux/hfs_fs_sb.h>
#include <linux/hfs_fs_i.h>
#include <linux/hfs_fs.h>

/*================ File-local variables ================*/

/* int->ASCII map for a single hex digit */
static char hex[16] = {'0','1','2','3','4','5','6','7',
		       '8','9','a','b','c','d','e','f'};
/*
 * Latin-1 to Mac character set map
 *
 * For the sake of consistency this map is generated from the Mac to
 * Latin-1 map the first time it is needed.  This means there is just
 * one map to maintain.
 */
static unsigned char latin2mac_map[128]; /* initially all zero */

/*
 * Mac to Latin-1 map for the upper 128 characters (both have ASCII in
 * the lower 128 positions)
 */
static unsigned char mac2latin_map[128] = {
	0xC4, 0xC5, 0xC7, 0xC9, 0xD1, 0xD6, 0xDC, 0xE1,
	0xE0, 0xE2, 0xE4, 0xE3, 0xE5, 0xE7, 0xE9, 0xE8,
	0xEA, 0xEB, 0xED, 0xEC, 0xEE, 0xEF, 0xF1, 0xF3,
	0xF2, 0xF4, 0xF6, 0xF5, 0xFA, 0xF9, 0xFB, 0xFC,
	0x00, 0xB0, 0xA2, 0xA3, 0xA7, 0xB7, 0xB6, 0xDF,
	0xAE, 0xA9, 0x00, 0xB4, 0xA8, 0x00, 0xC6, 0xD8,
	0x00, 0xB1, 0x00, 0x00, 0xA5, 0xB5, 0xF0, 0x00, 
	0x00, 0x00, 0x00, 0xAA, 0xBA, 0x00, 0xE6, 0xF8,
	0xBF, 0xA1, 0xAC, 0x00, 0x00, 0x00, 0x00, 0xAB,
	0xBB, 0x00, 0xA0, 0xC0, 0xC3, 0xD5, 0x00, 0x00, 
	0xAD, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF7, 0x00, 
	0xFF, 0x00, 0x00, 0xA4, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0xB8, 0x00, 0x00, 0xC2, 0xCA, 0xC1,
	0xCB, 0xC8, 0xCD, 0xCE, 0xCF, 0xCC, 0xD3, 0xD4,
	0x00, 0xD2, 0xDA, 0xDB, 0xD9, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

/*================ File-local functions ================*/

/*
 * dehex()
 *
 * Given a hexadecimal digit in ASCII, return the integer representation.
 */
static inline const unsigned char dehex(char c) {
	if ((c>='0')&&(c<='9')) {
		return c-'0';
	}
	if ((c>='a')&&(c<='f')) {
		return c-'a'+10;
	}
	if ((c>='A')&&(c<='F')) {
		return c-'A'+10;
	}
	return 0xff;
}

/*================ Global functions ================*/

/*
 * hfs_mac2nat()
 *
 * Given a 'Pascal String' (a string preceded by a length byte) in
 * the Macintosh character set produce the corresponding filename using
 * the Netatalk name-mangling scheme, returning the length of the
 * mangled filename.  Note that the output string is not NULL terminated.
 *
 * The name-mangling works as follows:
 * Characters 32-126 (' '-'~') except '/' and any initial '.' are passed
 * unchanged from input to output.  The remaining characters are replaced
 * by three characters: ':xx' where xx is the hexadecimal representation
 * of the character, using lowercase 'a' through 'f'.
 */
int hfs_mac2nat(char *out, const struct hfs_name *in) {
	unsigned char c;
	const unsigned char *p = in->Name;
	int len = in->Len;
	int count = 0;

	/* Special case for .AppleDesktop which in the
	   distant future may be a pseudodirectory. */
	if (strncmp(".AppleDesktop", p, len) == 0) {
		strncpy(out, p, 13);
		return 13;
	}

	while (len--) {
		c = *p++;
		if ((c<32) || (c=='/') || (c>126) || (!count && (c=='.'))) {
			*out++ = ':';
			*out++ = hex[(c>>4) & 0xf];
			*out++ = hex[c & 0xf];
			count += 3;
		} else {
			*out++ = c;
			count++;
		}
	}
	return count;
}

/*
 * hfs_mac2cap()
 *
 * Given a 'Pascal String' (a string preceded by a length byte) in
 * the Macintosh character set produce the corresponding filename using
 * the CAP name-mangling scheme, returning the length of the mangled
 * filename.  Note that the output string is not NULL terminated.
 *
 * The name-mangling works as follows:
 * Characters 32-126 (' '-'~') except '/' are passed unchanged from
 * input to output.  The remaining characters are replaced by three
 * characters: ':xx' where xx is the hexadecimal representation of the
 * character, using lowercase 'a' through 'f'.
 */
int hfs_mac2cap(char *out, const struct hfs_name *in) {
	unsigned char c;
	const unsigned char *p = in->Name;
	int len = in->Len;
	int count = 0;

	while (len--) {
		c = *p++;
		if ((c<32) || (c=='/') || (c>126)) {
			*out++ = ':';
			*out++ = hex[(c>>4) & 0xf];
			*out++ = hex[c & 0xf];
			count += 3;
		} else {
			*out++ = c;
			count++;
		}
	}
	return count;
}

/*
 * hfs_mac2eight()
 *
 * Given a 'Pascal String' (a string preceded by a length byte) in
 * the Macintosh character set produce the corresponding filename using
 * the '8-bit' name-mangling scheme, returning the length of the
 * mangled filename.  Note that the output string is not NULL
 * terminated.
 *
 * This is one of the three recommended naming conventions described
 * in Apple's document "AppleSingle/AppleDouble Formats: Developer's
 * Note (9/94)"
 *
 * The name-mangling works as follows:
 * Characters 0, '%' and '/' are replaced by three characters: '%xx'
 * where xx is the hexadecimal representation of the character, using
 * lowercase 'a' through 'f'.  All other characters are passed
 * unchanged from input to output.  Note that this format is mainly
 * implemented for completeness and is rather hard to read.
 */
int hfs_mac2eight(char *out, const struct hfs_name *in) {
	unsigned char c;
	const unsigned char *p = in->Name;
	int len = in->Len;
	int count = 0;

	while (len--) {
		c = *p++;
		if (!c || (c=='/') || (c=='%')) {
			*out++ = '%';
			*out++ = hex[(c>>4) & 0xf];
			*out++ = hex[c & 0xf];
			count += 3;
		} else {
			*out++ = c;
			count++;
		}
	}
	return count;
}

/*
 * hfs_mac2seven()
 *
 * Given a 'Pascal String' (a string preceded by a length byte) in
 * the Macintosh character set produce the corresponding filename using
 * the '7-bit ASCII' name-mangling scheme, returning the length of the
 * mangled filename.  Note that the output string is not NULL
 * terminated.
 *
 * This is one of the three recommended naming conventions described
 * in Apple's document "AppleSingle/AppleDouble Formats: Developer's
 * Note (9/94)"
 *
 * The name-mangling works as follows:
 * Characters 0, '%', '/' and 128-255 are replaced by three
 * characters: '%xx' where xx is the hexadecimal representation of the
 * character, using lowercase 'a' through 'f'.	All other characters
 * are passed unchanged from input to output.  Note that control
 * characters (including newline) and space are unchanged make reading
 * these filenames difficult.
 */
int hfs_mac2seven(char *out, const struct hfs_name *in) {
	unsigned char c;
	const unsigned char *p = in->Name;
	int len = in->Len;
	int count = 0;

	while (len--) {
		c = *p++;
		if (!c || (c=='/') || (c=='%') || (c&0x80)) {
			*out++ = '%';
			*out++ = hex[(c>>4) & 0xf];
			*out++ = hex[c & 0xf];
			count += 3;
		} else {
			*out++ = c;
			count++;
		}
	}
	return count;
}

/*
 * hfs_mac2alpha()
 *
 * Given a 'Pascal String' (a string preceded by a length byte) in
 * the Macintosh character set produce the corresponding filename using
 * the '7-bit alphanumeric' name-mangling scheme, returning the length
 * of the mangled filename.  Note that the output string is not NULL
 * terminated.
 *
 * This is one of the three recommended naming conventions described
 * in Apple's document "AppleSingle/AppleDouble Formats: Developer's
 * Note (9/94)"
 *
 * The name-mangling works as follows:
 * The characters 'a'-'z', 'A'-'Z', '0'-'9', '_' and the last '.' in
 * the filename are passed unchanged from input to output.  All
 * remaining characters (including any '.'s other than the last) are
 * replaced by three characters: '%xx' where xx is the hexadecimal
 * representation of the character, using lowercase 'a' through 'f'.
 */
int hfs_mac2alpha(char *out, const struct hfs_name *in) {
	unsigned char c;
	const unsigned char *p = in->Name;
	int len = in->Len;
	int count = 0;
	const unsigned char *lp;	/* last period */

	/* strrchr() would be good here, but 'in' is not null-terminated */
	for (lp=p+len-1; (lp>=p)&&(*lp!='.'); --lp) {}
	++lp;

	while (len--) {
		c = *p++;
		if ((p==lp) || ((c>='0')&&(c<='9')) || ((c>='A')&&(c<='Z')) ||
				((c>='a')&&(c<='z')) || (c=='_')) {
			*out++ = c;
			count++;
		} else {
			*out++ = '%';
			*out++ = hex[(c>>4) & 0xf];
			*out++ = hex[c & 0xf];
			count += 3;
		}
	}
	return count;
}

/*
 * hfs_mac2triv()
 *
 * Given a 'Pascal String' (a string preceded by a length byte) in
 * the Macintosh character set produce the corresponding filename using
 * the 'trivial' name-mangling scheme, returning the length of the
 * mangled filename.  Note that the output string is not NULL
 * terminated.
 *
 * The name-mangling works as follows:
 * The character '/', which is illegal in Linux filenames is replaced
 * by ':' which never appears in HFS filenames.	 All other characters
 * are passed unchanged from input to output.
 */
int hfs_mac2triv(char *out, const struct hfs_name *in) {
	unsigned char c;
	const unsigned char *p = in->Name;
	int len = in->Len;
	int count = 0;

	while (len--) {
		c = *p++;
		if (c=='/') {
			*out++ = ':';
		} else {
			*out++ = c;
		}
		count++;
	}
	return count;
}

/*
 * hfs_mac2latin()
 *
 * Given a 'Pascal String' (a string preceded by a length byte) in
 * the Macintosh character set produce the corresponding filename using
 * the 'Latin-1' name-mangling scheme, returning the length of the
 * mangled filename.  Note that the output string is not NULL
 * terminated.
 *
 * The Macintosh character set and Latin-1 are both extensions of the
 * ASCII character set.	 Some, but certainly not all, of the characters
 * in the Macintosh character set are also in Latin-1 but not with the
 * same encoding.  This name-mangling scheme replaces the characters in
 * the Macintosh character set that have Latin-1 equivalents by those
 * equivalents; the characters 32-126, excluding '/' and '%', are
 * passed unchanged from input to output.  The remaining characters
 * are replaced by three characters: '%xx' where xx is the hexadecimal
 * representation of the character, using lowercase 'a' through 'f'.
 *
 * The array mac2latin_map[] indicates the correspondence between the
 * two character sets.	The byte in element x-128 gives the Latin-1
 * encoding of the character with encoding x in the Macintosh
 * character set.  A value of zero indicates Latin-1 has no
 * corresponding character.
 */
int hfs_mac2latin(char *out, const struct hfs_name *in) {
	unsigned char c;
	const unsigned char *p = in->Name;
	int len = in->Len;
	int count = 0;

	while (len--) {
		c = *p++;

		if ((c & 0x80) && mac2latin_map[c & 0x7f]) {
			*out++ = mac2latin_map[c & 0x7f];
			count++;
		} else if ((c>=32) && (c<=126) && (c!='/') && (c!='%')) {
			*out++ =  c;
			count++;
		} else {
			*out++ = '%';
			*out++ = hex[(c>>4) & 0xf];
			*out++ = hex[c & 0xf];
			count += 3;
		}
	}
	return count;
}

/*
 * hfs_colon2mac()
 *
 * Given an ASCII string (not null-terminated) and its length,
 * generate the corresponding filename in the Macintosh character set
 * using the 'CAP' name-mangling scheme, returning the length of the
 * mangled filename.  Note that the output string is not NULL
 * terminated.
 *
 * This routine is a inverse to hfs_mac2cap() and hfs_mac2nat().
 * A ':' not followed by a 2-digit hexadecimal number (or followed
 * by the codes for NULL or ':') is replaced by a '|'.
 */
void hfs_colon2mac(struct hfs_name *out, const char *in, int len) {
	int hi, lo;
	unsigned char code, c, *count;
	unsigned char *p = out->Name;

	out->Len = 0;
	count = &out->Len;
	while (len-- && (*count < HFS_NAMELEN)) {
		c = *in++;
		(*count)++;
		if (c!=':') {
			*p++ = c;
		} else if ((len<2) ||
			   ((hi=dehex(in[0])) & 0xf0) ||
			   ((lo=dehex(in[1])) & 0xf0) ||
			   !(code = (hi << 4) | lo) ||
			   (code == ':')) {
			*p++ = '|';
		} else {
			*p++ = code;
			len -= 2;
			in += 2;
		}
	}
}

/*
 * hfs_prcnt2mac()
 *
 * Given an ASCII string (not null-terminated) and its length,
 * generate the corresponding filename in the Macintosh character set
 * using Apple's three recommended name-mangling schemes, returning
 * the length of the mangled filename.	Note that the output string is
 * not NULL terminated.
 *
 * This routine is a inverse to hfs_mac2alpha(), hfs_mac2seven() and
 * hfs_mac2eight().
 * A '%' not followed by a 2-digit hexadecimal number (or followed
 * by the code for NULL or ':') is unchanged.
 * A ':' is replaced by a '|'.
 */
void hfs_prcnt2mac(struct hfs_name *out, const char *in, int len) {
	int hi, lo;
	unsigned char code, c, *count;
	unsigned char *p = out->Name;

	out->Len = 0;
	count = &out->Len;
	while (len-- && (*count < HFS_NAMELEN)) {
		c = *in++;
		(*count)++;
		if (c==':') {
			*p++ = '|';
		} else if (c!='%') {
			*p++ = c;
		} else if ((len<2) ||
			   ((hi=dehex(in[0])) & 0xf0) ||
			   ((lo=dehex(in[1])) & 0xf0) ||
			   !(code = (hi << 4) | lo) ||
			   (code == ':')) {
			*p++ = '%';
		} else {
			*p++ = code;
			len -= 2;
			in += 2;
		}
	}
}

/*
 * hfs_triv2mac()
 *
 * Given an ASCII string (not null-terminated) and its length,
 * generate the corresponding filename in the Macintosh character set
 * using the 'trivial' name-mangling scheme, returning the length of
 * the mangled filename.  Note that the output string is not NULL
 * terminated.
 *
 * This routine is a inverse to hfs_mac2triv().
 * A ':' is replaced by a '/'.
 */
void hfs_triv2mac(struct hfs_name *out, const char *in, int len) {
	unsigned char c, *count;
	unsigned char *p = out->Name;

	out->Len = 0;
	count = &out->Len;
	while (len-- && (*count < HFS_NAMELEN)) {
		c = *in++;
		(*count)++;
		if (c==':') {
			*p++ = '/';
		} else {
			*p++ = c;
		}
	}
}

/*
 * hfs_latin2mac()
 *
 * Given an Latin-1 string (not null-terminated) and its length,
 * generate the corresponding filename in the Macintosh character set
 * using the 'Latin-1' name-mangling scheme, returning the length of
 * the mangled filename.  Note that the output string is not NULL
 * terminated.
 *
 * This routine is a inverse to hfs_latin2cap().
 * A '%' not followed by a 2-digit hexadecimal number (or followed
 * by the code for NULL or ':') is unchanged.
 * A ':' is replaced by a '|'.
 *
 * Note that the character map is built the first time it is needed.
 */
void hfs_latin2mac(struct hfs_name *out, const char *in, int len)
{
	int hi, lo;
	unsigned char code, c, *count;
	unsigned char *p = out->Name;
	static int map_initialized;

	if (!map_initialized) {
		int i;

		/* build the inverse mapping at run time */
		for (i = 0; i < 128; i++) {
			if ((c = mac2latin_map[i])) {
				latin2mac_map[(int)c - 128] = i + 128;
			}
		}
		map_initialized = 1;
	}

	out->Len = 0;
	count = &out->Len;
	while (len-- && (*count < HFS_NAMELEN)) {
		c = *in++;
		(*count)++;

		if (c==':') {
			*p++ = '|';
		} else if (c!='%') {
			if (c<128 || !(*p = latin2mac_map[c-128])) {
				*p = c;
			}
			p++;
		} else if ((len<2) ||
			   ((hi=dehex(in[0])) & 0xf0) ||
			   ((lo=dehex(in[1])) & 0xf0) ||
			   !(code = (hi << 4) | lo) ||
			   (code == ':')) {
			*p++ = '%';
		} else {
			*p++ = code;
			len -= 2;
			in += 2;
		}
	}
}
