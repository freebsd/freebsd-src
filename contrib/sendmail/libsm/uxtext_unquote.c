/*
 * Copyright (c) 2020 Proofpoint, Inc. and its suppliers.
 *	All rights reserved.
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the sendmail distribution.
 *
 */

#include <sm/gen.h>
#include <sm/sendmail.h>

/*
**  based on
**  https://github.com/aox/encodings/utf.cpp
**  see license.txt included below.
*/

#if USE_EAI
#include <ctype.h>
#define SM_ISDIGIT(c)	(isascii(c) && isdigit(c))

#include <sm/assert.h>

/* for prototype */
#include <sm/ixlen.h>

# if 0
/*
**   RFC 6533:
**
**   In the ABNF below, all productions not defined in this document are
**   defined in Appendix B of [RFC5234], in Section 4 of [RFC3629], or in
**   [RFC3464].
**
**   utf-8-type-addr     = "utf-8;" utf-8-enc-addr
**   utf-8-address       = Mailbox ; Mailbox as defined in [RFC6531].
**   utf-8-enc-addr      = utf-8-addr-xtext /
**                         utf-8-addr-unitext /
**                         utf-8-address
**   utf-8-addr-xtext    = 1*(QCHAR / EmbeddedUnicodeChar)
**                         ; 7bit form of utf-8-addr-unitext.
**                         ; Safe for use in the ORCPT [RFC3461]
**                         ; parameter even when SMTPUTF8 SMTP
**                         ; extension is not advertised.
**   utf-8-addr-unitext  = 1*(QUCHAR / EmbeddedUnicodeChar)
**                       ; MUST follow utf-8-address ABNF when
**                       ; dequoted.
**                       ; Safe for using in the ORCPT [RFC3461]
**                       ; parameter when SMTPUTF8 SMTP extension
**                       ; is also advertised.
**   QCHAR              = %x21-2a / %x2c-3c / %x3e-5b / %x5d-7e
**                       ; ASCII printable characters except
**                       ; CTLs, SP, '\', '+', '='.
**   QUCHAR              = QCHAR / UTF8-2 / UTF8-3 / UTF8-4
**                       ; ASCII printable characters except
**                       ; CTLs, SP, '\', '+' and '=', plus
**                       ; other Unicode characters encoded in UTF-8
**   EmbeddedUnicodeChar =   %x5C.78 "{" HEXPOINT "}"
**                       ; starts with "\x"
**   HEXPOINT = ( ( "0"/"1" ) %x31-39 ) / "10" / "20" /
**              "2B" / "3D" / "7F" /         ; all xtext-specials
**              "5C" / (HEXDIG8 HEXDIG) /    ; 2-digit forms
**              ( NZHEXDIG 2(HEXDIG) ) /     ; 3-digit forms
**              ( NZDHEXDIG 3(HEXDIG) ) /    ; 4-digit forms excluding
**              ( "D" %x30-37 2(HEXDIG) ) /  ; ... surrogate
**              ( NZHEXDIG 4(HEXDIG) ) /     ; 5-digit forms
**              ( "10" 4*HEXDIG )            ; 6-digit forms
**              ; represents either "\" or a Unicode code point outside
**              ; the ASCII repertoire
**   HEXDIG8             = %x38-39 / "A" / "B" / "C" / "D" / "E" / "F"
**                       ; HEXDIG excluding 0-7
**   NZHEXDIG            = %x31-39 / "A" / "B" / "C" / "D" / "E" / "F"
**                       ; HEXDIG excluding "0"
**   NZDHEXDIG           = %x31-39 / "A" / "B" / "C" / "E" / "F"
**                       ; HEXDIG excluding "0" and "D"
*/
# endif /* 0 */

/*
**  UXTEXT_UNQUOTE -- "unquote" a utf-8-addr-unitext
**
**	Parameters:
**		quoted -- original string [x]
**		unquoted -- "decoded" string [x] (buffer provided by caller)
**			if NULL this is basically a syntax check.
**		olen -- length of unquoted (must be > 0)
**
**	Returns:
**		>0: length of "decoded" string
**		<0: error
*/

