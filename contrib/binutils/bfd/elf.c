/* ELF executable support for BFD.
   Copyright 1993, 94, 95, 96, 97, 98, 99, 2000 Free Software Foundation, Inc.

This file is part of BFD, the Binary File Descriptor library.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

/*

SECTION
	ELF backends

	BFD support for ELF formats is being worked on.
	Currently, the best supported back ends are for sparc and i386
	(running svr4 or Solaris 2).

	Documentation of the internals of the support code still needs
	to be written.  The code is changing quickly enough that we
	haven't bothered yet.
 */

#include "bfd.h"
#include "sysdep.h"
#include "bfdlink.h"
#include "libbfd.h"
#define ARCH_SIZE 0
#include "elf-bfd.h"

static INLINE struct elf_segment_map *make_mapping
  PARAMS ((bfd *, asection **, unsigned int, unsigned int, boolean));
static boolean map_sections_to_segments PARAMS ((bfd *));
static int elf_sort_sections PARAMS ((const PTR, const PTR));
static boolean assign_file_positions_for_segments PARAMS ((bfd *));
static boolean assign_file_positions_except_relocs PARAMS ((bfd *));
static boolean prep_headers PARAMS ((bfd *));
static boolean swap_out_syms PARAMS ((bfd *, struct bfd_strtab_hash **, int));
static boolean copy_private_bfd_data PARAMS ((bfd *, bfd *));
static char *elf_read PARAMS ((bfd *, long, unsigned int));
static void elf_fake_sections PARAMS ((bfd *, asection *, PTR));
static boolean assign_section_numbers PARAMS ((bfd *));
static INLINE int sym_is_global PARAMS ((bfd *, asymbol *));
static boolean elf_map_symbols PARAMS ((bfd *));
static bfd_size_type get_program_header_size PARAMS ((bfd *));
static boolean elfcore_read_notes PARAMS ((bfd *, bfd_vma, bfd_vma));

/* Swap version information in and out.  The version information is
   currently size independent.  If that ever changes, this code will
   need to move into elfcode.h.  */

/* Swap in a Verdef structure.  */

void
_bfd_elf_swap_verdef_in (abfd, src, dst)
     bfd *abfd;
     const Elf_External_Verdef *src;
     Elf_Internal_Verdef *dst;
{
  dst->vd_version = bfd_h_get_16 (abfd, src->vd_version);
  dst->vd_flags   = bfd_h_get_16 (abfd, src->vd_flags);
  dst->vd_ndx     = bfd_h_get_16 (abfd, src->vd_ndx);
  dst->vd_cnt     = bfd_h_get_16 (abfd, src->vd_cnt);
  dst->vd_hash    = bfd_h_get_32 (abfd, src->vd_hash);
  dst->vd_aux     = bfd_h_get_32 (abfd, src->vd_aux);
  dst->vd_next    = bfd_h_get_32 (abfd, src->vd_next);
}

/* Swap out a Verdef structure.  */

void
_bfd_elf_swap_verdef_out (abfd, src, dst)
     bfd *abfd;
     const Elf_Internal_Verdef *src;
     Elf_External_Verdef *dst;
{
  bfd_h_put_16 (abfd, src->vd_version, dst->vd_version);
  bfd_h_put_16 (abfd, src->vd_flags, dst->vd_flags);
  bfd_h_put_16 (abfd, src->vd_ndx, dst->vd_ndx);
  bfd_h_put_16 (abfd, src->vd_cnt, dst->vd_cnt);
  bfd_h_put_32 (abfd, src->vd_hash, dst->vd_hash);
  bfd_h_put_32 (abfd, src->vd_aux, dst->vd_aux);
  bfd_h_put_32 (abfd, src->vd_next, dst->vd_next);
}

/* Swap in a Verdaux structure.  */

void
_bfd_elf_swap_verdaux_in (abfd, src, dst)
     bfd *abfd;
     const Elf_External_Verdaux *src;
     Elf_Internal_Verdaux *dst;
{
  dst->vda_name = bfd_h_get_32 (abfd, src->vda_name);
  dst->vda_next = bfd_h_get_32 (abfd, src->vda_next);
}

/* Swap out a Verdaux structure.  */

void
_bfd_elf_swap_verdaux_out (abfd, src, dst)
     bfd *abfd;
     const Elf_Internal_Verdaux *src;
     Elf_External_Verdaux *dst;
{
  bfd_h_put_32 (abfd, src->vda_name, dst->vda_name);
  bfd_h_put_32 (abfd, src->vda_next, dst->vda_next);
}

/* Swap in a Verneed structure.  */

void
_bfd_elf_swap_verneed_in (abfd, src, dst)
     bfd *abfd;
     const Elf_External_Verneed *src;
     Elf_Internal_Verneed *dst;
{
  dst->vn_version = bfd_h_get_16 (abfd, src->vn_version);
  dst->vn_cnt     = bfd_h_get_16 (abfd, src->vn_cnt);
  dst->vn_file    = bfd_h_get_32 (abfd, src->vn_file);
  dst->vn_aux     = bfd_h_get_32 (abfd, src->vn_aux);
  dst->vn_next    = bfd_h_get_32 (abfd, src->vn_next);
}

/* Swap out a Verneed structure.  */

void
_bfd_elf_swap_verneed_out (abfd, src, dst)
     bfd *abfd;
     const Elf_Internal_Verneed *src;
     Elf_External_Verneed *dst;
{
  bfd_h_put_16 (abfd, src->vn_version, dst->vn_version);
  bfd_h_put_16 (abfd, src->vn_cnt, dst->vn_cnt);
  bfd_h_put_32 (abfd, src->vn_file, dst->vn_file);
  bfd_h_put_32 (abfd, src->vn_aux, dst->vn_aux);
  bfd_h_put_32 (abfd, src->vn_next, dst->vn_next);
}

/* Swap in a Vernaux structure.  */

void
_bfd_elf_swap_vernaux_in (abfd, src, dst)
     bfd *abfd;
     const Elf_External_Vernaux *src;
     Elf_Internal_Vernaux *dst;
{
  dst->vna_hash  = bfd_h_get_32 (abfd, src->vna_hash);
  dst->vna_flags = bfd_h_get_16 (abfd, src->vna_flags);
  dst->vna_other = bfd_h_get_16 (abfd, src->vna_other);
  dst->vna_name  = bfd_h_get_32 (abfd, src->vna_name);
  dst->vna_next  = bfd_h_get_32 (abfd, src->vna_next);
}

/* Swap out a Vernaux structure.  */

void
_bfd_elf_swap_vernaux_out (abfd, src, dst)
     bfd *abfd;
     const Elf_Internal_Vernaux *src;
     Elf_External_Vernaux *dst;
{
  bfd_h_put_32 (abfd, src->vna_hash, dst->vna_hash);
  bfd_h_put_16 (abfd, src->vna_flags, dst->vna_flags);
  bfd_h_put_16 (abfd, src->vna_other, dst->vna_other);
  bfd_h_put_32 (abfd, src->vna_name, dst->vna_name);
  bfd_h_put_32 (abfd, src->vna_next, dst->vna_next);
}

/* Swap in a Versym structure.  */

void
_bfd_elf_swap_versym_in (abfd, src, dst)
     bfd *abfd;
     const Elf_External_Versym *src;
     Elf_Internal_Versym *dst;
{
  dst->vs_vers = bfd_h_get_16 (abfd, src->vs_vers);
}

/* Swap out a Versym structure.  */

void
_bfd_elf_swap_versym_out (abfd, src, dst)
     bfd *abfd;
     const Elf_Internal_Versym *src;
     Elf_External_Versym *dst;
{
  bfd_h_put_16 (abfd, src->vs_vers, dst->vs_vers);
}

/* Standard ELF hash function.  Do not change this function; you will
   cause invalid hash tables to be generated.  */

unsigned long
bfd_elf_hash (namearg)
     const char *namearg;
{
  const unsigned char *name = (const unsigned char *) namearg;
  unsigned long h = 0;
  unsigned long g;
  int ch;

  while ((ch = *name++) != '\0')
    {
      h = (h << 4) + ch;
      if ((g = (h & 0xf0000000)) != 0)
	{
	  h ^= g >> 24;
	  /* The ELF ABI says `h &= ~g', but this is equivalent in
	     this case and on some machines one insn instead of two.  */
	  h ^= g;
	}
    }
  return h;
}

/* Read a specified number of bytes at a specified offset in an ELF
   file, into a newly allocated buffer, and return a pointer to the
   buffer. */

static char *
elf_read (abfd, offset, size)
     bfd * abfd;
     long offset;
     unsigned int size;
{
  char *buf;

  if ((buf = bfd_alloc (abfd, size)) == NULL)
    return NULL;
  if (bfd_seek (abfd, offset, SEEK_SET) == -1)
    return NULL;
  if (bfd_read ((PTR) buf, size, 1, abfd) != size)
    {
      if (bfd_get_error () != bfd_error_system_call)
	bfd_set_error (bfd_error_file_truncated);
      return NULL;
    }
  return buf;
}

boolean
bfd_elf_mkobject (abfd)
     bfd * abfd;
{
  /* this just does initialization */
  /* coff_mkobject zalloc's space for tdata.coff_obj_data ... */
  elf_tdata (abfd) = (struct elf_obj_tdata *)
    bfd_zalloc (abfd, sizeof (struct elf_obj_tdata));
  if (elf_tdata (abfd) == 0)
    return false;
  /* since everything is done at close time, do we need any
     initialization? */

  return true;
}

boolean
bfd_elf_mkcorefile (abfd)
     bfd * abfd;
{
  /* I think this can be done just like an object file. */
  return bfd_elf_mkobject (abfd);
}

char *
bfd_elf_get_str_section (abfd, shindex)
     bfd * abfd;
     unsigned int shindex;
{
  Elf_Internal_Shdr **i_shdrp;
  char *shstrtab = NULL;
  unsigned int offset;
  unsigned int shstrtabsize;

  i_shdrp = elf_elfsections (abfd);
  if (i_shdrp == 0 || i_shdrp[shindex] == 0)
    return 0;

  shstrtab = (char *) i_shdrp[shindex]->contents;
  if (shstrtab == NULL)
    {
      /* No cached one, attempt to read, and cache what we read. */
      offset = i_shdrp[shindex]->sh_offset;
      shstrtabsize = i_shdrp[shindex]->sh_size;
      shstrtab = elf_read (abfd, offset, shstrtabsize);
      i_shdrp[shindex]->contents = (PTR) shstrtab;
    }
  return shstrtab;
}

char *
bfd_elf_string_from_elf_section (abfd, shindex, strindex)
     bfd * abfd;
     unsigned int shindex;
     unsigned int strindex;
{
  Elf_Internal_Shdr *hdr;

  if (strindex == 0)
    return "";

  hdr = elf_elfsections (abfd)[shindex];

  if (hdr->contents == NULL
      && bfd_elf_get_str_section (abfd, shindex) == NULL)
    return NULL;

  if (strindex >= hdr->sh_size)
    {
      (*_bfd_error_handler)
	(_("%s: invalid string offset %u >= %lu for section `%s'"),
	 bfd_get_filename (abfd), strindex, (unsigned long) hdr->sh_size,
	 ((shindex == elf_elfheader(abfd)->e_shstrndx
	   && strindex == hdr->sh_name)
	  ? ".shstrtab"
	  : elf_string_from_elf_strtab (abfd, hdr->sh_name)));
      return "";
    }

  return ((char *) hdr->contents) + strindex;
}

/* Make a BFD section from an ELF section.  We store a pointer to the
   BFD section in the bfd_section field of the header.  */

boolean
_bfd_elf_make_section_from_shdr (abfd, hdr, name)
     bfd *abfd;
     Elf_Internal_Shdr *hdr;
     const char *name;
{
  asection *newsect;
  flagword flags;

  if (hdr->bfd_section != NULL)
    {
      BFD_ASSERT (strcmp (name,
			  bfd_get_section_name (abfd, hdr->bfd_section)) == 0);
      return true;
    }

  newsect = bfd_make_section_anyway (abfd, name);
  if (newsect == NULL)
    return false;

  newsect->filepos = hdr->sh_offset;

  if (! bfd_set_section_vma (abfd, newsect, hdr->sh_addr)
      || ! bfd_set_section_size (abfd, newsect, hdr->sh_size)
      || ! bfd_set_section_alignment (abfd, newsect,
				      bfd_log2 (hdr->sh_addralign)))
    return false;

  flags = SEC_NO_FLAGS;
  if (hdr->sh_type != SHT_NOBITS)
    flags |= SEC_HAS_CONTENTS;
  if ((hdr->sh_flags & SHF_ALLOC) != 0)
    {
      flags |= SEC_ALLOC;
      if (hdr->sh_type != SHT_NOBITS)
	flags |= SEC_LOAD;
    }
  if ((hdr->sh_flags & SHF_WRITE) == 0)
    flags |= SEC_READONLY;
  if ((hdr->sh_flags & SHF_EXECINSTR) != 0)
    flags |= SEC_CODE;
  else if ((flags & SEC_LOAD) != 0)
    flags |= SEC_DATA;

  /* The debugging sections appear to be recognized only by name, not
     any sort of flag.  */
  if (strncmp (name, ".debug", sizeof ".debug" - 1) == 0
      || strncmp (name, ".line", sizeof ".line" - 1) == 0
      || strncmp (name, ".stab", sizeof ".stab" - 1) == 0)
    flags |= SEC_DEBUGGING;

  /* As a GNU extension, if the name begins with .gnu.linkonce, we
     only link a single copy of the section.  This is used to support
     g++.  g++ will emit each template expansion in its own section.
     The symbols will be defined as weak, so that multiple definitions
     are permitted.  The GNU linker extension is to actually discard
     all but one of the sections.  */
  if (strncmp (name, ".gnu.linkonce", sizeof ".gnu.linkonce" - 1) == 0)
    flags |= SEC_LINK_ONCE | SEC_LINK_DUPLICATES_DISCARD;

  if (! bfd_set_section_flags (abfd, newsect, flags))
    return false;

  if ((flags & SEC_ALLOC) != 0)
    {
      Elf_Internal_Phdr *phdr;
      unsigned int i;

      /* Look through the phdrs to see if we need to adjust the lma.
         If all the p_paddr fields are zero, we ignore them, since
         some ELF linkers produce such output.  */
      phdr = elf_tdata (abfd)->phdr;
      for (i = 0; i < elf_elfheader (abfd)->e_phnum; i++, phdr++)
	{
	  if (phdr->p_paddr != 0)
	    break;
	}
      if (i < elf_elfheader (abfd)->e_phnum)
	{
	  phdr = elf_tdata (abfd)->phdr;
	  for (i = 0; i < elf_elfheader (abfd)->e_phnum; i++, phdr++)
	    {
	      if (phdr->p_type == PT_LOAD
		  && phdr->p_vaddr != phdr->p_paddr
		  && phdr->p_vaddr <= hdr->sh_addr
		  && (phdr->p_vaddr + phdr->p_memsz
		      >= hdr->sh_addr + hdr->sh_size)
		  && ((flags & SEC_LOAD) == 0
		      || (phdr->p_offset <= (bfd_vma) hdr->sh_offset
			  && (phdr->p_offset + phdr->p_filesz
			      >= hdr->sh_offset + hdr->sh_size))))
		{
		  newsect->lma += phdr->p_paddr - phdr->p_vaddr;
		  break;
		}
	    }
	}
    }

  hdr->bfd_section = newsect;
  elf_section_data (newsect)->this_hdr = *hdr;

  return true;
}

/*
INTERNAL_FUNCTION
	bfd_elf_find_section

SYNOPSIS
	struct elf_internal_shdr *bfd_elf_find_section (bfd *abfd, char *name);

DESCRIPTION
	Helper functions for GDB to locate the string tables.
	Since BFD hides string tables from callers, GDB needs to use an
	internal hook to find them.  Sun's .stabstr, in particular,
	isn't even pointed to by the .stab section, so ordinary
	mechanisms wouldn't work to find it, even if we had some.
*/

struct elf_internal_shdr *
bfd_elf_find_section (abfd, name)
     bfd * abfd;
     char *name;
{
  Elf_Internal_Shdr **i_shdrp;
  char *shstrtab;
  unsigned int max;
  unsigned int i;

  i_shdrp = elf_elfsections (abfd);
  if (i_shdrp != NULL)
    {
      shstrtab = bfd_elf_get_str_section
	(abfd, elf_elfheader (abfd)->e_shstrndx);
      if (shstrtab != NULL)
	{
	  max = elf_elfheader (abfd)->e_shnum;
	  for (i = 1; i < max; i++)
	    if (!strcmp (&shstrtab[i_shdrp[i]->sh_name], name))
	      return i_shdrp[i];
	}
    }
  return 0;
}

const char *const bfd_elf_section_type_names[] = {
  "SHT_NULL", "SHT_PROGBITS", "SHT_SYMTAB", "SHT_STRTAB",
  "SHT_RELA", "SHT_HASH", "SHT_DYNAMIC", "SHT_NOTE",
  "SHT_NOBITS", "SHT_REL", "SHT_SHLIB", "SHT_DYNSYM",
};

/* ELF relocs are against symbols.  If we are producing relocateable
   output, and the reloc is against an external symbol, and nothing
   has given us any additional addend, the resulting reloc will also
   be against the same symbol.  In such a case, we don't want to
   change anything about the way the reloc is handled, since it will
   all be done at final link time.  Rather than put special case code
   into bfd_perform_relocation, all the reloc types use this howto
   function.  It just short circuits the reloc if producing
   relocateable output against an external symbol.  */

/*ARGSUSED*/
bfd_reloc_status_type
bfd_elf_generic_reloc (abfd,
		       reloc_entry,
		       symbol,
		       data,
		       input_section,
		       output_bfd,
		       error_message)
     bfd *abfd ATTRIBUTE_UNUSED;
     arelent *reloc_entry;
     asymbol *symbol;
     PTR data ATTRIBUTE_UNUSED;
     asection *input_section;
     bfd *output_bfd;
     char **error_message ATTRIBUTE_UNUSED;
{
  if (output_bfd != (bfd *) NULL
      && (symbol->flags & BSF_SECTION_SYM) == 0
      && (! reloc_entry->howto->partial_inplace
	  || reloc_entry->addend == 0))
    {
      reloc_entry->address += input_section->output_offset;
      return bfd_reloc_ok;
    }

  return bfd_reloc_continue;
}

/* Print out the program headers.  */

boolean
_bfd_elf_print_private_bfd_data (abfd, farg)
     bfd *abfd;
     PTR farg;
{
  FILE *f = (FILE *) farg;
  Elf_Internal_Phdr *p;
  asection *s;
  bfd_byte *dynbuf = NULL;

  p = elf_tdata (abfd)->phdr;
  if (p != NULL)
    {
      unsigned int i, c;

      fprintf (f, _("\nProgram Header:\n"));
      c = elf_elfheader (abfd)->e_phnum;
      for (i = 0; i < c; i++, p++)
	{
	  const char *s;
	  char buf[20];

	  switch (p->p_type)
	    {
	    case PT_NULL: s = "NULL"; break;
	    case PT_LOAD: s = "LOAD"; break;
	    case PT_DYNAMIC: s = "DYNAMIC"; break;
	    case PT_INTERP: s = "INTERP"; break;
	    case PT_NOTE: s = "NOTE"; break;
	    case PT_SHLIB: s = "SHLIB"; break;
	    case PT_PHDR: s = "PHDR"; break;
	    default: sprintf (buf, "0x%lx", p->p_type); s = buf; break;
	    }
	  fprintf (f, "%8s off    0x", s);
	  fprintf_vma (f, p->p_offset);
	  fprintf (f, " vaddr 0x");
	  fprintf_vma (f, p->p_vaddr);
	  fprintf (f, " paddr 0x");
	  fprintf_vma (f, p->p_paddr);
	  fprintf (f, " align 2**%u\n", bfd_log2 (p->p_align));
	  fprintf (f, "         filesz 0x");
	  fprintf_vma (f, p->p_filesz);
	  fprintf (f, " memsz 0x");
	  fprintf_vma (f, p->p_memsz);
	  fprintf (f, " flags %c%c%c",
		   (p->p_flags & PF_R) != 0 ? 'r' : '-',
		   (p->p_flags & PF_W) != 0 ? 'w' : '-',
		   (p->p_flags & PF_X) != 0 ? 'x' : '-');
	  if ((p->p_flags &~ (PF_R | PF_W | PF_X)) != 0)
	    fprintf (f, " %lx", p->p_flags &~ (PF_R | PF_W | PF_X));
	  fprintf (f, "\n");
	}
    }

  s = bfd_get_section_by_name (abfd, ".dynamic");
  if (s != NULL)
    {
      int elfsec;
      unsigned long link;
      bfd_byte *extdyn, *extdynend;
      size_t extdynsize;
      void (*swap_dyn_in) PARAMS ((bfd *, const PTR, Elf_Internal_Dyn *));

      fprintf (f, _("\nDynamic Section:\n"));

      dynbuf = (bfd_byte *) bfd_malloc (s->_raw_size);
      if (dynbuf == NULL)
	goto error_return;
      if (! bfd_get_section_contents (abfd, s, (PTR) dynbuf, (file_ptr) 0,
				      s->_raw_size))
	goto error_return;

      elfsec = _bfd_elf_section_from_bfd_section (abfd, s);
      if (elfsec == -1)
	goto error_return;
      link = elf_elfsections (abfd)[elfsec]->sh_link;

      extdynsize = get_elf_backend_data (abfd)->s->sizeof_dyn;
      swap_dyn_in = get_elf_backend_data (abfd)->s->swap_dyn_in;

      extdyn = dynbuf;
      extdynend = extdyn + s->_raw_size;
      for (; extdyn < extdynend; extdyn += extdynsize)
	{
	  Elf_Internal_Dyn dyn;
	  const char *name;
	  char ab[20];
	  boolean stringp;

	  (*swap_dyn_in) (abfd, (PTR) extdyn, &dyn);

	  if (dyn.d_tag == DT_NULL)
	    break;

	  stringp = false;
	  switch (dyn.d_tag)
	    {
	    default:
	      sprintf (ab, "0x%lx", (unsigned long) dyn.d_tag);
	      name = ab;
	      break;

	    case DT_NEEDED: name = "NEEDED"; stringp = true; break;
	    case DT_PLTRELSZ: name = "PLTRELSZ"; break;
	    case DT_PLTGOT: name = "PLTGOT"; break;
	    case DT_HASH: name = "HASH"; break;
	    case DT_STRTAB: name = "STRTAB"; break;
	    case DT_SYMTAB: name = "SYMTAB"; break;
	    case DT_RELA: name = "RELA"; break;
	    case DT_RELASZ: name = "RELASZ"; break;
	    case DT_RELAENT: name = "RELAENT"; break;
	    case DT_STRSZ: name = "STRSZ"; break;
	    case DT_SYMENT: name = "SYMENT"; break;
	    case DT_INIT: name = "INIT"; break;
	    case DT_FINI: name = "FINI"; break;
	    case DT_SONAME: name = "SONAME"; stringp = true; break;
	    case DT_RPATH: name = "RPATH"; stringp = true; break;
	    case DT_SYMBOLIC: name = "SYMBOLIC"; break;
	    case DT_REL: name = "REL"; break;
	    case DT_RELSZ: name = "RELSZ"; break;
	    case DT_RELENT: name = "RELENT"; break;
	    case DT_PLTREL: name = "PLTREL"; break;
	    case DT_DEBUG: name = "DEBUG"; break;
	    case DT_TEXTREL: name = "TEXTREL"; break;
	    case DT_JMPREL: name = "JMPREL"; break;
	    case DT_AUXILIARY: name = "AUXILIARY"; stringp = true; break;
	    case DT_FILTER: name = "FILTER"; stringp = true; break;
	    case DT_VERSYM: name = "VERSYM"; break;
	    case DT_VERDEF: name = "VERDEF"; break;
	    case DT_VERDEFNUM: name = "VERDEFNUM"; break;
	    case DT_VERNEED: name = "VERNEED"; break;
	    case DT_VERNEEDNUM: name = "VERNEEDNUM"; break;
	    }

	  fprintf (f, "  %-11s ", name);
	  if (! stringp)
	    fprintf (f, "0x%lx", (unsigned long) dyn.d_un.d_val);
	  else
	    {
	      const char *string;

	      string = bfd_elf_string_from_elf_section (abfd, link,
							dyn.d_un.d_val);
	      if (string == NULL)
		goto error_return;
	      fprintf (f, "%s", string);
	    }
	  fprintf (f, "\n");
	}

      free (dynbuf);
      dynbuf = NULL;
    }

  if ((elf_dynverdef (abfd) != 0 && elf_tdata (abfd)->verdef == NULL)
      || (elf_dynverref (abfd) != 0 && elf_tdata (abfd)->verref == NULL))
    {
      if (! _bfd_elf_slurp_version_tables (abfd))
	return false;
    }

  if (elf_dynverdef (abfd) != 0)
    {
      Elf_Internal_Verdef *t;

      fprintf (f, _("\nVersion definitions:\n"));
      for (t = elf_tdata (abfd)->verdef; t != NULL; t = t->vd_nextdef)
	{
	  fprintf (f, "%d 0x%2.2x 0x%8.8lx %s\n", t->vd_ndx,
		   t->vd_flags, t->vd_hash, t->vd_nodename);
	  if (t->vd_auxptr->vda_nextptr != NULL)
	    {
	      Elf_Internal_Verdaux *a;

	      fprintf (f, "\t");
	      for (a = t->vd_auxptr->vda_nextptr;
		   a != NULL;
		   a = a->vda_nextptr)
		fprintf (f, "%s ", a->vda_nodename);
	      fprintf (f, "\n");
	    }
	}
    }

  if (elf_dynverref (abfd) != 0)
    {
      Elf_Internal_Verneed *t;

      fprintf (f, _("\nVersion References:\n"));
      for (t = elf_tdata (abfd)->verref; t != NULL; t = t->vn_nextref)
	{
	  Elf_Internal_Vernaux *a;

	  fprintf (f, _("  required from %s:\n"), t->vn_filename);
	  for (a = t->vn_auxptr; a != NULL; a = a->vna_nextptr)
	    fprintf (f, "    0x%8.8lx 0x%2.2x %2.2d %s\n", a->vna_hash,
		     a->vna_flags, a->vna_other, a->vna_nodename);
	}
    }

  return true;

 error_return:
  if (dynbuf != NULL)
    free (dynbuf);
  return false;
}

