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
FILE_RCSID("@(#)$Id: readelf.c,v 1.22 2002/07/03 18:26:38 christos Exp $")
#endif

#ifdef	ELFCORE
static void dophn_core(int, int, int, off_t, int, size_t);
#endif
static void dophn_exec(int, int, int, off_t, int, size_t);
static void doshn(int, int, int, off_t, int, size_t);

static uint16_t getu16(int, uint16_t);
static uint32_t getu32(int, uint32_t);
static uint64_t getu64(int, uint64_t);

static uint16_t
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

static uint32_t
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

static uint64_t
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
#define shs_type	(class == ELFCLASS32		\
			 ? getu32(swap, sh32.sh_type)	\
			 : getu32(swap, sh64.sh_type))
#define ph_addr		(class == ELFCLASS32		\
			 ? (void *) &ph32		\
			 : (void *) &ph64)
#define ph_type		(class == ELFCLASS32		\
			 ? getu32(swap, ph32.p_type)	\
			 : getu32(swap, ph64.p_type))
#define ph_offset	(class == ELFCLASS32		\
			 ? getu32(swap, ph32.p_offset)	\
			 : getu64(swap, ph64.p_offset))
#define nh_size		(class == ELFCLASS32		\
			 ? sizeof *nh32			\
			 : sizeof *nh64)
#define nh_type		(class == ELFCLASS32		\
			 ? getu32(swap, nh32->n_type)	\
			 : getu32(swap, nh64->n_type))
#define nh_namesz	(class == ELFCLASS32		\
			 ? getu32(swap, nh32->n_namesz)	\
			 : getu32(swap, nh64->n_namesz))
#define nh_descsz	(class == ELFCLASS32		\
			 ? getu32(swap, nh32->n_descsz)	\
			 : getu32(swap, nh64->n_descsz))
#define prpsoffsets(i)	(class == ELFCLASS32		\
			 ? prpsoffsets32[i]		\
			 : prpsoffsets64[i])

static void
doshn(int class, int swap, int fd, off_t off, int num, size_t size)
{
	Elf32_Shdr sh32;
	Elf64_Shdr sh64;

	if (lseek(fd, off, SEEK_SET) == -1)
		error("lseek failed (%s).\n", strerror(errno));

	for ( ; num; num--) {
		if (read(fd, sh_addr, size) == -1)
			error("read failed (%s).\n", strerror(errno));
		if (shs_type == SHT_SYMTAB /* || shs_type == SHT_DYNSYM */) {
			(void) printf (", not stripped");
			return;
		}
	}
	(void) printf (", stripped");
}

/*
 * Look through the program headers of an executable image, searching
 * for a PT_INTERP section; if one is found, it's dynamically linked,
 * otherwise it's statically linked.
 */
