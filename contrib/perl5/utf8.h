/*    utf8.h
 *
 *    Copyright (c) 1998-2001, Larry Wall
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 */

START_EXTERN_C

#ifdef DOINIT
EXTCONST unsigned char PL_utf8skip[] = {
1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, /* ascii */
1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, /* ascii */
1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, /* ascii */
1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, /* ascii */
1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, /* bogus */
1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, /* bogus */
2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2, /* scripts */
3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,4,4,4,4,4,4,4,4,5,5,5,5,6,6,	 /* cjk etc. */
7,13, /* Perl extended (not UTF-8).  Up to 72bit allowed (64-bit + reserved). */
};
#else
EXTCONST unsigned char PL_utf8skip[];
#endif

END_EXTERN_C

#define UTF8_MAXLEN 13 /* how wide can a single UTF8 encoded character become */

/* #define IN_UTF8 (PL_curcop->op_private & HINT_UTF8) */
#define IN_BYTE (PL_curcop->op_private & HINT_BYTE)
#define DO_UTF8(sv) (SvUTF8(sv) && !IN_BYTE)

#define UTF8_ALLOW_EMPTY		0x0001
#define UTF8_ALLOW_CONTINUATION		0x0002
#define UTF8_ALLOW_NON_CONTINUATION	0x0004
#define UTF8_ALLOW_FE_FF		0x0008
#define UTF8_ALLOW_SHORT		0x0010
#define UTF8_ALLOW_SURROGATE		0x0020
#define UTF8_ALLOW_BOM			0x0040
#define UTF8_ALLOW_FFFF			0x0080
#define UTF8_ALLOW_LONG			0x0100
#define UTF8_ALLOW_ANYUV		(UTF8_ALLOW_EMPTY|UTF8_ALLOW_FE_FF|\
					 UTF8_ALLOW_SURROGATE|UTF8_ALLOW_BOM|\
					 UTF8_ALLOW_FFFF|UTF8_ALLOW_LONG)
#define UTF8_ALLOW_ANY			0x00ff
#define UTF8_CHECK_ONLY			0x0100

#define UNICODE_SURROGATE_FIRST		0xd800
#define UNICODE_SURROGATE_LAST		0xdfff
#define UNICODE_REPLACEMENT		0xfffd
#define UNICODE_BYTER_ORDER_MARK	0xfffe
#define UNICODE_ILLEGAL			0xffff

#define UNICODE_IS_SURROGATE(c)		((c) >= UNICODE_SURROGATE_FIRST && \
					 (c) <= UNICODE_SURROGATE_LAST)
#define UNICODE_IS_REPLACEMENT(c)	((c) == UNICODE_REPLACMENT)
#define UNICODE_IS_BYTE_ORDER_MARK(c)	((c) == UNICODE_BYTER_ORDER_MARK)
#define UNICODE_IS_ILLEGAL(c)		((c) == UNICODE_ILLEGAL)

#define UTF8SKIP(s) PL_utf8skip[*(U8*)s]

#define UTF8_QUAD_MAX	UINT64_C(0x1000000000)

/*
 
 The following table is from Unicode 3.1.

 Code Points		1st Byte  2nd Byte  3rd Byte  4th Byte

   U+0000..U+007F	00..7F   
   U+0080..U+07FF	C2..DF    80..BF   
   U+0800..U+0FFF	E0        A0..BF    80..BF  
   U+1000..U+FFFF	E1..EF    80..BF    80..BF  
  U+10000..U+3FFFF	F0        90..BF    80..BF    80..BF
  U+40000..U+FFFFF	F1..F3    80..BF    80..BF    80..BF
 U+100000..U+10FFFF	F4        80..8F    80..BF    80..BF

 */

#define UTF8_IS_ASCII(c) 		(((U8)c) <  0x80)
#define UTF8_IS_START(c)		(((U8)c) >= 0xc0 && (((U8)c) <= 0xfd))
#define UTF8_IS_CONTINUATION(c)		(((U8)c) >= 0x80 && (((U8)c) <= 0xbf))
#define UTF8_IS_CONTINUED(c) 		(((U8)c) &  0x80)
#define UTF8_IS_DOWNGRADEABLE_START(c)	(((U8)c & 0xfc) != 0xc0)

#define UTF8_CONTINUATION_MASK		((U8)0x3f)
#define UTF8_ACCUMULATION_SHIFT		6
#define UTF8_ACCUMULATE(old, new)	(((old) << UTF8_ACCUMULATION_SHIFT) | (((U8)new) & UTF8_CONTINUATION_MASK))

#define UTF8_EIGHT_BIT_HI(c)	( (((U8)(c))>>6)      |0xc0)
#define UTF8_EIGHT_BIT_LO(c)	(((((U8)(c))   )&0x3f)|0x80)

#ifdef HAS_QUAD
#define UNISKIP(uv) ( (uv) < 0x80           ? 1 : \
		      (uv) < 0x800          ? 2 : \
		      (uv) < 0x10000        ? 3 : \
		      (uv) < 0x200000       ? 4 : \
		      (uv) < 0x4000000      ? 5 : \
		      (uv) < 0x80000000     ? 6 : \
                      (uv) < UTF8_QUAD_MAX ? 7 : 13 ) 
#else
/* No, I'm not even going to *TRY* putting #ifdef inside a #define */
#define UNISKIP(uv) ( (uv) < 0x80           ? 1 : \
		      (uv) < 0x800          ? 2 : \
		      (uv) < 0x10000        ? 3 : \
		      (uv) < 0x200000       ? 4 : \
		      (uv) < 0x4000000      ? 5 : \
		      (uv) < 0x80000000     ? 6 : 7 )
#endif


/*
 * Note: we try to be careful never to call the isXXX_utf8() functions
 * unless we're pretty sure we've seen the beginning of a UTF-8 character
 * (that is, the two high bits are set).  Otherwise we risk loading in the
 * heavy-duty SWASHINIT and SWASHGET routines unnecessarily.
 */
#ifdef EBCDIC
#define isIDFIRST_lazy_if(p,c) isIDFIRST(*(p))
#define isALNUM_lazy_if(p,c)   isALNUM(*(p))
#else
#define isIDFIRST_lazy_if(p,c) ((IN_BYTE || (!c || (*((U8*)p) < 0xc0))) \
				? isIDFIRST(*(p)) \
				: isIDFIRST_utf8((U8*)p))
#define isALNUM_lazy_if(p,c)   ((IN_BYTE || (!c || (*((U8*)p) < 0xc0))) \
				? isALNUM(*(p)) \
				: isALNUM_utf8((U8*)p))
#endif
#define isIDFIRST_lazy(p)	isIDFIRST_lazy_if(p,1)
#define isALNUM_lazy(p)		isALNUM_lazy_if(p,1)