/* Display ELF-specific fields of a symbol.  */

void
bfd_elf_print_symbol (abfd, filep, symbol, how)
     bfd *abfd;
     PTR filep;
     asymbol *symbol;
     bfd_print_symbol_type how;
{
  FILE *file = (FILE *) filep;
  switch (how)
    {
    case bfd_print_symbol_name:
      fprintf (file, "%s", symbol->name);
      break;
    case bfd_print_symbol_more:
      fprintf (file, "elf ");
      fprintf_vma (file, symbol->value);
      fprintf (file, " %lx", (long) symbol->flags);
      break;
    case bfd_print_symbol_all:
      {
	CONST char *section_name;
	CONST char *name = NULL;
	struct elf_backend_data *bed;
	unsigned char st_other;
	
	section_name = symbol->section ? symbol->section->name : "(*none*)";

	bed = get_elf_backend_data (abfd);
	if (bed->elf_backend_print_symbol_all)
	    name = (*bed->elf_backend_print_symbol_all) (abfd, filep, symbol);

	if (name == NULL)
	  {
	    name = symbol->name;  
	    bfd_print_symbol_vandf ((PTR) file, symbol);
	  }

	fprintf (file, " %s\t", section_name);
	/* Print the "other" value for a symbol.  For common symbols,
	   we've already printed the size; now print the alignment.
	   For other symbols, we have no specified alignment, and
	   we've printed the address; now print the size.  */
	fprintf_vma (file,
		     (bfd_is_com_section (symbol->section)
		      ? ((elf_symbol_type *) symbol)->internal_elf_sym.st_value
		      : ((elf_symbol_type *) symbol)->internal_elf_sym.st_size));

	/* If we have version information, print it.  */
	if (elf_tdata (abfd)->dynversym_section != 0
	    && (elf_tdata (abfd)->dynverdef_section != 0
		|| elf_tdata (abfd)->dynverref_section != 0))
	  {
	    unsigned int vernum;
	    const char *version_string;

	    vernum = ((elf_symbol_type *) symbol)->version & VERSYM_VERSION;

	    if (vernum == 0)
	      version_string = "";
	    else if (vernum == 1)
	      version_string = "Base";
	    else if (vernum <= elf_tdata (abfd)->cverdefs)
	      version_string =
		elf_tdata (abfd)->verdef[vernum - 1].vd_nodename;
	    else
	      {
		Elf_Internal_Verneed *t;

		version_string = "";
		for (t = elf_tdata (abfd)->verref;
		     t != NULL;
		     t = t->vn_nextref)
		  {
		    Elf_Internal_Vernaux *a;

		    for (a = t->vn_auxptr; a != NULL; a = a->vna_nextptr)
		      {
			if (a->vna_other == vernum)
			  {
			    version_string = a->vna_nodename;
			    break;
			  }
		      }
		  }
	      }

	    if ((((elf_symbol_type *) symbol)->version & VERSYM_HIDDEN) == 0)
	      fprintf (file, "  %-11s", version_string);
	    else
	      {
		int i;

		fprintf (file, " (%s)", version_string);
		for (i = 10 - strlen (version_string); i > 0; --i)
		  putc (' ', file);
	      }
	  }

	/* If the st_other field is not zero, print it.  */
	st_other = ((elf_symbol_type *) symbol)->internal_elf_sym.st_other;
	
	switch (st_other)
	  {
	  case 0: break;
	  case STV_INTERNAL:  fprintf (file, " .internal");  break;
	  case STV_HIDDEN:    fprintf (file, " .hidden");    break;
	  case STV_PROTECTED: fprintf (file, " .protected"); break;
	  default:
	    /* Some other non-defined flags are also present, so print
	       everything hex.  */
	    fprintf (file, " 0x%02x", (unsigned int) st_other);
	  }

	fprintf (file, " %s", name);
      }
      break;
    }
}

/* Create an entry in an ELF linker hash table.  */

struct bfd_hash_entry *
_bfd_elf_link_hash_newfunc (entry, table, string)
     struct bfd_hash_entry *entry;
     struct bfd_hash_table *table;
     const char *string;
{
  struct elf_link_hash_entry *ret = (struct elf_link_hash_entry *) entry;

  /* Allocate the structure if it has not already been allocated by a
     subclass.  */
  if (ret == (struct elf_link_hash_entry *) NULL)
    ret = ((struct elf_link_hash_entry *)
	   bfd_hash_allocate (table, sizeof (struct elf_link_hash_entry)));
  if (ret == (struct elf_link_hash_entry *) NULL)
    return (struct bfd_hash_entry *) ret;

  /* Call the allocation method of the superclass.  */
  ret = ((struct elf_link_hash_entry *)
	 _bfd_link_hash_newfunc ((struct bfd_hash_entry *) ret,
				 table, string));
  if (ret != (struct elf_link_hash_entry *) NULL)
    {
      /* Set local fields.  */
      ret->indx = -1;
      ret->size = 0;
      ret->dynindx = -1;
      ret->dynstr_index = 0;
      ret->weakdef = NULL;
      ret->got.offset = (bfd_vma) -1;
      ret->plt.offset = (bfd_vma) -1;
      ret->linker_section_pointer = (elf_linker_section_pointers_t *)0;
      ret->verinfo.verdef = NULL;
      ret->vtable_entries_used = NULL;
      ret->vtable_entries_size = 0;
      ret->vtable_parent = NULL;
      ret->type = STT_NOTYPE;
      ret->other = 0;
      /* Assume that we have been called by a non-ELF symbol reader.
         This flag is then reset by the code which reads an ELF input
         file.  This ensures that a symbol created by a non-ELF symbol
         reader will have the flag set correctly.  */
      ret->elf_link_hash_flags = ELF_LINK_NON_ELF;
    }

  return (struct bfd_hash_entry *) ret;
}

/* Copy data from an indirect symbol to its direct symbol, hiding the
   old indirect symbol.  */

void
_bfd_elf_link_hash_copy_indirect (dir, ind)
     struct elf_link_hash_entry *dir, *ind;
{
  /* Copy down any references that we may have already seen to the
     symbol which just became indirect.  */

  dir->elf_link_hash_flags |=
    (ind->elf_link_hash_flags
     & (ELF_LINK_HASH_REF_DYNAMIC
	| ELF_LINK_HASH_REF_REGULAR
	| ELF_LINK_HASH_REF_REGULAR_NONWEAK
	| ELF_LINK_NON_GOT_REF));

  /* Copy over the global and procedure linkage table offset entries.
     These may have been already set up by a check_relocs routine.  */
  if (dir->got.offset == (bfd_vma) -1)
    {
      dir->got.offset = ind->got.offset;
      ind->got.offset = (bfd_vma) -1;
    }
  BFD_ASSERT (ind->got.offset == (bfd_vma) -1);

  if (dir->plt.offset == (bfd_vma) -1)
    {
      dir->plt.offset = ind->plt.offset;
      ind->plt.offset = (bfd_vma) -1;
    }
  BFD_ASSERT (ind->plt.offset == (bfd_vma) -1);

  if (dir->dynindx == -1)
    {
      dir->dynindx = ind->dynindx;
      dir->dynstr_index = ind->dynstr_index;
      ind->dynindx = -1;
      ind->dynstr_index = 0;
    }
  BFD_ASSERT (ind->dynindx == -1);
}

void
_bfd_elf_link_hash_hide_symbol(h)
     struct elf_link_hash_entry *h;
{
  h->elf_link_hash_flags &= ~ELF_LINK_HASH_NEEDS_PLT;
  h->dynindx = -1;
  h->plt.offset = (bfd_vma) -1;
}

/* Initialize an ELF linker hash table.  */

boolean
_bfd_elf_link_hash_table_init (table, abfd, newfunc)
     struct elf_link_hash_table *table;
     bfd *abfd;
     struct bfd_hash_entry *(*newfunc) PARAMS ((struct bfd_hash_entry *,
						struct bfd_hash_table *,
						const char *));
{
  table->dynamic_sections_created = false;
  table->dynobj = NULL;
  /* The first dynamic symbol is a dummy.  */
  table->dynsymcount = 1;
  table->dynstr = NULL;
  table->bucketcount = 0;
  table->needed = NULL;
  table->hgot = NULL;
  table->stab_info = NULL;
  table->dynlocal = NULL;
  return _bfd_link_hash_table_init (&table->root, abfd, newfunc);
}

/* Create an ELF linker hash table.  */

struct bfd_link_hash_table *
_bfd_elf_link_hash_table_create (abfd)
     bfd *abfd;
{
  struct elf_link_hash_table *ret;

  ret = ((struct elf_link_hash_table *)
	 bfd_alloc (abfd, sizeof (struct elf_link_hash_table)));
  if (ret == (struct elf_link_hash_table *) NULL)
    return NULL;

  if (! _bfd_elf_link_hash_table_init (ret, abfd, _bfd_elf_link_hash_newfunc))
    {
      bfd_release (abfd, ret);
      return NULL;
    }

  return &ret->root;
}

/* This is a hook for the ELF emulation code in the generic linker to
   tell the backend linker what file name to use for the DT_NEEDED
   entry for a dynamic object.  The generic linker passes name as an
   empty string to indicate that no DT_NEEDED entry should be made.  */

void
bfd_elf_set_dt_needed_name (abfd, name)
     bfd *abfd;
     const char *name;
{
  if (bfd_get_flavour (abfd) == bfd_target_elf_flavour
      && bfd_get_format (abfd) == bfd_object)
    elf_dt_name (abfd) = name;
}

/* Get the list of DT_NEEDED entries for a link.  This is a hook for
   the linker ELF emulation code.  */

struct bfd_link_needed_list *
bfd_elf_get_needed_list (abfd, info)
     bfd *abfd ATTRIBUTE_UNUSED;
     struct bfd_link_info *info;
{
  if (info->hash->creator->flavour != bfd_target_elf_flavour)
    return NULL;
  return elf_hash_table (info)->needed;
}

/* Get the name actually used for a dynamic object for a link.  This
   is the SONAME entry if there is one.  Otherwise, it is the string
   passed to bfd_elf_set_dt_needed_name, or it is the filename.  */

const char *
bfd_elf_get_dt_soname (abfd)
     bfd *abfd;
{
  if (bfd_get_flavour (abfd) == bfd_target_elf_flavour
      && bfd_get_format (abfd) == bfd_object)
    return elf_dt_name (abfd);
  return NULL;
}

/* Get the list of DT_NEEDED entries from a BFD.  This is a hook for
   the ELF linker emulation code.  */

boolean
bfd_elf_get_bfd_needed_list (abfd, pneeded)
     bfd *abfd;
     struct bfd_link_needed_list **pneeded;
{
  asection *s;
  bfd_byte *dynbuf = NULL;
  int elfsec;
  unsigned long link;
  bfd_byte *extdyn, *extdynend;
  size_t extdynsize;
  void (*swap_dyn_in) PARAMS ((bfd *, const PTR, Elf_Internal_Dyn *));

  *pneeded = NULL;

  if (bfd_get_flavour (abfd) != bfd_target_elf_flavour
      || bfd_get_format (abfd) != bfd_object)
    return true;

  s = bfd_get_section_by_name (abfd, ".dynamic");
  if (s == NULL || s->_raw_size == 0)
    return true;

  dynbuf = (bfd_byte *) bfd_malloc (s->_raw_size);
  if (dynbuf == NULL)
    goto error_return;

  if (! bfd_get_section_contents (abfd, s, (PTR) dynbuf, (file_ptr) 0,
				  s->_raw_size))
    goto error_return;

  elfsec = _bfd_elf_section_from_bfd_section (abfd, s);
  if (elfsec == -1)
    goto error_return;

  link = elf_elfsections (abfd)[elfsec]->sh_link;

  extdynsize = get_elf_backend_data (abfd)->s->sizeof_dyn;
  swap_dyn_in = get_elf_backend_data (abfd)->s->swap_dyn_in;

  extdyn = dynbuf;
  extdynend = extdyn + s->_raw_size;
  for (; extdyn < extdynend; extdyn += extdynsize)
    {
      Elf_Internal_Dyn dyn;

      (*swap_dyn_in) (abfd, (PTR) extdyn, &dyn);

      if (dyn.d_tag == DT_NULL)
	break;

      if (dyn.d_tag == DT_NEEDED)
	{
	  const char *string;
	  struct bfd_link_needed_list *l;

	  string = bfd_elf_string_from_elf_section (abfd, link,
						    dyn.d_un.d_val);
	  if (string == NULL)
	    goto error_return;

	  l = (struct bfd_link_needed_list *) bfd_alloc (abfd, sizeof *l);
	  if (l == NULL)
	    goto error_return;

	  l->by = abfd;
	  l->name = string;
	  l->next = *pneeded;
	  *pneeded = l;
	}
    }

  free (dynbuf);

  return true;

 error_return:
  if (dynbuf != NULL)
    free (dynbuf);
  return false;
}

/* Allocate an ELF string table--force the first byte to be zero.  */

struct bfd_strtab_hash *
_bfd_elf_stringtab_init ()
{
  struct bfd_strtab_hash *ret;

  ret = _bfd_stringtab_init ();
  if (ret != NULL)
    {
      bfd_size_type loc;

      loc = _bfd_stringtab_add (ret, "", true, false);
      BFD_ASSERT (loc == 0 || loc == (bfd_size_type) -1);
      if (loc == (bfd_size_type) -1)
	{
	  _bfd_stringtab_free (ret);
	  ret = NULL;
	}
    }
  return ret;
}

/* ELF .o/exec file reading */

/* Create a new bfd section from an ELF section header. */

boolean
bfd_section_from_shdr (abfd, shindex)
     bfd *abfd;
     unsigned int shindex;
{
  Elf_Internal_Shdr *hdr = elf_elfsections (abfd)[shindex];
  Elf_Internal_Ehdr *ehdr = elf_elfheader (abfd);
  struct elf_backend_data *bed = get_elf_backend_data (abfd);
  char *name;

  name = elf_string_from_elf_strtab (abfd, hdr->sh_name);

  switch (hdr->sh_type)
    {
    case SHT_NULL:
      /* Inactive section. Throw it away.  */
      return true;

    case SHT_PROGBITS:	/* Normal section with contents.  */
    case SHT_DYNAMIC:	/* Dynamic linking information.  */
    case SHT_NOBITS:	/* .bss section.  */
    case SHT_HASH:	/* .hash section.  */
    case SHT_NOTE:	/* .note section.  */
      return _bfd_elf_make_section_from_shdr (abfd, hdr, name);

    case SHT_SYMTAB:		/* A symbol table */
      if (elf_onesymtab (abfd) == shindex)
	return true;

      BFD_ASSERT (hdr->sh_entsize == bed->s->sizeof_sym);
      BFD_ASSERT (elf_onesymtab (abfd) == 0);
      elf_onesymtab (abfd) = shindex;
      elf_tdata (abfd)->symtab_hdr = *hdr;
      elf_elfsections (abfd)[shindex] = hdr = &elf_tdata (abfd)->symtab_hdr;
      abfd->flags |= HAS_SYMS;

      /* Sometimes a shared object will map in the symbol table.  If
         SHF_ALLOC is set, and this is a shared object, then we also
         treat this section as a BFD section.  We can not base the
         decision purely on SHF_ALLOC, because that flag is sometimes
         set in a relocateable object file, which would confuse the
         linker.  */
      if ((hdr->sh_flags & SHF_ALLOC) != 0
	  && (abfd->flags & DYNAMIC) != 0
	  && ! _bfd_elf_make_section_from_shdr (abfd, hdr, name))
	return false;

      return true;

    case SHT_DYNSYM:		/* A dynamic symbol table */
      if (elf_dynsymtab (abfd) == shindex)
	return true;

      BFD_ASSERT (hdr->sh_entsize == bed->s->sizeof_sym);
      BFD_ASSERT (elf_dynsymtab (abfd) == 0);
      elf_dynsymtab (abfd) = shindex;
      elf_tdata (abfd)->dynsymtab_hdr = *hdr;
      elf_elfsections (abfd)[shindex] = hdr = &elf_tdata (abfd)->dynsymtab_hdr;
      abfd->flags |= HAS_SYMS;

      /* Besides being a symbol table, we also treat this as a regular
	 section, so that objcopy can handle it.  */
      return _bfd_elf_make_section_from_shdr (abfd, hdr, name);

    case SHT_STRTAB:		/* A string table */
      if (hdr->bfd_section != NULL)
	return true;
      if (ehdr->e_shstrndx == shindex)
	{
	  elf_tdata (abfd)->shstrtab_hdr = *hdr;
	  elf_elfsections (abfd)[shindex] = &elf_tdata (abfd)->shstrtab_hdr;
	  return true;
	}
      {
	unsigned int i;

	for (i = 1; i < ehdr->e_shnum; i++)
	  {
	    Elf_Internal_Shdr *hdr2 = elf_elfsections (abfd)[i];
	    if (hdr2->sh_link == shindex)
	      {
		if (! bfd_section_from_shdr (abfd, i))
		  return false;
		if (elf_onesymtab (abfd) == i)
		  {
		    elf_tdata (abfd)->strtab_hdr = *hdr;
		    elf_elfsections (abfd)[shindex] =
		      &elf_tdata (abfd)->strtab_hdr;
		    return true;
		  }
		if (elf_dynsymtab (abfd) == i)
		  {
		    elf_tdata (abfd)->dynstrtab_hdr = *hdr;
		    elf_elfsections (abfd)[shindex] = hdr =
		      &elf_tdata (abfd)->dynstrtab_hdr;
		    /* We also treat this as a regular section, so
		       that objcopy can handle it.  */
		    break;
		  }
#if 0 /* Not handling other string tables specially right now.  */
		hdr2 = elf_elfsections (abfd)[i];	/* in case it moved */
		/* We have a strtab for some random other section.  */
		newsect = (asection *) hdr2->bfd_section;
		if (!newsect)
		  break;
		hdr->bfd_section = newsect;
		hdr2 = &elf_section_data (newsect)->str_hdr;
		*hdr2 = *hdr;
		elf_elfsections (abfd)[shindex] = hdr2;
#endif
	      }
	  }
      }

      return _bfd_elf_make_section_from_shdr (abfd, hdr, name);

    case SHT_REL:
    case SHT_RELA:
      /* *These* do a lot of work -- but build no sections!  */
      {
	asection *target_sect;
	Elf_Internal_Shdr *hdr2;

	/* Check for a bogus link to avoid crashing.  */
	if (hdr->sh_link >= ehdr->e_shnum)
	  {
	    ((*_bfd_error_handler)
	     (_("%s: invalid link %lu for reloc section %s (index %u)"),
	      bfd_get_filename (abfd), hdr->sh_link, name, shindex));
	    return _bfd_elf_make_section_from_shdr (abfd, hdr, name);
	  }

	/* For some incomprehensible reason Oracle distributes
	   libraries for Solaris in which some of the objects have
	   bogus sh_link fields.  It would be nice if we could just
	   reject them, but, unfortunately, some people need to use
	   them.  We scan through the section headers; if we find only
	   one suitable symbol table, we clobber the sh_link to point
	   to it.  I hope this doesn't break anything.  */
	if (elf_elfsections (abfd)[hdr->sh_link]->sh_type != SHT_SYMTAB
	    && elf_elfsections (abfd)[hdr->sh_link]->sh_type != SHT_DYNSYM)
	  {
	    int scan;
	    int found;

	    found = 0;
	    for (scan = 1; scan < ehdr->e_shnum; scan++)
	      {
		if (elf_elfsections (abfd)[scan]->sh_type == SHT_SYMTAB
		    || elf_elfsections (abfd)[scan]->sh_type == SHT_DYNSYM)
		  {
		    if (found != 0)
		      {
			found = 0;
			break;
		      }
		    found = scan;
		  }
	      }
	    if (found != 0)
	      hdr->sh_link = found;
	  }

	/* Get the symbol table.  */
	if (elf_elfsections (abfd)[hdr->sh_link]->sh_type == SHT_SYMTAB
	    && ! bfd_section_from_shdr (abfd, hdr->sh_link))
	  return false;

	/* If this reloc section does not use the main symbol table we
	   don't treat it as a reloc section.  BFD can't adequately
	   represent such a section, so at least for now, we don't
	   try.  We just present it as a normal section.  */
	if (hdr->sh_link != elf_onesymtab (abfd))
	  return _bfd_elf_make_section_from_shdr (abfd, hdr, name);

	if (! bfd_section_from_shdr (abfd, hdr->sh_info))
	  return false;
	target_sect = bfd_section_from_elf_index (abfd, hdr->sh_info);
	if (target_sect == NULL)
	  return false;

	if ((target_sect->flags & SEC_RELOC) == 0
	    || target_sect->reloc_count == 0)
	  hdr2 = &elf_section_data (target_sect)->rel_hdr;
	else
	  {
	    BFD_ASSERT (elf_section_data (target_sect)->rel_hdr2 == NULL);
	    hdr2 = (Elf_Internal_Shdr *) bfd_alloc (abfd, sizeof (*hdr2));
	    elf_section_data (target_sect)->rel_hdr2 = hdr2;
	  }
	*hdr2 = *hdr;
	elf_elfsections (abfd)[shindex] = hdr2;
	target_sect->reloc_count += hdr->sh_size / hdr->sh_entsize;
	target_sect->flags |= SEC_RELOC;
	target_sect->relocation = NULL;
	target_sect->rel_filepos = hdr->sh_offset;
	/* In the section to which the relocations apply, mark whether
	   its relocations are of the REL or RELA variety.  */
	if (hdr->sh_size != 0)
	  elf_section_data (target_sect)->use_rela_p
	    = (hdr->sh_type == SHT_RELA);
	abfd->flags |= HAS_RELOC;
	return true;
      }
      break;

    case SHT_GNU_verdef:
      elf_dynverdef (abfd) = shindex;
      elf_tdata (abfd)->dynverdef_hdr = *hdr;
      return _bfd_elf_make_section_from_shdr (abfd, hdr, name);
      break;

    case SHT_GNU_versym:
      elf_dynversym (abfd) = shindex;
      elf_tdata (abfd)->dynversym_hdr = *hdr;
      return _bfd_elf_make_section_from_shdr (abfd, hdr, name);
      break;

    case SHT_GNU_verneed:
      elf_dynverref (abfd) = shindex;
      elf_tdata (abfd)->dynverref_hdr = *hdr;
      return _bfd_elf_make_section_from_shdr (abfd, hdr, name);
      break;

    case SHT_SHLIB:
      return true;

    default:
      /* Check for any processor-specific section types.  */
      {
	if (bed->elf_backend_section_from_shdr)
	  (*bed->elf_backend_section_from_shdr) (abfd, hdr, name);
      }
      break;
    }

  return true;
}

