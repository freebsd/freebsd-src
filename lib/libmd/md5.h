/* $FreeBSD$ */

#ifndef _MD5_H_
#define _MD5_H_

#ifndef _KERNEL

/* Ensure libmd symbols do not clash with libcrypto */

#define MD5Init		_libmd_MD5Init
#define MD5Update	_libmd_MD5Update
#define MD5Pad		_libmd_MD5Pad
#define MD5Final	_libmd_MD5Final
#define MD5Transform	_libmd_MD5Transform
#define MD5End		_libmd_MD5End
#define MD5File		_libmd_MD5File
#define MD5FileChunk	_libmd_MD5FileChunk
#define MD5Data		_libmd_MD5Data

#endif

#include <sys/md5.h>
#endif /* _MD5_H_ */
