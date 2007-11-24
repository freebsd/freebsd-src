/*
 * Copyright (c) Christos Zoulas 2003.
 * All Rights Reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice immediately at the beginning of the file, without modification,
 *    this list of conditions, and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *  
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#include "file.h"

#ifdef BUILTIN_ELF
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "readelf.h"

#ifndef lint
FILE_RCSID("@(#)$Id: readelf.c,v 1.45 2004/11/24 17:38:24 christos Exp $")
#endif

#ifdef	ELFCORE
private int dophn_core(struct magic_set *, int, int, int, off_t, int, size_t);
#endif
private int dophn_exec(struct magic_set *, int, int, int, off_t, int, size_t);
private int doshn(struct magic_set *, int, int, int, off_t, int, size_t);
private size_t donote(struct magic_set *, unsigned char *, size_t, size_t, int,
    int, size_t);

#define	ELF_ALIGN(a)	((((a) + align - 1) / align) * align)

private uint16_t getu16(int, uint16_t);
private uint32_t getu32(int, uint32_t);
private uint64_t getu64(int, uint64_t);

private uint16_t
getu16(int swap, uint16_t value)
{
	union {
		uint16_t ui;
		char c[2];
	} retval, tmpval;

	if (swap) {
		tmpval.ui = value;

		retval.c[0] = tmpval.c[1];
		retval.c[1] = tmpval.c[0];
		
		return retval.ui;
	} else
		return value;
}

private uint32_t
getu32(int swap, uint32_t value)
{
	union {
		uint32_t ui;
		char c[4];
	} retval, tmpval;

	if (swap) {
		tmpval.ui = value;

		retval.c[0] = tmpval.c[3];
		retval.c[1] = tmpval.c[2];
		retval.c[2] = tmpval.c[1];
		retval.c[3] = tmpval.c[0];
		
		return retval.ui;
	} else
		return value;
}

private uint64_t
getu64(int swap, uint64_t value)
{
	union {
		uint64_t ui;
		char c[8];
	} retval, tmpval;

	if (swap) {
		tmpval.ui = value;

		retval.c[0] = tmpval.c[7];
		retval.c[1] = tmpval.c[6];
		retval.c[2] = tmpval.c[5];
		retval.c[3] = tmpval.c[4];
		retval.c[4] = tmpval.c[3];
		retval.c[5] = tmpval.c[2];
		retval.c[6] = tmpval.c[1];
		retval.c[7] = tmpval.c[0];
		
		return retval.ui;
	} else
		return value;
}

#define sh_addr		(class == ELFCLASS32		\
			 ? (void *) &sh32		\
			 : (void *) &sh64)
#define sh_size		(class == ELFCLASS32		\
			 ? sizeof sh32			\
			 : sizeof sh64)
#define shs_type	(class == ELFCLASS32		\
			 ? getu32(swap, sh32.sh_type)	\
			 : getu32(swap, sh64.sh_type))
#define ph_addr		(class == ELFCLASS32		\
			 ? (void *) &ph32		\
			 : (void *) &ph64)
#define ph_size		(class == ELFCLASS32		\
			 ? sizeof ph32			\
			 : sizeof ph64)
#define ph_type		(class == ELFCLASS32		\
			 ? getu32(swap, ph32.p_type)	\
			 : getu32(swap, ph64.p_type))
#define ph_offset	(class == ELFCLASS32		\
			 ? getu32(swap, ph32.p_offset)	\
			 : getu64(swap, ph64.p_offset))
#define ph_align	(size_t)((class == ELFCLASS32	\
			 ? (off_t) (ph32.p_align ? 	\
			    getu32(swap, ph32.p_align) : 4) \
			 : (off_t) (ph64.p_align ?	\
			    getu64(swap, ph64.p_align) : 4)))
#define ph_filesz	(size_t)((class == ELFCLASS32	\
			 ? getu32(swap, ph32.p_filesz)	\
			 : getu64(swap, ph64.p_filesz)))
#define ph_memsz	(size_t)((class == ELFCLASS32	\
			 ? getu32(swap, ph32.p_memsz)	\
			 : getu64(swap, ph64.p_memsz)))
#define nh_size		(class == ELFCLASS32		\
			 ? sizeof nh32			\
			 : sizeof nh64)
#define nh_type		(class == ELFCLASS32		\
			 ? getu32(swap, nh32.n_type)	\
			 : getu32(swap, nh64.n_type))
#define nh_namesz	(class == ELFCLASS32		\
			 ? getu32(swap, nh32.n_namesz)	\
			 : getu32(swap, nh64.n_namesz))
#define nh_descsz	(class == ELFCLASS32		\
			 ? getu32(swap, nh32.n_descsz)	\
			 : getu32(swap, nh64.n_descsz))
#define prpsoffsets(i)	(class == ELFCLASS32		\
			 ? prpsoffsets32[i]		\
			 : prpsoffsets64[i])

#ifdef ELFCORE
size_t	prpsoffsets32[] = {
	8,		/* FreeBSD */
	28,		/* Linux 2.0.36 */
	32,		/* Linux (I forget which kernel version) */
	84,		/* SunOS 5.x */
};