/* Given an ELF section number, retrieve the corresponding BFD
   section.  */

asection *
bfd_section_from_elf_index (abfd, index)
     bfd *abfd;
     unsigned int index;
{
  BFD_ASSERT (index > 0 && index < SHN_LORESERVE);
  if (index >= elf_elfheader (abfd)->e_shnum)
    return NULL;
  return elf_elfsections (abfd)[index]->bfd_section;
}

boolean
_bfd_elf_new_section_hook (abfd, sec)
     bfd *abfd;
     asection *sec;
{
  struct bfd_elf_section_data *sdata;

  sdata = (struct bfd_elf_section_data *) bfd_zalloc (abfd, sizeof (*sdata));
  if (!sdata)
    return false;
  sec->used_by_bfd = (PTR) sdata;

  /* Indicate whether or not this section should use RELA relocations.  */
  sdata->use_rela_p 
    = get_elf_backend_data (abfd)->default_use_rela_p;

  return true;
}

/* Create a new bfd section from an ELF program header.

   Since program segments have no names, we generate a synthetic name
   of the form segment<NUM>, where NUM is generally the index in the
   program header table.  For segments that are split (see below) we
   generate the names segment<NUM>a and segment<NUM>b.

   Note that some program segments may have a file size that is different than
   (less than) the memory size.  All this means is that at execution the
   system must allocate the amount of memory specified by the memory size,
   but only initialize it with the first "file size" bytes read from the
   file.  This would occur for example, with program segments consisting
   of combined data+bss.

   To handle the above situation, this routine generates TWO bfd sections
   for the single program segment.  The first has the length specified by
   the file size of the segment, and the second has the length specified
   by the difference between the two sizes.  In effect, the segment is split
   into it's initialized and uninitialized parts.

 */

boolean
_bfd_elf_make_section_from_phdr (abfd, hdr, index, typename)
     bfd *abfd;
     Elf_Internal_Phdr *hdr;
     int index;
     const char *typename;
{
  asection *newsect;
  char *name;
  char namebuf[64];
  int split;

  split = ((hdr->p_memsz > 0)
	    && (hdr->p_filesz > 0)
	    && (hdr->p_memsz > hdr->p_filesz));
  sprintf (namebuf, "%s%d%s", typename, index, split ? "a" : "");
  name = bfd_alloc (abfd, strlen (namebuf) + 1);
  if (!name)
    return false;
  strcpy (name, namebuf);
  newsect = bfd_make_section (abfd, name);
  if (newsect == NULL)
    return false;
  newsect->vma = hdr->p_vaddr;
  newsect->lma = hdr->p_paddr;
  newsect->_raw_size = hdr->p_filesz;
  newsect->filepos = hdr->p_offset;
  newsect->flags |= SEC_HAS_CONTENTS;
  if (hdr->p_type == PT_LOAD)
    {
      newsect->flags |= SEC_ALLOC;
      newsect->flags |= SEC_LOAD;
      if (hdr->p_flags & PF_X)
	{
	  /* FIXME: all we known is that it has execute PERMISSION,
	     may be data. */
	  newsect->flags |= SEC_CODE;
	}
    }
  if (!(hdr->p_flags & PF_W))
    {
      newsect->flags |= SEC_READONLY;
    }

  if (split)
    {
      sprintf (namebuf, "%s%db", typename, index);
      name = bfd_alloc (abfd, strlen (namebuf) + 1);
      if (!name)
	return false;
      strcpy (name, namebuf);
      newsect = bfd_make_section (abfd, name);
      if (newsect == NULL)
	return false;
      newsect->vma = hdr->p_vaddr + hdr->p_filesz;
      newsect->lma = hdr->p_paddr + hdr->p_filesz;
      newsect->_raw_size = hdr->p_memsz - hdr->p_filesz;
      if (hdr->p_type == PT_LOAD)
	{
	  newsect->flags |= SEC_ALLOC;
	  if (hdr->p_flags & PF_X)
	    newsect->flags |= SEC_CODE;
	}
      if (!(hdr->p_flags & PF_W))
	newsect->flags |= SEC_READONLY;
    }

  return true;
}

boolean
bfd_section_from_phdr (abfd, hdr, index)
     bfd *abfd;
     Elf_Internal_Phdr *hdr;
     int index;
{
  struct elf_backend_data *bed;

  switch (hdr->p_type)
    {
    case PT_NULL:
      return _bfd_elf_make_section_from_phdr (abfd, hdr, index, "null");

    case PT_LOAD:
      return _bfd_elf_make_section_from_phdr (abfd, hdr, index, "load");

    case PT_DYNAMIC:
      return _bfd_elf_make_section_from_phdr (abfd, hdr, index, "dynamic");

    case PT_INTERP:
      return _bfd_elf_make_section_from_phdr (abfd, hdr, index, "interp");

    case PT_NOTE:
      if (! _bfd_elf_make_section_from_phdr (abfd, hdr, index, "note"))
	return false;
      if (! elfcore_read_notes (abfd, hdr->p_offset, hdr->p_filesz))
	return false;
      return true;

    case PT_SHLIB:
      return _bfd_elf_make_section_from_phdr (abfd, hdr, index, "shlib");

    case PT_PHDR:
      return _bfd_elf_make_section_from_phdr (abfd, hdr, index, "phdr");

    default:
      /* Check for any processor-specific program segment types.
         If no handler for them, default to making "segment" sections. */
      bed = get_elf_backend_data (abfd);
      if (bed->elf_backend_section_from_phdr)
	return (*bed->elf_backend_section_from_phdr) (abfd, hdr, index);
      else
	return _bfd_elf_make_section_from_phdr (abfd, hdr, index, "segment");
    }
}

/* Initialize REL_HDR, the section-header for new section, containing
   relocations against ASECT.  If USE_RELA_P is true, we use RELA
   relocations; otherwise, we use REL relocations.  */

boolean
_bfd_elf_init_reloc_shdr (abfd, rel_hdr, asect, use_rela_p)
     bfd *abfd;
     Elf_Internal_Shdr *rel_hdr;
     asection *asect;
     boolean use_rela_p;
{
  char *name;
  struct elf_backend_data *bed;

  bed = get_elf_backend_data (abfd);
  name = bfd_alloc (abfd, sizeof ".rela" + strlen (asect->name));
  if (name == NULL)
    return false;
  sprintf (name, "%s%s", use_rela_p ? ".rela" : ".rel", asect->name);
  rel_hdr->sh_name =
    (unsigned int) _bfd_stringtab_add (elf_shstrtab (abfd), name,
				       true, false);
  if (rel_hdr->sh_name == (unsigned int) -1)
    return false;
  rel_hdr->sh_type = use_rela_p ? SHT_RELA : SHT_REL;
  rel_hdr->sh_entsize = (use_rela_p
			 ? bed->s->sizeof_rela
			 : bed->s->sizeof_rel);
  rel_hdr->sh_addralign = bed->s->file_align;
  rel_hdr->sh_flags = 0;
  rel_hdr->sh_addr = 0;
  rel_hdr->sh_size = 0;
  rel_hdr->sh_offset = 0;

  return true;
}

/* Set up an ELF internal section header for a section.  */

/*ARGSUSED*/
static void
elf_fake_sections (abfd, asect, failedptrarg)
     bfd *abfd;
     asection *asect;
     PTR failedptrarg;
{
  struct elf_backend_data *bed = get_elf_backend_data (abfd);
  boolean *failedptr = (boolean *) failedptrarg;
  Elf_Internal_Shdr *this_hdr;

  if (*failedptr)
    {
      /* We already failed; just get out of the bfd_map_over_sections
         loop.  */
      return;
    }

  this_hdr = &elf_section_data (asect)->this_hdr;

  this_hdr->sh_name = (unsigned long) _bfd_stringtab_add (elf_shstrtab (abfd),
							  asect->name,
							  true, false);
  if (this_hdr->sh_name == (unsigned long) -1)
    {
      *failedptr = true;
      return;
    }

  this_hdr->sh_flags = 0;

  if ((asect->flags & SEC_ALLOC) != 0
      || asect->user_set_vma)
    this_hdr->sh_addr = asect->vma;
  else
    this_hdr->sh_addr = 0;

  this_hdr->sh_offset = 0;
  this_hdr->sh_size = asect->_raw_size;
  this_hdr->sh_link = 0;
  this_hdr->sh_addralign = 1 << asect->alignment_power;
  /* The sh_entsize and sh_info fields may have been set already by
     copy_private_section_data.  */

  this_hdr->bfd_section = asect;
  this_hdr->contents = NULL;

  /* FIXME: This should not be based on section names.  */
  if (strcmp (asect->name, ".dynstr") == 0)
    this_hdr->sh_type = SHT_STRTAB;
  else if (strcmp (asect->name, ".hash") == 0)
    {
      this_hdr->sh_type = SHT_HASH;
      this_hdr->sh_entsize = bed->s->sizeof_hash_entry;
    }
  else if (strcmp (asect->name, ".dynsym") == 0)
    {
      this_hdr->sh_type = SHT_DYNSYM;
      this_hdr->sh_entsize = bed->s->sizeof_sym;
    }
  else if (strcmp (asect->name, ".dynamic") == 0)
    {
      this_hdr->sh_type = SHT_DYNAMIC;
      this_hdr->sh_entsize = bed->s->sizeof_dyn;
    }
  else if (strncmp (asect->name, ".rela", 5) == 0
	   && get_elf_backend_data (abfd)->may_use_rela_p)
    {
      this_hdr->sh_type = SHT_RELA;
      this_hdr->sh_entsize = bed->s->sizeof_rela;
    }
  else if (strncmp (asect->name, ".rel", 4) == 0
	   && get_elf_backend_data (abfd)->may_use_rel_p)
    {
      this_hdr->sh_type = SHT_REL;
      this_hdr->sh_entsize = bed->s->sizeof_rel;
    }
  else if (strncmp (asect->name, ".note", 5) == 0)
    this_hdr->sh_type = SHT_NOTE;
  else if (strncmp (asect->name, ".stab", 5) == 0
	   && strcmp (asect->name + strlen (asect->name) - 3, "str") == 0)
    this_hdr->sh_type = SHT_STRTAB;
  else if (strcmp (asect->name, ".gnu.version") == 0)
    {
      this_hdr->sh_type = SHT_GNU_versym;
      this_hdr->sh_entsize = sizeof (Elf_External_Versym);
    }
  else if (strcmp (asect->name, ".gnu.version_d") == 0)
    {
      this_hdr->sh_type = SHT_GNU_verdef;
      this_hdr->sh_entsize = 0;
      /* objcopy or strip will copy over sh_info, but may not set
         cverdefs.  The linker will set cverdefs, but sh_info will be
         zero.  */
      if (this_hdr->sh_info == 0)
	this_hdr->sh_info = elf_tdata (abfd)->cverdefs;
      else
	BFD_ASSERT (elf_tdata (abfd)->cverdefs == 0
		    || this_hdr->sh_info == elf_tdata (abfd)->cverdefs);
    }
  else if (strcmp (asect->name, ".gnu.version_r") == 0)
    {
      this_hdr->sh_type = SHT_GNU_verneed;
      this_hdr->sh_entsize = 0;
      /* objcopy or strip will copy over sh_info, but may not set
         cverrefs.  The linker will set cverrefs, but sh_info will be
         zero.  */
      if (this_hdr->sh_info == 0)
	this_hdr->sh_info = elf_tdata (abfd)->cverrefs;
      else
	BFD_ASSERT (elf_tdata (abfd)->cverrefs == 0
		    || this_hdr->sh_info == elf_tdata (abfd)->cverrefs);
    }
  else if ((asect->flags & SEC_ALLOC) != 0
	   && (asect->flags & SEC_LOAD) != 0)
    this_hdr->sh_type = SHT_PROGBITS;
  else if ((asect->flags & SEC_ALLOC) != 0
	   && ((asect->flags & SEC_LOAD) == 0))
    this_hdr->sh_type = SHT_NOBITS;
  else
    {
      /* Who knows?  */
      this_hdr->sh_type = SHT_PROGBITS;
    }

  if ((asect->flags & SEC_ALLOC) != 0)
    this_hdr->sh_flags |= SHF_ALLOC;
  if ((asect->flags & SEC_READONLY) == 0)
    this_hdr->sh_flags |= SHF_WRITE;
  if ((asect->flags & SEC_CODE) != 0)
    this_hdr->sh_flags |= SHF_EXECINSTR;

  /* Check for processor-specific section types.  */
  if (bed->elf_backend_fake_sections)
    (*bed->elf_backend_fake_sections) (abfd, this_hdr, asect);

  /* If the section has relocs, set up a section header for the
     SHT_REL[A] section.  If two relocation sections are required for
     this section, it is up to the processor-specific back-end to
     create the other.  */ 
  if ((asect->flags & SEC_RELOC) != 0
      && !_bfd_elf_init_reloc_shdr (abfd, 
				    &elf_section_data (asect)->rel_hdr,
				    asect, 
				    elf_section_data (asect)->use_rela_p))
    *failedptr = true;
}

/* Assign all ELF section numbers.  The dummy first section is handled here
   too.  The link/info pointers for the standard section types are filled
   in here too, while we're at it.  */

static boolean
assign_section_numbers (abfd)
     bfd *abfd;
{
  struct elf_obj_tdata *t = elf_tdata (abfd);
  asection *sec;
  unsigned int section_number;
  Elf_Internal_Shdr **i_shdrp;
  struct elf_backend_data *bed = get_elf_backend_data (abfd);

  section_number = 1;

  for (sec = abfd->sections; sec; sec = sec->next)
    {
      struct bfd_elf_section_data *d = elf_section_data (sec);

      d->this_idx = section_number++;
      if ((sec->flags & SEC_RELOC) == 0)
	d->rel_idx = 0;
      else
	d->rel_idx = section_number++;

      if (d->rel_hdr2)
	d->rel_idx2 = section_number++;
      else
	d->rel_idx2 = 0;
    }

  t->shstrtab_section = section_number++;
  elf_elfheader (abfd)->e_shstrndx = t->shstrtab_section;
  t->shstrtab_hdr.sh_size = _bfd_stringtab_size (elf_shstrtab (abfd));

  if (bfd_get_symcount (abfd) > 0)
    {
      t->symtab_section = section_number++;
      t->strtab_section = section_number++;
    }

  elf_elfheader (abfd)->e_shnum = section_number;

  /* Set up the list of section header pointers, in agreement with the
     indices.  */
  i_shdrp = ((Elf_Internal_Shdr **)
	     bfd_alloc (abfd, section_number * sizeof (Elf_Internal_Shdr *)));
  if (i_shdrp == NULL)
    return false;

  i_shdrp[0] = ((Elf_Internal_Shdr *)
		bfd_alloc (abfd, sizeof (Elf_Internal_Shdr)));
  if (i_shdrp[0] == NULL)
    {
      bfd_release (abfd, i_shdrp);
      return false;
    }
  memset (i_shdrp[0], 0, sizeof (Elf_Internal_Shdr));

  elf_elfsections (abfd) = i_shdrp;

  i_shdrp[t->shstrtab_section] = &t->shstrtab_hdr;
  if (bfd_get_symcount (abfd) > 0)
    {
      i_shdrp[t->symtab_section] = &t->symtab_hdr;
      i_shdrp[t->strtab_section] = &t->strtab_hdr;
      t->symtab_hdr.sh_link = t->strtab_section;
    }
  for (sec = abfd->sections; sec; sec = sec->next)
    {
      struct bfd_elf_section_data *d = elf_section_data (sec);
      asection *s;
      const char *name;

      i_shdrp[d->this_idx] = &d->this_hdr;
      if (d->rel_idx != 0)
	i_shdrp[d->rel_idx] = &d->rel_hdr;
      if (d->rel_idx2 != 0)
	i_shdrp[d->rel_idx2] = d->rel_hdr2;

      /* Fill in the sh_link and sh_info fields while we're at it.  */

      /* sh_link of a reloc section is the section index of the symbol
	 table.  sh_info is the section index of the section to which
	 the relocation entries apply.  */
      if (d->rel_idx != 0)
	{
	  d->rel_hdr.sh_link = t->symtab_section;
	  d->rel_hdr.sh_info = d->this_idx;
	}
      if (d->rel_idx2 != 0)
	{
	  d->rel_hdr2->sh_link = t->symtab_section;
	  d->rel_hdr2->sh_info = d->this_idx;
	}

      switch (d->this_hdr.sh_type)
	{
	case SHT_REL:
	case SHT_RELA:
	  /* A reloc section which we are treating as a normal BFD
	     section.  sh_link is the section index of the symbol
	     table.  sh_info is the section index of the section to
	     which the relocation entries apply.  We assume that an
	     allocated reloc section uses the dynamic symbol table.
	     FIXME: How can we be sure?  */
	  s = bfd_get_section_by_name (abfd, ".dynsym");
	  if (s != NULL)
	    d->this_hdr.sh_link = elf_section_data (s)->this_idx;

	  /* We look up the section the relocs apply to by name.  */
	  name = sec->name;
	  if (d->this_hdr.sh_type == SHT_REL)
	    name += 4;
	  else
	    name += 5;
	  s = bfd_get_section_by_name (abfd, name);
	  if (s != NULL)
	    d->this_hdr.sh_info = elf_section_data (s)->this_idx;
	  break;

	case SHT_STRTAB:
	  /* We assume that a section named .stab*str is a stabs
	     string section.  We look for a section with the same name
	     but without the trailing ``str'', and set its sh_link
	     field to point to this section.  */
	  if (strncmp (sec->name, ".stab", sizeof ".stab" - 1) == 0
	      && strcmp (sec->name + strlen (sec->name) - 3, "str") == 0)
	    {
	      size_t len;
	      char *alc;

	      len = strlen (sec->name);
	      alc = (char *) bfd_malloc (len - 2);
	      if (alc == NULL)
		return false;
	      strncpy (alc, sec->name, len - 3);
	      alc[len - 3] = '\0';
	      s = bfd_get_section_by_name (abfd, alc);
	      free (alc);
	      if (s != NULL)
		{
		  elf_section_data (s)->this_hdr.sh_link = d->this_idx;

		  /* This is a .stab section.  */
		  elf_section_data (s)->this_hdr.sh_entsize =
		    4 + 2 * (bed->s->arch_size / 8);
		}
	    }
	  break;

	case SHT_DYNAMIC:
	case SHT_DYNSYM:
	case SHT_GNU_verneed:
	case SHT_GNU_verdef:
	  /* sh_link is the section header index of the string table
	     used for the dynamic entries, or the symbol table, or the
	     version strings.  */
	  s = bfd_get_section_by_name (abfd, ".dynstr");
	  if (s != NULL)
	    d->this_hdr.sh_link = elf_section_data (s)->this_idx;
	  break;

	case SHT_HASH:
	case SHT_GNU_versym:
	  /* sh_link is the section header index of the symbol table
	     this hash table or version table is for.  */
	  s = bfd_get_section_by_name (abfd, ".dynsym");
	  if (s != NULL)
	    d->this_hdr.sh_link = elf_section_data (s)->this_idx;
	  break;
	}
    }

  return true;
}

/* Map symbol from it's internal number to the external number, moving
   all local symbols to be at the head of the list.  */

