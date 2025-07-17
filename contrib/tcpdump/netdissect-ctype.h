/*
 * Copyright (c) 1988-1997
 *	The Regents of the University of California.  All rights reserved.
 *
 * Copyright (c) 1998-2012  Michael Richardson <mcr@tcpdump.org>
 *      The TCPDUMP project
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that: (1) source code distributions
 * retain the above copyright notice and this paragraph in its entirety, (2)
 * distributions including binary code include the above copyright notice and
 * this paragraph in its entirety in the documentation or other materials
 * provided with the distribution, and (3) all advertising materials mentioning
 * features or use of this software display the following acknowledgement:
 * ``This product includes software developed by the University of California,
 * Lawrence Berkeley Laboratory and its contributors.'' Neither the name of
 * the University nor the names of its contributors may be used to endorse
 * or promote products derived from this software without specific prior
 * written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifndef netdissect_ctype_h
#define netdissect_ctype_h

/*
 * Locale-independent macros for testing character properties and
 * stripping the 8th bit from characters.
 *
 * Byte values outside the ASCII range are considered unprintable, so
 * both ND_ASCII_ISPRINT() and ND_ASCII_ISGRAPH() return "false" for them.
 *
 * Assumed to be handed a value between 0 and 255, i.e. don't hand them
 * a char, as those might be in the range -128 to 127.
 */
#define ND_ISASCII(c)		(!((c) & 0x80))	/* value is an ASCII code point */
#define ND_ASCII_ISPRINT(c)	((c) >= 0x20 && (c) <= 0x7E)
#define ND_ASCII_ISGRAPH(c)	((c) > 0x20 && (c) <= 0x7E)
#define ND_ASCII_ISDIGIT(c)	((c) >= '0' && (c) <= '9')
#define ND_TOASCII(c)		((c) & 0x7F)

/*
 * Locale-independent macros for converting to upper or lower case.
 *
 * Byte values outside the ASCII range are not converted.  Byte values
 * *in* the ASCII range are converted to byte values in the ASCII range;
 * in particular, 'i' is upper-cased to 'I" and 'I' is lower-cased to 'i',
 * even in Turkish locales.
 */
#define ND_ASCII_TOLOWER(c)	(((c) >= 'A' && (c) <= 'Z') ? (c) - 'A' + 'a' : (c))
#define ND_ASCII_TOUPPER(c)	(((c) >= 'a' && (c) <= 'z') ? (c) - 'a' + 'A' : (c))

#endif /* netdissect-ctype.h */

