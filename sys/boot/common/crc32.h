/*-
 *  COPYRIGHT (C) 1986 Gary S. Brown.  You may use this program, or
 *  code or tables extracted from it, as desired without restriction.
 *
 * $FreeBSD: src/sys/boot/common/crc32.h,v 1.1.2.2.2.1 2010/12/21 17:09:25 kensmith Exp $
 */

#ifndef _CRC32_H_
#define	_CRC32_H_

uint32_t crc32(const void *buf, size_t size);

#endif	/* !_CRC32_H_ */