static INLINE int
sym_is_global (abfd, sym)
     bfd *abfd;
     asymbol *sym;
{
  /* If the backend has a special mapping, use it.  */
  if (get_elf_backend_data (abfd)->elf_backend_sym_is_global)
    return ((*get_elf_backend_data (abfd)->elf_backend_sym_is_global)
	    (abfd, sym));

  return ((sym->flags & (BSF_GLOBAL | BSF_WEAK)) != 0
	  || bfd_is_und_section (bfd_get_section (sym))
	  || bfd_is_com_section (bfd_get_section (sym)));
}

static boolean
elf_map_symbols (abfd)
     bfd *abfd;
{
  int symcount = bfd_get_symcount (abfd);
  asymbol **syms = bfd_get_outsymbols (abfd);
  asymbol **sect_syms;
  int num_locals = 0;
  int num_globals = 0;
  int num_locals2 = 0;
  int num_globals2 = 0;
  int max_index = 0;
  int num_sections = 0;
  int idx;
  asection *asect;
  asymbol **new_syms;
  asymbol *sym;

#ifdef DEBUG
  fprintf (stderr, "elf_map_symbols\n");
  fflush (stderr);
#endif

  /* Add a section symbol for each BFD section.  FIXME: Is this really
     necessary?  */
  for (asect = abfd->sections; asect; asect = asect->next)
    {
      if (max_index < asect->index)
	max_index = asect->index;
    }

  max_index++;
  sect_syms = (asymbol **) bfd_zalloc (abfd, max_index * sizeof (asymbol *));
  if (sect_syms == NULL)
    return false;
  elf_section_syms (abfd) = sect_syms;

  for (idx = 0; idx < symcount; idx++)
    {
      sym = syms[idx];
      
      if ((sym->flags & BSF_SECTION_SYM) != 0
	  && sym->value == 0)
	{
	  asection *sec;

	  sec = sym->section;

	  if (sec->owner != NULL)
	    {
	      if (sec->owner != abfd)
		{
		  if (sec->output_offset != 0)
		    continue;
		  
		  sec = sec->output_section;

		  /* Empty sections in the input files may have had a section
		     symbol created for them.  (See the comment near the end of
		     _bfd_generic_link_output_symbols in linker.c).  If the linker
		     script discards such sections then we will reach this point.
		     Since we know that we cannot avoid this case, we detect it
		     and skip the abort and the assignment to the sect_syms array.
		     To reproduce this particular case try running the linker
		     testsuite test ld-scripts/weak.exp for an ELF port that uses
		     the generic linker.  */
		  if (sec->owner == NULL)
		    continue;

		  BFD_ASSERT (sec->owner == abfd);
		}
	      sect_syms[sec->index] = syms[idx];
	    }
	}
    }

  for (asect = abfd->sections; asect; asect = asect->next)
    {
      if (sect_syms[asect->index] != NULL)
	continue;

      sym = bfd_make_empty_symbol (abfd);
      if (sym == NULL)
	return false;
      sym->the_bfd = abfd;
      sym->name = asect->name;
      sym->value = 0;
      /* Set the flags to 0 to indicate that this one was newly added.  */
      sym->flags = 0;
      sym->section = asect;
      sect_syms[asect->index] = sym;
      num_sections++;
#ifdef DEBUG
      fprintf (stderr,
 _("creating section symbol, name = %s, value = 0x%.8lx, index = %d, section = 0x%.8lx\n"),
	       asect->name, (long) asect->vma, asect->index, (long) asect);
#endif
    }

  /* Classify all of the symbols.  */
  for (idx = 0; idx < symcount; idx++)
    {
      if (!sym_is_global (abfd, syms[idx]))
	num_locals++;
      else
	num_globals++;
    }
  for (asect = abfd->sections; asect; asect = asect->next)
    {
      if (sect_syms[asect->index] != NULL
	  && sect_syms[asect->index]->flags == 0)
	{
	  sect_syms[asect->index]->flags = BSF_SECTION_SYM;
	  if (!sym_is_global (abfd, sect_syms[asect->index]))
	    num_locals++;
	  else
	    num_globals++;
	  sect_syms[asect->index]->flags = 0;
	}
    }

  /* Now sort the symbols so the local symbols are first.  */
  new_syms = ((asymbol **)
	      bfd_alloc (abfd,
			 (num_locals + num_globals) * sizeof (asymbol *)));
  if (new_syms == NULL)
    return false;

  for (idx = 0; idx < symcount; idx++)
    {
      asymbol *sym = syms[idx];
      int i;

      if (!sym_is_global (abfd, sym))
	i = num_locals2++;
      else
	i = num_locals + num_globals2++;
      new_syms[i] = sym;
      sym->udata.i = i + 1;
    }
  for (asect = abfd->sections; asect; asect = asect->next)
    {
      if (sect_syms[asect->index] != NULL
	  && sect_syms[asect->index]->flags == 0)
	{
	  asymbol *sym = sect_syms[asect->index];
	  int i;

	  sym->flags = BSF_SECTION_SYM;
	  if (!sym_is_global (abfd, sym))
	    i = num_locals2++;
	  else
	    i = num_locals + num_globals2++;
	  new_syms[i] = sym;
	  sym->udata.i = i + 1;
	}
    }

  bfd_set_symtab (abfd, new_syms, num_locals + num_globals);

  elf_num_locals (abfd) = num_locals;
  elf_num_globals (abfd) = num_globals;
  return true;
}

/* Align to the maximum file alignment that could be required for any
   ELF data structure.  */

static INLINE file_ptr align_file_position PARAMS ((file_ptr, int));
static INLINE file_ptr
align_file_position (off, align)
     file_ptr off;
     int align;
{
  return (off + align - 1) & ~(align - 1);
}

/* Assign a file position to a section, optionally aligning to the
   required section alignment.  */

INLINE file_ptr
_bfd_elf_assign_file_position_for_section (i_shdrp, offset, align)
     Elf_Internal_Shdr *i_shdrp;
     file_ptr offset;
     boolean align;
{
  if (align)
    {
      unsigned int al;

      al = i_shdrp->sh_addralign;
      if (al > 1)
	offset = BFD_ALIGN (offset, al);
    }
  i_shdrp->sh_offset = offset;
  if (i_shdrp->bfd_section != NULL)
    i_shdrp->bfd_section->filepos = offset;
  if (i_shdrp->sh_type != SHT_NOBITS)
    offset += i_shdrp->sh_size;
  return offset;
}

/* Compute the file positions we are going to put the sections at, and
   otherwise prepare to begin writing out the ELF file.  If LINK_INFO
   is not NULL, this is being called by the ELF backend linker.  */

boolean
_bfd_elf_compute_section_file_positions (abfd, link_info)
     bfd *abfd;
     struct bfd_link_info *link_info;
{
  struct elf_backend_data *bed = get_elf_backend_data (abfd);
  boolean failed;
  struct bfd_strtab_hash *strtab;
  Elf_Internal_Shdr *shstrtab_hdr;

  if (abfd->output_has_begun)
    return true;

  /* Do any elf backend specific processing first.  */
  if (bed->elf_backend_begin_write_processing)
    (*bed->elf_backend_begin_write_processing) (abfd, link_info);

  if (! prep_headers (abfd))
    return false;

  /* Post process the headers if necessary.  */
  if (bed->elf_backend_post_process_headers)
    (*bed->elf_backend_post_process_headers) (abfd, link_info);

  failed = false;
  bfd_map_over_sections (abfd, elf_fake_sections, &failed);
  if (failed)
    return false;

  if (!assign_section_numbers (abfd))
    return false;

  /* The backend linker builds symbol table information itself.  */
  if (link_info == NULL && bfd_get_symcount (abfd) > 0)
    {
      /* Non-zero if doing a relocatable link.  */
      int relocatable_p = ! (abfd->flags & (EXEC_P | DYNAMIC));

      if (! swap_out_syms (abfd, &strtab, relocatable_p))
	return false;
    }

  shstrtab_hdr = &elf_tdata (abfd)->shstrtab_hdr;
  /* sh_name was set in prep_headers.  */
  shstrtab_hdr->sh_type = SHT_STRTAB;
  shstrtab_hdr->sh_flags = 0;
  shstrtab_hdr->sh_addr = 0;
  shstrtab_hdr->sh_size = _bfd_stringtab_size (elf_shstrtab (abfd));
  shstrtab_hdr->sh_entsize = 0;
  shstrtab_hdr->sh_link = 0;
  shstrtab_hdr->sh_info = 0;
  /* sh_offset is set in assign_file_positions_except_relocs.  */
  shstrtab_hdr->sh_addralign = 1;

  if (!assign_file_positions_except_relocs (abfd))
    return false;

  if (link_info == NULL && bfd_get_symcount (abfd) > 0)
    {
      file_ptr off;
      Elf_Internal_Shdr *hdr;

      off = elf_tdata (abfd)->next_file_pos;

      hdr = &elf_tdata (abfd)->symtab_hdr;
      off = _bfd_elf_assign_file_position_for_section (hdr, off, true);

      hdr = &elf_tdata (abfd)->strtab_hdr;
      off = _bfd_elf_assign_file_position_for_section (hdr, off, true);

      elf_tdata (abfd)->next_file_pos = off;

      /* Now that we know where the .strtab section goes, write it
         out.  */
      if (bfd_seek (abfd, hdr->sh_offset, SEEK_SET) != 0
	  || ! _bfd_stringtab_emit (abfd, strtab))
	return false;
      _bfd_stringtab_free (strtab);
    }

  abfd->output_has_begun = true;

  return true;
}

/* Create a mapping from a set of sections to a program segment.  */

static INLINE struct elf_segment_map *
make_mapping (abfd, sections, from, to, phdr)
     bfd *abfd;
     asection **sections;
     unsigned int from;
     unsigned int to;
     boolean phdr;
{
  struct elf_segment_map *m;
  unsigned int i;
  asection **hdrpp;

  m = ((struct elf_segment_map *)
       bfd_zalloc (abfd,
		   (sizeof (struct elf_segment_map)
		    + (to - from - 1) * sizeof (asection *))));
  if (m == NULL)
    return NULL;
  m->next = NULL;
  m->p_type = PT_LOAD;
  for (i = from, hdrpp = sections + from; i < to; i++, hdrpp++)
    m->sections[i - from] = *hdrpp;
  m->count = to - from;

  if (from == 0 && phdr)
    {
      /* Include the headers in the first PT_LOAD segment.  */
      m->includes_filehdr = 1;
      m->includes_phdrs = 1;
    }

  return m;
}

/* Set up a mapping from BFD sections to program segments.  */

static boolean
map_sections_to_segments (abfd)
     bfd *abfd;
{
  asection **sections = NULL;
  asection *s;
  unsigned int i;
  unsigned int count;
  struct elf_segment_map *mfirst;
  struct elf_segment_map **pm;
  struct elf_segment_map *m;
  asection *last_hdr;
  unsigned int phdr_index;
  bfd_vma maxpagesize;
  asection **hdrpp;
  boolean phdr_in_segment = true;
  boolean writable;
  asection *dynsec;

  if (elf_tdata (abfd)->segment_map != NULL)
    return true;

  if (bfd_count_sections (abfd) == 0)
    return true;

  /* Select the allocated sections, and sort them.  */

  sections = (asection **) bfd_malloc (bfd_count_sections (abfd)
				       * sizeof (asection *));
  if (sections == NULL)
    goto error_return;

  i = 0;
  for (s = abfd->sections; s != NULL; s = s->next)
    {
      if ((s->flags & SEC_ALLOC) != 0)
	{
	  sections[i] = s;
	  ++i;
	}
    }
  BFD_ASSERT (i <= bfd_count_sections (abfd));
  count = i;

  qsort (sections, (size_t) count, sizeof (asection *), elf_sort_sections);

  /* Build the mapping.  */

  mfirst = NULL;
  pm = &mfirst;

  /* If we have a .interp section, then create a PT_PHDR segment for
     the program headers and a PT_INTERP segment for the .interp
     section.  */
  s = bfd_get_section_by_name (abfd, ".interp");
  if (s != NULL && (s->flags & SEC_LOAD) != 0)
    {
      m = ((struct elf_segment_map *)
	   bfd_zalloc (abfd, sizeof (struct elf_segment_map)));
      if (m == NULL)
	goto error_return;
      m->next = NULL;
      m->p_type = PT_PHDR;
      /* FIXME: UnixWare and Solaris set PF_X, Irix 5 does not.  */
      m->p_flags = PF_R | PF_X;
      m->p_flags_valid = 1;
      m->includes_phdrs = 1;

      *pm = m;
      pm = &m->next;

      m = ((struct elf_segment_map *)
	   bfd_zalloc (abfd, sizeof (struct elf_segment_map)));
      if (m == NULL)
	goto error_return;
      m->next = NULL;
      m->p_type = PT_INTERP;
      m->count = 1;
      m->sections[0] = s;

      *pm = m;
      pm = &m->next;
    }

  /* Look through the sections.  We put sections in the same program
     segment when the start of the second section can be placed within
     a few bytes of the end of the first section.  */
  last_hdr = NULL;
  phdr_index = 0;
  maxpagesize = get_elf_backend_data (abfd)->maxpagesize;
  writable = false;
  dynsec = bfd_get_section_by_name (abfd, ".dynamic");
  if (dynsec != NULL
      && (dynsec->flags & SEC_LOAD) == 0)
    dynsec = NULL;

  /* Deal with -Ttext or something similar such that the first section
     is not adjacent to the program headers.  This is an
     approximation, since at this point we don't know exactly how many
     program headers we will need.  */
  if (count > 0)
    {
      bfd_size_type phdr_size;

      phdr_size = elf_tdata (abfd)->program_header_size;
      if (phdr_size == 0)
	phdr_size = get_elf_backend_data (abfd)->s->sizeof_phdr;
      if ((abfd->flags & D_PAGED) == 0
	  || sections[0]->lma < phdr_size
	  || sections[0]->lma % maxpagesize < phdr_size % maxpagesize)
	phdr_in_segment = false;
    }

  for (i = 0, hdrpp = sections; i < count; i++, hdrpp++)
    {
      asection *hdr;
      boolean new_segment;

      hdr = *hdrpp;

      /* See if this section and the last one will fit in the same
         segment.  */

      if (last_hdr == NULL)
	{
	  /* If we don't have a segment yet, then we don't need a new
	     one (we build the last one after this loop).  */
	  new_segment = false;
	}
      else if (last_hdr->lma - last_hdr->vma != hdr->lma - hdr->vma)
	{
	  /* If this section has a different relation between the
             virtual address and the load address, then we need a new
             segment.  */
	  new_segment = true;
	}
      else if (BFD_ALIGN (last_hdr->lma + last_hdr->_raw_size, maxpagesize)
	       < BFD_ALIGN (hdr->lma, maxpagesize))
	{
	  /* If putting this section in this segment would force us to
             skip a page in the segment, then we need a new segment.  */
	  new_segment = true;
	}
      else if ((last_hdr->flags & SEC_LOAD) == 0
	       && (hdr->flags & SEC_LOAD) != 0)
	{
	  /* We don't want to put a loadable section after a
             nonloadable section in the same segment.  */
	  new_segment = true;
	}
      else if ((abfd->flags & D_PAGED) == 0)
	{
	  /* If the file is not demand paged, which means that we
             don't require the sections to be correctly aligned in the
             file, then there is no other reason for a new segment.  */
	  new_segment = false;
	}
      else if (! writable
	       && (hdr->flags & SEC_READONLY) == 0
	       && (BFD_ALIGN (last_hdr->lma + last_hdr->_raw_size, maxpagesize)
		   == hdr->lma))
	{
	  /* We don't want to put a writable section in a read only
             segment, unless they are on the same page in memory
             anyhow.  We already know that the last section does not
             bring us past the current section on the page, so the
             only case in which the new section is not on the same
             page as the previous section is when the previous section
             ends precisely on a page boundary.  */
	  new_segment = true;
	}
      else
	{
	  /* Otherwise, we can use the same segment.  */
	  new_segment = false;
	}

      if (! new_segment)
	{
	  if ((hdr->flags & SEC_READONLY) == 0)
	    writable = true;
	  last_hdr = hdr;
	  continue;
	}

      /* We need a new program segment.  We must create a new program
         header holding all the sections from phdr_index until hdr.  */

      m = make_mapping (abfd, sections, phdr_index, i, phdr_in_segment);
      if (m == NULL)
	goto error_return;

      *pm = m;
      pm = &m->next;

      if ((hdr->flags & SEC_READONLY) == 0)
	writable = true;
      else
	writable = false;

      last_hdr = hdr;
      phdr_index = i;
      phdr_in_segment = false;
    }

  /* Create a final PT_LOAD program segment.  */
  if (last_hdr != NULL)
    {
      m = make_mapping (abfd, sections, phdr_index, i, phdr_in_segment);
      if (m == NULL)
	goto error_return;

      *pm = m;
      pm = &m->next;
    }

  /* If there is a .dynamic section, throw in a PT_DYNAMIC segment.  */
  if (dynsec != NULL)
    {
      m = ((struct elf_segment_map *)
	   bfd_zalloc (abfd, sizeof (struct elf_segment_map)));
      if (m == NULL)
	goto error_return;
      m->next = NULL;
      m->p_type = PT_DYNAMIC;
      m->count = 1;
      m->sections[0] = dynsec;

      *pm = m;
      pm = &m->next;
    }

  /* For each loadable .note section, add a PT_NOTE segment.  We don't
     use bfd_get_section_by_name, because if we link together
     nonloadable .note sections and loadable .note sections, we will
     generate two .note sections in the output file.  FIXME: Using
     names for section types is bogus anyhow.  */
  for (s = abfd->sections; s != NULL; s = s->next)
    {
      if ((s->flags & SEC_LOAD) != 0
	  && strncmp (s->name, ".note", 5) == 0)
	{
	  m = ((struct elf_segment_map *)
	       bfd_zalloc (abfd, sizeof (struct elf_segment_map)));
	  if (m == NULL)
	    goto error_return;
	  m->next = NULL;
	  m->p_type = PT_NOTE;
	  m->count = 1;
	  m->sections[0] = s;

	  *pm = m;
	  pm = &m->next;
	}
    }

  free (sections);
  sections = NULL;

  elf_tdata (abfd)->segment_map = mfirst;
  return true;

 error_return:
  if (sections != NULL)
    free (sections);
  return false;
}

/* Sort sections by address.  */

static int
elf_sort_sections (arg1, arg2)
     const PTR arg1;
     const PTR arg2;
{
  const asection *sec1 = *(const asection **) arg1;
  const asection *sec2 = *(const asection **) arg2;

  /* Sort by LMA first, since this is the address used to
     place the section into a segment.  */
  if (sec1->lma < sec2->lma)
    return -1;
  else if (sec1->lma > sec2->lma)
    return 1;

  /* Then sort by VMA.  Normally the LMA and the VMA will be
     the same, and this will do nothing.  */
  if (sec1->vma < sec2->vma)
    return -1;
  else if (sec1->vma > sec2->vma)
    return 1;

  /* Put !SEC_LOAD sections after SEC_LOAD ones.  */

#define TOEND(x) (((x)->flags & SEC_LOAD) == 0)

  if (TOEND (sec1))
    {
      if (TOEND (sec2))
	return sec1->target_index - sec2->target_index;
      else
	return 1;
    }

  if (TOEND (sec2))
    return -1;

#undef TOEND

  /* Sort by size, to put zero sized sections before others at the
     same address.  */

  if (sec1->_raw_size < sec2->_raw_size)
    return -1;
  if (sec1->_raw_size > sec2->_raw_size)
    return 1;

  return sec1->target_index - sec2->target_index;
}

/* Assign file positions to the sections based on the mapping from
   sections to segments.  This function also sets up some fields in
   the file header, and writes out the program headers.  */