int
uxtext_unquote(quoted, unquoted, olen)
	const char *quoted;
	char *unquoted;
	int olen;
{
	const unsigned char *cp;
	int ch, len;

#define APPCH(ch) do	\
	{		\
		if (len >= olen)	\
			return 0 - olen;	\
		if (NULL !=  unquoted)	\
			unquoted[len] = (char) (ch);	\
		len++;	\
	} while (0)

	SM_REQUIRE(olen > 0);
	SM_REQUIRE(NULL != quoted);
	len = 0;
	for (cp = (const unsigned char *) quoted; (ch = *cp) != 0; cp++)
	{
		if (ch == '\\' && cp[1] == 'x' && cp[2] == '{')
		{
			int	 uc = 0;

			cp += 2;
			while ((ch = *++cp) != '}')
			{
				if (SM_ISDIGIT(ch))
					uc = (uc << 4) + (ch - '0');
				else if (ch >= 'a' && ch <= 'f')
					uc = (uc << 4) + (ch - 'a' + 10);
				else if (ch >= 'A' && ch <= 'F')
					uc = (uc << 4) + (ch - 'A' + 10);
				else
					return 0 - len;
				if (uc > 0x10ffff)
					return 0 - len;
			}

			if (uc < 0x80)
				APPCH(uc);
			else if (uc < 0x800)
			{
				APPCH(0xc0 | ((char) (uc >> 6)));
				APPCH(0x80 | ((char) (uc & 0x3f)));
			}
			else if (uc < 0x10000)
			{
				APPCH(0xe0 | ((char) (uc >> 12)));
				APPCH(0x80 | ((char) (uc >> 6) & 0x3f));
				APPCH(0x80 | ((char) (uc & 0x3f)));
			}
			else if (uc < 0x200000)
			{
				APPCH(0xf0 | ((char) (uc >> 18)));
				APPCH(0x80 | ((char) (uc >> 12) & 0x3f));
				APPCH(0x80 | ((char) (uc >> 6) & 0x3f));
				APPCH(0x80 | ((char) (uc & 0x3f)));
			}
			else if (uc < 0x4000000)
			{
				APPCH(0xf8 | ((char) (uc >> 24)));
				APPCH(0x80 | ((char) (uc >> 18) & 0x3f));
				APPCH(0x80 | ((char) (uc >> 12) & 0x3f));
				APPCH(0x80 | ((char) (uc >> 6) & 0x3f));
				APPCH(0x80 | ((char) (uc & 0x3f)));
			}
			else
			{
				APPCH(0xfc | ((char) (uc >> 30)));
				APPCH(0x80 | ((char) (uc >> 24) & 0x3f));
				APPCH(0x80 | ((char) (uc >> 18) & 0x3f));
				APPCH(0x80 | ((char) (uc >> 12) & 0x3f));
				APPCH(0x80 | ((char) (uc >> 6) & 0x3f));
				APPCH(0x80 | ((char) (uc & 0x3f)));
			}
		}
		else
			APPCH(ch);
	}
	APPCH('\0');
	return len;
}

# if 0
aox/doc/readme/license.txt

Copyright (c) 2003-2014, Archiveopteryx and its contributors.

Permission to use, copy, modify, and distribute this software and its
documentation for any purpose, without fee, and without a written
agreement is hereby granted, provided that the above copyright notice
and this paragraph and the following two paragraphs appear in all
copies.

IN NO EVENT SHALL ORYX BE LIABLE TO ANY PARTY FOR DIRECT, INDIRECT,
SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES, INCLUDING LOST PROFITS,
ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION, EVEN IF
ORYX HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

ORYX SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING, BUT NOT LIMITED
TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
PARTICULAR PURPOSE. THE SOFTWARE PROVIDED HEREUNDER IS ON AN "AS IS"
BASIS, AND ORYX HAS NO OBLIGATIONS TO PROVIDE MAINTENANCE, SUPPORT,
UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
# endif /* 0 */
#endif /* USE_EAI */