size_t	prpsoffsets64[] = {
       120,		/* SunOS 5.x, 64-bit */
};

#define	NOFFSETS32	(sizeof prpsoffsets32 / sizeof prpsoffsets32[0])
#define NOFFSETS64	(sizeof prpsoffsets64 / sizeof prpsoffsets64[0])

#define NOFFSETS	(class == ELFCLASS32 ? NOFFSETS32 : NOFFSETS64)

/*
 * Look through the program headers of an executable image, searching
 * for a PT_NOTE section of type NT_PRPSINFO, with a name "CORE" or
 * "FreeBSD"; if one is found, try looking in various places in its
 * contents for a 16-character string containing only printable
 * characters - if found, that string should be the name of the program
 * that dropped core.  Note: right after that 16-character string is,
 * at least in SunOS 5.x (and possibly other SVR4-flavored systems) and
 * Linux, a longer string (80 characters, in 5.x, probably other
 * SVR4-flavored systems, and Linux) containing the start of the
 * command line for that program.
 *
 * The signal number probably appears in a section of type NT_PRSTATUS,
 * but that's also rather OS-dependent, in ways that are harder to
 * dissect with heuristics, so I'm not bothering with the signal number.
 * (I suppose the signal number could be of interest in situations where
 * you don't have the binary of the program that dropped core; if you
 * *do* have that binary, the debugger will probably tell you what
 * signal it was.)
 */

#define	OS_STYLE_SVR4		0
#define	OS_STYLE_FREEBSD	1
#define	OS_STYLE_NETBSD		2

private const char *os_style_names[] = {
	"SVR4",
	"FreeBSD",
	"NetBSD",
};

private int
dophn_core(struct magic_set *ms, int class, int swap, int fd, off_t off,
    int num, size_t size)
{
	Elf32_Phdr ph32;
	Elf64_Phdr ph64;
	size_t offset;
	unsigned char nbuf[BUFSIZ];
	ssize_t bufsize;

	if (size != ph_size) {
		if (file_printf(ms, ", corrupted program header size") == -1)
			return -1;
		return 0;
	}
	/*
	 * Loop through all the program headers.
	 */
	for ( ; num; num--) {
		if (lseek(fd, off, SEEK_SET) == (off_t)-1) {
			file_badseek(ms);
			return -1;
		}
		if (read(fd, ph_addr, ph_size) == -1) {
			file_badread(ms);
			return -1;
		}
		off += size;
		if (ph_type != PT_NOTE)
			continue;

		/*
		 * This is a PT_NOTE section; loop through all the notes
		 * in the section.
		 */
		if (lseek(fd, (off_t) ph_offset, SEEK_SET) == (off_t)-1) {
			file_badseek(ms);
			return -1;
		}
		bufsize = read(fd, nbuf,
		    ((ph_filesz < sizeof(nbuf)) ? ph_filesz : sizeof(nbuf)));
		if (bufsize == -1) {
			file_badread(ms);
			return -1;
		}
		offset = 0;
		for (;;) {
			if (offset >= (size_t)bufsize)
				break;
			offset = donote(ms, nbuf, offset, (size_t)bufsize,
			    class, swap, 4);
			if (offset == 0)
				break;

		}
	}
	return 0;
}
#endif