static boolean
assign_file_positions_for_segments (abfd)
     bfd *abfd;
{
  const struct elf_backend_data *bed = get_elf_backend_data (abfd);
  unsigned int count;
  struct elf_segment_map *m;
  unsigned int alloc;
  Elf_Internal_Phdr *phdrs;
  file_ptr off, voff;
  bfd_vma filehdr_vaddr, filehdr_paddr;
  bfd_vma phdrs_vaddr, phdrs_paddr;
  Elf_Internal_Phdr *p;

  if (elf_tdata (abfd)->segment_map == NULL)
    {
      if (! map_sections_to_segments (abfd))
	return false;
    }

  if (bed->elf_backend_modify_segment_map)
    {
      if (! (*bed->elf_backend_modify_segment_map) (abfd))
	return false;
    }

  count = 0;
  for (m = elf_tdata (abfd)->segment_map; m != NULL; m = m->next)
    ++count;

  elf_elfheader (abfd)->e_phoff = bed->s->sizeof_ehdr;
  elf_elfheader (abfd)->e_phentsize = bed->s->sizeof_phdr;
  elf_elfheader (abfd)->e_phnum = count;

  if (count == 0)
    return true;

  /* If we already counted the number of program segments, make sure
     that we allocated enough space.  This happens when SIZEOF_HEADERS
     is used in a linker script.  */
  alloc = elf_tdata (abfd)->program_header_size / bed->s->sizeof_phdr;
  if (alloc != 0 && count > alloc)
    {
      ((*_bfd_error_handler)
       (_("%s: Not enough room for program headers (allocated %u, need %u)"),
	bfd_get_filename (abfd), alloc, count));
      bfd_set_error (bfd_error_bad_value);
      return false;
    }

  if (alloc == 0)
    alloc = count;

  phdrs = ((Elf_Internal_Phdr *)
	   bfd_alloc (abfd, alloc * sizeof (Elf_Internal_Phdr)));
  if (phdrs == NULL)
    return false;

  off = bed->s->sizeof_ehdr;
  off += alloc * bed->s->sizeof_phdr;

  filehdr_vaddr = 0;
  filehdr_paddr = 0;
  phdrs_vaddr = 0;
  phdrs_paddr = 0;

  for (m = elf_tdata (abfd)->segment_map, p = phdrs;
       m != NULL;
       m = m->next, p++)
    {
      unsigned int i;
      asection **secpp;

      /* If elf_segment_map is not from map_sections_to_segments, the
         sections may not be correctly ordered.  */
      if (m->count > 0)
	qsort (m->sections, (size_t) m->count, sizeof (asection *),
	       elf_sort_sections);

      p->p_type = m->p_type;
      p->p_flags = m->p_flags;

      if (p->p_type == PT_LOAD
	  && m->count > 0
	  && (m->sections[0]->flags & SEC_ALLOC) != 0)
	{
	  if ((abfd->flags & D_PAGED) != 0)
	    off += (m->sections[0]->vma - off) % bed->maxpagesize;
	  else
	    {
	      bfd_size_type align;

	      align = 0;
	      for (i = 0, secpp = m->sections; i < m->count; i++, secpp++)
		{
		  bfd_size_type secalign;

		  secalign = bfd_get_section_alignment (abfd, *secpp);
		  if (secalign > align)
		    align = secalign;
		}

	      off += (m->sections[0]->vma - off) % (1 << align);
	    }
	}

      if (m->count == 0)
	p->p_vaddr = 0;
      else
	p->p_vaddr = m->sections[0]->vma;

      if (m->p_paddr_valid)
	p->p_paddr = m->p_paddr;
      else if (m->count == 0)
	p->p_paddr = 0;
      else
	p->p_paddr = m->sections[0]->lma;

      if (p->p_type == PT_LOAD
	  && (abfd->flags & D_PAGED) != 0)
	p->p_align = bed->maxpagesize;
      else if (m->count == 0)
	p->p_align = bed->s->file_align;
      else
	p->p_align = 0;

      p->p_offset = 0;
      p->p_filesz = 0;
      p->p_memsz = 0;

      if (m->includes_filehdr)
	{
	  if (! m->p_flags_valid)
	    p->p_flags |= PF_R;
	  p->p_offset = 0;
	  p->p_filesz = bed->s->sizeof_ehdr;
	  p->p_memsz = bed->s->sizeof_ehdr;
	  if (m->count > 0)
	    {
	      BFD_ASSERT (p->p_type == PT_LOAD);

	      if (p->p_vaddr < (bfd_vma) off)
		{
		  _bfd_error_handler (_("%s: Not enough room for program headers, try linking with -N"),
				      bfd_get_filename (abfd));
		  bfd_set_error (bfd_error_bad_value);
		  return false;
		}

	      p->p_vaddr -= off;
	      if (! m->p_paddr_valid)
		p->p_paddr -= off;
	    }
	  if (p->p_type == PT_LOAD)
	    {
	      filehdr_vaddr = p->p_vaddr;
	      filehdr_paddr = p->p_paddr;
	    }
	}

      if (m->includes_phdrs)
	{
	  if (! m->p_flags_valid)
	    p->p_flags |= PF_R;

	  if (m->includes_filehdr)
	    {
	      if (p->p_type == PT_LOAD)
		{
		  phdrs_vaddr = p->p_vaddr + bed->s->sizeof_ehdr;
		  phdrs_paddr = p->p_paddr + bed->s->sizeof_ehdr;
		}
	    }
	  else
	    {
	      p->p_offset = bed->s->sizeof_ehdr;

	      if (m->count > 0)
		{
		  BFD_ASSERT (p->p_type == PT_LOAD);
		  p->p_vaddr -= off - p->p_offset;
		  if (! m->p_paddr_valid)
		    p->p_paddr -= off - p->p_offset;
		}

	      if (p->p_type == PT_LOAD)
		{
		  phdrs_vaddr = p->p_vaddr;
		  phdrs_paddr = p->p_paddr;
		}
	      else
		phdrs_vaddr = bed->maxpagesize + bed->s->sizeof_ehdr;
	    }

	  p->p_filesz += alloc * bed->s->sizeof_phdr;
	  p->p_memsz += alloc * bed->s->sizeof_phdr;
	}

      if (p->p_type == PT_LOAD
	  || (p->p_type == PT_NOTE && bfd_get_format (abfd) == bfd_core))
	{
	  if (! m->includes_filehdr && ! m->includes_phdrs)
	    p->p_offset = off;
	  else
	    {
	      file_ptr adjust;

	      adjust = off - (p->p_offset + p->p_filesz);
	      p->p_filesz += adjust;
	      p->p_memsz += adjust;
	    }
	}

      voff = off;

      for (i = 0, secpp = m->sections; i < m->count; i++, secpp++)
	{
	  asection *sec;
	  flagword flags;
	  bfd_size_type align;

	  sec = *secpp;
	  flags = sec->flags;
	  align = 1 << bfd_get_section_alignment (abfd, sec);

	  /* The section may have artificial alignment forced by a
	     link script.  Notice this case by the gap between the
	     cumulative phdr vma and the section's vma.  */
	  if (p->p_vaddr + p->p_memsz < sec->vma)
	    {
	      bfd_vma adjust = sec->vma - (p->p_vaddr + p->p_memsz);

	      p->p_memsz += adjust;
	      off += adjust;
	      voff += adjust;
	      if ((flags & SEC_LOAD) != 0)
		p->p_filesz += adjust;
	    }

	  if (p->p_type == PT_LOAD)
	    {
	      bfd_signed_vma adjust;

	      if ((flags & SEC_LOAD) != 0)
		{
		  adjust = sec->lma - (p->p_paddr + p->p_memsz);
		  if (adjust < 0)
		    adjust = 0;
		}
	      else if ((flags & SEC_ALLOC) != 0)
		{
		  /* The section VMA must equal the file position
		     modulo the page size.  FIXME: I'm not sure if
		     this adjustment is really necessary.  We used to
		     not have the SEC_LOAD case just above, and then
		     this was necessary, but now I'm not sure.  */
		  if ((abfd->flags & D_PAGED) != 0)
		    adjust = (sec->vma - voff) % bed->maxpagesize;
		  else
		    adjust = (sec->vma - voff) % align;
		}
	      else
		adjust = 0;

	      if (adjust != 0)
		{
		  if (i == 0)
		    {
		      (* _bfd_error_handler)
			(_("Error: First section in segment (%s) starts at 0x%x"),
			 bfd_section_name (abfd, sec), sec->lma);
		      (* _bfd_error_handler)
			(_("       whereas segment starts at 0x%x"),
			 p->p_paddr);

		      return false;
		    }
		  p->p_memsz += adjust;
		  off += adjust;
		  voff += adjust;
		  if ((flags & SEC_LOAD) != 0)
		    p->p_filesz += adjust;
		}

	      sec->filepos = off;

	      /* We check SEC_HAS_CONTENTS here because if NOLOAD is
                 used in a linker script we may have a section with
                 SEC_LOAD clear but which is supposed to have
                 contents.  */
	      if ((flags & SEC_LOAD) != 0
		  || (flags & SEC_HAS_CONTENTS) != 0)
		off += sec->_raw_size;

	      if ((flags & SEC_ALLOC) != 0)
		voff += sec->_raw_size;
	    }

	  if (p->p_type == PT_NOTE && bfd_get_format (abfd) == bfd_core)
	    {
	      if (i == 0)	/* the actual "note" segment */
		{		/* this one actually contains everything. */
		  sec->filepos = off;
		  p->p_filesz = sec->_raw_size;
		  off += sec->_raw_size;
		  voff = off;
		}
	      else	/* fake sections -- don't need to be written */
		{
		  sec->filepos = 0;
		  sec->_raw_size = 0;
		  flags = sec->flags = 0;	/* no contents */
		}
	      p->p_memsz = 0;
	      p->p_align = 1;
	    }
	  else
	    {
	      p->p_memsz += sec->_raw_size;

	      if ((flags & SEC_LOAD) != 0)
		p->p_filesz += sec->_raw_size;

	      if (align > p->p_align
		  && (p->p_type != PT_LOAD || (abfd->flags & D_PAGED) == 0))
		p->p_align = align;
	    }

	  if (! m->p_flags_valid)
	    {
	      p->p_flags |= PF_R;
	      if ((flags & SEC_CODE) != 0)
		p->p_flags |= PF_X;
	      if ((flags & SEC_READONLY) == 0)
		p->p_flags |= PF_W;
	    }
	}
    }

  /* Now that we have set the section file positions, we can set up
     the file positions for the non PT_LOAD segments.  */
  for (m = elf_tdata (abfd)->segment_map, p = phdrs;
       m != NULL;
       m = m->next, p++)
    {
      if (p->p_type != PT_LOAD && m->count > 0)
	{
	  BFD_ASSERT (! m->includes_filehdr && ! m->includes_phdrs);
	  p->p_offset = m->sections[0]->filepos;
	}
      if (m->count == 0)
	{
	  if (m->includes_filehdr)
	    {
	      p->p_vaddr = filehdr_vaddr;
	      if (! m->p_paddr_valid)
		p->p_paddr = filehdr_paddr;
	    }
	  else if (m->includes_phdrs)
	    {
	      p->p_vaddr = phdrs_vaddr;
	      if (! m->p_paddr_valid)
		p->p_paddr = phdrs_paddr;
	    }
	}
    }

  /* Clear out any program headers we allocated but did not use.  */
  for (; count < alloc; count++, p++)
    {
      memset (p, 0, sizeof *p);
      p->p_type = PT_NULL;
    }

  elf_tdata (abfd)->phdr = phdrs;

  elf_tdata (abfd)->next_file_pos = off;

  /* Write out the program headers.  */
  if (bfd_seek (abfd, bed->s->sizeof_ehdr, SEEK_SET) != 0
      || bed->s->write_out_phdrs (abfd, phdrs, alloc) != 0)
    return false;

  return true;
}

/* Get the size of the program header.

   If this is called by the linker before any of the section VMA's are set, it
   can't calculate the correct value for a strange memory layout.  This only
   happens when SIZEOF_HEADERS is used in a linker script.  In this case,
   SORTED_HDRS is NULL and we assume the normal scenario of one text and one
   data segment (exclusive of .interp and .dynamic).

   ??? User written scripts must either not use SIZEOF_HEADERS, or assume there
   will be two segments.  */

static bfd_size_type
get_program_header_size (abfd)
     bfd *abfd;
{
  size_t segs;
  asection *s;
  struct elf_backend_data *bed = get_elf_backend_data (abfd);

  /* We can't return a different result each time we're called.  */
  if (elf_tdata (abfd)->program_header_size != 0)
    return elf_tdata (abfd)->program_header_size;

  if (elf_tdata (abfd)->segment_map != NULL)
    {
      struct elf_segment_map *m;

      segs = 0;
      for (m = elf_tdata (abfd)->segment_map; m != NULL; m = m->next)
	++segs;
      elf_tdata (abfd)->program_header_size = segs * bed->s->sizeof_phdr;
      return elf_tdata (abfd)->program_header_size;
    }

  /* Assume we will need exactly two PT_LOAD segments: one for text
     and one for data.  */
  segs = 2;

  s = bfd_get_section_by_name (abfd, ".interp");
  if (s != NULL && (s->flags & SEC_LOAD) != 0)
    {
      /* If we have a loadable interpreter section, we need a
	 PT_INTERP segment.  In this case, assume we also need a
	 PT_PHDR segment, although that may not be true for all
	 targets.  */
      segs += 2;
    }

  if (bfd_get_section_by_name (abfd, ".dynamic") != NULL)
    {
      /* We need a PT_DYNAMIC segment.  */
      ++segs;
    }

  for (s = abfd->sections; s != NULL; s = s->next)
    {
      if ((s->flags & SEC_LOAD) != 0
	  && strncmp (s->name, ".note", 5) == 0)
	{
	  /* We need a PT_NOTE segment.  */
	  ++segs;
	}
    }

  /* Let the backend count up any program headers it might need.  */
  if (bed->elf_backend_additional_program_headers)
    {
      int a;

      a = (*bed->elf_backend_additional_program_headers) (abfd);
      if (a == -1)
	abort ();
      segs += a;
    }

  elf_tdata (abfd)->program_header_size = segs * bed->s->sizeof_phdr;
  return elf_tdata (abfd)->program_header_size;
}

/* Work out the file positions of all the sections.  This is called by
   _bfd_elf_compute_section_file_positions.  All the section sizes and
   VMAs must be known before this is called.

   We do not consider reloc sections at this point, unless they form
   part of the loadable image.  Reloc sections are assigned file
   positions in assign_file_positions_for_relocs, which is called by
   write_object_contents and final_link.

   We also don't set the positions of the .symtab and .strtab here.  */

static boolean
assign_file_positions_except_relocs (abfd)
     bfd *abfd;
{
  struct elf_obj_tdata * const tdata = elf_tdata (abfd);
  Elf_Internal_Ehdr * const i_ehdrp = elf_elfheader (abfd);
  Elf_Internal_Shdr ** const i_shdrpp = elf_elfsections (abfd);
  file_ptr off;
  struct elf_backend_data *bed = get_elf_backend_data (abfd);

  if ((abfd->flags & (EXEC_P | DYNAMIC)) == 0
      && bfd_get_format (abfd) != bfd_core)
    {
      Elf_Internal_Shdr **hdrpp;
      unsigned int i;

      /* Start after the ELF header.  */
      off = i_ehdrp->e_ehsize;

      /* We are not creating an executable, which means that we are
	 not creating a program header, and that the actual order of
	 the sections in the file is unimportant.  */
      for (i = 1, hdrpp = i_shdrpp + 1; i < i_ehdrp->e_shnum; i++, hdrpp++)
	{
	  Elf_Internal_Shdr *hdr;

	  hdr = *hdrpp;
	  if (hdr->sh_type == SHT_REL || hdr->sh_type == SHT_RELA)
	    {
	      hdr->sh_offset = -1;
	      continue;
	    }
	  if (i == tdata->symtab_section
	      || i == tdata->strtab_section)
	    {
	      hdr->sh_offset = -1;
	      continue;
	    }

	  off = _bfd_elf_assign_file_position_for_section (hdr, off, true);
	}
    }
  else
    {
      unsigned int i;
      Elf_Internal_Shdr **hdrpp;

      /* Assign file positions for the loaded sections based on the
         assignment of sections to segments.  */
      if (! assign_file_positions_for_segments (abfd))
	return false;

      /* Assign file positions for the other sections.  */

      off = elf_tdata (abfd)->next_file_pos;
      for (i = 1, hdrpp = i_shdrpp + 1; i < i_ehdrp->e_shnum; i++, hdrpp++)
	{
	  Elf_Internal_Shdr *hdr;

	  hdr = *hdrpp;
	  if (hdr->bfd_section != NULL
	      && hdr->bfd_section->filepos != 0)
	    hdr->sh_offset = hdr->bfd_section->filepos;
	  else if ((hdr->sh_flags & SHF_ALLOC) != 0)
	    {
	      ((*_bfd_error_handler)
	       (_("%s: warning: allocated section `%s' not in segment"),
		bfd_get_filename (abfd),
		(hdr->bfd_section == NULL
		 ? "*unknown*"
		 : hdr->bfd_section->name)));
	      if ((abfd->flags & D_PAGED) != 0)
		off += (hdr->sh_addr - off) % bed->maxpagesize;
	      else
		off += (hdr->sh_addr - off) % hdr->sh_addralign;
	      off = _bfd_elf_assign_file_position_for_section (hdr, off,
							       false);
	    }
	  else if (hdr->sh_type == SHT_REL
		   || hdr->sh_type == SHT_RELA
		   || hdr == i_shdrpp[tdata->symtab_section]
		   || hdr == i_shdrpp[tdata->strtab_section])
	    hdr->sh_offset = -1;
	  else
	    off = _bfd_elf_assign_file_position_for_section (hdr, off, true);
	}
    }

  /* Place the section headers.  */
  off = align_file_position (off, bed->s->file_align);
  i_ehdrp->e_shoff = off;
  off += i_ehdrp->e_shnum * i_ehdrp->e_shentsize;

  elf_tdata (abfd)->next_file_pos = off;

  return true;
}

static boolean
prep_headers (abfd)
     bfd *abfd;
{
  Elf_Internal_Ehdr *i_ehdrp;	/* Elf file header, internal form */
  Elf_Internal_Phdr *i_phdrp = 0; /* Program header table, internal form */
  Elf_Internal_Shdr **i_shdrp;	/* Section header table, internal form */
  int count;
  struct bfd_strtab_hash *shstrtab;
  struct elf_backend_data *bed = get_elf_backend_data (abfd);

  i_ehdrp = elf_elfheader (abfd);
  i_shdrp = elf_elfsections (abfd);

  shstrtab = _bfd_elf_stringtab_init ();
  if (shstrtab == NULL)
    return false;

  elf_shstrtab (abfd) = shstrtab;

  i_ehdrp->e_ident[EI_MAG0] = ELFMAG0;
  i_ehdrp->e_ident[EI_MAG1] = ELFMAG1;
  i_ehdrp->e_ident[EI_MAG2] = ELFMAG2;
  i_ehdrp->e_ident[EI_MAG3] = ELFMAG3;

  i_ehdrp->e_ident[EI_CLASS] = bed->s->elfclass;
  i_ehdrp->e_ident[EI_DATA] =
    bfd_big_endian (abfd) ? ELFDATA2MSB : ELFDATA2LSB;
  i_ehdrp->e_ident[EI_VERSION] = bed->s->ev_current;

  i_ehdrp->e_ident[EI_OSABI] = ELFOSABI_NONE;
  i_ehdrp->e_ident[EI_ABIVERSION] = 0;

  for (count = EI_PAD; count < EI_NIDENT; count++)
    i_ehdrp->e_ident[count] = 0;

  if ((abfd->flags & DYNAMIC) != 0)
    i_ehdrp->e_type = ET_DYN;
  else if ((abfd->flags & EXEC_P) != 0)
    i_ehdrp->e_type = ET_EXEC;
  else if (bfd_get_format (abfd) == bfd_core)
    i_ehdrp->e_type = ET_CORE;
  else
    i_ehdrp->e_type = ET_REL;

  switch (bfd_get_arch (abfd))
    {
    case bfd_arch_unknown:
      i_ehdrp->e_machine = EM_NONE;
      break;
    case bfd_arch_sparc:
      if (bed->s->arch_size == 64)
	i_ehdrp->e_machine = EM_SPARCV9;
      else
	i_ehdrp->e_machine = EM_SPARC;
      break;
    case bfd_arch_i370:
      i_ehdrp->e_machine = EM_S370;
      break;
    case bfd_arch_i386:
      i_ehdrp->e_machine = EM_386;
      break;
    case bfd_arch_m68k:
      i_ehdrp->e_machine = EM_68K;
      break;
    case bfd_arch_m88k:
      i_ehdrp->e_machine = EM_88K;
      break;
    case bfd_arch_i860:
      i_ehdrp->e_machine = EM_860;
      break;
    case bfd_arch_i960:
      i_ehdrp->e_machine = EM_960;
      break;
    case bfd_arch_mips:	/* MIPS Rxxxx */
      i_ehdrp->e_machine = EM_MIPS;	/* only MIPS R3000 */
      break;
    case bfd_arch_hppa:
      i_ehdrp->e_machine = EM_PARISC;
      break;
    case bfd_arch_powerpc:
      i_ehdrp->e_machine = EM_PPC;
      break;
    case bfd_arch_alpha:
      i_ehdrp->e_machine = EM_ALPHA;
      break;
    case bfd_arch_sh:
      i_ehdrp->e_machine = EM_SH;
      break;
    case bfd_arch_d10v:
      i_ehdrp->e_machine = EM_CYGNUS_D10V;
      break;
    case bfd_arch_d30v:
      i_ehdrp->e_machine = EM_CYGNUS_D30V;
      break;
    case bfd_arch_fr30:
      i_ehdrp->e_machine = EM_CYGNUS_FR30;
      break;
    case bfd_arch_mcore:
      i_ehdrp->e_machine = EM_MCORE;
      break;
    case bfd_arch_avr:
      i_ehdrp->e_machine = EM_AVR;
      break;
    case bfd_arch_v850:
      switch (bfd_get_mach (abfd))
	{
	default:
	case 0:               i_ehdrp->e_machine = EM_CYGNUS_V850; break;
	}
      break;
   case bfd_arch_arc:
      i_ehdrp->e_machine = EM_CYGNUS_ARC;
      break;
   case bfd_arch_arm:
      i_ehdrp->e_machine = EM_ARM;
      break;
    case bfd_arch_m32r:
      i_ehdrp->e_machine = EM_CYGNUS_M32R;
      break;
    case bfd_arch_mn10200:
      i_ehdrp->e_machine = EM_CYGNUS_MN10200;
      break;
    case bfd_arch_mn10300:
      i_ehdrp->e_machine = EM_CYGNUS_MN10300;
      break;
    case bfd_arch_pj:
      i_ehdrp->e_machine = EM_PJ;
      break;
      /* also note that EM_M32, AT&T WE32100 is unknown to bfd */
    default:
      i_ehdrp->e_machine = EM_NONE;
    }
  i_ehdrp->e_version = bed->s->ev_current;
  i_ehdrp->e_ehsize = bed->s->sizeof_ehdr;

  /* no program header, for now. */
  i_ehdrp->e_phoff = 0;
  i_ehdrp->e_phentsize = 0;
  i_ehdrp->e_phnum = 0;

  /* each bfd section is section header entry */
  i_ehdrp->e_entry = bfd_get_start_address (abfd);
  i_ehdrp->e_shentsize = bed->s->sizeof_shdr;

  /* if we're building an executable, we'll need a program header table */
  if (abfd->flags & EXEC_P)
    {
      /* it all happens later */
#if 0
      i_ehdrp->e_phentsize = sizeof (Elf_External_Phdr);

      /* elf_build_phdrs() returns a (NULL-terminated) array of
	 Elf_Internal_Phdrs */
      i_phdrp = elf_build_phdrs (abfd, i_ehdrp, i_shdrp, &i_ehdrp->e_phnum);
      i_ehdrp->e_phoff = outbase;
      outbase += i_ehdrp->e_phentsize * i_ehdrp->e_phnum;
#endif
    }
  else
    {
      i_ehdrp->e_phentsize = 0;
      i_phdrp = 0;
      i_ehdrp->e_phoff = 0;
    }

  elf_tdata (abfd)->symtab_hdr.sh_name =
    (unsigned int) _bfd_stringtab_add (shstrtab, ".symtab", true, false);
  elf_tdata (abfd)->strtab_hdr.sh_name =
    (unsigned int) _bfd_stringtab_add (shstrtab, ".strtab", true, false);
  elf_tdata (abfd)->shstrtab_hdr.sh_name =
    (unsigned int) _bfd_stringtab_add (shstrtab, ".shstrtab", true, false);
  if (elf_tdata (abfd)->symtab_hdr.sh_name == (unsigned int) -1
      || elf_tdata (abfd)->symtab_hdr.sh_name == (unsigned int) -1
      || elf_tdata (abfd)->shstrtab_hdr.sh_name == (unsigned int) -1)
    return false;

  return true;
}

/* Assign file positions for all the reloc sections which are not part
   of the loadable file image.  */

void
_bfd_elf_assign_file_positions_for_relocs (abfd)
     bfd *abfd;
{
  file_ptr off;
  unsigned int i;
  Elf_Internal_Shdr **shdrpp;

  off = elf_tdata (abfd)->next_file_pos;

  for (i = 1, shdrpp = elf_elfsections (abfd) + 1;
       i < elf_elfheader (abfd)->e_shnum;
       i++, shdrpp++)
    {
      Elf_Internal_Shdr *shdrp;

      shdrp = *shdrpp;
      if ((shdrp->sh_type == SHT_REL || shdrp->sh_type == SHT_RELA)
	  && shdrp->sh_offset == -1)
	off = _bfd_elf_assign_file_position_for_section (shdrp, off, true);
    }

  elf_tdata (abfd)->next_file_pos = off;
}

