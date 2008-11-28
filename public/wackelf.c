/*
 * Copyright (c) 2002-2008 Sam Leffler, Errno Consulting
 * Copyright (c) 2002-2008 Atheros Communications, Inc.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * $Id: wackelf.c,v 1.2 2008/11/10 03:44:51 sam Exp $
 */

/*
 * Program to zap flags field in the ELF header of an object
 * file so that it appears to use VFP soft floating point.
 * This is done because there is no standard way to specify
 * this on the command line to gcc/binutils.
 *
 * Derived from code by Olivier Houchard <cognet@freebsd.org>
 */
#include <stdio.h>
#include <stdlib.h>
#include <elf.h>
#include <fcntl.h>
#include <err.h>

#ifdef __linux__
#include <endian.h>
#include <byteswap.h>
#define	_LITTLE_ENDIAN	__LITTLE_ENDIAN
#define	_BIG_ENDIAN	__BIG_ENDIAN
#if __BYTE_ORDER == __LITTLE_ENDIAN
#define	htobe16(x)	__bswap_16((x))
#define	htobe32(x)	__bswap_32((x))
#define	htole16(x)	((uint16_t)(x))
#define	htole32(x)	((uint32_t)(x))
#else /* _BYTE_ORDER != _LITTLE_ENDIAN */
#define	htobe16(x)	((uint16_t)(x))
#define	htobe32(x)	((uint32_t)(x))
#define	htole16(x)	__bswap_16((x))
#define	htole32(x)	__bswap_32((x))
#endif /* _BYTE_ORDER == _LITTLE_ENDIAN */
#else
#include <sys/endian.h>
#endif

int
main(int argc, char *argv[])
{
	int fd, endian, oflags;
	int format = 0x400;		/* default to VFP */
	Elf32_Ehdr ehdr;

	if (argc > 2) {
		if (strcmp(argv[1], "-fpa") == 0) {
			format = 0x200;
			argc--, argv++;
		} else if (strcmp(argv[1], "-vfp") == 0) {
			format = 0x400;
			argc--, argv++;
		} else if (strcmp(argv[1], "-none") == 0) {
			format = 0;
			argc--, argv++;
		}
	}
	if (argc != 2) {
		fprintf(stderr, "usage: %s [-fpa|-vfp|-none] file\n", argv[0]);
		exit(-1);
	}
	fd = open(argv[1], O_RDWR);
	if (fd < 0)
		err(1, "could not open %s", argv[1]);
	if (read(fd, &ehdr, sizeof(ehdr)) != sizeof(ehdr))
		err(1, "could not read the ELF header");
	if (ehdr.e_machine == htole16(EM_ARM))
		endian = _LITTLE_ENDIAN;
	else if (ehdr.e_machine == htobe16(EM_ARM))
		endian = _BIG_ENDIAN;
	else
		errx(1, "not an ARM ELF object (machine 0x%x)", ehdr.e_machine);
	oflags = ehdr.e_flags;
	if (endian == _BIG_ENDIAN) {
		ehdr.e_flags &= ~htobe32(0x600); /* Remove FPA Soft float */
		ehdr.e_flags |= htobe32(format); /* VFP Soft Float */
	} else {
		ehdr.e_flags &= ~htole32(0x600); /* Remove FPA Soft float */
		ehdr.e_flags |= htole32(format); /* VFP Soft Float */
	}
	printf("%s: e_flags 0x%x => 0x%x\n", argv[1], oflags, ehdr.e_flags);
	if (lseek(fd, (off_t) 0, SEEK_SET) != 0)
		err(1, "lseek");
	if (write(fd, &ehdr, sizeof(ehdr)) != sizeof(ehdr))
		err(1, "yow, elf header write failed");
	close(fd);
	return 0;
}
