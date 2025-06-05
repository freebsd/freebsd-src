/*
 * Copyright 1998-2008 The OpenLDAP Foundation.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted only as authorized by the OpenLDAP
 * Public License.
 *
 * A copy of this license is available in file LICENSE in the
 * top-level directory of the distribution or, alternatively, at
 * <https://www.OpenLDAP.org/license.html>.
 */
/* Copyright 2001 Computing Research Labs, New Mexico State University
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE COMPUTING RESEARCH LAB OR NEW MEXICO STATE UNIVERSITY BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT
 * OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR
 * THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/*
 * This work is part of OpenLDAP Software <https://www.openldap.org/>.
 * $OpenLDAP: pkg/ldap/libraries/liblunicode/ucdata/ucdata.h,v 1.21 2008/01/07 23:20:05 kurt Exp $
 * $Id: ucdata.h,v 1.6 2001/01/02 18:46:20 mleisher Exp $
 */

#ifndef _h_ucdata
#define _h_ucdata

#define UCDATA_VERSION "2.4"

/**************************************************************************
 *
 * Masks and macros for character properties.
 *
 **************************************************************************/

/*
 * Values that can appear in the `mask1' parameter of the ucisprop()
 * function.
 */
#define UC_MN 0x00000001 /* Mark, Non-Spacing          */
#define UC_MC 0x00000002 /* Mark, Spacing Combining    */
#define UC_ME 0x00000004 /* Mark, Enclosing            */
#define UC_ND 0x00000008 /* Number, Decimal Digit      */
#define UC_NL 0x00000010 /* Number, Letter             */
#define UC_NO 0x00000020 /* Number, Other              */
#define UC_ZS 0x00000040 /* Separator, Space           */
#define UC_ZL 0x00000080 /* Separator, Line            */
#define UC_ZP 0x00000100 /* Separator, Paragraph       */
#define UC_CC 0x00000200 /* Other, Control             */
#define UC_CF 0x00000400 /* Other, Format              */
#define UC_OS 0x00000800 /* Other, Surrogate           */
#define UC_CO 0x00001000 /* Other, Private Use         */
#define UC_CN 0x00002000 /* Other, Not Assigned        */
#define UC_LU 0x00004000 /* Letter, Uppercase          */
#define UC_LL 0x00008000 /* Letter, Lowercase          */
#define UC_LT 0x00010000 /* Letter, Titlecase          */
#define UC_LM 0x00020000 /* Letter, Modifier           */
#define UC_LO 0x00040000 /* Letter, Other              */
#define UC_PC 0x00080000 /* Punctuation, Connector     */
#define UC_PD 0x00100000 /* Punctuation, Dash          */
#define UC_PS 0x00200000 /* Punctuation, Open          */
#define UC_PE 0x00400000 /* Punctuation, Close         */
#define UC_PO 0x00800000 /* Punctuation, Other         */
#define UC_SM 0x01000000 /* Symbol, Math               */
#define UC_SC 0x02000000 /* Symbol, Currency           */
#define UC_SK 0x04000000 /* Symbol, Modifier           */
#define UC_SO 0x08000000 /* Symbol, Other              */
#define UC_L  0x10000000 /* Left-To-Right              */
#define UC_R  0x20000000 /* Right-To-Left              */
#define UC_EN 0x40000000 /* European Number            */
#define UC_ES 0x80000000 /* European Number Separator  */

/*
 * Values that can appear in the `mask2' parameter of the ucisprop()
 * function.
 */
#define UC_ET 0x00000001 /* European Number Terminator */
#define UC_AN 0x00000002 /* Arabic Number              */
#define UC_CS 0x00000004 /* Common Number Separator    */
#define UC_B  0x00000008 /* Block Separator            */
#define UC_S  0x00000010 /* Segment Separator          */
#define UC_WS 0x00000020 /* Whitespace                 */
#define UC_ON 0x00000040 /* Other Neutrals             */
/*
 * Implementation specific character properties.
 */
#define UC_CM 0x00000080 /* Composite                  */
#define UC_NB 0x00000100 /* Non-Breaking               */
#define UC_SY 0x00000200 /* Symmetric                  */
#define UC_HD 0x00000400 /* Hex Digit                  */
#define UC_QM 0x00000800 /* Quote Mark                 */
#define UC_MR 0x00001000 /* Mirroring                  */
#define UC_SS 0x00002000 /* Space, other               */

#define UC_CP 0x00004000 /* Defined                    */

/*
 * Added for UnicodeData-2.1.3.
 */
#define UC_PI 0x00008000 /* Punctuation, Initial       */
#define UC_PF 0x00010000 /* Punctuation, Final         */

/*
 * This is the primary function for testing to see if a character has some set
 * of properties.  The macros that test for various character properties all
 * call this function with some set of masks.
 */
int
ucisprop (krb5_ui_4 code, krb5_ui_4 mask1, krb5_ui_4 mask2);