boolean
_bfd_elf_write_object_contents (abfd)
     bfd *abfd;
{
  struct elf_backend_data *bed = get_elf_backend_data (abfd);
  Elf_Internal_Ehdr *i_ehdrp;
  Elf_Internal_Shdr **i_shdrp;
  boolean failed;
  unsigned int count;

  if (! abfd->output_has_begun
      && ! _bfd_elf_compute_section_file_positions
             (abfd, (struct bfd_link_info *) NULL))
    return false;

  i_shdrp = elf_elfsections (abfd);
  i_ehdrp = elf_elfheader (abfd);

  failed = false;
  bfd_map_over_sections (abfd, bed->s->write_relocs, &failed);
  if (failed)
    return false;

  _bfd_elf_assign_file_positions_for_relocs (abfd);

  /* After writing the headers, we need to write the sections too... */
  for (count = 1; count < i_ehdrp->e_shnum; count++)
    {
      if (bed->elf_backend_section_processing)
	(*bed->elf_backend_section_processing) (abfd, i_shdrp[count]);
      if (i_shdrp[count]->contents)
	{
	  if (bfd_seek (abfd, i_shdrp[count]->sh_offset, SEEK_SET) != 0
	      || (bfd_write (i_shdrp[count]->contents, i_shdrp[count]->sh_size,
			     1, abfd)
		  != i_shdrp[count]->sh_size))
	    return false;
	}
    }

  /* Write out the section header names.  */
  if (bfd_seek (abfd, elf_tdata (abfd)->shstrtab_hdr.sh_offset, SEEK_SET) != 0
      || ! _bfd_stringtab_emit (abfd, elf_shstrtab (abfd)))
    return false;

  if (bed->elf_backend_final_write_processing)
    (*bed->elf_backend_final_write_processing) (abfd,
						elf_tdata (abfd)->linker);

  return bed->s->write_shdrs_and_ehdr (abfd);
}

boolean
_bfd_elf_write_corefile_contents (abfd)
     bfd *abfd;
{
  /* Hopefully this can be done just like an object file. */
  return _bfd_elf_write_object_contents (abfd);
}
/* given a section, search the header to find them... */
int
_bfd_elf_section_from_bfd_section (abfd, asect)
     bfd *abfd;
     struct sec *asect;
{
  struct elf_backend_data *bed = get_elf_backend_data (abfd);
  Elf_Internal_Shdr **i_shdrp = elf_elfsections (abfd);
  int index;
  Elf_Internal_Shdr *hdr;
  int maxindex = elf_elfheader (abfd)->e_shnum;

  for (index = 0; index < maxindex; index++)
    {
      hdr = i_shdrp[index];
      if (hdr->bfd_section == asect)
	return index;
    }

  if (bed->elf_backend_section_from_bfd_section)
    {
      for (index = 0; index < maxindex; index++)
	{
	  int retval;

	  hdr = i_shdrp[index];
	  retval = index;
	  if ((*bed->elf_backend_section_from_bfd_section)
	      (abfd, hdr, asect, &retval))
	    return retval;
	}
    }

  if (bfd_is_abs_section (asect))
    return SHN_ABS;
  if (bfd_is_com_section (asect))
    return SHN_COMMON;
  if (bfd_is_und_section (asect))
    return SHN_UNDEF;

  bfd_set_error (bfd_error_nonrepresentable_section);

  return -1;
}

/* Given a BFD symbol, return the index in the ELF symbol table, or -1
   on error.  */

int
_bfd_elf_symbol_from_bfd_symbol (abfd, asym_ptr_ptr)
     bfd *abfd;
     asymbol **asym_ptr_ptr;
{
  asymbol *asym_ptr = *asym_ptr_ptr;
  int idx;
  flagword flags = asym_ptr->flags;

  /* When gas creates relocations against local labels, it creates its
     own symbol for the section, but does put the symbol into the
     symbol chain, so udata is 0.  When the linker is generating
     relocatable output, this section symbol may be for one of the
     input sections rather than the output section.  */
  if (asym_ptr->udata.i == 0
      && (flags & BSF_SECTION_SYM)
      && asym_ptr->section)
    {
      int indx;

      if (asym_ptr->section->output_section != NULL)
	indx = asym_ptr->section->output_section->index;
      else
	indx = asym_ptr->section->index;
      if (elf_section_syms (abfd)[indx])
	asym_ptr->udata.i = elf_section_syms (abfd)[indx]->udata.i;
    }

  idx = asym_ptr->udata.i;

  if (idx == 0)
    {
      /* This case can occur when using --strip-symbol on a symbol
         which is used in a relocation entry.  */
      (*_bfd_error_handler)
	(_("%s: symbol `%s' required but not present"),
	 bfd_get_filename (abfd), bfd_asymbol_name (asym_ptr));
      bfd_set_error (bfd_error_no_symbols);
      return -1;
    }

#if DEBUG & 4
  {
    fprintf (stderr,
	     _("elf_symbol_from_bfd_symbol 0x%.8lx, name = %s, sym num = %d, flags = 0x%.8lx%s\n"),
	     (long) asym_ptr, asym_ptr->name, idx, flags,
	     elf_symbol_flags (flags));
    fflush (stderr);
  }
#endif

  return idx;
}

/* Copy private BFD data.  This copies any program header information.  */

static boolean
copy_private_bfd_data (ibfd, obfd)
     bfd *ibfd;
     bfd *obfd;
{
  Elf_Internal_Ehdr *iehdr;
  struct elf_segment_map *mfirst;
  struct elf_segment_map **pm;
  struct elf_segment_map *m;
  Elf_Internal_Phdr *p;
  unsigned int i;
  unsigned int num_segments;
  boolean phdr_included = false;

  if (bfd_get_flavour (ibfd) != bfd_target_elf_flavour
      || bfd_get_flavour (obfd) != bfd_target_elf_flavour)
    return true;

  if (elf_tdata (ibfd)->phdr == NULL)
    return true;

  iehdr = elf_elfheader (ibfd);

  mfirst = NULL;
  pm = &mfirst;

  num_segments = elf_elfheader (ibfd)->e_phnum;

#define IS_CONTAINED_BY(addr, len, bottom, phdr)		 	\
	  ((addr) >= (bottom)				 	  	\
	   && (   ((addr) + (len)) <= ((bottom) + (phdr)->p_memsz)	\
	       || ((addr) + (len)) <= ((bottom) + (phdr)->p_filesz)))

  /* Special case: corefile "NOTE" section containing regs, prpsinfo etc. */

#define IS_COREFILE_NOTE(p, s)                                          \
	    (p->p_type == PT_NOTE                                       \
	     && bfd_get_format (ibfd) == bfd_core                       \
	     && s->vma == 0 && s->lma == 0                              \
	     && (bfd_vma) s->filepos >= p->p_offset                     \
	     && (bfd_vma) s->filepos + s->_raw_size                     \
	     <= p->p_offset + p->p_filesz)

  /* The complicated case when p_vaddr is 0 is to handle the Solaris
     linker, which generates a PT_INTERP section with p_vaddr and
     p_memsz set to 0.  */

#define IS_SOLARIS_PT_INTERP(p, s)					\
	    (p->p_vaddr == 0						\
	     && p->p_filesz > 0						\
	     && (s->flags & SEC_HAS_CONTENTS) != 0			\
	     && s->_raw_size > 0					\
	     && (bfd_vma) s->filepos >= p->p_offset			\
	     && ((bfd_vma) s->filepos + s->_raw_size			\
		     <= p->p_offset + p->p_filesz))

  /* Scan through the segments specified in the program header
     of the input BFD.  */
  for (i = 0, p = elf_tdata (ibfd)->phdr; i < num_segments; i++, p++)
    {
      unsigned int csecs;
      asection *s;
      asection **sections;
      asection *os;
      unsigned int isec;
      bfd_vma matching_lma;
      bfd_vma suggested_lma;
      unsigned int j;

      /* For each section in the input BFD, decide if it should be
	 included in the current segment.  A section will be included
	 if it is within the address space of the segment, and it is
	 an allocated segment, and there is an output section
	 associated with it.  */
      csecs = 0;
      for (s = ibfd->sections; s != NULL; s = s->next)
	if (s->output_section != NULL)
	  {
	    if ((IS_CONTAINED_BY (s->vma, s->_raw_size, p->p_vaddr, p)
		 || IS_SOLARIS_PT_INTERP (p, s))
		&& (s->flags & SEC_ALLOC) != 0)
	      ++csecs;
	    else if (IS_COREFILE_NOTE (p, s))
	      ++csecs;
	  }

      /* Allocate a segment map big enough to contain all of the
	 sections we have selected.  */
      m = ((struct elf_segment_map *)
	   bfd_alloc (obfd,
		      (sizeof (struct elf_segment_map)
		       + ((size_t) csecs - 1) * sizeof (asection *))));
      if (m == NULL)
	return false;

      /* Initialise the fields of the segment map.  Default to
	 using the physical address of the segment in the input BFD.  */
      m->next          = NULL;
      m->p_type        = p->p_type;
      m->p_flags       = p->p_flags;
      m->p_flags_valid = 1;
      m->p_paddr       = p->p_paddr;
      m->p_paddr_valid = 1;

      /* Determine if this segment contains the ELF file header
	 and if it contains the program headers themselves.  */
      m->includes_filehdr = (p->p_offset == 0
			     && p->p_filesz >= iehdr->e_ehsize);

      m->includes_phdrs = 0;

      if (! phdr_included || p->p_type != PT_LOAD)
	{
	  m->includes_phdrs =
	    (p->p_offset <= (bfd_vma) iehdr->e_phoff
	     && (p->p_offset + p->p_filesz
		 >= ((bfd_vma) iehdr->e_phoff
		     + iehdr->e_phnum * iehdr->e_phentsize)));
	  if (p->p_type == PT_LOAD && m->includes_phdrs)
	    phdr_included = true;
	}

      if (csecs == 0)
	{
	  /* Special segments, such as the PT_PHDR segment, may contain
	     no sections, but ordinary, loadable segments should contain
	     something.  */

	  if (p->p_type == PT_LOAD)
	      _bfd_error_handler
		(_("%s: warning: Empty loadable segment detected\n"),
		 bfd_get_filename (ibfd));

	  m->count = 0;
	  *pm = m;
	  pm = &m->next;

	  continue;
	}

      /* Now scan the sections in the input BFD again and attempt
	 to add their corresponding output sections to the segment map.
	 The problem here is how to handle an output section which has
	 been moved (ie had its LMA changed).  There are four possibilities:

	 1. None of the sections have been moved.
	    In this case we can continue to use the segment LMA from the
	    input BFD.

	 2. All of the sections have been moved by the same amount.
	    In this case we can change the segment's LMA to match the LMA
	    of the first section.

	 3. Some of the sections have been moved, others have not.
	    In this case those sections which have not been moved can be
	    placed in the current segment which will have to have its size,
	    and possibly its LMA changed, and a new segment or segments will
	    have to be created to contain the other sections.

	 4. The sections have been moved, but not be the same amount.
	    In this case we can change the segment's LMA to match the LMA
	    of the first section and we will have to create a new segment
	    or segments to contain the other sections.

	 In order to save time, we allocate an array to hold the section
	 pointers that we are interested in.  As these sections get assigned
	 to a segment, they are removed from this array.  */

      sections = (asection **) bfd_malloc (sizeof (asection *) * csecs);
      if (sections == NULL)
	return false;

      /* Step One: Scan for segment vs section LMA conflicts.
	 Also add the sections to the section array allocated above.
	 Also add the sections to the current segment.  In the common
	 case, where the sections have not been moved, this means that
	 we have completely filled the segment, and there is nothing
	 more to do.  */

      isec = 0;
      matching_lma = 0;
      suggested_lma = 0;

      for (j = 0, s = ibfd->sections; s != NULL; s = s->next)
	{
	  os = s->output_section;

	  if ((((IS_CONTAINED_BY (s->vma, s->_raw_size, p->p_vaddr, p)
		 || IS_SOLARIS_PT_INTERP (p, s))
		&& (s->flags & SEC_ALLOC) != 0)
	       || IS_COREFILE_NOTE (p, s))
	      && os != NULL)
	    {
	      sections[j++] = s;

	      /* The Solaris native linker always sets p_paddr to 0.
		 We try to catch that case here, and set it to the
		 correct value.  */
	      if (p->p_paddr == 0
		  && p->p_vaddr != 0
		  && isec == 0
		  && os->lma != 0
		  && (os->vma == (p->p_vaddr
				  + (m->includes_filehdr
				     ? iehdr->e_ehsize
				     : 0)
				  + (m->includes_phdrs
				     ? iehdr->e_phnum * iehdr->e_phentsize
				     : 0))))
		m->p_paddr = p->p_vaddr;

	      /* Match up the physical address of the segment with the
		 LMA address of the output section.  */
	      if (IS_CONTAINED_BY (os->lma, os->_raw_size, m->p_paddr, p)
		  || IS_COREFILE_NOTE (p, s))
		{
		  if (matching_lma == 0)
		    matching_lma = os->lma;

		  /* We assume that if the section fits within the segment
		     that it does not overlap any other section within that
		     segment.  */
		  m->sections[isec++] = os;
		}
	      else if (suggested_lma == 0)
		suggested_lma = os->lma;
	    }
	}

      BFD_ASSERT (j == csecs);

      /* Step Two: Adjust the physical address of the current segment,
	 if necessary.  */
      if (isec == csecs)
	{
	  /* All of the sections fitted within the segment as currently
	     specified.  This is the default case.  Add the segment to
	     the list of built segments and carry on to process the next
	     program header in the input BFD.  */
	  m->count = csecs;
	  *pm = m;
	  pm = &m->next;

	  free (sections);
	  continue;
	}
      else
	{
	  if (matching_lma != 0)
	    {
	      /* At least one section fits inside the current segment.
		 Keep it, but modify its physical address to match the
		 LMA of the first section that fitted.  */

	      m->p_paddr = matching_lma;
	    }
	  else
	    {
	      /* None of the sections fitted inside the current segment.
		 Change the current segment's physical address to match
		 the LMA of the first section.  */

	      m->p_paddr = suggested_lma;
	    }

	  /* Offset the segment physical address from the lma to allow
	     for space taken up by elf headers.  */
	  if (m->includes_filehdr)
	    m->p_paddr -= iehdr->e_ehsize;

	  if (m->includes_phdrs)
	    m->p_paddr -= iehdr->e_phnum * iehdr->e_phentsize;
	}

      /* Step Three: Loop over the sections again, this time assigning
	 those that fit to the current segment and remvoing them from the
	 sections array; but making sure not to leave large gaps.  Once all
	 possible sections have been assigned to the current segment it is
	 added to the list of built segments and if sections still remain
	 to be assigned, a new segment is constructed before repeating
	 the loop.  */
      isec = 0;
      do
	{
	  m->count = 0;
	  suggested_lma = 0;

	  /* Fill the current segment with sections that fit.  */
	  for (j = 0; j < csecs; j++)
	    {
	      s = sections[j];

	      if (s == NULL)
		continue;

	      os = s->output_section;

	      if (IS_CONTAINED_BY (os->lma, os->_raw_size, m->p_paddr, p)
		  || IS_COREFILE_NOTE (p, s))
		{
		  if (m->count == 0)
		    {
		      /* If the first section in a segment does not start at
			 the beginning of the segment, then something is wrong.  */
		      if (os->lma != (m->p_paddr
				      + (m->includes_filehdr
					 ? iehdr->e_ehsize : 0)
				      + (m->includes_phdrs
					 ? iehdr->e_phnum * iehdr->e_phentsize
					 : 0)))
			abort ();
		    }
		  else
		    {
		      asection * prev_sec;
		      bfd_vma maxpagesize;

		      prev_sec = m->sections[m->count - 1];
		      maxpagesize = get_elf_backend_data (obfd)->maxpagesize;

		      /* If the gap between the end of the previous section
			 and the start of this section is more than maxpagesize
			 then we need to start a new segment.  */
		      if (BFD_ALIGN (prev_sec->lma + prev_sec->_raw_size, maxpagesize)
			  < BFD_ALIGN (os->lma, maxpagesize))
			{
			  if (suggested_lma == 0)
			    suggested_lma = os->lma;

			  continue;
			}
		    }

		  m->sections[m->count++] = os;
		  ++isec;
		  sections[j] = NULL;
		}
	      else if (suggested_lma == 0)
		suggested_lma = os->lma;
	    }

	  BFD_ASSERT (m->count > 0);

	  /* Add the current segment to the list of built segments.  */
	  *pm = m;
	  pm = &m->next;

	  if (isec < csecs)
	    {
	      /* We still have not allocated all of the sections to
		 segments.  Create a new segment here, initialise it
		 and carry on looping.  */

	      m = ((struct elf_segment_map *)
		   bfd_alloc (obfd,
			      (sizeof (struct elf_segment_map)
			       + ((size_t) csecs - 1) * sizeof (asection *))));
	      if (m == NULL)
		return false;

	      /* Initialise the fields of the segment map.  Set the physical
		 physical address to the LMA of the first section that has
		 not yet been assigned.  */

	      m->next             = NULL;
	      m->p_type           = p->p_type;
	      m->p_flags          = p->p_flags;
	      m->p_flags_valid    = 1;
	      m->p_paddr          = suggested_lma;
	      m->p_paddr_valid    = 1;
	      m->includes_filehdr = 0;
	      m->includes_phdrs   = 0;
	    }
	}
      while (isec < csecs);

      free (sections);
    }

  /* The Solaris linker creates program headers in which all the
     p_paddr fields are zero.  When we try to objcopy or strip such a
     file, we get confused.  Check for this case, and if we find it
     reset the p_paddr_valid fields.  */
  for (m = mfirst; m != NULL; m = m->next)
    if (m->p_paddr != 0)
      break;
  if (m == NULL)
    {
      for (m = mfirst; m != NULL; m = m->next)
	m->p_paddr_valid = 0;
    }

  elf_tdata (obfd)->segment_map = mfirst;

#if 0
  /* Final Step: Sort the segments into ascending order of physical address. */
  if (mfirst != NULL)
    {
      struct elf_segment_map* prev;

      prev = mfirst;
      for (m = mfirst->next; m != NULL; prev = m, m = m->next)
	{
	  /* Yes I know - its a bubble sort....*/
	  if (m->next != NULL && (m->next->p_paddr < m->p_paddr))
	    {
	      /* swap m and m->next */
	      prev->next = m->next;
	      m->next = m->next->next;
	      prev->next->next = m;

	      /* restart loop. */
	      m = mfirst;
	    }
	}
    }
#endif

#undef IS_CONTAINED_BY
#undef IS_SOLARIS_PT_INTERP
#undef IS_COREFILE_NOTE
  return true;
}

/* Copy private section information.  This copies over the entsize
   field, and sometimes the info field.  */

boolean
_bfd_elf_copy_private_section_data (ibfd, isec, obfd, osec)
     bfd *ibfd;
     asection *isec;
     bfd *obfd;
     asection *osec;
{
  Elf_Internal_Shdr *ihdr, *ohdr;

  if (ibfd->xvec->flavour != bfd_target_elf_flavour
      || obfd->xvec->flavour != bfd_target_elf_flavour)
    return true;

  /* Copy over private BFD data if it has not already been copied.
     This must be done here, rather than in the copy_private_bfd_data
     entry point, because the latter is called after the section
     contents have been set, which means that the program headers have
     already been worked out.  */
  if (elf_tdata (obfd)->segment_map == NULL
      && elf_tdata (ibfd)->phdr != NULL)
    {
      asection *s;

      /* Only set up the segments if there are no more SEC_ALLOC
         sections.  FIXME: This won't do the right thing if objcopy is
         used to remove the last SEC_ALLOC section, since objcopy
         won't call this routine in that case.  */
      for (s = isec->next; s != NULL; s = s->next)
	if ((s->flags & SEC_ALLOC) != 0)
	  break;
      if (s == NULL)
	{
	  if (! copy_private_bfd_data (ibfd, obfd))
	    return false;
	}
    }

  ihdr = &elf_section_data (isec)->this_hdr;
  ohdr = &elf_section_data (osec)->this_hdr;

  ohdr->sh_entsize = ihdr->sh_entsize;

  if (ihdr->sh_type == SHT_SYMTAB
      || ihdr->sh_type == SHT_DYNSYM
      || ihdr->sh_type == SHT_GNU_verneed
      || ihdr->sh_type == SHT_GNU_verdef)
    ohdr->sh_info = ihdr->sh_info;

  elf_section_data (osec)->use_rela_p
    = elf_section_data (isec)->use_rela_p;

  return true;
}

/* Copy private symbol information.  If this symbol is in a section
   which we did not map into a BFD section, try to map the section
   index correctly.  We use special macro definitions for the mapped
   section indices; these definitions are interpreted by the
   swap_out_syms function.  */

#define MAP_ONESYMTAB (SHN_LORESERVE - 1)
#define MAP_DYNSYMTAB (SHN_LORESERVE - 2)
#define MAP_STRTAB (SHN_LORESERVE - 3)
#define MAP_SHSTRTAB (SHN_LORESERVE - 4)

boolean
_bfd_elf_copy_private_symbol_data (ibfd, isymarg, obfd, osymarg)
     bfd *ibfd;
     asymbol *isymarg;
     bfd *obfd;
     asymbol *osymarg;
{
  elf_symbol_type *isym, *osym;

  if (bfd_get_flavour (ibfd) != bfd_target_elf_flavour
      || bfd_get_flavour (obfd) != bfd_target_elf_flavour)
    return true;

  isym = elf_symbol_from (ibfd, isymarg);
  osym = elf_symbol_from (obfd, osymarg);

  if (isym != NULL
      && osym != NULL
      && bfd_is_abs_section (isym->symbol.section))
    {
      unsigned int shndx;

      shndx = isym->internal_elf_sym.st_shndx;
      if (shndx == elf_onesymtab (ibfd))
	shndx = MAP_ONESYMTAB;
      else if (shndx == elf_dynsymtab (ibfd))
	shndx = MAP_DYNSYMTAB;
      else if (shndx == elf_tdata (ibfd)->strtab_section)
	shndx = MAP_STRTAB;
      else if (shndx == elf_tdata (ibfd)->shstrtab_section)
	shndx = MAP_SHSTRTAB;
      osym->internal_elf_sym.st_shndx = shndx;
    }

  return true;
}

/* Swap out the symbols.  */

