/*
 * Prototypes for the OSTA functions
 *
 * $FreeBSD: src/sys/fs/udf/osta.h,v 1.1 2002/04/14 16:52:14 scottl Exp $
 */

#ifndef UNIX
#define	UNIX
#endif

#ifndef MAXLEN
#define	MAXLEN	255
#endif

/***********************************************************************
 * The following two typedef's are to remove compiler dependancies.
 * byte needs to be unsigned 8-bit, and unicode_t needs to be
 * unsigned 16-bit.
 */
typedef unsigned short unicode_t;
typedef unsigned char byte;

int udf_UncompressUnicode(int, byte *, unicode_t *);
int udf_CompressUnicode(int, int, unicode_t *, byte *);
unsigned short udf_cksum(unsigned char *, int);
unsigned short udf_unicode_cksum(unsigned short *, int);
int UDFTransName(unicode_t *, unicode_t *, int);
