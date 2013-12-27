/*-
 * Copyright (c) 2006-2008 Joseph Koshy
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/mman.h>
#include <sys/param.h>

#include <assert.h>
#include <errno.h>
#include <gelf.h>
#include <libelf.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "_libelf.h"

/*
 * Layout strategy:
 *
 * - Case 1: ELF_F_LAYOUT is asserted
 *     In this case the application has full control over where the
 *     section header table, program header table, and section data
 *     will reside.   The library only perform error checks.
 *
 * - Case 2: ELF_F_LAYOUT is not asserted
 *
 *     The library will do the object layout using the following
 *     ordering:
 *     - The executable header is placed first, are required by the
 *     	 ELF specification.
 *     - The program header table is placed immediately following the
 *       executable header.
 *     - Section data, if any, is placed after the program header
 *       table, aligned appropriately.
 *     - The section header table, if needed, is placed last.
 *
 *     There are two sub-cases to be taken care of:
 *
 *     - Case 2a: e->e_cmd == ELF_C_READ or ELF_C_RDWR
 *
 *       In this sub-case, the underlying ELF object may already have
 *       content in it, which the application may have modified.  The
 *       library will retrieve content from the existing object as
 *       needed.
 *
 *     - Case 2b: e->e_cmd == ELF_C_WRITE
 *
 *       The ELF object is being created afresh in this sub-case;
 *       there is no pre-existing content in the underlying ELF
 *       object.
 */

/*
 * Compute the extents of a section, by looking at the data
 * descriptors associated with it.  The function returns 1 if
 * successful, or zero if an error was detected.
 */
