/*
 * Copyright (c) 1997 - 2001 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden). 
 * All rights reserved. 
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions 
 * are met: 
 *
 * 1. Redistributions of source code must retain the above copyright 
 *    notice, this list of conditions and the following disclaimer. 
 *
 * 2. Redistributions in binary form must reproduce the above copyright 
 *    notice, this list of conditions and the following disclaimer in the 
 *    documentation and/or other materials provided with the distribution. 
 *
 * 3. Neither the name of the Institute nor the names of its contributors 
 *    may be used to endorse or promote products derived from this software 
 *    without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND 
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE 
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL 
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS 
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) 
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT 
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY 
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF 
 * SUCH DAMAGE. 
 */

/* $Id: der.h,v 1.20 2001/01/29 08:31:27 assar Exp $ */

#ifndef __DER_H__
#define __DER_H__

#include <time.h>

typedef enum {UNIV = 0, APPL = 1, CONTEXT = 2 , PRIVATE = 3} Der_class;

typedef enum {PRIM = 0, CONS = 1} Der_type;

/* Universal tags */

enum {
     UT_Integer		= 2,	
     UT_BitString	= 3,
     UT_OctetString	= 4,
     UT_Null		= 5,
     UT_ObjID		= 6,
     UT_Sequence	= 16,
     UT_Set		= 17,
     UT_PrintableString	= 19,
     UT_IA5String	= 22,
     UT_UTCTime		= 23,
     UT_GeneralizedTime	= 24,
     UT_VisibleString	= 26,
     UT_GeneralString	= 27
};

#define ASN1_INDEFINITE 0xdce0deed

#ifndef HAVE_TIMEGM
time_t timegm (struct tm *);
#endif

int time2generalizedtime (time_t t, octet_string *s);

int der_get_int (const unsigned char *p, size_t len, int *ret, size_t *size);
int der_get_length (const unsigned char *p, size_t len,
		    size_t *val, size_t *size);
int der_get_general_string (const unsigned char *p, size_t len, 
			    general_string *str, size_t *size);
int der_get_octet_string (const unsigned char *p, size_t len, 
			  octet_string *data, size_t *size);
int der_get_tag (const unsigned char *p, size_t len, 
		 Der_class *class, Der_type *type,
		 int *tag, size_t *size);

int der_match_tag (const unsigned char *p, size_t len, 
		   Der_class class, Der_type type,
		   int tag, size_t *size);
int der_match_tag_and_length (const unsigned char *p, size_t len,
			      Der_class class, Der_type type, int tag,
			      size_t *length_ret, size_t *size);

int decode_integer (const unsigned char*, size_t, int*, size_t*);
int decode_unsigned (const unsigned char*, size_t, unsigned*, size_t*);
int decode_general_string (const unsigned char*, size_t,
			   general_string*, size_t*);
int decode_octet_string (const unsigned char*, size_t, octet_string*, size_t*);
int decode_generalized_time (const unsigned char*, size_t, time_t*, size_t*);

int der_put_int (unsigned char *p, size_t len, int val, size_t*);
int der_put_length (unsigned char *p, size_t len, size_t val, size_t*);
int der_put_general_string (unsigned char *p, size_t len,
			    const general_string *str, size_t*);
int der_put_octet_string (unsigned char *p, size_t len,
			  const octet_string *data, size_t*);
int der_put_tag (unsigned char *p, size_t len, Der_class class, Der_type type,
		 int tag, size_t*);
int der_put_length_and_tag (unsigned char*, size_t, size_t, 
			    Der_class, Der_type, int, size_t*);

int encode_integer (unsigned char *p, size_t len,
		    const int *data, size_t*);
int encode_unsigned (unsigned char *p, size_t len,
		     const unsigned *data, size_t*);
int encode_general_string (unsigned char *p, size_t len, 
			   const general_string *data, size_t*);
int encode_octet_string (unsigned char *p, size_t len,
			 const octet_string *k, size_t*);
int encode_generalized_time (unsigned char *p, size_t len,
			     const time_t *t, size_t*);

void free_integer (int *num);
void free_general_string (general_string *str);
void free_octet_string (octet_string *k);
void free_generalized_time (time_t *t);

size_t length_len (size_t len);
size_t length_integer (const int *data);
size_t length_unsigned (const unsigned *data);
size_t length_general_string (const general_string *data);
size_t length_octet_string (const octet_string *k);
size_t length_generalized_time (const time_t *t);

int copy_general_string (const general_string *from, general_string *to);
int copy_octet_string (const octet_string *from, octet_string *to);

int fix_dce(size_t reallen, size_t *len);

#endif /* __DER_H__ */