static void
dophn_exec(int class, int swap, int fd, off_t off, int num, size_t size)
{
	Elf32_Phdr ph32;
	Elf32_Nhdr *nh32 = NULL;
	Elf64_Phdr ph64;
	Elf64_Nhdr *nh64 = NULL;
	char *linking_style = "statically";
	char *shared_libraries = "";
	char nbuf[BUFSIZ];
	int bufsize;
	size_t offset, nameoffset;

	if (lseek(fd, off, SEEK_SET) == -1)
		error("lseek failed (%s).\n", strerror(errno));

  	for ( ; num; num--) {
  		if (read(fd, ph_addr, size) == -1)
  			error("read failed (%s).\n", strerror(errno));

		switch (ph_type) {
		case PT_DYNAMIC:
			linking_style = "dynamically";
			break;
		case PT_INTERP:
			shared_libraries = " (uses shared libs)";
			break;
		case PT_NOTE:
			/*
			 * This is a PT_NOTE section; loop through all the notes
			 * in the section.
			 */
			if (lseek(fd, (off_t) ph_offset, SEEK_SET) == -1)
				error("lseek failed (%s).\n", strerror(errno));
			bufsize = read(fd, nbuf, BUFSIZ);
			if (bufsize == -1)
				error(": " "read failed (%s).\n",
				    strerror(errno));
			offset = 0;
			for (;;) {
				if (offset >= bufsize)
					break;
				if (class == ELFCLASS32)
					nh32 = (Elf32_Nhdr *)&nbuf[offset];
				else
					nh64 = (Elf64_Nhdr *)&nbuf[offset];
				offset += nh_size;
	
				if (offset + nh_namesz >= bufsize) {
					/*
					 * We're past the end of the buffer.
					 */
					break;
				}

				nameoffset = offset;
				offset += nh_namesz;
				offset = ((offset + 3)/4)*4;

				if (offset + nh_descsz >= bufsize)
					break;

				if (nh_namesz == 4 &&
				    strcmp(&nbuf[nameoffset], "GNU") == 0 &&
				    nh_type == NT_GNU_VERSION &&
				    nh_descsz == 16) {
					uint32_t *desc =
					    (uint32_t *)&nbuf[offset];

					printf(", for GNU/");
					switch (getu32(swap, desc[0])) {
					case GNU_OS_LINUX:
						printf("Linux");
						break;
					case GNU_OS_HURD:
						printf("Hurd");
						break;
					case GNU_OS_SOLARIS:
						printf("Solaris");
						break;
					default:
						printf("<unknown>");
					}
					printf(" %d.%d.%d",
					    getu32(swap, desc[1]),
					    getu32(swap, desc[2]),
					    getu32(swap, desc[3]));
				}

				if (nh_namesz == 7 &&
				    strcmp(&nbuf[nameoffset], "NetBSD") == 0 &&
				    nh_type == NT_NETBSD_VERSION &&
				    nh_descsz == 4) {
					printf(", for NetBSD");
					/*
					 * Version number is stuck at 199905,
					 * and hence is basically content-free.
					 */
				}

				if (nh_namesz == 8 &&
				    strcmp(&nbuf[nameoffset], "FreeBSD") == 0 &&
				    nh_type == NT_FREEBSD_VERSION &&
				    nh_descsz == 4) {
					uint32_t desc = getu32(swap,
					    *(uint32_t *)&nbuf[offset]);
					printf(", for FreeBSD");
					/*
					 * Contents is __FreeBSD_version,
					 * whose relation to OS versions is
					 * defined by a huge table in the
					 * Porters' Handbook.  Happily, the
					 * first three digits are the version
					 * number, at least in versions of
					 * FreeBSD that use this note.
					 */

					printf(" %d.%d", desc / 100000,
					    desc / 10000 % 10);
					if (desc / 1000 % 10 > 0)
						printf(".%d",
						    desc / 1000 % 10);
				}

				if (nh_namesz == 8 &&
				    strcmp(&nbuf[nameoffset], "OpenBSD") == 0 &&
				    nh_type == NT_OPENBSD_VERSION &&
				    nh_descsz == 4) {
					printf(", for OpenBSD");
					/* Content of note is always 0 */
				}
			}
			break;
		}
	}
	printf(", %s linked%s", linking_style, shared_libraries);
}

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

static const char *os_style_names[] = {
	"SVR4",
	"FreeBSD",
	"NetBSD",
};