static int
_libelf_compute_section_extents(Elf *e, Elf_Scn *s, off_t rc)
{
	int ec;
	size_t fsz, msz;
	Elf_Data *d;
	Elf32_Shdr *shdr32;
	Elf64_Shdr *shdr64;
	unsigned int elftype;
	uint32_t sh_type;
	uint64_t d_align;
	uint64_t sh_align, sh_entsize, sh_offset, sh_size;
	uint64_t scn_size, scn_alignment;

	ec = e->e_class;

	shdr32 = &s->s_shdr.s_shdr32;
	shdr64 = &s->s_shdr.s_shdr64;
	if (ec == ELFCLASS32) {
		sh_type    = shdr32->sh_type;
		sh_align   = (uint64_t) shdr32->sh_addralign;
		sh_entsize = (uint64_t) shdr32->sh_entsize;
		sh_offset  = (uint64_t) shdr32->sh_offset;
		sh_size    = (uint64_t) shdr32->sh_size;
	} else {
		sh_type    = shdr64->sh_type;
		sh_align   = shdr64->sh_addralign;
		sh_entsize = shdr64->sh_entsize;
		sh_offset  = shdr64->sh_offset;
		sh_size    = shdr64->sh_size;
	}

	assert(sh_type != SHT_NULL && sh_type != SHT_NOBITS);

	elftype = _libelf_xlate_shtype(sh_type);
	if (elftype > ELF_T_LAST) {
		LIBELF_SET_ERROR(SECTION, 0);
		return (0);
	}

	if (sh_align == 0)
		sh_align = _libelf_falign(elftype, ec);

	/*
	 * Check the section's data buffers for sanity and compute the
	 * section's alignment.
	 * Compute the section's size and alignment using the data
	 * descriptors associated with the section.
	 */
	if (STAILQ_EMPTY(&s->s_data)) {
		/*
		 * The section's content (if any) has not been read in
		 * yet.  If section is not dirty marked dirty, we can
		 * reuse the values in the 'sh_size' and 'sh_offset'
		 * fields of the section header.
		 */
		if ((s->s_flags & ELF_F_DIRTY) == 0) {
			/*
			 * If the library is doing the layout, then we
			 * compute the new start offset for the
			 * section based on the current offset and the
			 * section's alignment needs.
			 *
			 * If the application is doing the layout, we
			 * can use the value in the 'sh_offset' field
			 * in the section header directly.
			 */
			if (e->e_flags & ELF_F_LAYOUT)
				goto updatedescriptor;
			else
				goto computeoffset;
		}

		/*
		 * Otherwise, we need to bring in the section's data
		 * from the underlying ELF object.
		 */
		if (e->e_cmd != ELF_C_WRITE && elf_getdata(s, NULL) == NULL)
			return (0);
	}

	/*
	 * Loop through the section's data descriptors.
	 */
	scn_size = 0L;
	scn_alignment = 0L;
	STAILQ_FOREACH(d, &s->s_data, d_next)  {
		if (d->d_type > ELF_T_LAST) {
			LIBELF_SET_ERROR(DATA, 0);
			return (0);
		}
		if (d->d_version != e->e_version) {
			LIBELF_SET_ERROR(VERSION, 0);
			return (0);
		}
		if ((d_align = d->d_align) == 0 || (d_align & (d_align - 1))) {
			LIBELF_SET_ERROR(DATA, 0);
			return (0);
		}

		/*
		 * The buffer's size should be a multiple of the
		 * memory size of the underlying type.
		 */
		msz = _libelf_msize(d->d_type, ec, e->e_version);
		if (d->d_size % msz) {
			LIBELF_SET_ERROR(DATA, 0);
			return (0);
		}

		/*
		 * Compute the section's size.
		 */
		if (e->e_flags & ELF_F_LAYOUT) {
			if ((uint64_t) d->d_off + d->d_size > scn_size)
				scn_size = d->d_off + d->d_size;
		} else {
			scn_size = roundup2(scn_size, d->d_align);
			d->d_off = scn_size;
			fsz = _libelf_fsize(d->d_type, ec, d->d_version,
			    d->d_size / msz);
			scn_size += fsz;
		}

		/*
		 * The section's alignment is the maximum alignment
		 * needed for its data buffers.
		 */
		if (d_align > scn_alignment)
			scn_alignment = d_align;
	}


	/*
	 * If the application is requesting full control over the layout
	 * of the section, check its values for sanity.
	 */
	if (e->e_flags & ELF_F_LAYOUT) {
		if (scn_alignment > sh_align || sh_offset % sh_align ||
		    sh_size < scn_size) {
			LIBELF_SET_ERROR(LAYOUT, 0);
			return (0);
		}
		goto updatedescriptor;
	}

	/*
	 * Otherwise compute the values in the section header.
	 *
	 * The section alignment is the maximum alignment for any of
	 * its contained data descriptors.
	 */
	if (scn_alignment > sh_align)
		sh_align = scn_alignment;

	/*
	 * If the section entry size is zero, try and fill in an
	 * appropriate entry size.  Per the elf(5) manual page
	 * sections without fixed-size entries should have their
	 * 'sh_entsize' field set to zero.
	 */
	if (sh_entsize == 0 &&
	    (sh_entsize = _libelf_fsize(elftype, ec, e->e_version,
	    (size_t) 1)) == 1)
		sh_entsize = 0;

	sh_size = scn_size;

computeoffset:
	/*
	 * Compute the new offset for the section based on
	 * the section's alignment needs.
	 */
	sh_offset = roundup(rc, sh_align);

	/*
	 * Update the section header.
	 */
	if (ec == ELFCLASS32) {
		shdr32->sh_addralign = (uint32_t) sh_align;
		shdr32->sh_entsize   = (uint32_t) sh_entsize;
		shdr32->sh_offset    = (uint32_t) sh_offset;
		shdr32->sh_size      = (uint32_t) sh_size;
	} else {
		shdr64->sh_addralign = sh_align;
		shdr64->sh_entsize   = sh_entsize;
		shdr64->sh_offset    = sh_offset;
		shdr64->sh_size      = sh_size;
	}

updatedescriptor:
	/*
	 * Update the section descriptor.
	 */
	s->s_size = sh_size;
	s->s_offset = sh_offset;

	return (1);
}


/*
 * Insert a section in ascending order in the list
 */

static int
_libelf_insert_section(Elf *e, Elf_Scn *s)
{
	Elf_Scn *t, *prevt;
	uint64_t smax, smin, tmax, tmin;

	smin = s->s_offset;
	smax = smin + s->s_size;

	prevt = NULL;
	STAILQ_FOREACH(t, &e->e_u.e_elf.e_scn, s_next) {
		tmin = t->s_offset;
		tmax = tmin + t->s_size;

		if (tmax <= smin) {
			/*
			 * 't' lies entirely before 's': ...| t |...| s |...
			 */
			prevt = t;
			continue;
		} else if (smax <= tmin)
			/*
			 * 's' lies entirely before 't', and after 'prevt':
			 *      ...| prevt |...| s |...| t |...
			 */
			break;
		else {	/* 's' and 't' overlap. */
			LIBELF_SET_ERROR(LAYOUT, 0);
			return (0);
		}
	}

	if (prevt)
		STAILQ_INSERT_AFTER(&e->e_u.e_elf.e_scn, prevt, s, s_next);
	else
		STAILQ_INSERT_HEAD(&e->e_u.e_elf.e_scn, s, s_next);
	return (1);
}