static boolean
swap_out_syms (abfd, sttp, relocatable_p)
     bfd *abfd;
     struct bfd_strtab_hash **sttp;
     int relocatable_p;
{
  struct elf_backend_data *bed = get_elf_backend_data (abfd);

  if (!elf_map_symbols (abfd))
    return false;

  /* Dump out the symtabs. */
  {
    int symcount = bfd_get_symcount (abfd);
    asymbol **syms = bfd_get_outsymbols (abfd);
    struct bfd_strtab_hash *stt;
    Elf_Internal_Shdr *symtab_hdr;
    Elf_Internal_Shdr *symstrtab_hdr;
    char *outbound_syms;
    int idx;

    stt = _bfd_elf_stringtab_init ();
    if (stt == NULL)
      return false;

    symtab_hdr = &elf_tdata (abfd)->symtab_hdr;
    symtab_hdr->sh_type = SHT_SYMTAB;
    symtab_hdr->sh_entsize = bed->s->sizeof_sym;
    symtab_hdr->sh_size = symtab_hdr->sh_entsize * (symcount + 1);
    symtab_hdr->sh_info = elf_num_locals (abfd) + 1;
    symtab_hdr->sh_addralign = bed->s->file_align;

    symstrtab_hdr = &elf_tdata (abfd)->strtab_hdr;
    symstrtab_hdr->sh_type = SHT_STRTAB;

    outbound_syms = bfd_alloc (abfd,
			       (1 + symcount) * bed->s->sizeof_sym);
    if (outbound_syms == NULL)
      return false;
    symtab_hdr->contents = (PTR) outbound_syms;

    /* now generate the data (for "contents") */
    {
      /* Fill in zeroth symbol and swap it out.  */
      Elf_Internal_Sym sym;
      sym.st_name = 0;
      sym.st_value = 0;
      sym.st_size = 0;
      sym.st_info = 0;
      sym.st_other = 0;
      sym.st_shndx = SHN_UNDEF;
      bed->s->swap_symbol_out (abfd, &sym, (PTR) outbound_syms);
      outbound_syms += bed->s->sizeof_sym;
    }
    for (idx = 0; idx < symcount; idx++)
      {
	Elf_Internal_Sym sym;
	bfd_vma value = syms[idx]->value;
	elf_symbol_type *type_ptr;
	flagword flags = syms[idx]->flags;
	int type;

	if (flags & BSF_SECTION_SYM)
	  /* Section symbols have no names.  */
	  sym.st_name = 0;
	else
	  {
	    sym.st_name = (unsigned long) _bfd_stringtab_add (stt,
							      syms[idx]->name,
							      true, false);
	    if (sym.st_name == (unsigned long) -1)
	      return false;
	  }

	type_ptr = elf_symbol_from (abfd, syms[idx]);

	if ((flags & BSF_SECTION_SYM) == 0
	    && bfd_is_com_section (syms[idx]->section))
	  {
	    /* ELF common symbols put the alignment into the `value' field,
	       and the size into the `size' field.  This is backwards from
	       how BFD handles it, so reverse it here.  */
	    sym.st_size = value;
	    if (type_ptr == NULL
		|| type_ptr->internal_elf_sym.st_value == 0)
	      sym.st_value = value >= 16 ? 16 : (1 << bfd_log2 (value));
	    else
	      sym.st_value = type_ptr->internal_elf_sym.st_value;
	    sym.st_shndx = _bfd_elf_section_from_bfd_section
	      (abfd, syms[idx]->section);
	  }
	else
	  {
	    asection *sec = syms[idx]->section;
	    int shndx;

	    if (sec->output_section)
	      {
		value += sec->output_offset;
		sec = sec->output_section;
	      }
	    /* Don't add in the section vma for relocatable output.  */
	    if (! relocatable_p)
	      value += sec->vma;
	    sym.st_value = value;
	    sym.st_size = type_ptr ? type_ptr->internal_elf_sym.st_size : 0;

	    if (bfd_is_abs_section (sec)
		&& type_ptr != NULL
		&& type_ptr->internal_elf_sym.st_shndx != 0)
	      {
		/* This symbol is in a real ELF section which we did
                   not create as a BFD section.  Undo the mapping done
                   by copy_private_symbol_data.  */
		shndx = type_ptr->internal_elf_sym.st_shndx;
		switch (shndx)
		  {
		  case MAP_ONESYMTAB:
		    shndx = elf_onesymtab (abfd);
		    break;
		  case MAP_DYNSYMTAB:
		    shndx = elf_dynsymtab (abfd);
		    break;
		  case MAP_STRTAB:
		    shndx = elf_tdata (abfd)->strtab_section;
		    break;
		  case MAP_SHSTRTAB:
		    shndx = elf_tdata (abfd)->shstrtab_section;
		    break;
		  default:
		    break;
		  }
	      }
	    else
	      {
		shndx = _bfd_elf_section_from_bfd_section (abfd, sec);

		if (shndx == -1)
		  {
		    asection *sec2;

		    /* Writing this would be a hell of a lot easier if
		       we had some decent documentation on bfd, and
		       knew what to expect of the library, and what to
		       demand of applications.  For example, it
		       appears that `objcopy' might not set the
		       section of a symbol to be a section that is
		       actually in the output file.  */
		    sec2 = bfd_get_section_by_name (abfd, sec->name);
		    BFD_ASSERT (sec2 != 0);
		    shndx = _bfd_elf_section_from_bfd_section (abfd, sec2);
		    BFD_ASSERT (shndx != -1);
		  }
	      }

	    sym.st_shndx = shndx;
	  }

	if ((flags & BSF_FUNCTION) != 0)
	  type = STT_FUNC;
	else if ((flags & BSF_OBJECT) != 0)
	  type = STT_OBJECT;
	else
	  type = STT_NOTYPE;

        /* Processor-specific types */
        if (type_ptr != NULL
	    && bed->elf_backend_get_symbol_type)
          type = (*bed->elf_backend_get_symbol_type) (&type_ptr->internal_elf_sym, type);

	if (flags & BSF_SECTION_SYM)
	  sym.st_info = ELF_ST_INFO (STB_LOCAL, STT_SECTION);
	else if (bfd_is_com_section (syms[idx]->section))
	  sym.st_info = ELF_ST_INFO (STB_GLOBAL, type);
	else if (bfd_is_und_section (syms[idx]->section))
	  sym.st_info = ELF_ST_INFO (((flags & BSF_WEAK)
				      ? STB_WEAK
				      : STB_GLOBAL),
				     type);
	else if (flags & BSF_FILE)
	  sym.st_info = ELF_ST_INFO (STB_LOCAL, STT_FILE);
	else
	  {
	    int bind = STB_LOCAL;

	    if (flags & BSF_LOCAL)
	      bind = STB_LOCAL;
	    else if (flags & BSF_WEAK)
	      bind = STB_WEAK;
	    else if (flags & BSF_GLOBAL)
	      bind = STB_GLOBAL;

	    sym.st_info = ELF_ST_INFO (bind, type);
	  }

	if (type_ptr != NULL)
	  sym.st_other = type_ptr->internal_elf_sym.st_other;
	else
	  sym.st_other = 0;

	bed->s->swap_symbol_out (abfd, &sym, (PTR) outbound_syms);
	outbound_syms += bed->s->sizeof_sym;
      }

    *sttp = stt;
    symstrtab_hdr->sh_size = _bfd_stringtab_size (stt);
    symstrtab_hdr->sh_type = SHT_STRTAB;

    symstrtab_hdr->sh_flags = 0;
    symstrtab_hdr->sh_addr = 0;
    symstrtab_hdr->sh_entsize = 0;
    symstrtab_hdr->sh_link = 0;
    symstrtab_hdr->sh_info = 0;
    symstrtab_hdr->sh_addralign = 1;
  }

  return true;
}

/* Return the number of bytes required to hold the symtab vector.

   Note that we base it on the count plus 1, since we will null terminate
   the vector allocated based on this size.  However, the ELF symbol table
   always has a dummy entry as symbol #0, so it ends up even.  */

long
_bfd_elf_get_symtab_upper_bound (abfd)
     bfd *abfd;
{
  long symcount;
  long symtab_size;
  Elf_Internal_Shdr *hdr = &elf_tdata (abfd)->symtab_hdr;

  symcount = hdr->sh_size / get_elf_backend_data (abfd)->s->sizeof_sym;
  symtab_size = (symcount - 1 + 1) * (sizeof (asymbol *));

  return symtab_size;
}

long
_bfd_elf_get_dynamic_symtab_upper_bound (abfd)
     bfd *abfd;
{
  long symcount;
  long symtab_size;
  Elf_Internal_Shdr *hdr = &elf_tdata (abfd)->dynsymtab_hdr;

  if (elf_dynsymtab (abfd) == 0)
    {
      bfd_set_error (bfd_error_invalid_operation);
      return -1;
    }

  symcount = hdr->sh_size / get_elf_backend_data (abfd)->s->sizeof_sym;
  symtab_size = (symcount - 1 + 1) * (sizeof (asymbol *));

  return symtab_size;
}

long
_bfd_elf_get_reloc_upper_bound (abfd, asect)
     bfd *abfd ATTRIBUTE_UNUSED;
     sec_ptr asect;
{
  return (asect->reloc_count + 1) * sizeof (arelent *);
}

/* Canonicalize the relocs.  */

long
_bfd_elf_canonicalize_reloc (abfd, section, relptr, symbols)
     bfd *abfd;
     sec_ptr section;
     arelent **relptr;
     asymbol **symbols;
{
  arelent *tblptr;
  unsigned int i;

  if (! get_elf_backend_data (abfd)->s->slurp_reloc_table (abfd,
							   section,
							   symbols,
							   false))
    return -1;

  tblptr = section->relocation;
  for (i = 0; i < section->reloc_count; i++)
    *relptr++ = tblptr++;

  *relptr = NULL;

  return section->reloc_count;
}

long
_bfd_elf_get_symtab (abfd, alocation)
     bfd *abfd;
     asymbol **alocation;
{
  long symcount = get_elf_backend_data (abfd)->s->slurp_symbol_table
    (abfd, alocation, false);

  if (symcount >= 0)
    bfd_get_symcount (abfd) = symcount;
  return symcount;
}

long
_bfd_elf_canonicalize_dynamic_symtab (abfd, alocation)
     bfd *abfd;
     asymbol **alocation;
{
  return get_elf_backend_data (abfd)->s->slurp_symbol_table
    (abfd, alocation, true);
}

/* Return the size required for the dynamic reloc entries.  Any
   section that was actually installed in the BFD, and has type
   SHT_REL or SHT_RELA, and uses the dynamic symbol table, is
   considered to be a dynamic reloc section.  */

long
_bfd_elf_get_dynamic_reloc_upper_bound (abfd)
     bfd *abfd;
{
  long ret;
  asection *s;

  if (elf_dynsymtab (abfd) == 0)
    {
      bfd_set_error (bfd_error_invalid_operation);
      return -1;
    }

  ret = sizeof (arelent *);
  for (s = abfd->sections; s != NULL; s = s->next)
    if (elf_section_data (s)->this_hdr.sh_link == elf_dynsymtab (abfd)
	&& (elf_section_data (s)->this_hdr.sh_type == SHT_REL
	    || elf_section_data (s)->this_hdr.sh_type == SHT_RELA))
      ret += ((s->_raw_size / elf_section_data (s)->this_hdr.sh_entsize)
	      * sizeof (arelent *));

  return ret;
}

/* Canonicalize the dynamic relocation entries.  Note that we return
   the dynamic relocations as a single block, although they are
   actually associated with particular sections; the interface, which
   was designed for SunOS style shared libraries, expects that there
   is only one set of dynamic relocs.  Any section that was actually
   installed in the BFD, and has type SHT_REL or SHT_RELA, and uses
   the dynamic symbol table, is considered to be a dynamic reloc
   section.  */

long
_bfd_elf_canonicalize_dynamic_reloc (abfd, storage, syms)
     bfd *abfd;
     arelent **storage;
     asymbol **syms;
{
  boolean (*slurp_relocs) PARAMS ((bfd *, asection *, asymbol **, boolean));
  asection *s;
  long ret;

  if (elf_dynsymtab (abfd) == 0)
    {
      bfd_set_error (bfd_error_invalid_operation);
      return -1;
    }

  slurp_relocs = get_elf_backend_data (abfd)->s->slurp_reloc_table;
  ret = 0;
  for (s = abfd->sections; s != NULL; s = s->next)
    {
      if (elf_section_data (s)->this_hdr.sh_link == elf_dynsymtab (abfd)
	  && (elf_section_data (s)->this_hdr.sh_type == SHT_REL
	      || elf_section_data (s)->this_hdr.sh_type == SHT_RELA))
	{
	  arelent *p;
	  long count, i;

	  if (! (*slurp_relocs) (abfd, s, syms, true))
	    return -1;
	  count = s->_raw_size / elf_section_data (s)->this_hdr.sh_entsize;
	  p = s->relocation;
	  for (i = 0; i < count; i++)
	    *storage++ = p++;
	  ret += count;
	}
    }

  *storage = NULL;

  return ret;
}

/* Read in the version information.  */

boolean
_bfd_elf_slurp_version_tables (abfd)
     bfd *abfd;
{
  bfd_byte *contents = NULL;

  if (elf_dynverdef (abfd) != 0)
    {
      Elf_Internal_Shdr *hdr;
      Elf_External_Verdef *everdef;
      Elf_Internal_Verdef *iverdef;
      unsigned int i;

      hdr = &elf_tdata (abfd)->dynverdef_hdr;

      elf_tdata (abfd)->verdef =
	((Elf_Internal_Verdef *)
	 bfd_zalloc (abfd, hdr->sh_info * sizeof (Elf_Internal_Verdef)));
      if (elf_tdata (abfd)->verdef == NULL)
	goto error_return;

      elf_tdata (abfd)->cverdefs = hdr->sh_info;

      contents = (bfd_byte *) bfd_malloc (hdr->sh_size);
      if (contents == NULL)
	goto error_return;
      if (bfd_seek (abfd, hdr->sh_offset, SEEK_SET) != 0
	  || bfd_read ((PTR) contents, 1, hdr->sh_size, abfd) != hdr->sh_size)
	goto error_return;

      everdef = (Elf_External_Verdef *) contents;
      iverdef = elf_tdata (abfd)->verdef;
      for (i = 0; i < hdr->sh_info; i++, iverdef++)
	{
	  Elf_External_Verdaux *everdaux;
	  Elf_Internal_Verdaux *iverdaux;
	  unsigned int j;

	  _bfd_elf_swap_verdef_in (abfd, everdef, iverdef);

	  iverdef->vd_bfd = abfd;

	  iverdef->vd_auxptr = ((Elf_Internal_Verdaux *)
				bfd_alloc (abfd,
					   (iverdef->vd_cnt
					    * sizeof (Elf_Internal_Verdaux))));
	  if (iverdef->vd_auxptr == NULL)
	    goto error_return;

	  everdaux = ((Elf_External_Verdaux *)
		      ((bfd_byte *) everdef + iverdef->vd_aux));
	  iverdaux = iverdef->vd_auxptr;
	  for (j = 0; j < iverdef->vd_cnt; j++, iverdaux++)
	    {
	      _bfd_elf_swap_verdaux_in (abfd, everdaux, iverdaux);

	      iverdaux->vda_nodename =
		bfd_elf_string_from_elf_section (abfd, hdr->sh_link,
						 iverdaux->vda_name);
	      if (iverdaux->vda_nodename == NULL)
		goto error_return;

	      if (j + 1 < iverdef->vd_cnt)
		iverdaux->vda_nextptr = iverdaux + 1;
	      else
		iverdaux->vda_nextptr = NULL;

	      everdaux = ((Elf_External_Verdaux *)
			  ((bfd_byte *) everdaux + iverdaux->vda_next));
	    }

	  iverdef->vd_nodename = iverdef->vd_auxptr->vda_nodename;

	  if (i + 1 < hdr->sh_info)
	    iverdef->vd_nextdef = iverdef + 1;
	  else
	    iverdef->vd_nextdef = NULL;

	  everdef = ((Elf_External_Verdef *)
		     ((bfd_byte *) everdef + iverdef->vd_next));
	}

      free (contents);
      contents = NULL;
    }

  if (elf_dynverref (abfd) != 0)
    {
      Elf_Internal_Shdr *hdr;
      Elf_External_Verneed *everneed;
      Elf_Internal_Verneed *iverneed;
      unsigned int i;

      hdr = &elf_tdata (abfd)->dynverref_hdr;

      elf_tdata (abfd)->verref =
	((Elf_Internal_Verneed *)
	 bfd_zalloc (abfd, hdr->sh_info * sizeof (Elf_Internal_Verneed)));
      if (elf_tdata (abfd)->verref == NULL)
	goto error_return;

      elf_tdata (abfd)->cverrefs = hdr->sh_info;

      contents = (bfd_byte *) bfd_malloc (hdr->sh_size);
      if (contents == NULL)
	goto error_return;
      if (bfd_seek (abfd, hdr->sh_offset, SEEK_SET) != 0
	  || bfd_read ((PTR) contents, 1, hdr->sh_size, abfd) != hdr->sh_size)
	goto error_return;

      everneed = (Elf_External_Verneed *) contents;
      iverneed = elf_tdata (abfd)->verref;
      for (i = 0; i < hdr->sh_info; i++, iverneed++)
	{
	  Elf_External_Vernaux *evernaux;
	  Elf_Internal_Vernaux *ivernaux;
	  unsigned int j;

	  _bfd_elf_swap_verneed_in (abfd, everneed, iverneed);

	  iverneed->vn_bfd = abfd;

	  iverneed->vn_filename =
	    bfd_elf_string_from_elf_section (abfd, hdr->sh_link,
					     iverneed->vn_file);
	  if (iverneed->vn_filename == NULL)
	    goto error_return;

	  iverneed->vn_auxptr =
	    ((Elf_Internal_Vernaux *)
	     bfd_alloc (abfd,
			iverneed->vn_cnt * sizeof (Elf_Internal_Vernaux)));

	  evernaux = ((Elf_External_Vernaux *)
		      ((bfd_byte *) everneed + iverneed->vn_aux));
	  ivernaux = iverneed->vn_auxptr;
	  for (j = 0; j < iverneed->vn_cnt; j++, ivernaux++)
	    {
	      _bfd_elf_swap_vernaux_in (abfd, evernaux, ivernaux);

	      ivernaux->vna_nodename =
		bfd_elf_string_from_elf_section (abfd, hdr->sh_link,
						 ivernaux->vna_name);
	      if (ivernaux->vna_nodename == NULL)
		goto error_return;

	      if (j + 1 < iverneed->vn_cnt)
		ivernaux->vna_nextptr = ivernaux + 1;
	      else
		ivernaux->vna_nextptr = NULL;

	      evernaux = ((Elf_External_Vernaux *)
			  ((bfd_byte *) evernaux + ivernaux->vna_next));
	    }

	  if (i + 1 < hdr->sh_info)
	    iverneed->vn_nextref = iverneed + 1;
	  else
	    iverneed->vn_nextref = NULL;

	  everneed = ((Elf_External_Verneed *)
		      ((bfd_byte *) everneed + iverneed->vn_next));
	}

      free (contents);
      contents = NULL;
    }

  return true;

 error_return:
  if (contents == NULL)
    free (contents);
  return false;
}

asymbol *
_bfd_elf_make_empty_symbol (abfd)
     bfd *abfd;
{
  elf_symbol_type *newsym;

  newsym = (elf_symbol_type *) bfd_zalloc (abfd, sizeof (elf_symbol_type));
  if (!newsym)
    return NULL;
  else
    {
      newsym->symbol.the_bfd = abfd;
      return &newsym->symbol;
    }
}

void
_bfd_elf_get_symbol_info (ignore_abfd, symbol, ret)
     bfd *ignore_abfd ATTRIBUTE_UNUSED;
     asymbol *symbol;
     symbol_info *ret;
{
  bfd_symbol_info (symbol, ret);
}

/* Return whether a symbol name implies a local symbol.  Most targets
   use this function for the is_local_label_name entry point, but some
   override it.  */

boolean
_bfd_elf_is_local_label_name (abfd, name)
     bfd *abfd ATTRIBUTE_UNUSED;
     const char *name;
{
  /* Normal local symbols start with ``.L''.  */
  if (name[0] == '.' && name[1] == 'L')
    return true;

  /* At least some SVR4 compilers (e.g., UnixWare 2.1 cc) generate
     DWARF debugging symbols starting with ``..''.  */
  if (name[0] == '.' && name[1] == '.')
    return true;

  /* gcc will sometimes generate symbols beginning with ``_.L_'' when
     emitting DWARF debugging output.  I suspect this is actually a
     small bug in gcc (it calls ASM_OUTPUT_LABEL when it should call
     ASM_GENERATE_INTERNAL_LABEL, and this causes the leading
     underscore to be emitted on some ELF targets).  For ease of use,
     we treat such symbols as local.  */
  if (name[0] == '_' && name[1] == '.' && name[2] == 'L' && name[3] == '_')
    return true;

  return false;
}

alent *
_bfd_elf_get_lineno (ignore_abfd, symbol)
     bfd *ignore_abfd ATTRIBUTE_UNUSED;
     asymbol *symbol ATTRIBUTE_UNUSED;
{
  abort ();
  return NULL;
}

boolean
_bfd_elf_set_arch_mach (abfd, arch, machine)
     bfd *abfd;
     enum bfd_architecture arch;
     unsigned long machine;
{
  /* If this isn't the right architecture for this backend, and this
     isn't the generic backend, fail.  */
  if (arch != get_elf_backend_data (abfd)->arch
      && arch != bfd_arch_unknown
      && get_elf_backend_data (abfd)->arch != bfd_arch_unknown)
    return false;

  return bfd_default_set_arch_mach (abfd, arch, machine);
}

/* Find the nearest line to a particular section and offset, for error
   reporting.  */

boolean
_bfd_elf_find_nearest_line (abfd,
			    section,
			    symbols,
			    offset,
			    filename_ptr,
			    functionname_ptr,
			    line_ptr)
     bfd *abfd;
     asection *section;
     asymbol **symbols;
     bfd_vma offset;
     CONST char **filename_ptr;
     CONST char **functionname_ptr;
     unsigned int *line_ptr;
{
  boolean found;
  const char *filename;
  asymbol *func;
  bfd_vma low_func;
  asymbol **p;

  if (_bfd_dwarf1_find_nearest_line (abfd, section, symbols, offset,
				     filename_ptr, functionname_ptr, 
				     line_ptr))
    return true;

  if (_bfd_dwarf2_find_nearest_line (abfd, section, symbols, offset,
				     filename_ptr, functionname_ptr,
				     line_ptr, 0))
    return true;

  if (! _bfd_stab_section_find_nearest_line (abfd, symbols, section, offset,
					     &found, filename_ptr,
					     functionname_ptr, line_ptr,
					     &elf_tdata (abfd)->line_info))
    return false;
  if (found)
    return true;

  if (symbols == NULL)
    return false;

  filename = NULL;
  func = NULL;
  low_func = 0;

  for (p = symbols; *p != NULL; p++)
    {
      elf_symbol_type *q;

      q = (elf_symbol_type *) *p;

      if (bfd_get_section (&q->symbol) != section)
	continue;

      switch (ELF_ST_TYPE (q->internal_elf_sym.st_info))
	{
	default:
	  break;
	case STT_FILE:
	  filename = bfd_asymbol_name (&q->symbol);
	  break;
	case STT_NOTYPE:
	case STT_FUNC:
	  if (q->symbol.section == section
	      && q->symbol.value >= low_func
	      && q->symbol.value <= offset)
	    {
	      func = (asymbol *) q;
	      low_func = q->symbol.value;
	    }
	  break;
	}
    }

  if (func == NULL)
    return false;

  *filename_ptr = filename;
  *functionname_ptr = bfd_asymbol_name (func);
  *line_ptr = 0;
  return true;
}

