/* ctype.h */

/* This file contains macros definitions and extern declarations for a
 * version of <ctype.h> which is aware of the o_flipcase letters used in
 * elvis.
 *
 * This file uses the "uchar" data type and "UCHAR" conversion macro which
 * are defined in "config.h".  Consequently, any file that includes this
 * header must include config.h first.
 */

#ifndef _CT_UPPER

#define _CT_UPPER	0x01
#define _CT_LOWER	0x02
#define _CT_SPACE	0x04
#define _CT_DIGIT	0x08
#define _CT_ALNUM	0x10
#define _CT_CNTRL	0x20

#define isalnum(c)	(_ct_ctypes[UCHAR(c)] & _CT_ALNUM)
#define isalpha(c)	(_ct_ctypes[UCHAR(c)] & (_CT_LOWER|_CT_UPPER))
#define isdigit(c)	(_ct_ctypes[UCHAR(c)] & _CT_DIGIT)
#define islower(c)	(_ct_ctypes[UCHAR(c)] & _CT_LOWER)
#define isspace(c)	(_ct_ctypes[UCHAR(c)] & _CT_SPACE)
#define isupper(c)	(_ct_ctypes[UCHAR(c)] & _CT_UPPER)
#define iscntrl(c)	(_ct_ctypes[UCHAR(c)] & _CT_CNTRL)
#define ispunct(c)	(!_ct_ctypes[UCHAR(c)]) /* punct = "none of the above" */

#define isascii(c)	(!((c) & 0x80))

#define toupper(c)	_ct_toupper[UCHAR(c)]
#define tolower(c)	_ct_tolower[UCHAR(c)]

extern uchar	_ct_toupper[];
extern uchar	_ct_tolower[];
extern uchar	_ct_ctypes[];
extern void	_ct_init(/* char *flipcase */);

#endif /* ndef _CT_UPPER */