/*
 * Recompute section layout.
 */

static off_t
_libelf_resync_sections(Elf *e, off_t rc)
{
	int ec;
	Elf_Scn *s;
	size_t sh_type, shdr_start, shdr_end;

	ec = e->e_class;

	/*
	 * Make a pass through sections, computing the extent of each
	 * section. Order in increasing order of addresses.
	 */
	STAILQ_FOREACH(s, &e->e_u.e_elf.e_scn, s_next) {
		if (ec == ELFCLASS32)
			sh_type = s->s_shdr.s_shdr32.sh_type;
		else
			sh_type = s->s_shdr.s_shdr64.sh_type;

		if (sh_type == SHT_NOBITS || sh_type == SHT_NULL)
			continue;

		if (_libelf_compute_section_extents(e, s, rc) == 0)
			return ((off_t) -1);

		if (s->s_size == 0)
			continue;

		if (s->s_offset + s->s_size < (size_t) rc) {
			/*
			 * Try insert this section in the
			 * correct place in the list,
			 * detecting overlaps if any.
			 */
			STAILQ_REMOVE(&e->e_u.e_elf.e_scn, s, _Elf_Scn,
			    s_next);
			if (_libelf_insert_section(e, s) == 0)
				return ((off_t) -1);
		} else
			rc = s->s_offset + s->s_size;
	}

	/*
	 * If the application is controlling file layout, check for an
	 * overlap between this section's extents and the SHDR table.
	 */
	if (e->e_flags & ELF_F_LAYOUT) {

		if (e->e_class == ELFCLASS32)
			shdr_start = e->e_u.e_elf.e_ehdr.e_ehdr32->e_shoff;
		else
			shdr_start = e->e_u.e_elf.e_ehdr.e_ehdr64->e_shoff;

		shdr_end = shdr_start + _libelf_fsize(ELF_T_SHDR, e->e_class,
		    e->e_version, e->e_u.e_elf.e_nscn);

		STAILQ_FOREACH(s, &e->e_u.e_elf.e_scn, s_next) {
			if (s->s_offset >= shdr_end ||
			    s->s_offset + s->s_size <= shdr_start)
				continue;
			LIBELF_SET_ERROR(LAYOUT, 0);
			return ((off_t) -1);
		}
	}

	return (rc);
}