private size_t
donote(struct magic_set *ms, unsigned char *nbuf, size_t offset, size_t size,
    int class, int swap, size_t align)
{
	Elf32_Nhdr nh32;
	Elf64_Nhdr nh64;
	size_t noff, doff;
#ifdef ELFCORE
	int os_style = -1;
#endif
	uint32_t namesz, descsz;

	if (class == ELFCLASS32)
		memcpy(&nh32, &nbuf[offset], sizeof(nh32));
	else
		memcpy(&nh64, &nbuf[offset], sizeof(nh64));
	offset += nh_size;

	namesz = nh_namesz;
	descsz = nh_descsz;
	if ((namesz == 0) && (descsz == 0)) {
		/*
		 * We're out of note headers.
		 */
		return offset;
	}

	if (namesz & 0x80000000) {
	    (void)file_printf(ms, ", bad note name size 0x%lx",
		(unsigned long)namesz);
	    return offset;
	}

	if (descsz & 0x80000000) {
	    (void)file_printf(ms, ", bad note description size 0x%lx",
		(unsigned long)descsz);
	    return offset;
	}


	noff = offset;
	doff = ELF_ALIGN(offset + namesz);

	if (offset + namesz > size) {
		/*
		 * We're past the end of the buffer.
		 */
		return doff;
	}

	offset = ELF_ALIGN(doff + descsz);
	if (doff + descsz > size) {
		return offset;
	}

	if (namesz == 4 && strcmp((char *)&nbuf[noff], "GNU") == 0 &&
	    nh_type == NT_GNU_VERSION && descsz == 16) {
		uint32_t desc[4];
		(void)memcpy(desc, &nbuf[doff], sizeof(desc));

		if (file_printf(ms, ", for GNU/") == -1)
			return size;
		switch (getu32(swap, desc[0])) {
		case GNU_OS_LINUX:
			if (file_printf(ms, "Linux") == -1)
				return size;
			break;
		case GNU_OS_HURD:
			if (file_printf(ms, "Hurd") == -1)
				return size;
			break;
		case GNU_OS_SOLARIS:
			if (file_printf(ms, "Solaris") == -1)
				return size;
			break;
		default:
			if (file_printf(ms, "<unknown>") == -1)
				return size; 
		}
		if (file_printf(ms, " %d.%d.%d", getu32(swap, desc[1]),
		    getu32(swap, desc[2]), getu32(swap, desc[3])) == -1)
			return size;
		return size;
	}

	if (namesz == 7 && strcmp((char *)&nbuf[noff], "NetBSD") == 0 &&
	    nh_type == NT_NETBSD_VERSION && descsz == 4) {
		uint32_t desc;
		(void)memcpy(&desc, &nbuf[doff], sizeof(desc));
		desc = getu32(swap, desc);

		if (file_printf(ms, ", for NetBSD") == -1)
			return size;
		/*
		 * The version number used to be stuck as 199905, and was thus
		 * basically content-free.  Newer versions of NetBSD have fixed
		 * this and now use the encoding of __NetBSD_Version__:
		 *
		 *	MMmmrrpp00
		 *
		 * M = major version
		 * m = minor version
		 * r = release ["",A-Z,Z[A-Z] but numeric]
		 * p = patchlevel
		 */
		if (desc > 100000000U) {
			u_int ver_patch = (desc / 100) % 100;
			u_int ver_rel = (desc / 10000) % 100;
			u_int ver_min = (desc / 1000000) % 100;
			u_int ver_maj = desc / 100000000;

			if (file_printf(ms, " %u.%u", ver_maj, ver_min) == -1)
				return size;
			if (ver_rel == 0 && ver_patch != 0) {
				if (file_printf(ms, ".%u", ver_patch) == -1)
					return size;
			} else if (ver_rel != 0) {
				while (ver_rel > 26) {
					file_printf(ms, "Z");
					ver_rel -= 26;
				}
				file_printf(ms, "%c", 'A' + ver_rel - 1);
			}
		}
		return size;
	}

	if (namesz == 8 && strcmp((char *)&nbuf[noff], "FreeBSD") == 0 &&
	    nh_type == NT_FREEBSD_VERSION && descsz == 4) {
		uint32_t desc;
		(void)memcpy(&desc, &nbuf[doff], sizeof(desc));
		desc = getu32(swap, desc);
		if (file_printf(ms, ", for FreeBSD") == -1)
			return size;

		/*
		 * Contents is __FreeBSD_version, whose relation to OS
		 * versions is defined by a huge table in the Porter's
		 * Handbook.  This is the general scheme:
		 * 
		 * Releases:
		 * 	Mmp000 (before 4.10)
		 * 	Mmi0p0 (before 5.0)
		 * 	Mmm0p0
		 * 
		 * Development branches:
		 * 	Mmpxxx (before 4.6)
		 * 	Mmp1xx (before 4.10)
		 * 	Mmi1xx (before 5.0)
		 * 	M000xx (pre-M.0)
		 * 	Mmm1xx
		 * 
		 * M = major version
		 * m = minor version
		 * i = minor version increment (491000 -> 4.10)
		 * p = patchlevel
		 * x = revision
		 * 
		 * The first release of FreeBSD to use ELF by default
		 * was version 3.0.
		 */
		if (desc == 460002) {
			if (file_printf(ms, " 4.6.2") == -1)
				return size;
		} else if (desc < 460100) {
			if (file_printf(ms, " %d.%d", desc / 100000,
			    desc / 10000 % 10) == -1)
				return size;
			if (desc / 1000 % 10 > 0)
				if (file_printf(ms, ".%d", desc / 1000 % 10)
				    == -1)
					return size;
			if ((desc % 1000 > 0) || (desc % 100000 == 0))
				if (file_printf(ms, " (%d)", desc) == -1)
					return size;
		} else if (desc < 500000) {
			if (file_printf(ms, " %d.%d", desc / 100000,
			    desc / 10000 % 10 + desc / 1000 % 10) == -1)
				return size;
			if (desc / 100 % 10 > 0) {
				if (file_printf(ms, " (%d)", desc) == -1)
					return size;
			} else if (desc / 10 % 10 > 0) {
				if (file_printf(ms, ".%d", desc / 10 % 10)
				    == -1)
					return size;
			}
		} else {
			if (file_printf(ms, " %d.%d", desc / 100000,
			    desc / 1000 % 100) == -1)
				return size;
			if ((desc / 100 % 10 > 0) ||
			    (desc % 100000 / 100 == 0)) {
				if (file_printf(ms, " (%d)", desc) == -1)
					return size;
			} else if (desc / 10 % 10 > 0) {
				if (file_printf(ms, ".%d", desc / 10 % 10)
				    == -1)
					return size;
			}
		}
		return size;
	}

	if (namesz == 8 && strcmp((char *)&nbuf[noff], "OpenBSD") == 0 &&
	    nh_type == NT_OPENBSD_VERSION && descsz == 4) {
		if (file_printf(ms, ", for OpenBSD") == -1)
			return size;
		/* Content of note is always 0 */
		return size;
	}

	/*
	 * Sigh.  The 2.0.36 kernel in Debian 2.1, at
	 * least, doesn't correctly implement name
	 * sections, in core dumps, as specified by
	 * the "Program Linking" section of "UNIX(R) System
	 * V Release 4 Programmer's Guide: ANSI C and
	 * Programming Support Tools", because my copy
	 * clearly says "The first 'namesz' bytes in 'name'
	 * contain a *null-terminated* [emphasis mine]
	 * character representation of the entry's owner
	 * or originator", but the 2.0.36 kernel code
	 * doesn't include the terminating null in the
	 * name....
	 */
	if ((namesz == 4 && strncmp((char *)&nbuf[noff], "CORE", 4) == 0) ||
	    (namesz == 5 && strcmp((char *)&nbuf[noff], "CORE") == 0)) {
		os_style = OS_STYLE_SVR4;
	} 

	if ((namesz == 8 && strcmp((char *)&nbuf[noff], "FreeBSD") == 0)) {
		os_style = OS_STYLE_FREEBSD;
	}

	if ((namesz >= 11 && strncmp((char *)&nbuf[noff], "NetBSD-CORE", 11)
	    == 0)) {
		os_style = OS_STYLE_NETBSD;
	}

#ifdef ELFCORE
	if (os_style != -1)
		if (file_printf(ms, ", %s-style", os_style_names[os_style]) == -1)
			return size;

	if (os_style == OS_STYLE_NETBSD && nh_type == NT_NETBSD_CORE_PROCINFO) {
		uint32_t signo;
		/*
		 * Extract the program name.  It is at
		 * offset 0x7c, and is up to 32-bytes,
		 * including the terminating NUL.
		 */
		if (file_printf(ms, ", from '%.31s'", &nbuf[doff + 0x7c]) == -1)
			return size;
		
		/*
		 * Extract the signal number.  It is at
		 * offset 0x08.
		 */
		memcpy(&signo, &nbuf[doff + 0x08],
		    sizeof(signo));
		if (file_printf(ms, " (signal %u)", getu32(swap, signo)) == -1)
			return size;
		return size;
	} else if (os_style != OS_STYLE_NETBSD && nh_type == NT_PRPSINFO) {
		size_t i, j;
		unsigned char c;
		/*
		 * Extract the program name.  We assume
		 * it to be 16 characters (that's what it
		 * is in SunOS 5.x and Linux).
		 *
		 * Unfortunately, it's at a different offset
		 * in varous OSes, so try multiple offsets.
		 * If the characters aren't all printable,
		 * reject it.
		 */
		for (i = 0; i < NOFFSETS; i++) {
			size_t reloffset = prpsoffsets(i);
			size_t noffset = doff + reloffset;
			for (j = 0; j < 16; j++, noffset++, reloffset++) {
				/*
				 * Make sure we're not past
				 * the end of the buffer; if
				 * we are, just give up.
				 */
				if (noffset >= size)
					goto tryanother;

				/*
				 * Make sure we're not past
				 * the end of the contents;
				 * if we are, this obviously
				 * isn't the right offset.
				 */
				if (reloffset >= descsz)
					goto tryanother;

				c = nbuf[noffset];
				if (c == '\0') {
					/*
					 * A '\0' at the
					 * beginning is
					 * obviously wrong.
					 * Any other '\0'
					 * means we're done.
					 */
					if (j == 0)
						goto tryanother;
					else
						break;
				} else {
					/*
					 * A nonprintable
					 * character is also
					 * wrong.
					 */
#define isquote(c) (strchr("'\"`", (c)) != NULL)
					if (!isprint(c) || isquote(c))
						goto tryanother;
				}
			}

			/*
			 * Well, that worked.
			 */
			if (file_printf(ms, ", from '%.16s'",
			    &nbuf[doff + prpsoffsets(i)]) == -1)
				return size;
			return size;

		tryanother:
			;
		}
		return offset;
	}
#endif
	return offset;
}

