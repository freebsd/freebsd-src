/*
 * Copyright (C) 1996
 *	Brandon Gillespie.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY Brandon Gillespie AND CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Brandon Gillespie OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

/*
// --------------------------------------------------------------------------
// to add a new algorithm, have it export the function 'crypt_<algo>'
// (where <algo> is whichever algorithm, such as 'des' or 'md5), with
// the arguments ordered as follows, and the return value 'char *':
//
//    register const unsigned char *  -- word to encrypt
//    const unsigned int              -- length of word to encrypt
//    register const unsigned char *  -- salt to encrypt with
//    const unsigned int              -- salt length
//    char *                          -- output buffer, _CRYPT_OUTPUT_SIZE max
//    char *                          -- identifier token
//
// such as:
//
//    char *
//    crypt_des(register const unsigned char *pw,
//              const unsigned int pl,
//              register const unsigned char *sp,
//              const unsigned int sl,
//              char * passwd,
//              char * token);
//
// Prototype the function below, include libraries here.
// You can use the macro CRYPT_HOOK() to make it easy.
*/

#include <md5.h>
#include <pwd.h>
#include "shs.h"

#define _DES_CRYPT		0
#define _MD5_CRYPT		1
#define _MD5_CRYPT_OLD		3
#define _BF_CRYPT		2
#define _BF_CRYPT_OpenBSD	4
#define _SHS_CRYPT		5

/*
// --------------------------------------------------------------------------
// Prototypes
*/

#define _CRYPT_HOOK(_type_) \
    char * crypt_ ## _type_ ( \
          register const unsigned char * pw, \
          const unsigned int pl, \
          register const unsigned char * sw, \
          const unsigned int sp, \
          char * passwd, \
          char * token)

#ifdef DES_CRYPT
_CRYPT_HOOK(des);
#endif

_CRYPT_HOOK(md5);
_CRYPT_HOOK(shs);

#undef _CRYPT_HOOK

/*
// --------------------------------------------------------------------------
// What is the default?
*/
#ifdef _CRYPT_DEFAULT_DES

/* use the 'best' encryption */
/* currently SHA-1 */
#define _CRYPT_DEFAULT_VERSION _SHS_CRYPT

#else

/* else use DES encryption */
#define _CRYPT_DEFAULT_VERSION _DES_CRYPT

#endif

/*
// --------------------------------------------------------------------------
// other unimportant magic, enlarge as algorithms warrant, do not reduce.
*/

/* largest size of encrypted password */
#define _CRYPT_OUTPUT_SIZE _PASSWORD_LEN
#define _CRYPT_MAX_SALT_LEN 24 /* token=5 salt=16 extra=4 */

/* magic sizes not defined elsewhere, cleaner through defs */
#define _MD5_SIZE 16
#define _SHS_SIZE 20

#ifndef _CRYPT_C_
extern void to64(char * s, unsigned long v, int n);
#endif