static off_t
_libelf_resync_elf(Elf *e)
{
	int ec, eh_class, eh_type;
	unsigned int eh_byteorder, eh_version;
	size_t align, fsz;
	size_t phnum, shnum;
	off_t rc, phoff, shoff;
	void *ehdr;
	Elf32_Ehdr *eh32;
	Elf64_Ehdr *eh64;

	rc = 0;

	ec = e->e_class;

	assert(ec == ELFCLASS32 || ec == ELFCLASS64);

	/*
	 * Prepare the EHDR.
	 */
	if ((ehdr = _libelf_ehdr(e, ec, 0)) == NULL)
		return ((off_t) -1);

	eh32 = ehdr;
	eh64 = ehdr;

	if (ec == ELFCLASS32) {
		eh_byteorder = eh32->e_ident[EI_DATA];
		eh_class     = eh32->e_ident[EI_CLASS];
		phoff        = (uint64_t) eh32->e_phoff;
		shoff        = (uint64_t) eh32->e_shoff;
		eh_type      = eh32->e_type;
		eh_version   = eh32->e_version;
	} else {
		eh_byteorder = eh64->e_ident[EI_DATA];
		eh_class     = eh64->e_ident[EI_CLASS];
		phoff        = eh64->e_phoff;
		shoff        = eh64->e_shoff;
		eh_type      = eh64->e_type;
		eh_version   = eh64->e_version;
	}

	if (eh_version == EV_NONE)
		eh_version = EV_CURRENT;

	if (eh_version != e->e_version) {	/* always EV_CURRENT */
		LIBELF_SET_ERROR(VERSION, 0);
		return ((off_t) -1);
	}

	if (eh_class != e->e_class) {
		LIBELF_SET_ERROR(CLASS, 0);
		return ((off_t) -1);
	}

	if (e->e_cmd != ELF_C_WRITE && eh_byteorder != e->e_byteorder) {
		LIBELF_SET_ERROR(HEADER, 0);
		return ((off_t) -1);
	}

	shnum = e->e_u.e_elf.e_nscn;
	phnum = e->e_u.e_elf.e_nphdr;

	e->e_byteorder = eh_byteorder;

#define	INITIALIZE_EHDR(E,EC,V)	do {					\
		(E)->e_ident[EI_MAG0] = ELFMAG0;			\
		(E)->e_ident[EI_MAG1] = ELFMAG1;			\
		(E)->e_ident[EI_MAG2] = ELFMAG2;			\
		(E)->e_ident[EI_MAG3] = ELFMAG3;			\
		(E)->e_ident[EI_CLASS] = (EC);				\
		(E)->e_ident[EI_VERSION] = (V);				\
		(E)->e_ehsize = _libelf_fsize(ELF_T_EHDR, (EC), (V),	\
		    (size_t) 1);					\
		(E)->e_phentsize = (phnum == 0) ? 0 : _libelf_fsize(	\
		    ELF_T_PHDR, (EC), (V), (size_t) 1);			\
		(E)->e_shentsize = _libelf_fsize(ELF_T_SHDR, (EC), (V),	\
		    (size_t) 1);					\
	} while (0)

	if (ec == ELFCLASS32)
		INITIALIZE_EHDR(eh32, ec, eh_version);
	else
		INITIALIZE_EHDR(eh64, ec, eh_version);

	(void) elf_flagehdr(e, ELF_C_SET, ELF_F_DIRTY);

	rc += _libelf_fsize(ELF_T_EHDR, ec, eh_version, (size_t) 1);

	/*
	 * Compute the layout the program header table, if one is
	 * present.  The program header table needs to be aligned to a
	 * `natural' boundary.
	 */
	if (phnum) {
		fsz = _libelf_fsize(ELF_T_PHDR, ec, eh_version, phnum);
		align = _libelf_falign(ELF_T_PHDR, ec);

		if (e->e_flags & ELF_F_LAYOUT) {
			/*
			 * Check offsets for sanity.
			 */
			if (rc > phoff) {
				LIBELF_SET_ERROR(HEADER, 0);
				return ((off_t) -1);
			}

			if (phoff % align) {
				LIBELF_SET_ERROR(LAYOUT, 0);
				return ((off_t) -1);
			}

		} else
			phoff = roundup(rc, align);

		rc = phoff + fsz;
	} else
		phoff = 0;

	/*
	 * Compute the layout of the sections associated with the
	 * file.
	 */

	if (e->e_cmd != ELF_C_WRITE &&
	    (e->e_flags & LIBELF_F_SHDRS_LOADED) == 0 &&
	    _libelf_load_scn(e, ehdr) == 0)
		return ((off_t) -1);

	if ((rc = _libelf_resync_sections(e, rc)) < 0)
		return ((off_t) -1);

	/*
	 * Compute the space taken up by the section header table, if
	 * one is needed.  If ELF_F_LAYOUT is asserted, the
	 * application may have placed the section header table in
	 * between existing sections, so the net size of the file need
	 * not increase due to the presence of the section header
	 * table.
	 */
	if (shnum) {
		fsz = _libelf_fsize(ELF_T_SHDR, ec, eh_version, (size_t) 1);
		align = _libelf_falign(ELF_T_SHDR, ec);

		if (e->e_flags & ELF_F_LAYOUT) {
			if (shoff % align) {
				LIBELF_SET_ERROR(LAYOUT, 0);
				return ((off_t) -1);
			}
		} else
			shoff = roundup(rc, align);

		if (shoff + fsz * shnum > (size_t) rc)
			rc = shoff + fsz * shnum;
	} else
		shoff = 0;

	/*
	 * Set the fields of the Executable Header that could potentially use
	 * extended numbering.
	 */
	_libelf_setphnum(e, ehdr, ec, phnum);
	_libelf_setshnum(e, ehdr, ec, shnum);

	/*
	 * Update the `e_phoff' and `e_shoff' fields if the library is
	 * doing the layout.
	 */
	if ((e->e_flags & ELF_F_LAYOUT) == 0) {
		if (ec == ELFCLASS32) {
			eh32->e_phoff = (uint32_t) phoff;
			eh32->e_shoff = (uint32_t) shoff;
		} else {
			eh64->e_phoff = (uint64_t) phoff;
			eh64->e_shoff = (uint64_t) shoff;
		}
	}

	return (rc);
}