#define ucisalpha(cc) ucisprop(cc, UC_LU|UC_LL|UC_LM|UC_LO|UC_LT, 0)
#define ucisdigit(cc) ucisprop(cc, UC_ND, 0)
#define ucisalnum(cc) ucisprop(cc, UC_LU|UC_LL|UC_LM|UC_LO|UC_LT|UC_ND, 0)
#define uciscntrl(cc) ucisprop(cc, UC_CC|UC_CF, 0)
#define ucisspace(cc) ucisprop(cc, UC_ZS|UC_SS, 0)
#define ucisblank(cc) ucisprop(cc, UC_ZS, 0)
#define ucispunct(cc) ucisprop(cc, UC_PD|UC_PS|UC_PE|UC_PO, UC_PI|UC_PF)
#define ucisgraph(cc) ucisprop(cc, UC_MN|UC_MC|UC_ME|UC_ND|UC_NL|UC_NO|\
                               UC_LU|UC_LL|UC_LT|UC_LM|UC_LO|UC_PC|UC_PD|\
                               UC_PS|UC_PE|UC_PO|UC_SM|UC_SM|UC_SC|UC_SK|\
                               UC_SO, UC_PI|UC_PF)
#define ucisprint(cc) ucisprop(cc, UC_MN|UC_MC|UC_ME|UC_ND|UC_NL|UC_NO|\
                               UC_LU|UC_LL|UC_LT|UC_LM|UC_LO|UC_PC|UC_PD|\
                               UC_PS|UC_PE|UC_PO|UC_SM|UC_SM|UC_SC|UC_SK|\
                               UC_SO|UC_ZS, UC_PI|UC_PF)
#define ucisupper(cc) ucisprop(cc, UC_LU, 0)
#define ucislower(cc) ucisprop(cc, UC_LL, 0)
#define ucistitle(cc) ucisprop(cc, UC_LT, 0)
#define ucisxdigit(cc) ucisprop(cc, 0, UC_HD)

#define ucisisocntrl(cc) ucisprop(cc, UC_CC, 0)
#define ucisfmtcntrl(cc) ucisprop(cc, UC_CF, 0)

#define ucissymbol(cc) ucisprop(cc, UC_SM|UC_SC|UC_SO|UC_SK, 0)
#define ucisnumber(cc) ucisprop(cc, UC_ND|UC_NO|UC_NL, 0)
#define ucisnonspacing(cc) ucisprop(cc, UC_MN, 0)
#define ucisopenpunct(cc) ucisprop(cc, UC_PS, 0)
#define ucisclosepunct(cc) ucisprop(cc, UC_PE, 0)
#define ucisinitialpunct(cc) ucisprop(cc, 0, UC_PI)
#define ucisfinalpunct(cc) ucisprop(cc, 0, UC_PF)

#define uciscomposite(cc) ucisprop(cc, 0, UC_CM)
#define ucishex(cc) ucisprop(cc, 0, UC_HD)
#define ucisquote(cc) ucisprop(cc, 0, UC_QM)
#define ucissymmetric(cc) ucisprop(cc, 0, UC_SY)
#define ucismirroring(cc) ucisprop(cc, 0, UC_MR)
#define ucisnonbreaking(cc) ucisprop(cc, 0, UC_NB)

/*
 * Directionality macros.
 */
#define ucisrtl(cc) ucisprop(cc, UC_R, 0)
#define ucisltr(cc) ucisprop(cc, UC_L, 0)
#define ucisstrong(cc) ucisprop(cc, UC_L|UC_R, 0)
#define ucisweak(cc) ucisprop(cc, UC_EN|UC_ES, UC_ET|UC_AN|UC_CS)
#define ucisneutral(cc) ucisprop(cc, 0, UC_B|UC_S|UC_WS|UC_ON)
#define ucisseparator(cc) ucisprop(cc, 0, UC_B|UC_S)

/*
 * Other macros inspired by John Cowan.
 */
#define ucismark(cc) ucisprop(cc, UC_MN|UC_MC|UC_ME, 0)
#define ucismodif(cc) ucisprop(cc, UC_LM, 0)
#define ucisletnum(cc) ucisprop(cc, UC_NL, 0)
#define ucisconnect(cc) ucisprop(cc, UC_PC, 0)
#define ucisdash(cc) ucisprop(cc, UC_PD, 0)
#define ucismath(cc) ucisprop(cc, UC_SM, 0)
#define uciscurrency(cc) ucisprop(cc, UC_SC, 0)
#define ucismodifsymbol(cc) ucisprop(cc, UC_SK, 0)
#define ucisnsmark(cc) ucisprop(cc, UC_MN, 0)
#define ucisspmark(cc) ucisprop(cc, UC_MC, 0)
#define ucisenclosing(cc) ucisprop(cc, UC_ME, 0)
#define ucisprivate(cc) ucisprop(cc, UC_CO, 0)
#define ucissurrogate(cc) ucisprop(cc, UC_OS, 0)
#define ucislsep(cc) ucisprop(cc, UC_ZL, 0)
#define ucispsep(cc) ucisprop(cc, UC_ZP, 0)

#define ucisidentstart(cc) ucisprop(cc, UC_LU|UC_LL|UC_LT|UC_LO|UC_NL, 0)
#define ucisidentpart(cc) ucisprop(cc, UC_LU|UC_LL|UC_LT|UC_LO|UC_NL|\
                                   UC_MN|UC_MC|UC_ND|UC_PC|UC_CF, 0)