int
_bfd_elf_sizeof_headers (abfd, reloc)
     bfd *abfd;
     boolean reloc;
{
  int ret;

  ret = get_elf_backend_data (abfd)->s->sizeof_ehdr;
  if (! reloc)
    ret += get_program_header_size (abfd);
  return ret;
}

boolean
_bfd_elf_set_section_contents (abfd, section, location, offset, count)
     bfd *abfd;
     sec_ptr section;
     PTR location;
     file_ptr offset;
     bfd_size_type count;
{
  Elf_Internal_Shdr *hdr;

  if (! abfd->output_has_begun
      && ! _bfd_elf_compute_section_file_positions
      (abfd, (struct bfd_link_info *) NULL))
    return false;

  hdr = &elf_section_data (section)->this_hdr;

  if (bfd_seek (abfd, hdr->sh_offset + offset, SEEK_SET) == -1)
    return false;
  if (bfd_write (location, 1, count, abfd) != count)
    return false;

  return true;
}

void
_bfd_elf_no_info_to_howto (abfd, cache_ptr, dst)
     bfd *abfd ATTRIBUTE_UNUSED;
     arelent *cache_ptr ATTRIBUTE_UNUSED;
     Elf_Internal_Rela *dst ATTRIBUTE_UNUSED;
{
  abort ();
}

#if 0
void
_bfd_elf_no_info_to_howto_rel (abfd, cache_ptr, dst)
     bfd *abfd;
     arelent *cache_ptr;
     Elf_Internal_Rel *dst;
{
  abort ();
}
#endif

/* Try to convert a non-ELF reloc into an ELF one.  */

boolean
_bfd_elf_validate_reloc (abfd, areloc)
     bfd *abfd;
     arelent *areloc;
{
  /* Check whether we really have an ELF howto. */

  if ((*areloc->sym_ptr_ptr)->the_bfd->xvec != abfd->xvec)
    {
      bfd_reloc_code_real_type code;
      reloc_howto_type *howto;

      /* Alien reloc: Try to determine its type to replace it with an
	 equivalent ELF reloc. */

      if (areloc->howto->pc_relative)
	{
	  switch (areloc->howto->bitsize)
	    {
	    case 8:
	      code = BFD_RELOC_8_PCREL;
	      break;
	    case 12:
	      code = BFD_RELOC_12_PCREL;
	      break;
	    case 16:
	      code = BFD_RELOC_16_PCREL;
	      break;
	    case 24:
	      code = BFD_RELOC_24_PCREL;
	      break;
	    case 32:
	      code = BFD_RELOC_32_PCREL;
	      break;
	    case 64:
	      code = BFD_RELOC_64_PCREL;
	      break;
	    default:
	      goto fail;
	    }

	  howto = bfd_reloc_type_lookup (abfd, code);

	  if (areloc->howto->pcrel_offset != howto->pcrel_offset)
	    {
	      if (howto->pcrel_offset)
		areloc->addend += areloc->address;
	      else
		areloc->addend -= areloc->address; /* addend is unsigned!! */
	    }
	}
      else
	{
	  switch (areloc->howto->bitsize)
	    {
	    case 8:
	      code = BFD_RELOC_8;
	      break;
	    case 14:
	      code = BFD_RELOC_14;
	      break;
	    case 16:
	      code = BFD_RELOC_16;
	      break;
	    case 26:
	      code = BFD_RELOC_26;
	      break;
	    case 32:
	      code = BFD_RELOC_32;
	      break;
	    case 64:
	      code = BFD_RELOC_64;
	      break;
	    default:
	      goto fail;
	    }

	  howto = bfd_reloc_type_lookup (abfd, code);
	}

      if (howto)
	areloc->howto = howto;
      else
	goto fail;
    }

  return true;

 fail:
  (*_bfd_error_handler)
    (_("%s: unsupported relocation type %s"),
     bfd_get_filename (abfd), areloc->howto->name);
  bfd_set_error (bfd_error_bad_value);
  return false;
}

boolean
_bfd_elf_close_and_cleanup (abfd)
     bfd *abfd;
{
  if (bfd_get_format (abfd) == bfd_object)
    {
      if (elf_shstrtab (abfd) != NULL)
	_bfd_stringtab_free (elf_shstrtab (abfd));
    }

  return _bfd_generic_close_and_cleanup (abfd);
}

/* For Rel targets, we encode meaningful data for BFD_RELOC_VTABLE_ENTRY
   in the relocation's offset.  Thus we cannot allow any sort of sanity
   range-checking to interfere.  There is nothing else to do in processing
   this reloc.  */

bfd_reloc_status_type
_bfd_elf_rel_vtable_reloc_fn (abfd, re, symbol, data, is, obfd, errmsg)
     bfd *abfd ATTRIBUTE_UNUSED;
     arelent *re ATTRIBUTE_UNUSED;
     struct symbol_cache_entry *symbol ATTRIBUTE_UNUSED;
     PTR data ATTRIBUTE_UNUSED;
     asection *is ATTRIBUTE_UNUSED;
     bfd *obfd ATTRIBUTE_UNUSED;
     char **errmsg ATTRIBUTE_UNUSED;
{
  return bfd_reloc_ok;
}


/* Elf core file support.  Much of this only works on native
   toolchains, since we rely on knowing the
   machine-dependent procfs structure in order to pick
   out details about the corefile. */

#ifdef HAVE_SYS_PROCFS_H
# include <sys/procfs.h>
#endif


/* Define offsetof for those systems which lack it. */

#ifndef offsetof
# define offsetof(TYPE, MEMBER) ((unsigned long) &((TYPE *)0)->MEMBER)
#endif


/* FIXME: this is kinda wrong, but it's what gdb wants. */

static int
elfcore_make_pid (abfd)
     bfd* abfd;
{
  return ((elf_tdata (abfd)->core_lwpid << 16)
	  + (elf_tdata (abfd)->core_pid));
}


/* If there isn't a section called NAME, make one, using
   data from SECT.  Note, this function will generate a
   reference to NAME, so you shouldn't deallocate or
   overwrite it. */

static boolean
elfcore_maybe_make_sect (abfd, name, sect)
     bfd* abfd;
     char* name;
     asection* sect;
{
  asection* sect2;

  if (bfd_get_section_by_name (abfd, name) != NULL)
    return true;

  sect2 = bfd_make_section (abfd, name);
  if (sect2 == NULL)
    return false;

  sect2->_raw_size = sect->_raw_size;
  sect2->filepos = sect->filepos;
  sect2->flags = sect->flags;
  sect2->alignment_power = sect->alignment_power;
  return true;
}


/* prstatus_t exists on:
     solaris 2.[567]
     linux 2.[01] + glibc
     unixware 4.2
*/

#if defined (HAVE_PRSTATUS_T)
static boolean
elfcore_grok_prstatus (abfd, note)
     bfd* abfd;
     Elf_Internal_Note* note;
{
  prstatus_t prstat;
  char buf[100];
  char* name;
  asection* sect;

  if (note->descsz != sizeof (prstat))
    return true;

  memcpy (&prstat, note->descdata, sizeof (prstat));

  elf_tdata (abfd)->core_signal = prstat.pr_cursig;
  elf_tdata (abfd)->core_pid = prstat.pr_pid;

  /* pr_who exists on:
       solaris 2.[567]
       unixware 4.2
     pr_who doesn't exist on:
       linux 2.[01]
  */
#if defined (HAVE_PRSTATUS_T_PR_WHO)
  elf_tdata (abfd)->core_lwpid = prstat.pr_who;
#endif

  /* Make a ".reg/999" section. */

  sprintf (buf, ".reg/%d", elfcore_make_pid (abfd));
  name = bfd_alloc (abfd, strlen (buf) + 1);
  if (name == NULL)
    return false;
  strcpy (name, buf);

  sect = bfd_make_section (abfd, name);
  if (sect == NULL)
    return false;
  sect->_raw_size = sizeof (prstat.pr_reg);
  sect->filepos = note->descpos + offsetof (prstatus_t, pr_reg);
  sect->flags = SEC_HAS_CONTENTS;
  sect->alignment_power = 2;

  if (! elfcore_maybe_make_sect (abfd, ".reg", sect))
    return false;

  return true;
}
#endif /* defined (HAVE_PRSTATUS_T) */


/* Create a pseudosection containing the exact contents of NOTE.  This
   actually creates up to two pseudosections:
   - For the single-threaded case, a section named NAME, unless
     such a section already exists.
   - For the multi-threaded case, a section named "NAME/PID", where
     PID is elfcore_make_pid (abfd).
   Both pseudosections have identical contents: the contents of NOTE.  */

static boolean
elfcore_make_note_pseudosection (abfd, name, note)
     bfd* abfd;
     char *name;
     Elf_Internal_Note* note;
{
  char buf[100];
  char *threaded_name;
  asection* sect;

  /* Build the section name.  */

  sprintf (buf, "%s/%d", name, elfcore_make_pid (abfd));
  threaded_name = bfd_alloc (abfd, strlen (buf) + 1);
  if (threaded_name == NULL)
    return false;
  strcpy (threaded_name, buf);

  sect = bfd_make_section (abfd, threaded_name);
  if (sect == NULL)
    return false;
  sect->_raw_size = note->descsz;
  sect->filepos = note->descpos;
  sect->flags = SEC_HAS_CONTENTS;
  sect->alignment_power = 2;

  if (! elfcore_maybe_make_sect (abfd, name, sect))
    return false;

  return true;
}


/* There isn't a consistent prfpregset_t across platforms,
   but it doesn't matter, because we don't have to pick this
   data structure apart. */
static boolean
elfcore_grok_prfpreg (abfd, note)
     bfd* abfd;
     Elf_Internal_Note* note;
{
  return elfcore_make_note_pseudosection (abfd, ".reg2", note);
}


/* Linux dumps the Intel SSE regs in a note named "LINUX" with a note
   type of 5 (NT_PRXFPREG).  Just include the whole note's contents
   literally.  */
static boolean
elfcore_grok_prxfpreg (abfd, note)
     bfd* abfd;
     Elf_Internal_Note* note;
{
  return elfcore_make_note_pseudosection (abfd, ".reg-xfp", note);
}


#if defined (HAVE_PRPSINFO_T)
# define elfcore_psinfo_t prpsinfo_t
#endif

#if defined (HAVE_PSINFO_T)
# define elfcore_psinfo_t psinfo_t
#endif


#if defined (HAVE_PRPSINFO_T) || defined (HAVE_PSINFO_T)

/* return a malloc'ed copy of a string at START which is at
   most MAX bytes long, possibly without a terminating '\0'.
   the copy will always have a terminating '\0'. */

static char*
elfcore_strndup (abfd, start, max)
     bfd* abfd;
     char* start;
     int max;
{
  char* dup;
  char* end = memchr (start, '\0', max);
  int len;

  if (end == NULL)
    len = max;
  else
    len = end - start;

  dup = bfd_alloc (abfd, len + 1);
  if (dup == NULL)
    return NULL;

  memcpy (dup, start, len);
  dup[len] = '\0';

  return dup;
}

static boolean
elfcore_grok_psinfo (abfd, note)
     bfd* abfd;
     Elf_Internal_Note* note;
{
  elfcore_psinfo_t psinfo;

  if (note->descsz != sizeof (elfcore_psinfo_t))
    return true;

  memcpy (&psinfo, note->descdata, note->descsz);

  elf_tdata (abfd)->core_program
    = elfcore_strndup (abfd, psinfo.pr_fname, sizeof (psinfo.pr_fname));

  elf_tdata (abfd)->core_command
    = elfcore_strndup (abfd, psinfo.pr_psargs, sizeof (psinfo.pr_psargs));

  /* Note that for some reason, a spurious space is tacked
     onto the end of the args in some (at least one anyway)
     implementations, so strip it off if it exists. */

  {
    char* command = elf_tdata (abfd)->core_command;
    int n = strlen (command);

    if (0 < n && command[n - 1] == ' ')
      command[n - 1] = '\0';
  }

  return true;
}
#endif /* defined (HAVE_PRPSINFO_T) || defined (HAVE_PSINFO_T) */


#if defined (HAVE_PSTATUS_T)
static boolean
elfcore_grok_pstatus (abfd, note)
     bfd* abfd;
     Elf_Internal_Note* note;
{
  pstatus_t pstat;

  if (note->descsz != sizeof (pstat))
    return true;

  memcpy (&pstat, note->descdata, sizeof (pstat));

  elf_tdata (abfd)->core_pid = pstat.pr_pid;

  /* Could grab some more details from the "representative"
     lwpstatus_t in pstat.pr_lwp, but we'll catch it all in an
     NT_LWPSTATUS note, presumably. */

  return true;
}
#endif /* defined (HAVE_PSTATUS_T) */


#if defined (HAVE_LWPSTATUS_T)
static boolean
elfcore_grok_lwpstatus (abfd, note)
     bfd* abfd;
     Elf_Internal_Note* note;
{
  lwpstatus_t lwpstat;
  char buf[100];
  char* name;
  asection* sect;

  if (note->descsz != sizeof (lwpstat))
    return true;

  memcpy (&lwpstat, note->descdata, sizeof (lwpstat));

  elf_tdata (abfd)->core_lwpid = lwpstat.pr_lwpid;
  elf_tdata (abfd)->core_signal = lwpstat.pr_cursig;

  /* Make a ".reg/999" section. */

  sprintf (buf, ".reg/%d", elfcore_make_pid (abfd));
  name = bfd_alloc (abfd, strlen (buf) + 1);
  if (name == NULL)
    return false;
  strcpy (name, buf);

  sect = bfd_make_section (abfd, name);
  if (sect == NULL)
    return false;

#if defined (HAVE_LWPSTATUS_T_PR_CONTEXT)
  sect->_raw_size = sizeof (lwpstat.pr_context.uc_mcontext.gregs);
  sect->filepos = note->descpos
    + offsetof (lwpstatus_t, pr_context.uc_mcontext.gregs);
#endif

#if defined (HAVE_LWPSTATUS_T_PR_REG)
  sect->_raw_size = sizeof (lwpstat.pr_reg);
  sect->filepos = note->descpos + offsetof (lwpstatus_t, pr_reg);
#endif

  sect->flags = SEC_HAS_CONTENTS;
  sect->alignment_power = 2;

  if (!elfcore_maybe_make_sect (abfd, ".reg", sect))
    return false;

  /* Make a ".reg2/999" section */

  sprintf (buf, ".reg2/%d", elfcore_make_pid (abfd));
  name = bfd_alloc (abfd, strlen (buf) + 1);
  if (name == NULL)
    return false;
  strcpy (name, buf);

  sect = bfd_make_section (abfd, name);
  if (sect == NULL)
    return false;

#if defined (HAVE_LWPSTATUS_T_PR_CONTEXT)
  sect->_raw_size = sizeof (lwpstat.pr_context.uc_mcontext.fpregs);
  sect->filepos = note->descpos
    + offsetof (lwpstatus_t, pr_context.uc_mcontext.fpregs);
#endif

#if defined (HAVE_LWPSTATUS_T_PR_FPREG)
  sect->_raw_size = sizeof (lwpstat.pr_fpreg);
  sect->filepos = note->descpos + offsetof (lwpstatus_t, pr_fpreg);
#endif

  sect->flags = SEC_HAS_CONTENTS;
  sect->alignment_power = 2;

  if (!elfcore_maybe_make_sect (abfd, ".reg2", sect))
    return false;

  return true;
}
#endif /* defined (HAVE_LWPSTATUS_T) */

#if defined (HAVE_WIN32_PSTATUS_T)
static boolean
elfcore_grok_win32pstatus (abfd, note)
     bfd * abfd;
     Elf_Internal_Note * note;
{
  char buf[30];
  char * name;
  asection * sect;
  win32_pstatus_t pstatus;

  if (note->descsz < sizeof (pstatus))
    return true;

  memcpy (& pstatus, note->descdata, note->descsz);
  
  switch (pstatus.data_type) 
    {
    case NOTE_INFO_PROCESS:
      /* FIXME: need to add ->core_command.  */
      elf_tdata (abfd)->core_signal = pstatus.data.process_info.signal;
      elf_tdata (abfd)->core_pid = pstatus.data.process_info.pid;
      break ;

    case NOTE_INFO_THREAD:
      /* Make a ".reg/999" section.  */
      sprintf (buf, ".reg/%d", pstatus.data.thread_info.tid);
      
      name = bfd_alloc (abfd, strlen (buf) + 1);
      if (name == NULL)
        return false;
      
      strcpy (name, buf);

      sect = bfd_make_section (abfd, name);
      if (sect == NULL)
        return false;
      
      sect->_raw_size = sizeof (pstatus.data.thread_info.thread_context);
      sect->filepos = note->descpos + offsetof (struct win32_pstatus,
						data.thread_info.thread_context);
      sect->flags = SEC_HAS_CONTENTS;
      sect->alignment_power = 2;

      if (pstatus.data.thread_info.is_active_thread)
	if (! elfcore_maybe_make_sect (abfd, ".reg", sect))
	  return false;
      break;

    case NOTE_INFO_MODULE:
      /* Make a ".module/xxxxxxxx" section.  */
      sprintf (buf, ".module/%08x" , pstatus.data.module_info.base_address);
      
      name = bfd_alloc (abfd, strlen (buf) + 1);
      if (name == NULL)
	return false;
      
      strcpy (name, buf);

      sect = bfd_make_section (abfd, name);
      
      if (sect == NULL)
	return false;
      
      sect->_raw_size = note->descsz;
      sect->filepos = note->descpos;
      sect->flags = SEC_HAS_CONTENTS;
      sect->alignment_power = 2;
      break;

    default:
      return true;
    }

  return true;
}
#endif /* HAVE_WIN32_PSTATUS_T */

static boolean
elfcore_grok_note (abfd, note)
     bfd* abfd;
     Elf_Internal_Note* note;
{
  switch (note->type)
    {
    default:
      return true;

#if defined (HAVE_PRSTATUS_T)
    case NT_PRSTATUS:
      return elfcore_grok_prstatus (abfd, note);
#endif

#if defined (HAVE_PSTATUS_T)
    case NT_PSTATUS:
      return elfcore_grok_pstatus (abfd, note);
#endif

#if defined (HAVE_LWPSTATUS_T)
    case NT_LWPSTATUS:
      return elfcore_grok_lwpstatus (abfd, note);
#endif

    case NT_FPREGSET:		/* FIXME: rename to NT_PRFPREG */
      return elfcore_grok_prfpreg (abfd, note);

#if defined (HAVE_WIN32_PSTATUS_T)
    case NT_WIN32PSTATUS:	
      return elfcore_grok_win32pstatus (abfd, note);
#endif

  case NT_PRXFPREG:		/* Linux SSE extension */
      if (note->namesz == 5
	  && ! strcmp (note->namedata, "LINUX"))
	return elfcore_grok_prxfpreg (abfd, note);
      else
	return true;

#if defined (HAVE_PRPSINFO_T) || defined (HAVE_PSINFO_T)
    case NT_PRPSINFO:
    case NT_PSINFO:
      return elfcore_grok_psinfo (abfd, note);
#endif
    }
}


static boolean
elfcore_read_notes (abfd, offset, size)
     bfd* abfd;
     bfd_vma offset;
     bfd_vma size;
{
  char* buf;
  char* p;

  if (size <= 0)
    return true;

  if (bfd_seek (abfd, offset, SEEK_SET) == -1)
    return false;

  buf = bfd_malloc ((size_t) size);
  if (buf == NULL)
    return false;

  if (bfd_read (buf, size, 1, abfd) != size)
    {
    error:
      free (buf);
      return false;
    }

  p = buf;
  while (p < buf + size)
    {
      /* FIXME: bad alignment assumption. */
      Elf_External_Note* xnp = (Elf_External_Note*) p;
      Elf_Internal_Note in;

      in.type = bfd_h_get_32 (abfd, (bfd_byte *) xnp->type);

      in.namesz = bfd_h_get_32 (abfd, (bfd_byte *) xnp->namesz);
      in.namedata = xnp->name;

      in.descsz = bfd_h_get_32 (abfd, (bfd_byte *) xnp->descsz);
      in.descdata = in.namedata + BFD_ALIGN (in.namesz, 4);
      in.descpos = offset + (in.descdata - buf);

      if (! elfcore_grok_note (abfd, &in))
	goto error;

      p = in.descdata + BFD_ALIGN (in.descsz, 4);
    }

  free (buf);
  return true;
}


/* FIXME: This function is now unnecessary.  Callers can just call
   bfd_section_from_phdr directly.  */

boolean
_bfd_elfcore_section_from_phdr (abfd, phdr, sec_num)
     bfd* abfd;
     Elf_Internal_Phdr* phdr;
     int sec_num;
{
  if (! bfd_section_from_phdr (abfd, phdr, sec_num))
    return false;

  return true;
}



/* Providing external access to the ELF program header table.  */

/* Return an upper bound on the number of bytes required to store a
   copy of ABFD's program header table entries.  Return -1 if an error
   occurs; bfd_get_error will return an appropriate code.  */
long
bfd_get_elf_phdr_upper_bound (abfd)
     bfd *abfd;
{
  if (abfd->xvec->flavour != bfd_target_elf_flavour)
    {
      bfd_set_error (bfd_error_wrong_format);
      return -1;
    }

  return (elf_elfheader (abfd)->e_phnum
	  * sizeof (Elf_Internal_Phdr));
}


/* Copy ABFD's program header table entries to *PHDRS.  The entries
   will be stored as an array of Elf_Internal_Phdr structures, as
   defined in include/elf/internal.h.  To find out how large the
   buffer needs to be, call bfd_get_elf_phdr_upper_bound.

   Return the number of program header table entries read, or -1 if an
   error occurs; bfd_get_error will return an appropriate code.  */
int
bfd_get_elf_phdrs (abfd, phdrs)
     bfd *abfd;
     void *phdrs;
{
  int num_phdrs;

  if (abfd->xvec->flavour != bfd_target_elf_flavour)
    {
      bfd_set_error (bfd_error_wrong_format);
      return -1;
    }

  num_phdrs = elf_elfheader (abfd)->e_phnum;
  memcpy (phdrs, elf_tdata (abfd)->phdr, 
	  num_phdrs * sizeof (Elf_Internal_Phdr));

  return num_phdrs;
}