static void
dophn_core(int class, int swap, int fd, off_t off, int num, size_t size)
{
	Elf32_Phdr ph32;
	Elf32_Nhdr *nh32 = NULL;
	Elf64_Phdr ph64;
	Elf64_Nhdr *nh64 = NULL;
	size_t offset, nameoffset, noffset, reloffset;
	unsigned char c;
	int i, j;
	char nbuf[BUFSIZ];
	int bufsize;
	int os_style = -1;

	/*
	 * Loop through all the program headers.
	 */
	for ( ; num; num--) {
		if (lseek(fd, off, SEEK_SET) == -1)
			error("lseek failed (%s).\n", strerror(errno));
		if (read(fd, ph_addr, size) == -1)
			error("read failed (%s).\n", strerror(errno));
		off += size;
		if (ph_type != PT_NOTE)
			continue;

		/*
		 * This is a PT_NOTE section; loop through all the notes
		 * in the section.
		 */
		if (lseek(fd, (off_t) ph_offset, SEEK_SET) == -1)
			error("lseek failed (%s).\n", strerror(errno));
		bufsize = read(fd, nbuf, BUFSIZ);
		if (bufsize == -1)
			error(": " "read failed (%s).\n", strerror(errno));
		offset = 0;
		for (;;) {
			if (offset >= bufsize)
				break;
			if (class == ELFCLASS32)
				nh32 = (Elf32_Nhdr *)&nbuf[offset];
			else
				nh64 = (Elf64_Nhdr *)&nbuf[offset];
			offset += nh_size;

			/*
			 * Check whether this note has the name "CORE" or
			 * "FreeBSD", or "NetBSD-CORE".
			 */
			if (offset + nh_namesz >= bufsize) {
				/*
				 * We're past the end of the buffer.
				 */
				break;
			}

			nameoffset = offset;
			offset += nh_namesz;
			offset = ((offset + 3)/4)*4;

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
			if (os_style == -1) {
				if ((nh_namesz == 4 &&
				     strncmp(&nbuf[nameoffset],
					    "CORE", 4) == 0) ||
				    (nh_namesz == 5 &&
				     strcmp(&nbuf[nameoffset],
				     	    "CORE") == 0)) {
					os_style = OS_STYLE_SVR4;
				} else
				if ((nh_namesz == 8 &&
				     strcmp(&nbuf[nameoffset],
				     	    "FreeBSD") == 0)) {
					os_style = OS_STYLE_FREEBSD;
				} else
				if ((nh_namesz >= 11 &&
				     strncmp(&nbuf[nameoffset],
				     	     "NetBSD-CORE", 11) == 0)) {
					os_style = OS_STYLE_NETBSD;
				} else
					continue;
				printf(", %s-style", os_style_names[os_style]);
			}

			if (os_style == OS_STYLE_NETBSD &&
			    nh_type == NT_NETBSD_CORE_PROCINFO) {
				uint32_t signo;

				/*
				 * Extract the program name.  It is at
				 * offset 0x7c, and is up to 32-bytes,
				 * including the terminating NUL.
				 */
				printf(", from '%.31s'", &nbuf[offset + 0x7c]);
				
				/*
				 * Extract the signal number.  It is at
				 * offset 0x08.
				 */
				memcpy(&signo, &nbuf[offset + 0x08],
				    sizeof(signo));
				printf(" (signal %u)", getu32(swap, signo));
			} else
			if (os_style != OS_STYLE_NETBSD &&
			    nh_type == NT_PRPSINFO) {
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
					reloffset = prpsoffsets(i);
					noffset = offset + reloffset;
					for (j = 0; j < 16;
					    j++, noffset++, reloffset++) {
						/*
						 * Make sure we're not past
						 * the end of the buffer; if
						 * we are, just give up.
						 */
						if (noffset >= bufsize)
							goto tryanother;

						/*
						 * Make sure we're not past
						 * the end of the contents;
						 * if we are, this obviously
						 * isn't the right offset.
						 */
						if (reloffset >= nh_descsz)
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
							if (!isprint(c) ||
							     isquote(c))
								goto tryanother;
						}
					}

					/*
					 * Well, that worked.
					 */
					printf(", from '%.16s'",
					    &nbuf[offset + prpsoffsets(i)]);
					break;

				tryanother:
					;
				}
				break;
			}
			offset += nh_descsz;
			offset = ((offset + 3)/4)*4;
		}
	}
}
#endif