#define ucisdefined(cc) ucisprop(cc, 0, UC_CP)
#define ucisundefined(cc) !ucisprop(cc, 0, UC_CP)

/*
 * Other miscellaneous character property macros.
 */
#define ucishan(cc) (((cc) >= 0x4e00 && (cc) <= 0x9fff) ||\
                     ((cc) >= 0xf900 && (cc) <= 0xfaff))
#define ucishangul(cc) ((cc) >= 0xac00 && (cc) <= 0xd7ff)

/**************************************************************************
 *
 * Functions for case conversion.
 *
 **************************************************************************/

krb5_ui_4 uctoupper(krb5_ui_4 code);
krb5_ui_4 uctolower(krb5_ui_4 code);
krb5_ui_4 uctotitle(krb5_ui_4 code);

/**************************************************************************
 *
 * Functions for getting compositions.
 *
 **************************************************************************/

/*
 * This routine determines if there exists a composition of node1 and node2.
 * If it returns 0, there is no composition.  Any other value indicates a
 * composition was returned in comp.
 */
int uccomp(krb5_ui_4 node1, krb5_ui_4 node2, krb5_ui_4 *comp);

/*
 * Does Hangul composition on the string str with length len, and returns
 * the length of the composed string.
 */
int uccomp_hangul(krb5_ui_4 *str, int len);

/*
 * Does canonical composition on the string str with length len, and returns
 * the length of the composed string.
 */
int uccanoncomp(krb5_ui_4 *str, int len);

/**************************************************************************
 *
 * Functions for getting decompositions.
 *
 **************************************************************************/

/*
 * This routine determines if the code has a decomposition.  If it returns 0,
 * there is no decomposition.  Any other value indicates a decomposition was
 * returned.
 */
int ucdecomp(krb5_ui_4 code, krb5_ui_4 *num, krb5_ui_4 **decomp);

/*
 * Equivalent to ucdecomp() except that it includes compatibility
 * decompositions.
 */
int uckdecomp(krb5_ui_4 code, krb5_ui_4 *num, krb5_ui_4 **decomp);

/*
 * If the code is a Hangul syllable, this routine decomposes it into the array
 * passed.  The array size should be at least 3.
 */
int ucdecomp_hangul(krb5_ui_4 code, krb5_ui_4 *num, krb5_ui_4 decomp[]);

/*
 * This routine does canonical decomposition of the string in of length
 * inlen, and returns the decomposed string in out with length outlen.
 * The memory for out is allocated by this routine. It returns the length
 * of the decomposed string if okay, and -1 on error.
 */
int uccanondecomp (const krb5_ui_4 *in, int inlen,
		     krb5_ui_4 **out, int *outlen);

/*
 * Equivalent to uccanondecomp() except that it includes compatibility
 * decompositions.
 */
int uccompatdecomp(const krb5_ui_4 *in, int inlen,
		     krb5_ui_4 **out, int *outlen);

/**************************************************************************
 *
 * Functions for getting combining classes.
 *
 **************************************************************************/

/*
 * This will return the combining class for a character to be used with the
 * Canonical Ordering algorithm.
 */
krb5_ui_4 uccombining_class(krb5_ui_4 code);

/**************************************************************************
 *
 * Functions for getting numbers and digits.
 *
 **************************************************************************/

struct ucnumber {
    int numerator;
    int denominator;
};

int
ucnumber_lookup (krb5_ui_4 code, struct ucnumber *num);

int
ucdigit_lookup (krb5_ui_4 code, int *digit);

/*
 * For compatibility with John Cowan's "uctype" package.
 */
struct ucnumber ucgetnumber (krb5_ui_4 code);
int ucgetdigit (krb5_ui_4 code);

/**************************************************************************
 *
 * Functions library initialization and cleanup.
 *
 **************************************************************************/

/*
 * Macros for specifying the data tables to be loaded, unloaded, or reloaded
 * by the ucdata_load(), ucdata_unload(), and ucdata_reload() routines.
 */
#define UCDATA_CASE   0x01
#define UCDATA_CTYPE  0x02
#define UCDATA_DECOMP 0x04
#define UCDATA_CMBCL  0x08
#define UCDATA_NUM    0x10
#define UCDATA_COMP   0x20
#define UCDATA_KDECOMP 0x40

#define UCDATA_ALL (UCDATA_CASE|UCDATA_CTYPE|UCDATA_DECOMP|\
                    UCDATA_CMBCL|UCDATA_NUM|UCDATA_COMP|UCDATA_KDECOMP)

/*
 * Functions to load, unload, and reload specific data files.
 */
int ucdata_load (char *paths, int mask);
void ucdata_unload (int mask);
int ucdata_reload (char *paths, int mask);

#ifdef UCDATA_DEPRECATED
/*
 * Deprecated functions, now just compatibility macros.
 */
#define ucdata_setup(p) ucdata_load(p, UCDATA_ALL)
#define ucdata_cleanup() ucdata_unload(UCDATA_ALL)
#endif

#endif /* _h_ucdata */