/*
 * Write out the contents of a section.
 */

static off_t
_libelf_write_scn(Elf *e, char *nf, Elf_Scn *s, off_t rc)
{
	int ec;
	size_t fsz, msz, nobjects;
	uint32_t sh_type;
	uint64_t sh_off, sh_size;
	int elftype;
	Elf_Data *d, dst;

	if ((ec = e->e_class) == ELFCLASS32) {
		sh_type = s->s_shdr.s_shdr32.sh_type;
		sh_size = (uint64_t) s->s_shdr.s_shdr32.sh_size;
	} else {
		sh_type = s->s_shdr.s_shdr64.sh_type;
		sh_size = s->s_shdr.s_shdr64.sh_size;
	}

	/*
	 * Ignore sections that do not allocate space in the file.
	 */
	if (sh_type == SHT_NOBITS || sh_type == SHT_NULL || sh_size == 0)
		return (rc);

	elftype = _libelf_xlate_shtype(sh_type);
	assert(elftype >= ELF_T_FIRST && elftype <= ELF_T_LAST);

	sh_off = s->s_offset;
	assert(sh_off % _libelf_falign(elftype, ec) == 0);

	/*
	 * If the section has a `rawdata' descriptor, and the section
	 * contents have not been modified, use its contents directly.
	 * The `s_rawoff' member contains the offset into the original
	 * file, while `s_offset' contains its new location in the
	 * destination.
	 */

	if (STAILQ_EMPTY(&s->s_data)) {

		if ((d = elf_rawdata(s, NULL)) == NULL)
			return ((off_t) -1);

		STAILQ_FOREACH(d, &s->s_rawdata, d_next) {
			if ((uint64_t) rc < sh_off + d->d_off)
				(void) memset(nf + rc,
				    LIBELF_PRIVATE(fillchar), sh_off +
				    d->d_off - rc);
			rc = sh_off + d->d_off;

			assert(d->d_buf != NULL);
			assert(d->d_type == ELF_T_BYTE);
			assert(d->d_version == e->e_version);

			(void) memcpy(nf + rc,
			    e->e_rawfile + s->s_rawoff + d->d_off, d->d_size);

			rc += d->d_size;
		}

		return (rc);
	}

	/*
	 * Iterate over the set of data descriptors for this section.
	 * The prior call to _libelf_resync_elf() would have setup the
	 * descriptors for this step.
	 */

	dst.d_version = e->e_version;

	STAILQ_FOREACH(d, &s->s_data, d_next) {

		msz = _libelf_msize(d->d_type, ec, e->e_version);

		if ((uint64_t) rc < sh_off + d->d_off)
			(void) memset(nf + rc,
			    LIBELF_PRIVATE(fillchar), sh_off + d->d_off - rc);

		rc = sh_off + d->d_off;

		assert(d->d_buf != NULL);
		assert(d->d_version == e->e_version);
		assert(d->d_size % msz == 0);

		nobjects = d->d_size / msz;

		fsz = _libelf_fsize(d->d_type, ec, e->e_version, nobjects);

		dst.d_buf    = nf + rc;
		dst.d_size   = fsz;

		if (_libelf_xlate(&dst, d, e->e_byteorder, ec, ELF_TOFILE) ==
		    NULL)
			return ((off_t) -1);

		rc += fsz;
	}

	return ((off_t) rc);
}

/*
 * Write out the file image.
 *
 * The original file could have been mapped in with an ELF_C_RDWR
 * command and the application could have added new content or
 * re-arranged its sections before calling elf_update().  Consequently
 * its not safe to work `in place' on the original file.  So we
 * malloc() the required space for the updated ELF object and build
 * the object there and write it out to the underlying file at the
 * end.  Note that the application may have opened the underlying file
 * in ELF_C_RDWR and only retrieved/modified a few sections.  We take
 * care to avoid translating file sections unnecessarily.
 *
 * Gaps in the coverage of the file by the file's sections will be
 * filled with the fill character set by elf_fill(3).
 */