void
tryelf(int fd, unsigned char *buf, int nbytes)
{
	union {
		int32_t l;
		char c[sizeof (int32_t)];
	} u;
	int class;
	int swap;

	/*
	 * If we can't seek, it must be a pipe, socket or fifo.
	 */
	if((lseek(fd, (off_t)0, SEEK_SET) == (off_t)-1) && (errno == ESPIPE))
		fd = pipe2file(fd, buf, nbytes);

	/*
	 * ELF executables have multiple section headers in arbitrary
	 * file locations and thus file(1) cannot determine it from easily.
	 * Instead we traverse thru all section headers until a symbol table
	 * one is found or else the binary is stripped.
	 */
	if (buf[EI_MAG0] != ELFMAG0
	    || (buf[EI_MAG1] != ELFMAG1 && buf[EI_MAG1] != OLFMAG1)
	    || buf[EI_MAG2] != ELFMAG2 || buf[EI_MAG3] != ELFMAG3)
	    return;


	class = buf[4];

	if (class == ELFCLASS32) {
		Elf32_Ehdr elfhdr;
		if (nbytes <= sizeof (Elf32_Ehdr))
			return;


		u.l = 1;
		(void) memcpy(&elfhdr, buf, sizeof elfhdr);
		swap = (u.c[sizeof(int32_t) - 1] + 1) != elfhdr.e_ident[5];

		if (getu16(swap, elfhdr.e_type) == ET_CORE) 
#ifdef ELFCORE
			dophn_core(class, swap,
				   fd,
				   getu32(swap, elfhdr.e_phoff),
				   getu16(swap, elfhdr.e_phnum), 
				   getu16(swap, elfhdr.e_phentsize));
#else
			;
#endif
		else {
			if (getu16(swap, elfhdr.e_type) == ET_EXEC) {
				dophn_exec(class, swap,
					   fd,
					   getu32(swap, elfhdr.e_phoff),
					   getu16(swap, elfhdr.e_phnum), 
					   getu16(swap, elfhdr.e_phentsize));
			}
			doshn(class, swap,
			      fd,
			      getu32(swap, elfhdr.e_shoff),
			      getu16(swap, elfhdr.e_shnum),
			      getu16(swap, elfhdr.e_shentsize));
		}
		return;
	}

        if (class == ELFCLASS64) {
		Elf64_Ehdr elfhdr;
		if (nbytes <= sizeof (Elf64_Ehdr))
			return;


		u.l = 1;
		(void) memcpy(&elfhdr, buf, sizeof elfhdr);
		swap = (u.c[sizeof(int32_t) - 1] + 1) != elfhdr.e_ident[5];

		if (getu16(swap, elfhdr.e_type) == ET_CORE) 
#ifdef ELFCORE
			dophn_core(class, swap,
				   fd,
#ifdef USE_ARRAY_FOR_64BIT_TYPES
				   getu32(swap, elfhdr.e_phoff[1]),
#else
				   getu64(swap, elfhdr.e_phoff),
#endif
				   getu16(swap, elfhdr.e_phnum), 
				   getu16(swap, elfhdr.e_phentsize));
#else
			;
#endif
		else
		{
			if (getu16(swap, elfhdr.e_type) == ET_EXEC) {
				dophn_exec(class, swap,
					   fd,
#ifdef USE_ARRAY_FOR_64BIT_TYPES
					   getu32(swap, elfhdr.e_phoff[1]),
#else
					   getu64(swap, elfhdr.e_phoff),
#endif
					   getu16(swap, elfhdr.e_phnum), 
					   getu16(swap, elfhdr.e_phentsize));
			}
			doshn(class, swap,
			      fd,
#ifdef USE_ARRAY_FOR_64BIT_TYPES
			      getu32(swap, elfhdr.e_shoff[1]),
#else
			      getu64(swap, elfhdr.e_shoff),
#endif
			      getu16(swap, elfhdr.e_shnum),
			      getu16(swap, elfhdr.e_shentsize));
		}
		return;
	}
}
#endif