private int
doshn(struct magic_set *ms, int class, int swap, int fd, off_t off, int num,
    size_t size)
{
	Elf32_Shdr sh32;
	Elf64_Shdr sh64;

	if (size != sh_size) {
		if (file_printf(ms, ", corrupted section header size") == -1)
			return -1;
		return 0;
	}

	if (lseek(fd, off, SEEK_SET) == (off_t)-1) {
		file_badseek(ms);
		return -1;
	}

	for ( ; num; num--) {
		if (read(fd, sh_addr, sh_size) == -1) {
			file_badread(ms);
			return -1;
		}
		if (shs_type == SHT_SYMTAB /* || shs_type == SHT_DYNSYM */) {
			if (file_printf(ms, ", not stripped") == -1)
				return -1;
			return 0;
		}
	}
	if (file_printf(ms, ", stripped") == -1)
		return -1;
	return 0;
}

/*
 * Look through the program headers of an executable image, searching
 * for a PT_INTERP section; if one is found, it's dynamically linked,
 * otherwise it's statically linked.
 */
private int
dophn_exec(struct magic_set *ms, int class, int swap, int fd, off_t off,
    int num, size_t size)
{
	Elf32_Phdr ph32;
	Elf64_Phdr ph64;
	const char *linking_style = "statically";
	const char *shared_libraries = "";
	unsigned char nbuf[BUFSIZ];
	int bufsize;
	size_t offset, align;
	off_t savedoffset;

	if (size != ph_size) {
		if (file_printf(ms, ", corrupted program header size") == -1)
		    return -1;
		return 0;
	}
	if (lseek(fd, off, SEEK_SET) == (off_t)-1) {
		file_badseek(ms);
		return -1;
	}

  	for ( ; num; num--) {
  		if (read(fd, ph_addr, ph_size) == -1) {
  			file_badread(ms);
			return -1;
		}
		if ((savedoffset = lseek(fd, (off_t)0, SEEK_CUR)) == (off_t)-1) {
  			file_badseek(ms);
			return -1;
		}

		switch (ph_type) {
		case PT_DYNAMIC:
			linking_style = "dynamically";
			break;
		case PT_INTERP:
			shared_libraries = " (uses shared libs)";
			break;
		case PT_NOTE:
			if ((align = ph_align) & 0x80000000) {
				if (file_printf(ms, 
				    ", invalid note alignment 0x%lx",
				    (unsigned long)align) == -1)
					return -1;
				align = 4;
			}
			/*
			 * This is a PT_NOTE section; loop through all the notes
			 * in the section.
			 */
			if (lseek(fd, (off_t) ph_offset, SEEK_SET)
			    == (off_t)-1) {
				file_badseek(ms);
				return -1;
			}
			bufsize = read(fd, nbuf, ((ph_filesz < sizeof(nbuf)) ?
			    ph_filesz : sizeof(nbuf)));
			if (bufsize == -1) {
				file_badread(ms);
				return -1;
			}
			offset = 0;
			for (;;) {
				if (offset >= (size_t)bufsize)
					break;
				offset = donote(ms, nbuf, offset,
				    (size_t)bufsize, class, swap, align);
				if (offset == 0)
					break;
			}
			if (lseek(fd, savedoffset, SEEK_SET) == (off_t)-1) {
				file_badseek(ms);
				return -1;
			}
			break;
		}
	}
	if (file_printf(ms, ", %s linked%s", linking_style, shared_libraries)
	    == -1)
	    return -1;
	return 0;
}