static off_t
_libelf_write_elf(Elf *e, off_t newsize)
{
	int ec;
	off_t maxrc, rc;
	size_t fsz, msz, phnum, shnum;
	uint64_t phoff, shoff;
	void *ehdr;
	char *newfile;
	Elf_Data dst, src;
	Elf_Scn *scn, *tscn;
	Elf32_Ehdr *eh32;
	Elf64_Ehdr *eh64;

	assert(e->e_kind == ELF_K_ELF);
	assert(e->e_cmd != ELF_C_READ);
	assert(e->e_fd >= 0);

	if ((newfile = malloc((size_t) newsize)) == NULL) {
		LIBELF_SET_ERROR(RESOURCE, errno);
		return ((off_t) -1);
	}

	ec = e->e_class;

	ehdr = _libelf_ehdr(e, ec, 0);
	assert(ehdr != NULL);

	phnum = e->e_u.e_elf.e_nphdr;

	if (ec == ELFCLASS32) {
		eh32 = (Elf32_Ehdr *) ehdr;

		phoff = (uint64_t) eh32->e_phoff;
		shnum = eh32->e_shnum;
		shoff = (uint64_t) eh32->e_shoff;
	} else {
		eh64 = (Elf64_Ehdr *) ehdr;

		phoff = eh64->e_phoff;
		shnum = eh64->e_shnum;
		shoff = eh64->e_shoff;
	}

	fsz = _libelf_fsize(ELF_T_EHDR, ec, e->e_version, (size_t) 1);
	msz = _libelf_msize(ELF_T_EHDR, ec, e->e_version);

	(void) memset(&dst, 0, sizeof(dst));
	(void) memset(&src, 0, sizeof(src));

	src.d_buf     = ehdr;
	src.d_size    = msz;
	src.d_type    = ELF_T_EHDR;
	src.d_version = dst.d_version = e->e_version;

	rc = 0;

	dst.d_buf     = newfile + rc;
	dst.d_size    = fsz;

	if (_libelf_xlate(&dst, &src, e->e_byteorder, ec, ELF_TOFILE) ==
	    NULL)
		goto error;

	rc += fsz;

	/*
	 * Write the program header table if present.
	 */

	if (phnum != 0 && phoff != 0) {
		assert((unsigned) rc <= phoff);

		fsz = _libelf_fsize(ELF_T_PHDR, ec, e->e_version, phnum);

		assert(phoff % _libelf_falign(ELF_T_PHDR, ec) == 0);
		assert(fsz > 0);

		src.d_buf = _libelf_getphdr(e, ec);
		src.d_version = dst.d_version = e->e_version;
		src.d_type = ELF_T_PHDR;
		src.d_size = phnum * _libelf_msize(ELF_T_PHDR, ec,
		    e->e_version);

		dst.d_size = fsz;

		if ((uint64_t) rc < phoff)
			(void) memset(newfile + rc,
			    LIBELF_PRIVATE(fillchar), phoff - rc);

		dst.d_buf = newfile + rc;

		if (_libelf_xlate(&dst, &src, e->e_byteorder, ec, ELF_TOFILE) ==
		    NULL)
			goto error;

		rc = phoff + fsz;
	}

	/*
	 * Write out individual sections.
	 */

	STAILQ_FOREACH(scn, &e->e_u.e_elf.e_scn, s_next)
 		if ((rc = _libelf_write_scn(e, newfile, scn, rc)) < 0)
			goto error;

	/*
	 * Write out the section header table, if required.  Note that
	 * if flag ELF_F_LAYOUT has been set the section header table
	 * could reside in between byte ranges mapped by section
	 * descriptors.
	 */
	if (shnum != 0 && shoff != 0) {
		if ((uint64_t) rc < shoff)
			(void) memset(newfile + rc,
			    LIBELF_PRIVATE(fillchar), shoff - rc);

		maxrc = rc;
		rc = shoff;

		assert(rc % _libelf_falign(ELF_T_SHDR, ec) == 0);

		src.d_type = ELF_T_SHDR;
		src.d_size = _libelf_msize(ELF_T_SHDR, ec, e->e_version);
		src.d_version = dst.d_version = e->e_version;

		fsz = _libelf_fsize(ELF_T_SHDR, ec, e->e_version, (size_t) 1);

		STAILQ_FOREACH(scn, &e->e_u.e_elf.e_scn, s_next) {
			if (ec == ELFCLASS32)
				src.d_buf = &scn->s_shdr.s_shdr32;
			else
				src.d_buf = &scn->s_shdr.s_shdr64;

			dst.d_size = fsz;
			dst.d_buf = newfile + rc + scn->s_ndx * fsz;

			if (_libelf_xlate(&dst, &src, e->e_byteorder, ec,
				ELF_TOFILE) != &dst)
				goto error;
		}

		rc += e->e_u.e_elf.e_nscn * fsz;
		if (maxrc > rc)
			rc = maxrc;
	}

	assert(rc == newsize);

	/*
	 * Write out the constructed contents and remap the file in
	 * read-only.
	 */

	if (e->e_rawfile && munmap(e->e_rawfile, e->e_rawsize) < 0) {
		LIBELF_SET_ERROR(IO, errno);
		goto error;
	}

	if (write(e->e_fd, newfile, (size_t) newsize) != newsize ||
	    lseek(e->e_fd, (off_t) 0, SEEK_SET) < 0) {
		LIBELF_SET_ERROR(IO, errno);
		goto error;
	}

	if (e->e_cmd != ELF_C_WRITE) {
		if ((e->e_rawfile = mmap(NULL, (size_t) newsize, PROT_READ,
		    MAP_PRIVATE, e->e_fd, (off_t) 0)) == MAP_FAILED) {
			LIBELF_SET_ERROR(IO, errno);
			goto error;
		}
		e->e_rawsize = newsize;
	}

	/*
	 * Reset flags, remove existing section descriptors and
	 * {E,P}HDR pointers so that a subsequent elf_get{e,p}hdr()
	 * and elf_getscn() will function correctly.
	 */

	e->e_flags &= ~ELF_F_DIRTY;

	STAILQ_FOREACH_SAFE(scn, &e->e_u.e_elf.e_scn, s_next, tscn)
		_libelf_release_scn(scn);

	if (ec == ELFCLASS32) {
		free(e->e_u.e_elf.e_ehdr.e_ehdr32);
		if (e->e_u.e_elf.e_phdr.e_phdr32)
			free(e->e_u.e_elf.e_phdr.e_phdr32);

		e->e_u.e_elf.e_ehdr.e_ehdr32 = NULL;
		e->e_u.e_elf.e_phdr.e_phdr32 = NULL;
	} else {
		free(e->e_u.e_elf.e_ehdr.e_ehdr64);
		if (e->e_u.e_elf.e_phdr.e_phdr64)
			free(e->e_u.e_elf.e_phdr.e_phdr64);

		e->e_u.e_elf.e_ehdr.e_ehdr64 = NULL;
		e->e_u.e_elf.e_phdr.e_phdr64 = NULL;
	}

	free(newfile);

	return (rc);

 error:
	free(newfile);

	return ((off_t) -1);
}

