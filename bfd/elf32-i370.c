/* i370-specific support for 32-bit ELF
   Copyright 1994, 1995, 1996, 1997, 1998, 2000, 2001, 2002, 2003, 2004, 2005
   Free Software Foundation, Inc.
   Written by Ian Lance Taylor, Cygnus Support.
   Hacked by Linas Vepstas for i370 linas@linas.org

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
   Foundation, Inc., 51 Franklin Street - Fifth Floor, Boston,
   MA 02110-1301, USA.  */

/* This file is based on a preliminary PowerPC ELF ABI.
   But its been hacked on for the IBM 360/370 architectures.
   Basically, the 31bit relocation works, and just about everything
   else is a wild card.  In particular, don't expect shared libs or
   dynamic loading to work ...  its never been tested.  */

#include "bfd.h"
#include "sysdep.h"
#include "bfdlink.h"
#include "libbfd.h"
#include "elf-bfd.h"
#include "elf/i370.h"

static reloc_howto_type *i370_elf_howto_table[ (int)R_I370_max ];

static reloc_howto_type i370_elf_howto_raw[] =
{
  /* This reloc does nothing.  */
  HOWTO (R_I370_NONE,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_I370_NONE",		/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* A standard 31 bit relocation.  */
  HOWTO (R_I370_ADDR31,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 31,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_I370_ADDR31",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0x7fffffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* A standard 32 bit relocation.  */
  HOWTO (R_I370_ADDR32,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_I370_ADDR32",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffffffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* A standard 16 bit relocation.  */
  HOWTO (R_I370_ADDR16,		/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_I370_ADDR16",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* 31-bit PC relative.  */
  HOWTO (R_I370_REL31,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 31,			/* bitsize */
	 TRUE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_I370_REL31",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0x7fffffff,		/* dst_mask */
	 TRUE),			/* pcrel_offset */

  /* 32-bit PC relative.  */
  HOWTO (R_I370_REL32,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 TRUE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_I370_REL32",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffffffff,		/* dst_mask */
	 TRUE),			/* pcrel_offset */

  /* A standard 12 bit relocation.  */
  HOWTO (R_I370_ADDR12,		/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 12,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_I370_ADDR12",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xfff,			/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* 12-bit PC relative.  */
  HOWTO (R_I370_REL12,		/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 12,			/* bitsize */
	 TRUE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_I370_REL12",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xfff,			/* dst_mask */
	 TRUE),			/* pcrel_offset */

  /* A standard 8 bit relocation.  */
  HOWTO (R_I370_ADDR8,		/* type */
	 0,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 8,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_I370_ADDR8",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xff,			/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* 8-bit PC relative.  */
  HOWTO (R_I370_REL8,		/* type */
	 0,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 8,			/* bitsize */
	 TRUE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_I370_REL8",		/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xff,			/* dst_mask */
	 TRUE),			/* pcrel_offset */

  /* This is used only by the dynamic linker.  The symbol should exist
     both in the object being run and in some shared library.  The
     dynamic linker copies the data addressed by the symbol from the
     shared library into the object, because the object being
     run has to have the data at some particular address.  */
  HOWTO (R_I370_COPY,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	 /* special_function */
	 "R_I370_COPY",		/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* Used only by the dynamic linker.  When the object is run, this
     longword is set to the load address of the object, plus the
     addend.  */
  HOWTO (R_I370_RELATIVE,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	 /* special_function */
	 "R_I370_RELATIVE",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffffffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

};

/* Initialize the i370_elf_howto_table, so that linear accesses can be done.  */

static void
i370_elf_howto_init (void)
{
  unsigned int i, type;

  for (i = 0; i < sizeof (i370_elf_howto_raw) / sizeof (i370_elf_howto_raw[0]); i++)
    {
      type = i370_elf_howto_raw[i].type;
      BFD_ASSERT (type < sizeof (i370_elf_howto_table) / sizeof (i370_elf_howto_table[0]));
      i370_elf_howto_table[type] = &i370_elf_howto_raw[i];
    }
}

static reloc_howto_type *
i370_elf_reloc_type_lookup (bfd *abfd ATTRIBUTE_UNUSED,
			    bfd_reloc_code_real_type code)
{
  enum i370_reloc_type i370_reloc = R_I370_NONE;

  if (!i370_elf_howto_table[ R_I370_ADDR31 ])
    /* Initialize howto table if needed.  */
    i370_elf_howto_init ();

  switch ((int) code)
    {
    default:
      return NULL;

    case BFD_RELOC_NONE:	i370_reloc = R_I370_NONE;	break;
    case BFD_RELOC_32:		i370_reloc = R_I370_ADDR31;	break;
    case BFD_RELOC_16:		i370_reloc = R_I370_ADDR16;	break;
    case BFD_RELOC_32_PCREL:	i370_reloc = R_I370_REL31;	break;
    case BFD_RELOC_CTOR:	i370_reloc = R_I370_ADDR31;	break;
    case BFD_RELOC_I370_D12:	i370_reloc = R_I370_ADDR12;	break;
    }

  return i370_elf_howto_table[ (int)i370_reloc ];
};

/* The name of the dynamic interpreter.  This is put in the .interp
    section.  */

#define ELF_DYNAMIC_INTERPRETER "/lib/ld.so"

/* Set the howto pointer for an i370 ELF reloc.  */

static void
i370_elf_info_to_howto (bfd *abfd ATTRIBUTE_UNUSED,
			arelent *cache_ptr,
			Elf_Internal_Rela *dst)
{
  if (!i370_elf_howto_table[ R_I370_ADDR31 ])
    /* Initialize howto table.  */
    i370_elf_howto_init ();

  BFD_ASSERT (ELF32_R_TYPE (dst->r_info) < (unsigned int) R_I370_max);
  cache_ptr->howto = i370_elf_howto_table[ELF32_R_TYPE (dst->r_info)];
}

/* Hack alert --  the following several routines look generic to me ...
   why are we bothering with them ?  */
/* Function to set whether a module needs the -mrelocatable bit set.  */

static bfd_boolean
i370_elf_set_private_flags (bfd *abfd, flagword flags)
{
  BFD_ASSERT (!elf_flags_init (abfd)
	      || elf_elfheader (abfd)->e_flags == flags);

  elf_elfheader (abfd)->e_flags = flags;
  elf_flags_init (abfd) = TRUE;
  return TRUE;
}

/* Merge backend specific data from an object file to the output
   object file when linking.  */

static bfd_boolean
i370_elf_merge_private_bfd_data (bfd *ibfd, bfd *obfd)
{
  flagword old_flags;
  flagword new_flags;

  if (bfd_get_flavour (ibfd) != bfd_target_elf_flavour
      || bfd_get_flavour (obfd) != bfd_target_elf_flavour)
    return TRUE;

  new_flags = elf_elfheader (ibfd)->e_flags;
  old_flags = elf_elfheader (obfd)->e_flags;
  if (!elf_flags_init (obfd))	/* First call, no flags set.  */
    {
      elf_flags_init (obfd) = TRUE;
      elf_elfheader (obfd)->e_flags = new_flags;
    }

  else if (new_flags == old_flags)	/* Compatible flags are ok.  */
    ;

  else					/* Incompatible flags.  */
    {
      (*_bfd_error_handler)
	("%B: uses different e_flags (0x%lx) fields than previous modules (0x%lx)",
	 ibfd, (long) new_flags, (long) old_flags);

      bfd_set_error (bfd_error_bad_value);
      return FALSE;
    }

  return TRUE;
}

/* Handle an i370 specific section when reading an object file.  This
   is called when elfcode.h finds a section with an unknown type.  */
/* XXX hack alert bogus This routine is mostly all junk and almost
   certainly does the wrong thing.  Its here simply because it does
   just enough to allow glibc-2.1 ld.so to compile & link.  */

static bfd_boolean
i370_elf_section_from_shdr (bfd *abfd,
			    Elf_Internal_Shdr *hdr,
			    const char *name,
			    int shindex)
{
  asection *newsect;
  flagword flags;

  if (! _bfd_elf_make_section_from_shdr (abfd, hdr, name, shindex))
    return FALSE;

  newsect = hdr->bfd_section;
  flags = bfd_get_section_flags (abfd, newsect);
  if (hdr->sh_flags & SHF_EXCLUDE)
    flags |= SEC_EXCLUDE;

  if (hdr->sh_type == SHT_ORDERED)
    flags |= SEC_SORT_ENTRIES;

  bfd_set_section_flags (abfd, newsect, flags);
  return TRUE;
}

/* Set up any other section flags and such that may be necessary.  */
/* XXX hack alert bogus This routine is mostly all junk and almost
   certainly does the wrong thing.  Its here simply because it does
   just enough to allow glibc-2.1 ld.so to compile & link.  */

static bfd_boolean
i370_elf_fake_sections (bfd *abfd ATTRIBUTE_UNUSED,
			Elf_Internal_Shdr *shdr,
			asection *asect)
{
  if ((asect->flags & (SEC_GROUP | SEC_EXCLUDE)) == SEC_EXCLUDE)
    shdr->sh_flags |= SHF_EXCLUDE;

  if ((asect->flags & SEC_SORT_ENTRIES) != 0)
    shdr->sh_type = SHT_ORDERED;

  return TRUE;
}

/* We have to create .dynsbss and .rela.sbss here so that they get mapped
   to output sections (just like _bfd_elf_create_dynamic_sections has
   to create .dynbss and .rela.bss).  */
/* XXX hack alert bogus This routine is mostly all junk and almost
   certainly does the wrong thing.  Its here simply because it does
   just enough to allow glibc-2.1 ld.so to compile & link.  */

static bfd_boolean
i370_elf_create_dynamic_sections (bfd *abfd, struct bfd_link_info *info)
{
  asection *s;
  flagword flags;

  if (!_bfd_elf_create_dynamic_sections(abfd, info))
    return FALSE;

  flags = (SEC_ALLOC | SEC_LOAD | SEC_HAS_CONTENTS | SEC_IN_MEMORY
	   | SEC_LINKER_CREATED);

  s = bfd_make_section_with_flags (abfd, ".dynsbss",
				   SEC_ALLOC | SEC_LINKER_CREATED);
  if (s == NULL)
    return FALSE;

  if (! info->shared)
    {
      s = bfd_make_section_with_flags (abfd, ".rela.sbss",
				       flags | SEC_READONLY);
      if (s == NULL
	  || ! bfd_set_section_alignment (abfd, s, 2))
	return FALSE;
    }

   /* XXX beats me, seem to need a rela.text ...  */
   s = bfd_make_section_with_flags (abfd, ".rela.text",
				    flags | SEC_READONLY);
   if (s == NULL
      || ! bfd_set_section_alignment (abfd, s, 2))
    return FALSE;
  return TRUE;
}

/* Adjust a symbol defined by a dynamic object and referenced by a
   regular object.  The current definition is in some section of the
   dynamic object, but we're not including those sections.  We have to
   change the definition to something the rest of the link can
   understand.  */
/* XXX hack alert bogus This routine is mostly all junk and almost
   certainly does the wrong thing.  Its here simply because it does
   just enough to allow glibc-2.1 ld.so to compile & link.  */

static bfd_boolean
i370_elf_adjust_dynamic_symbol (struct bfd_link_info *info,
				struct elf_link_hash_entry *h)
{
  bfd *dynobj = elf_hash_table (info)->dynobj;
  asection *s;
  unsigned int power_of_two;

#ifdef DEBUG
  fprintf (stderr, "i370_elf_adjust_dynamic_symbol called for %s\n",
	   h->root.root.string);
#endif

  /* Make sure we know what is going on here.  */
  BFD_ASSERT (dynobj != NULL
	      && (h->needs_plt
		  || h->u.weakdef != NULL
		  || (h->def_dynamic
		      && h->ref_regular
		      && !h->def_regular)));

  s = bfd_get_section_by_name (dynobj, ".rela.text");
  BFD_ASSERT (s != NULL);
  s->size += sizeof (Elf32_External_Rela);

  /* If this is a weak symbol, and there is a real definition, the
     processor independent code will have arranged for us to see the
     real definition first, and we can just use the same value.  */
  if (h->u.weakdef != NULL)
    {
      BFD_ASSERT (h->u.weakdef->root.type == bfd_link_hash_defined
		  || h->u.weakdef->root.type == bfd_link_hash_defweak);
      h->root.u.def.section = h->u.weakdef->root.u.def.section;
      h->root.u.def.value = h->u.weakdef->root.u.def.value;
      return TRUE;
    }

  /* This is a reference to a symbol defined by a dynamic object which
     is not a function.  */

  /* If we are creating a shared library, we must presume that the
     only references to the symbol are via the global offset table.
     For such cases we need not do anything here; the relocations will
     be handled correctly by relocate_section.  */
  if (info->shared)
    return TRUE;

  if (h->size == 0)
    {
      (*_bfd_error_handler) (_("dynamic variable `%s' is zero size"),
			     h->root.root.string);
      return TRUE;
    }

  /* We must allocate the symbol in our .dynbss section, which will
     become part of the .bss section of the executable.  There will be
     an entry for this symbol in the .dynsym section.  The dynamic
     object will contain position independent code, so all references
     from the dynamic object to this symbol will go through the global
     offset table.  The dynamic linker will use the .dynsym entry to
     determine the address it must put in the global offset table, so
     both the dynamic object and the regular object will refer to the
     same memory location for the variable.

     Of course, if the symbol is sufficiently small, we must instead
     allocate it in .sbss.  FIXME: It would be better to do this if and
     only if there were actually SDAREL relocs for that symbol.  */

  if (h->size <= elf_gp_size (dynobj))
    s = bfd_get_section_by_name (dynobj, ".dynsbss");
  else
    s = bfd_get_section_by_name (dynobj, ".dynbss");
  BFD_ASSERT (s != NULL);

  /* We must generate a R_I370_COPY reloc to tell the dynamic linker to
     copy the initial value out of the dynamic object and into the
     runtime process image.  We need to remember the offset into the
     .rela.bss section we are going to use.  */
  if ((h->root.u.def.section->flags & SEC_ALLOC) != 0)
    {
      asection *srel;

      if (h->size <= elf_gp_size (dynobj))
	srel = bfd_get_section_by_name (dynobj, ".rela.sbss");
      else
	srel = bfd_get_section_by_name (dynobj, ".rela.bss");
      BFD_ASSERT (srel != NULL);
      srel->size += sizeof (Elf32_External_Rela);
      h->needs_copy = 1;
    }

  /* We need to figure out the alignment required for this symbol.  I
     have no idea how ELF linkers handle this.  */
  power_of_two = bfd_log2 (h->size);
  if (power_of_two > 4)
    power_of_two = 4;

  /* Apply the required alignment.  */
  s->size = BFD_ALIGN (s->size, (bfd_size_type) (1 << power_of_two));
  if (power_of_two > bfd_get_section_alignment (dynobj, s))
    {
      if (! bfd_set_section_alignment (dynobj, s, power_of_two))
	return FALSE;
    }

  /* Define the symbol as being at this point in the section.  */
  h->root.u.def.section = s;
  h->root.u.def.value = s->size;

  /* Increment the section size to make room for the symbol.  */
  s->size += h->size;

  return TRUE;
}

/* Increment the index of a dynamic symbol by a given amount.  Called
   via elf_link_hash_traverse.  */
/* XXX hack alert bogus This routine is mostly all junk and almost
   certainly does the wrong thing.  Its here simply because it does
   just enough to allow glibc-2.1 ld.so to compile & link.  */

static bfd_boolean
i370_elf_adjust_dynindx (struct elf_link_hash_entry *h, void * cparg)
{
  int *cp = (int *) cparg;

#ifdef DEBUG
  fprintf (stderr,
	   "i370_elf_adjust_dynindx called, h->dynindx = %d, *cp = %d\n",
	   h->dynindx, *cp);
#endif

  if (h->root.type == bfd_link_hash_warning)
    h = (struct elf_link_hash_entry *) h->root.u.i.link;

  if (h->dynindx != -1)
    h->dynindx += *cp;

  return TRUE;
}

/* Set the sizes of the dynamic sections.  */
/* XXX hack alert bogus This routine is mostly all junk and almost
   certainly does the wrong thing.  Its here simply because it does
   just enough to allow glibc-2.1 ld.so to compile & link.  */

static bfd_boolean
i370_elf_size_dynamic_sections (bfd *output_bfd,
				struct bfd_link_info *info)
{
  bfd *dynobj;
  asection *s;
  bfd_boolean plt;
  bfd_boolean relocs;
  bfd_boolean reltext;

#ifdef DEBUG
  fprintf (stderr, "i370_elf_size_dynamic_sections called\n");
#endif

  dynobj = elf_hash_table (info)->dynobj;
  BFD_ASSERT (dynobj != NULL);

  if (elf_hash_table (info)->dynamic_sections_created)
    {
      /* Set the contents of the .interp section to the interpreter.  */
      if (info->executable)
	{
	  s = bfd_get_section_by_name (dynobj, ".interp");
	  BFD_ASSERT (s != NULL);
	  s->size = sizeof ELF_DYNAMIC_INTERPRETER;
	  s->contents = (unsigned char *) ELF_DYNAMIC_INTERPRETER;
	}
    }
  else
    {
      /* We may have created entries in the .rela.got, .rela.sdata, and
	 .rela.sdata2 sections.  However, if we are not creating the
	 dynamic sections, we will not actually use these entries.  Reset
	 the size of .rela.got, et al, which will cause it to get
	 stripped from the output file below.  */
      static char *rela_sections[] = { ".rela.got", ".rela.sdata",
				       ".rela.sdata2", ".rela.sbss",
				       NULL };
      char **p;

      for (p = rela_sections; *p != NULL; p++)
	{
	  s = bfd_get_section_by_name (dynobj, *p);
	  if (s != NULL)
	    s->size = 0;
	}
    }

  /* The check_relocs and adjust_dynamic_symbol entry points have
     determined the sizes of the various dynamic sections.  Allocate
     memory for them.  */
  plt = FALSE;
  relocs = FALSE;
  reltext = FALSE;
  for (s = dynobj->sections; s != NULL; s = s->next)
    {
      const char *name;

      if ((s->flags & SEC_LINKER_CREATED) == 0)
	continue;

      /* It's OK to base decisions on the section name, because none
	 of the dynobj section names depend upon the input files.  */
      name = bfd_get_section_name (dynobj, s);

      if (strcmp (name, ".plt") == 0)
	{
	  /* Remember whether there is a PLT.  */
	  plt = s->size != 0;
	}
      else if (strncmp (name, ".rela", 5) == 0)
	{
	  if (s->size != 0)
	    {
	      asection *target;
	      const char *outname;

	      /* Remember whether there are any relocation sections.  */
	      relocs = TRUE;

	      /* If this relocation section applies to a read only
		 section, then we probably need a DT_TEXTREL entry.  */
	      outname = bfd_get_section_name (output_bfd,
					      s->output_section);
	      target = bfd_get_section_by_name (output_bfd, outname + 5);
	      if (target != NULL
		  && (target->flags & SEC_READONLY) != 0
		  && (target->flags & SEC_ALLOC) != 0)
		reltext = TRUE;

	      /* We use the reloc_count field as a counter if we need
		 to copy relocs into the output file.  */
	      s->reloc_count = 0;
	    }
	}
      else if (strcmp (name, ".got") != 0
	       && strcmp (name, ".sdata") != 0
	       && strcmp (name, ".sdata2") != 0
	       && strcmp (name, ".dynbss") != 0
	       && strcmp (name, ".dynsbss") != 0)
	{
	  /* It's not one of our sections, so don't allocate space.  */
	  continue;
	}

      if (s->size == 0)
	{
	  /* If we don't need this section, strip it from the
	     output file.  This is mostly to handle .rela.bss and
	     .rela.plt.  We must create both sections in
	     create_dynamic_sections, because they must be created
	     before the linker maps input sections to output
	     sections.  The linker does that before
	     adjust_dynamic_symbol is called, and it is that
	     function which decides whether anything needs to go
	     into these sections.  */
	  s->flags |= SEC_EXCLUDE;
	  continue;
	}

      if ((s->flags & SEC_HAS_CONTENTS) == 0)
	continue;

      /* Allocate memory for the section contents.  */
      s->contents = bfd_zalloc (dynobj, s->size);
      if (s->contents == NULL)
	return FALSE;
    }

  if (elf_hash_table (info)->dynamic_sections_created)
    {
      /* Add some entries to the .dynamic section.  We fill in the
	 values later, in i370_elf_finish_dynamic_sections, but we
	 must add the entries now so that we get the correct size for
	 the .dynamic section.  The DT_DEBUG entry is filled in by the
	 dynamic linker and used by the debugger.  */
#define add_dynamic_entry(TAG, VAL) \
  _bfd_elf_add_dynamic_entry (info, TAG, VAL)

      if (!info->shared)
	{
	  if (!add_dynamic_entry (DT_DEBUG, 0))
	    return FALSE;
	}

      if (plt)
	{
	  if (!add_dynamic_entry (DT_PLTGOT, 0)
	      || !add_dynamic_entry (DT_PLTRELSZ, 0)
	      || !add_dynamic_entry (DT_PLTREL, DT_RELA)
	      || !add_dynamic_entry (DT_JMPREL, 0))
	    return FALSE;
	}

      if (relocs)
	{
	  if (!add_dynamic_entry (DT_RELA, 0)
	      || !add_dynamic_entry (DT_RELASZ, 0)
	      || !add_dynamic_entry (DT_RELAENT, sizeof (Elf32_External_Rela)))
	    return FALSE;
	}

      if (reltext)
	{
	  if (!add_dynamic_entry (DT_TEXTREL, 0))
	    return FALSE;
	  info->flags |= DF_TEXTREL;
	}
    }
#undef add_dynamic_entry

  /* If we are generating a shared library, we generate a section
     symbol for each output section.  These are local symbols, which
     means that they must come first in the dynamic symbol table.
     That means we must increment the dynamic symbol index of every
     other dynamic symbol.

     FIXME: We assume that there will never be relocations to
     locations in linker-created sections that do not have
     externally-visible names. Instead, we should work out precisely
     which sections relocations are targeted at.  */
  if (info->shared)
    {
      int c;

      for (c = 0, s = output_bfd->sections; s != NULL; s = s->next)
	{
	  if ((s->flags & SEC_LINKER_CREATED) != 0
	      || (s->flags & SEC_ALLOC) == 0)
	    {
	      elf_section_data (s)->dynindx = -1;
	      continue;
	    }

	  /* These symbols will have no names, so we don't need to
	     fiddle with dynstr_index.  */

	  elf_section_data (s)->dynindx = c + 1;

	  c++;
	}

      elf_link_hash_traverse (elf_hash_table (info),
			      i370_elf_adjust_dynindx, & c);
      elf_hash_table (info)->dynsymcount += c;
    }

  return TRUE;
}

/* Look through the relocs for a section during the first phase, and
   allocate space in the global offset table or procedure linkage
   table.  */
/* XXX hack alert bogus This routine is mostly all junk and almost
   certainly does the wrong thing.  Its here simply because it does
   just enough to allow glibc-2.1 ld.so to compile & link.  */

static bfd_boolean
i370_elf_check_relocs (bfd *abfd,
		       struct bfd_link_info *info,
		       asection *sec,
		       const Elf_Internal_Rela *relocs)
{
  bfd *dynobj;
  Elf_Internal_Shdr *symtab_hdr;
  struct elf_link_hash_entry **sym_hashes;
  const Elf_Internal_Rela *rel;
  const Elf_Internal_Rela *rel_end;
  bfd_vma *local_got_offsets;
  asection *sreloc;

  if (info->relocatable)
    return TRUE;

#ifdef DEBUG
  _bfd_error_handler ("i370_elf_check_relocs called for section %A in %B",
		      sec, abfd);
#endif

  dynobj = elf_hash_table (info)->dynobj;
  symtab_hdr = &elf_tdata (abfd)->symtab_hdr;
  sym_hashes = elf_sym_hashes (abfd);
  local_got_offsets = elf_local_got_offsets (abfd);

  sreloc = NULL;

  rel_end = relocs + sec->reloc_count;
  for (rel = relocs; rel < rel_end; rel++)
    {
      unsigned long r_symndx;
      struct elf_link_hash_entry *h;

      r_symndx = ELF32_R_SYM (rel->r_info);
      if (r_symndx < symtab_hdr->sh_info)
	h = NULL;
      else
	{
	  h = sym_hashes[r_symndx - symtab_hdr->sh_info];
	  while (h->root.type == bfd_link_hash_indirect
		 || h->root.type == bfd_link_hash_warning)
	    h = (struct elf_link_hash_entry *) h->root.u.i.link;
	}

      if (info->shared)
	{
#ifdef DEBUG
	  fprintf (stderr,
		   "i370_elf_check_relocs needs to create relocation for %s\n",
		   (h && h->root.root.string)
		   ? h->root.root.string : "<unknown>");
#endif
	  if (sreloc == NULL)
	    {
	      const char *name;

	      name = (bfd_elf_string_from_elf_section
		      (abfd,
		       elf_elfheader (abfd)->e_shstrndx,
		       elf_section_data (sec)->rel_hdr.sh_name));
	      if (name == NULL)
		return FALSE;

	      BFD_ASSERT (strncmp (name, ".rela", 5) == 0
			  && strcmp (bfd_get_section_name (abfd, sec), name + 5) == 0);

	      sreloc = bfd_get_section_by_name (dynobj, name);
	      if (sreloc == NULL)
		{
		  flagword flags;

		  flags = (SEC_HAS_CONTENTS | SEC_READONLY
			   | SEC_IN_MEMORY | SEC_LINKER_CREATED);
		  if ((sec->flags & SEC_ALLOC) != 0)
		    flags |= SEC_ALLOC | SEC_LOAD;
		  sreloc = bfd_make_section_with_flags (dynobj, name,
							flags);
		  if (sreloc == NULL
		      || ! bfd_set_section_alignment (dynobj, sreloc, 2))
		    return FALSE;
		}
	    }

	  sreloc->size += sizeof (Elf32_External_Rela);

	  /* FIXME: We should here do what the m68k and i386
	     backends do: if the reloc is pc-relative, record it
	     in case it turns out that the reloc is unnecessary
	     because the symbol is forced local by versioning or
	     we are linking with -Bdynamic.  Fortunately this
	     case is not frequent.  */
	}
    }

  return TRUE;
}

/* Finish up the dynamic sections.  */
/* XXX hack alert bogus This routine is mostly all junk and almost
   certainly does the wrong thing.  Its here simply because it does
   just enough to allow glibc-2.1 ld.so to compile & link.  */

static bfd_boolean
i370_elf_finish_dynamic_sections (bfd *output_bfd,
				  struct bfd_link_info *info)
{
  asection *sdyn;
  bfd *dynobj = elf_hash_table (info)->dynobj;
  asection *sgot = bfd_get_section_by_name (dynobj, ".got");

#ifdef DEBUG
  fprintf (stderr, "i370_elf_finish_dynamic_sections called\n");
#endif

  sdyn = bfd_get_section_by_name (dynobj, ".dynamic");

  if (elf_hash_table (info)->dynamic_sections_created)
    {
      asection *splt;
      Elf32_External_Dyn *dyncon, *dynconend;

      splt = bfd_get_section_by_name (dynobj, ".plt");
      BFD_ASSERT (splt != NULL && sdyn != NULL);

      dyncon = (Elf32_External_Dyn *) sdyn->contents;
      dynconend = (Elf32_External_Dyn *) (sdyn->contents + sdyn->size);
      for (; dyncon < dynconend; dyncon++)
	{
	  Elf_Internal_Dyn dyn;
	  const char *name;
	  bfd_boolean size;

	  bfd_elf32_swap_dyn_in (dynobj, dyncon, &dyn);

	  switch (dyn.d_tag)
	    {
	    case DT_PLTGOT:   name = ".plt";	  size = FALSE; break;
	    case DT_PLTRELSZ: name = ".rela.plt"; size = TRUE;  break;
	    case DT_JMPREL:   name = ".rela.plt"; size = FALSE; break;
	    default:	      name = NULL;	  size = FALSE; break;
	    }

	  if (name != NULL)
	    {
	      asection *s;

	      s = bfd_get_section_by_name (output_bfd, name);
	      if (s == NULL)
		dyn.d_un.d_val = 0;
	      else
		{
		  if (! size)
		    dyn.d_un.d_ptr = s->vma;
		  else
		    dyn.d_un.d_val = s->size;
		}
	      bfd_elf32_swap_dyn_out (output_bfd, &dyn, dyncon);
	    }
	}
    }

  if (sgot && sgot->size != 0)
    {
      unsigned char *contents = sgot->contents;

      if (sdyn == NULL)
	bfd_put_32 (output_bfd, (bfd_vma) 0, contents);
      else
	bfd_put_32 (output_bfd,
		    sdyn->output_section->vma + sdyn->output_offset,
		    contents);

      elf_section_data (sgot->output_section)->this_hdr.sh_entsize = 4;
    }

  if (info->shared)
    {
      asection *sdynsym;
      asection *s;
      Elf_Internal_Sym sym;
      int maxdindx = 0;

      /* Set up the section symbols for the output sections.  */

      sdynsym = bfd_get_section_by_name (dynobj, ".dynsym");
      BFD_ASSERT (sdynsym != NULL);

      sym.st_size = 0;
      sym.st_name = 0;
      sym.st_info = ELF_ST_INFO (STB_LOCAL, STT_SECTION);
      sym.st_other = 0;

      for (s = output_bfd->sections; s != NULL; s = s->next)
	{
	  int indx, dindx;
	  Elf32_External_Sym *esym;

	  sym.st_value = s->vma;

	  indx = elf_section_data (s)->this_idx;
	  dindx = elf_section_data (s)->dynindx;
	  if (dindx != -1)
	    {
	      BFD_ASSERT(indx > 0);
	      BFD_ASSERT(dindx > 0);

	      if (dindx > maxdindx)
		maxdindx = dindx;

	      sym.st_shndx = indx;

	      esym = (Elf32_External_Sym *) sdynsym->contents + dindx;
	      bfd_elf32_swap_symbol_out (output_bfd, &sym, esym, NULL);
	    }
	}

      /* Set the sh_info field of the output .dynsym section to the
	 index of the first global symbol.  */
      elf_section_data (sdynsym->output_section)->this_hdr.sh_info =
	maxdindx + 1;
    }

  return TRUE;
}

/* The RELOCATE_SECTION function is called by the ELF backend linker
   to handle the relocations for a section.

   The relocs are always passed as Rela structures; if the section
   actually uses Rel structures, the r_addend field will always be
   zero.

   This function is responsible for adjust the section contents as
   necessary, and (if using Rela relocs and generating a
   relocatable output file) adjusting the reloc addend as
   necessary.

   This function does not have to worry about setting the reloc
   address or the reloc symbol index.

   LOCAL_SYMS is a pointer to the swapped in local symbols.

   LOCAL_SECTIONS is an array giving the section in the input file
   corresponding to the st_shndx field of each local symbol.

   The global hash table entry for the global symbols can be found
   via elf_sym_hashes (input_bfd).

   When generating relocatable output, this function must handle
   STB_LOCAL/STT_SECTION symbols specially.  The output symbol is
   going to be the section symbol corresponding to the output
   section, which means that the addend must be adjusted
   accordingly.  */

static bfd_boolean
i370_elf_relocate_section (bfd *output_bfd,
			   struct bfd_link_info *info,
			   bfd *input_bfd,
			   asection *input_section,
			   bfd_byte *contents,
			   Elf_Internal_Rela *relocs,
			   Elf_Internal_Sym *local_syms,
			   asection **local_sections)
{
  Elf_Internal_Shdr *symtab_hdr = &elf_tdata (input_bfd)->symtab_hdr;
  struct elf_link_hash_entry **sym_hashes = elf_sym_hashes (input_bfd);
  bfd *dynobj = elf_hash_table (info)->dynobj;
  Elf_Internal_Rela *rel = relocs;
  Elf_Internal_Rela *relend = relocs + input_section->reloc_count;
  asection *sreloc = NULL;
  bfd_vma *local_got_offsets;
  bfd_boolean ret = TRUE;

  if (info->relocatable)
    return TRUE;

#ifdef DEBUG
  _bfd_error_handler ("i370_elf_relocate_section called for %B section %A, %ld relocations%s",
		      input_bfd, input_section,
		      (long) input_section->reloc_count,
		      (info->relocatable) ? " (relocatable)" : "");
#endif

  if (!i370_elf_howto_table[ R_I370_ADDR31 ])
    /* Initialize howto table if needed.  */
    i370_elf_howto_init ();

  local_got_offsets = elf_local_got_offsets (input_bfd);

  for (; rel < relend; rel++)
    {
      enum i370_reloc_type r_type    = (enum i370_reloc_type) ELF32_R_TYPE (rel->r_info);
      bfd_vma offset		     = rel->r_offset;
      bfd_vma addend		     = rel->r_addend;
      bfd_reloc_status_type r	     = bfd_reloc_other;
      Elf_Internal_Sym *sym	     = NULL;
      asection *sec		     = NULL;
      struct elf_link_hash_entry * h = NULL;
      const char *sym_name	     = NULL;
      reloc_howto_type *howto;
      unsigned long r_symndx;
      bfd_vma relocation;

      /* Unknown relocation handling.  */
      if ((unsigned) r_type >= (unsigned) R_I370_max
	  || !i370_elf_howto_table[(int)r_type])
	{
	  (*_bfd_error_handler) ("%B: unknown relocation type %d",
				 input_bfd,
				 (int) r_type);

	  bfd_set_error (bfd_error_bad_value);
	  ret = FALSE;
	  continue;
	}

      howto = i370_elf_howto_table[(int) r_type];
      r_symndx = ELF32_R_SYM (rel->r_info);

      if (r_symndx < symtab_hdr->sh_info)
	{
	  sym = local_syms + r_symndx;
	  sec = local_sections[r_symndx];
	  sym_name = "<local symbol>";

	  relocation = _bfd_elf_rela_local_sym (output_bfd, sym, & sec, rel);
	  addend = rel->r_addend;
	}
      else
	{
	  h = sym_hashes[r_symndx - symtab_hdr->sh_info];
	  while (h->root.type == bfd_link_hash_indirect
		 || h->root.type == bfd_link_hash_warning)
	    h = (struct elf_link_hash_entry *) h->root.u.i.link;
	  sym_name = h->root.root.string;
	  if (h->root.type == bfd_link_hash_defined
	      || h->root.type == bfd_link_hash_defweak)
	    {
	      sec = h->root.u.def.section;
	      if (info->shared
		  && ((! info->symbolic && h->dynindx != -1)
		      || !h->def_regular)
		  && (input_section->flags & SEC_ALLOC) != 0
		  && (r_type == R_I370_ADDR31
		      || r_type == R_I370_COPY
		      || r_type == R_I370_ADDR16
		      || r_type == R_I370_RELATIVE))
		/* In these cases, we don't need the relocation
		   value.  We check specially because in some
		   obscure cases sec->output_section will be NULL.  */
		relocation = 0;
	      else
		relocation = (h->root.u.def.value
			      + sec->output_section->vma
			      + sec->output_offset);
	    }
	  else if (h->root.type == bfd_link_hash_undefweak)
	    relocation = 0;
	  else if (info->unresolved_syms_in_objects == RM_IGNORE
		   && ELF_ST_VISIBILITY (h->other) == STV_DEFAULT)
	    relocation = 0;
	  else
	    {
	      if ((*info->callbacks->undefined_symbol)
		  (info, h->root.root.string, input_bfd,
		   input_section, rel->r_offset,
		   (info->unresolved_syms_in_objects == RM_GENERATE_ERROR
		    || ELF_ST_VISIBILITY (h->other))))
		{
		  ret = FALSE;
		  continue;
		}
	      relocation = 0;
	    }
	}

      switch ((int) r_type)
	{
	default:
	  (*_bfd_error_handler)
	    ("%B: unknown relocation type %d for symbol %s",
	     input_bfd, (int) r_type, sym_name);

	  bfd_set_error (bfd_error_bad_value);
	  ret = FALSE;
	  continue;

	case (int) R_I370_NONE:
	  continue;

	/* Relocations that may need to be propagated if this is a shared
	   object.  */
	case (int) R_I370_REL31:
	  /* If these relocations are not to a named symbol, they can be
	     handled right here, no need to bother the dynamic linker.  */
	  if (h == NULL
	      || strcmp (h->root.root.string, "_GLOBAL_OFFSET_TABLE_") == 0)
	    break;
	/* Fall through.  */

	/* Relocations that always need to be propagated if this is a shared
	   object.  */
	case (int) R_I370_ADDR31:
	case (int) R_I370_ADDR16:
	  if (info->shared
	      && r_symndx != 0)
	    {
	      Elf_Internal_Rela outrel;
	      bfd_byte *loc;
	      int skip;

#ifdef DEBUG
	      fprintf (stderr,
		       "i370_elf_relocate_section needs to create relocation for %s\n",
		       (h && h->root.root.string) ? h->root.root.string : "<unknown>");
#endif

	      /* When generating a shared object, these relocations
		 are copied into the output file to be resolved at run
		 time.  */

	      if (sreloc == NULL)
		{
		  const char *name;

		  name = (bfd_elf_string_from_elf_section
			  (input_bfd,
			   elf_elfheader (input_bfd)->e_shstrndx,
			   elf_section_data (input_section)->rel_hdr.sh_name));
		  if (name == NULL)
		    return FALSE;

		  BFD_ASSERT (strncmp (name, ".rela", 5) == 0
			      && strcmp (bfd_get_section_name (input_bfd,
							       input_section),
					 name + 5) == 0);

		  sreloc = bfd_get_section_by_name (dynobj, name);
		  BFD_ASSERT (sreloc != NULL);
		}

	      skip = 0;

	      outrel.r_offset =
		_bfd_elf_section_offset (output_bfd, info, input_section,
					 rel->r_offset);
	      if (outrel.r_offset == (bfd_vma) -1
		  || outrel.r_offset == (bfd_vma) -2)
		skip = (int) outrel.r_offset;
	      outrel.r_offset += (input_section->output_section->vma
				  + input_section->output_offset);

	      if (skip)
		memset (&outrel, 0, sizeof outrel);
	      /* h->dynindx may be -1 if this symbol was marked to
		 become local.  */
	      else if (h != NULL
		       && ((! info->symbolic && h->dynindx != -1)
			   || !h->def_regular))
		{
		  BFD_ASSERT (h->dynindx != -1);
		  outrel.r_info = ELF32_R_INFO (h->dynindx, r_type);
		  outrel.r_addend = rel->r_addend;
		}
	      else
		{
		  if (r_type == R_I370_ADDR31)
		    {
		      outrel.r_info = ELF32_R_INFO (0, R_I370_RELATIVE);
		      outrel.r_addend = relocation + rel->r_addend;
		    }
		  else
		    {
		      long indx;

		      if (bfd_is_abs_section (sec))
			indx = 0;
		      else if (sec == NULL || sec->owner == NULL)
			{
			  bfd_set_error (bfd_error_bad_value);
			  return FALSE;
			}
		      else
			{
			  asection *osec;

			  osec = sec->output_section;
			  indx = elf_section_data (osec)->dynindx;
			  BFD_ASSERT(indx > 0);
#ifdef DEBUG
			  if (indx <= 0)
			    {
			      printf ("indx=%d section=%s flags=%08x name=%s\n",
				      indx, osec->name, osec->flags,
				      h->root.root.string);
			    }
#endif
			}

		      outrel.r_info = ELF32_R_INFO (indx, r_type);
		      outrel.r_addend = relocation + rel->r_addend;
		    }
		}

	      loc = sreloc->contents;
	      loc += sreloc->reloc_count++ * sizeof (Elf32_External_Rela);
	      bfd_elf32_swap_reloca_out (output_bfd, &outrel, loc);

	      /* This reloc will be computed at runtime, so there's no
		 need to do anything now, unless this is a RELATIVE
		 reloc in an unallocated section.  */
	      if (skip == -1
		  || (input_section->flags & SEC_ALLOC) != 0
		  || ELF32_R_TYPE (outrel.r_info) != R_I370_RELATIVE)
		continue;
	    }
	  break;

	case (int) R_I370_COPY:
	case (int) R_I370_RELATIVE:
	  (*_bfd_error_handler)
	    ("%B: Relocation %s is not yet supported for symbol %s.",
	     input_bfd,
	     i370_elf_howto_table[(int) r_type]->name,
	     sym_name);

	  bfd_set_error (bfd_error_invalid_operation);
	  ret = FALSE;
	  continue;
	}

#ifdef DEBUG
      fprintf (stderr, "\ttype = %s (%d), name = %s, symbol index = %ld, offset = %ld, addend = %ld\n",
	       howto->name,
	       (int)r_type,
	       sym_name,
	       r_symndx,
	       (long) offset,
	       (long) addend);
#endif

      r = _bfd_final_link_relocate (howto, input_bfd, input_section, contents,
				    offset, relocation, addend);

      if (r != bfd_reloc_ok)
	{
	  ret = FALSE;
	  switch (r)
	    {
	    default:
	      break;

	    case bfd_reloc_overflow:
	      {
		const char *name;

		if (h != NULL)
		  name = NULL;
		else
		  {
		    name = bfd_elf_string_from_elf_section (input_bfd,
							    symtab_hdr->sh_link,
							    sym->st_name);
		    if (name == NULL)
		      break;

		    if (*name == '\0')
		      name = bfd_section_name (input_bfd, sec);
		  }

		(*info->callbacks->reloc_overflow) (info,
						    (h ? &h->root : NULL),
						    name,
						    howto->name,
						    (bfd_vma) 0,
						    input_bfd,
						    input_section,
						    offset);
	      }
	      break;
	    }
	}
    }

#ifdef DEBUG
  fprintf (stderr, "\n");
#endif

  return ret;
}

static void
i370_elf_post_process_headers (bfd * abfd,
			       struct bfd_link_info * link_info ATTRIBUTE_UNUSED)
{
  Elf_Internal_Ehdr * i_ehdrp;  /* Elf file header, internal form.  */

  i_ehdrp = elf_elfheader (abfd);
  i_ehdrp->e_ident[EI_OSABI] = ELFOSABI_LINUX;
}

#define TARGET_BIG_SYM		bfd_elf32_i370_vec
#define TARGET_BIG_NAME		"elf32-i370"
#define ELF_ARCH		bfd_arch_i370
#define ELF_MACHINE_CODE	EM_S370
#ifdef EM_I370_OLD
#define ELF_MACHINE_ALT1	EM_I370_OLD
#endif
#define ELF_MAXPAGESIZE		0x1000
#define elf_info_to_howto	i370_elf_info_to_howto

#define elf_backend_plt_not_loaded 1
#define elf_backend_rela_normal    1

#define bfd_elf32_bfd_reloc_type_lookup		i370_elf_reloc_type_lookup
#define bfd_elf32_bfd_set_private_flags		i370_elf_set_private_flags
#define bfd_elf32_bfd_merge_private_bfd_data	i370_elf_merge_private_bfd_data
#define elf_backend_relocate_section		i370_elf_relocate_section

/* Dynamic loader support is mostly broken; just enough here to be able to
   link glibc's ld.so without errors.  */
#define elf_backend_create_dynamic_sections	i370_elf_create_dynamic_sections
#define elf_backend_size_dynamic_sections	i370_elf_size_dynamic_sections
#define elf_backend_finish_dynamic_sections	i370_elf_finish_dynamic_sections
#define elf_backend_fake_sections		i370_elf_fake_sections
#define elf_backend_section_from_shdr		i370_elf_section_from_shdr
#define elf_backend_adjust_dynamic_symbol	i370_elf_adjust_dynamic_symbol
#define elf_backend_check_relocs		i370_elf_check_relocs
#define elf_backend_post_process_headers	i370_elf_post_process_headers

static int
i370_noop (void)
{
  return 1;
}

/* We need to define these at least as no-ops to link glibc ld.so.  */

#define elf_backend_add_symbol_hook \
  (bfd_boolean (*) \
     (bfd *, struct bfd_link_info *, Elf_Internal_Sym *, \
      const char **, flagword *, asection **, bfd_vma *)) i370_noop
#define elf_backend_finish_dynamic_symbol \
  (bfd_boolean (*) \
     (bfd *, struct bfd_link_info *, struct elf_link_hash_entry *, \
      Elf_Internal_Sym *)) i370_noop
#define elf_backend_additional_program_headers \
  (int (*) (bfd *)) i370_noop
#define elf_backend_modify_segment_map \
  (bfd_boolean (*) (bfd *, struct bfd_link_info *)) i370_noop

#include "elf32-target.h"
