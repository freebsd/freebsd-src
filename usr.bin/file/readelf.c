
#ifndef lint
static const char rcsid[] =
  "$FreeBSD: src/usr.bin/file/readelf.c,v 1.6 1999/08/28 01:01:02 peter Exp $";
#endif /* not lint */

#ifdef BUILTIN_ELF
#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#include "readelf.h"
#include "file.h"

static void doshn __P((int, off_t, int, size_t, char *));
static void dophn_exec __P((int, off_t, int, size_t, char *));
static void dophn_core __P((int, off_t, int, size_t, char *));

static void
doshn(fd, off, num, size, buf)
	int fd;
	off_t off;
	int num;
	size_t size;
	char *buf;
{
	/*
	 * This works for both 32-bit and 64-bit ELF formats,
	 * because it looks only at the "sh_type" field, which is
	 * always 32 bits, and is preceded only by the "sh_name"
	 * field which is also always 32 bits, and because it uses
	 * the shdr size from the ELF header rather than using
	 * the size of an "Elf32_Shdr".
	 */
	Elf32_Shdr *sh = (Elf32_Shdr *) buf;

	if (lseek(fd, off, SEEK_SET) == -1)
		err(1, "lseek failed");

	for ( ; num; num--) {
		if (read(fd, buf, size) == -1)
			err(1, "read failed");
		if (sh->sh_type == SHT_SYMTAB) {
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
dophn_exec(fd, off, num, size, buf)
	int fd;
	off_t off;
	int num;
	size_t size;
	char *buf;
{
	/* I am not sure if this works for 64 bit elf formats */
	Elf32_Phdr *ph = (Elf32_Phdr *) buf;

	if (lseek(fd, off, SEEK_SET) == -1)
		err(1, "lseek failed");

  	for ( ; num; num--) {
  		if (read(fd, buf, size) == -1)
  			err(1, "read failed");
		if (ph->p_type == PT_INTERP) {
			/*
			 * Has an interpreter - must be a dynamically-linked
			 * executable.
			 */
			printf(", dynamically linked");
			return;
		}
	}
	printf(", statically linked");
}

size_t	prpsoffsets[] = {
	8,		/* FreeBSD */
	100,		/* SunOS 5.x */
	32,		/* Linux */
};

#define	NOFFSETS	(sizeof prpsoffsets / sizeof prpsoffsets[0])

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
 */
static void
dophn_core(fd, off, num, size, buf)
	int fd;
	off_t off;
	int num;
	size_t size;
	char *buf;
{
	/*
	 * This doesn't work for 64-bit ELF, as the "p_offset" field is
	 * 64 bits in 64-bit ELF.
	 */
	/*
	 * This doesn't work for 64-bit ELF, as the "p_offset" field is
	 * 64 bits in 64-bit ELF.
	 */
	Elf32_Phdr *ph = (Elf32_Phdr *) buf;
	Elf32_Nhdr *nh;
	size_t offset, noffset, reloffset;
	unsigned char c;
	int i, j;
	char nbuf[BUFSIZ];
	int bufsize;

	for ( ; num; num--) {
		if (lseek(fd, off, SEEK_SET) == -1)
			err(1, "lseek failed");
		if (read(fd, buf, size) == -1)
			err(1, "read failed");
		off += size;
		if (ph->p_type != PT_NOTE)
			continue;
		if (lseek(fd, ph->p_offset, SEEK_SET) == -1)
			err(1, "lseek failed");
		bufsize = read(fd, nbuf, BUFSIZ);
		if (bufsize == -1)
			err(1, "read failed");
		offset = 0;
		for (;;) {
			if (offset >= bufsize)
				break;
			nh = (Elf32_Nhdr *)&nbuf[offset];
			offset += sizeof *nh;

			/*
			 * If this note isn't an NT_PRPSINFO note, it's
			 * not what we're looking for.
			 */
			if (nh->n_type != NT_PRPSINFO) {
				offset += nh->n_namesz;
				offset = ((offset + 3)/4)*4;
				offset += nh->n_descsz;
				offset = ((offset + 3)/4)*4;
				continue;
			}

			/*
			 * Make sure this note has the name "CORE" or
			 * "FreeBSD".
			 */
			if (offset + nh->n_namesz >= bufsize) {
				/*
				 * We're past the end of the buffer.
				 */
				break;
			}
			if (nbuf[offset + nh->n_namesz - 1] != '\0' ||
			    (strcmp(&nbuf[offset], "CORE") != 0 &&
			    strcmp(&nbuf[offset], "FreeBSD") != 0))
				continue;
			offset += nh->n_namesz;
			offset = ((offset + 3)/4)*4;

			/*
			 * Extract the program name.  We assume it to be
			 * 16 characters (that's what it is in SunOS 5.x
			 * and Linux).
			 *
			 * Unfortunately, it's at a different offset in
			 * SunOS 5.x and Linux, so try multiple offsets.
			 * If the characters aren't all printable, reject
			 * it.
			 */
			for (i = 0; i < NOFFSETS; i++) {
				reloffset = prpsoffsets[i];
				noffset = offset + reloffset;
				for (j = 0; j < 16;
				    j++, noffset++, reloffset++) {
					/*
					 * Make sure we're not past the end
					 * of the buffer; if we are, just
					 * give up.
					 */
					if (noffset >= bufsize)
						return;

					/*
					 * Make sure we're not past the
					 * end of the contents; if we
					 * are, this obviously isn't
					 * the right offset.
					 */
					if (reloffset >= nh->n_descsz)
						goto tryanother;

					c = nbuf[noffset];
					if (c != '\0' && !isprint(c))
						goto tryanother;
				}

				/*
				 * Well, that worked.
				 */
				printf(", from '%.16s'",
				    &nbuf[offset + prpsoffsets[i]]);
				return;

			tryanother:
				;
			}
			offset += nh->n_descsz;
			offset = ((offset + 3)/4)*4;
		}
	}
}

void
tryelf(fd, buf, nbytes)
	int fd;
	char *buf;
	int nbytes;
{
	union {
		int32 l;
		char c[sizeof (int32)];
	} u;

	/*
	 * ELF executables have multiple section headers in arbitrary
	 * file locations and thus file(1) cannot determine it from easily.
	 * Instead we traverse thru all section headers until a symbol table
	 * one is found or else the binary is stripped.
	 */
	if (buf[EI_MAG0] != ELFMAG0 || buf[EI_MAG1] != ELFMAG1
	    || buf[EI_MAG2] != ELFMAG2 || buf[EI_MAG3] != ELFMAG3)
	    return;


	if (buf[4] == ELFCLASS32) {
		Elf32_Ehdr elfhdr;
		if (nbytes <= sizeof (Elf32_Ehdr))
			return;


		u.l = 1;
		(void) memcpy(&elfhdr, buf, sizeof elfhdr);
		/*
		 * If the system byteorder does not equal the
		 * object byteorder then don't test.
		 * XXX - we could conceivably fix up the "dophn_XXX()" and
		 * "doshn()" routines to extract stuff in the right
		 * byte order....
		 */
		if ((u.c[sizeof(long) - 1] + 1) == elfhdr.e_ident[5]) {
			if (elfhdr.e_type == ET_CORE) 
				dophn_core(fd, elfhdr.e_phoff, elfhdr.e_phnum, 
				      elfhdr.e_phentsize, buf);
			else {
				if (elfhdr.e_type == ET_EXEC) {
					dophn_exec(fd, elfhdr.e_phoff,
					    elfhdr.e_phnum, 
					      elfhdr.e_phentsize, buf);
				}
				doshn(fd, elfhdr.e_shoff, elfhdr.e_shnum,
				      elfhdr.e_shentsize, buf);
			}
		}
		return;
	}

        if (buf[4] == ELFCLASS64) {
		Elf64_Ehdr elfhdr;
		if (nbytes <= sizeof (Elf64_Ehdr))
			return;


		u.l = 1;
		(void) memcpy(&elfhdr, buf, sizeof elfhdr);

		/*
		 * If the system byteorder does not equal the
		 * object byteorder then don't test.
		 * XXX - we could conceivably fix up the "dophn_XXX()" and
		 * "doshn()" routines to extract stuff in the right
		 * byte order....
		 */
		if ((u.c[sizeof(long) - 1] + 1) == elfhdr.e_ident[5]) {
#ifdef notyet
			if (elfhdr.e_type == ET_CORE) 
				dophn_core(fd, elfhdr.e_phoff, elfhdr.e_phnum, 
				      elfhdr.e_phentsize, buf);
			else
#endif
			{
#ifdef notyet
				if (elfhdr.e_type == ET_EXEC) {
					dophn_exec(fd, elfhdr.e_phoff,
					    elfhdr.e_phnum, 
					      elfhdr.e_phentsize, buf);
				}
#endif
				doshn(fd, elfhdr.e_shoff, elfhdr.e_shnum,
				      elfhdr.e_shentsize, buf);
			}
		}
		return;
	}
}
#endif