off_t
elf_update(Elf *e, Elf_Cmd c)
{
	int ec;
	off_t rc;

	rc = (off_t) -1;

	if (e == NULL || e->e_kind != ELF_K_ELF ||
	    (c != ELF_C_NULL && c != ELF_C_WRITE)) {
		LIBELF_SET_ERROR(ARGUMENT, 0);
		return (rc);
	}

	if ((ec = e->e_class) != ELFCLASS32 && ec != ELFCLASS64) {
		LIBELF_SET_ERROR(CLASS, 0);
		return (rc);
	}

	if (e->e_version == EV_NONE)
		e->e_version = EV_CURRENT;

	if (c == ELF_C_WRITE && e->e_cmd == ELF_C_READ) {
		LIBELF_SET_ERROR(MODE, 0);
		return (rc);
	}

	if ((rc = _libelf_resync_elf(e)) < 0)
		return (rc);

	if (c == ELF_C_NULL)
		return (rc);

	if (e->e_cmd == ELF_C_READ) {
		/*
		 * This descriptor was opened in read-only mode or by
		 * elf_memory().
		 */
		if (e->e_fd)
			LIBELF_SET_ERROR(MODE, 0);
		else
			LIBELF_SET_ERROR(ARGUMENT, 0);
		return ((off_t) -1);
	}

	if (e->e_fd < 0) {
		LIBELF_SET_ERROR(SEQUENCE, 0);
		return ((off_t) -1);
	}

	return (_libelf_write_elf(e, rc));
}