protected int
file_tryelf(struct magic_set *ms, int fd, const unsigned char *buf,
    size_t nbytes)
{
	union {
		int32_t l;
		char c[sizeof (int32_t)];
	} u;
	int class;
	int swap;

	/*
	 * If we cannot seek, it must be a pipe, socket or fifo.
	 */
	if((lseek(fd, (off_t)0, SEEK_SET) == (off_t)-1) && (errno == ESPIPE))
		fd = file_pipe2file(ms, fd, buf, nbytes);

	/*
	 * ELF executables have multiple section headers in arbitrary
	 * file locations and thus file(1) cannot determine it from easily.
	 * Instead we traverse thru all section headers until a symbol table
	 * one is found or else the binary is stripped.
	 */
	if (buf[EI_MAG0] != ELFMAG0
	    || (buf[EI_MAG1] != ELFMAG1 && buf[EI_MAG1] != OLFMAG1)
	    || buf[EI_MAG2] != ELFMAG2 || buf[EI_MAG3] != ELFMAG3)
	    return 0;


	class = buf[4];

	if (class == ELFCLASS32) {
		Elf32_Ehdr elfhdr;
		if (nbytes <= sizeof (Elf32_Ehdr))
			return 0;


		u.l = 1;
		(void) memcpy(&elfhdr, buf, sizeof elfhdr);
		swap = (u.c[sizeof(int32_t) - 1] + 1) != elfhdr.e_ident[5];

		if (getu16(swap, elfhdr.e_type) == ET_CORE) {
#ifdef ELFCORE
			if (dophn_core(ms, class, swap, fd,
			    (off_t)getu32(swap, elfhdr.e_phoff),
			    getu16(swap, elfhdr.e_phnum), 
			    (size_t)getu16(swap, elfhdr.e_phentsize)) == -1)
				return -1;
#else
			;
#endif
		} else {
			if (getu16(swap, elfhdr.e_type) == ET_EXEC) {
				if (dophn_exec(ms, class, swap,
				    fd, (off_t)getu32(swap, elfhdr.e_phoff),
				    getu16(swap, elfhdr.e_phnum), 
				    (size_t)getu16(swap, elfhdr.e_phentsize))
				    == -1)
					return -1;
			}
			if (doshn(ms, class, swap, fd,
			    (off_t)getu32(swap, elfhdr.e_shoff),
			    getu16(swap, elfhdr.e_shnum),
			    (size_t)getu16(swap, elfhdr.e_shentsize)) == -1)
				return -1;
		}
		return 1;
	}

        if (class == ELFCLASS64) {
		Elf64_Ehdr elfhdr;
		if (nbytes <= sizeof (Elf64_Ehdr))
			return 0;


		u.l = 1;
		(void) memcpy(&elfhdr, buf, sizeof elfhdr);
		swap = (u.c[sizeof(int32_t) - 1] + 1) != elfhdr.e_ident[5];

		if (getu16(swap, elfhdr.e_type) == ET_CORE) {
#ifdef ELFCORE
			if (dophn_core(ms, class, swap, fd,
#ifdef USE_ARRAY_FOR_64BIT_TYPES
			    (off_t)getu32(swap, elfhdr.e_phoff[1]),
#else
			    (off_t)getu64(swap, elfhdr.e_phoff),
#endif
			    getu16(swap, elfhdr.e_phnum), 
			    (size_t)getu16(swap, elfhdr.e_phentsize)) == -1)
				return -1;
#else
			;
#endif
		} else {
			if (getu16(swap, elfhdr.e_type) == ET_EXEC) {
				if (dophn_exec(ms, class, swap, fd,
#ifdef USE_ARRAY_FOR_64BIT_TYPES
				    (off_t)getu32(swap, elfhdr.e_phoff[1]),
#else
				    (off_t)getu64(swap, elfhdr.e_phoff),
#endif
				    getu16(swap, elfhdr.e_phnum), 
				    (size_t)getu16(swap, elfhdr.e_phentsize))
				    == -1)
					return -1;
			}
			if (doshn(ms, class, swap, fd,
#ifdef USE_ARRAY_FOR_64BIT_TYPES
			    (off_t)getu32(swap, elfhdr.e_shoff[1]),
#else
			    (off_t)getu64(swap, elfhdr.e_shoff),
#endif
			    getu16(swap, elfhdr.e_shnum),
			    (size_t)getu16(swap, elfhdr.e_shentsize)) == -1)
				return -1;
		}
		return 1;
	}
	return 0;
}
#endif
