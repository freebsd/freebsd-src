/* vms.c -- BFD back-end for VAX (openVMS/VAX) and
   EVAX (openVMS/Alpha) files.
   Copyright 1996, 1997, 1998, 1999, 2000, 2001, 2002, 2003, 2004, 2005,
   2006 Free Software Foundation, Inc.

   Written by Klaus K"ampf (kkaempf@rmi.de)

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
   Foundation, Inc., 51 Franklin Street - Fifth Floor, Boston, MA 02110-1301, USA.  */

#include "bfd.h"
#include "sysdep.h"
#include "bfdlink.h"
#include "libbfd.h"

#include "vms.h"

#define vms_bfd_is_target_special_symbol ((bfd_boolean (*) (bfd *, asymbol *)) bfd_false)
#define vms_make_empty_symbol             _bfd_generic_make_empty_symbol
#define vms_bfd_link_just_syms            _bfd_generic_link_just_syms
#define vms_bfd_is_group_section          bfd_generic_is_group_section
#define vms_bfd_discard_group             bfd_generic_discard_group
#define vms_section_already_linked        _bfd_generic_section_already_linked
#define vms_bfd_copy_private_header_data  _bfd_generic_bfd_copy_private_header_data
#define vms_get_synthetic_symtab          _bfd_nodynamic_get_synthetic_symtab

static unsigned int priv_section_count;
extern const bfd_target vms_vax_vec;
extern const bfd_target vms_alpha_vec;

/* Initialize private data.  */

static bfd_boolean
vms_initialize (bfd * abfd)
{
  int i;
  bfd_size_type amt;

  bfd_set_start_address (abfd, (bfd_vma) -1);

  amt = sizeof (struct vms_private_data_struct);
  abfd->tdata.any = bfd_alloc (abfd, amt);
  if (abfd->tdata.any == NULL)
    return FALSE;

#ifdef __ALPHA
  PRIV (is_vax) = FALSE;
#else
  PRIV (is_vax) = TRUE;
#endif
  PRIV (vms_buf) = NULL;
  PRIV (buf_size) = 0;
  PRIV (rec_length) = 0;
  PRIV (file_format) = FF_UNKNOWN;
  PRIV (fixup_done) = FALSE;
  PRIV (sections) = NULL;

  amt = sizeof (struct stack_struct) * STACKSIZE;
  PRIV (stack) = bfd_alloc (abfd, amt);
  if (PRIV (stack) == NULL)
    goto error_ret1;
  PRIV (stackptr) = 0;

  amt = sizeof (struct bfd_hash_table);
  PRIV (vms_symbol_table) = bfd_alloc (abfd, amt);
  if (PRIV (vms_symbol_table) == NULL)
    goto error_ret1;

  if (!bfd_hash_table_init (PRIV (vms_symbol_table), _bfd_vms_hash_newfunc,
			    sizeof (vms_symbol_entry)))
    goto error_ret1;

  amt = sizeof (struct location_struct) * LOCATION_SAVE_SIZE;
  PRIV (location_stack) = bfd_alloc (abfd, amt);
  if (PRIV (location_stack) == NULL)
    goto error_ret2;

  for (i = 0; i < VMS_SECTION_COUNT; i++)
    PRIV (vms_section_table)[i] = NULL;

  amt = MAX_OUTREC_SIZE;
  PRIV (output_buf) = bfd_alloc (abfd, amt);
  if (PRIV (output_buf) == NULL)
    goto error_ret2;

  PRIV (push_level) = 0;
  PRIV (pushed_size) = 0;
  PRIV (length_pos) = 2;
  PRIV (output_size) = 0;
  PRIV (output_alignment) = 1;

  return TRUE;

 error_ret2:
  bfd_hash_table_free (PRIV (vms_symbol_table));
 error_ret1:
  bfd_release (abfd, abfd->tdata.any);
  abfd->tdata.any = NULL;
  return FALSE;
}

/* Fill symbol->section with section ptr
   symbol->section is filled with the section index for defined symbols
   during reading the GSD/EGSD section. But we need the pointer to the
   bfd section later.

   It has the correct value for referenced (undefined section) symbols

   called from bfd_hash_traverse in vms_fixup_sections.  */

static bfd_boolean
fill_section_ptr (struct bfd_hash_entry * entry, void * sections)
{
  asection *sec;
  asymbol *sym;

  sym = ((vms_symbol_entry *) entry)->symbol;
  sec = sym->section;

#if VMS_DEBUG
  vms_debug (6, "fill_section_ptr: sym %p, sec %p\n", sym, sec);
#endif

  /* Fill forward references (these contain section number, not section ptr).  */
  if ((unsigned int) (size_t) sec < priv_section_count)
    sec = ((vms_symbol_entry *) entry)->symbol->section =
      ((asection **) sections)[(unsigned int) (size_t) sec];

  if (strcmp (sym->name, sec->name) == 0)
    sym->flags |= BSF_SECTION_SYM;

  return TRUE;
}

/* Fixup sections
   set up all pointers and arrays, counters and sizes are fixed now

   we build a private sections vector for easy access since sections
   are always referenced by an index number.

   alloc PRIV(sections) according to abfd->section_count
	copy abfd->sections to PRIV(sections).  */

static bfd_boolean
vms_fixup_sections (bfd * abfd)
{
  if (PRIV (fixup_done))
    return TRUE;

  /* Traverse symbol table and fill in all section pointers.  */

  /* Can't provide section count as argument to fill_section_ptr().  */
  priv_section_count = PRIV (section_count);
  bfd_hash_traverse (PRIV (vms_symbol_table), fill_section_ptr, (PRIV (sections)));

  PRIV (fixup_done) = TRUE;

  return TRUE;
}

/* Check the format for a file being read.
   Return a (bfd_target *) if it's an object file or zero if not.  */

static const struct bfd_target *
vms_object_p (bfd * abfd)
{
  int err = 0;
  int prev_type;
  const struct bfd_target *target_vector = NULL;
  const bfd_arch_info_type *arch = NULL;
  void * tdata_save = abfd->tdata.any;
  bfd_vma saddr_save = bfd_get_start_address (abfd);

#if VMS_DEBUG
  vms_debug (1, "vms_object_p (%p)\n", abfd);
#endif

  if (!vms_initialize (abfd))
    goto error_ret;

  if (bfd_seek (abfd, (file_ptr) 0, SEEK_SET))
    goto err_wrong_format;

  prev_type = -1;

  do
    {
#if VMS_DEBUG
      vms_debug (7, "reading at %08lx\n", bfd_tell (abfd));
#endif
      if (_bfd_vms_next_record (abfd) < 0)
	{
#if VMS_DEBUG
	  vms_debug (2, "next_record failed\n");
#endif
	  goto err_wrong_format;
	}

      if ((prev_type == EOBJ_S_C_EGSD)
	   && (PRIV (rec_type) != EOBJ_S_C_EGSD))
	{
	  if (! vms_fixup_sections (abfd))
	    {
#if VMS_DEBUG
	      vms_debug (2, "vms_fixup_sections failed\n");
#endif
	      goto err_wrong_format;
	    }
	}

      prev_type = PRIV (rec_type);

      if (target_vector == NULL)
	{
	  if (prev_type <= OBJ_S_C_MAXRECTYP)
	    target_vector = & vms_vax_vec;
	  else
	    target_vector = & vms_alpha_vec;
	}

      switch (prev_type)
	{
	  case OBJ_S_C_HDR:
	  case EOBJ_S_C_EMH:
	    err = _bfd_vms_slurp_hdr (abfd, prev_type);
	    break;
	  case OBJ_S_C_EOM:
	  case OBJ_S_C_EOMW:
	  case EOBJ_S_C_EEOM:
	    err = _bfd_vms_slurp_eom (abfd, prev_type);
	    break;
	  case OBJ_S_C_GSD:
	  case EOBJ_S_C_EGSD:
	    err = _bfd_vms_slurp_gsd (abfd, prev_type);
	    break;
	  case OBJ_S_C_TIR:
	  case EOBJ_S_C_ETIR:
	    err = _bfd_vms_slurp_tir (abfd, prev_type);
	    break;
	  case OBJ_S_C_DBG:
	  case EOBJ_S_C_EDBG:
	    err = _bfd_vms_slurp_dbg (abfd, prev_type);
	    break;
	  case OBJ_S_C_TBT:
	  case EOBJ_S_C_ETBT:
	    err = _bfd_vms_slurp_tbt (abfd, prev_type);
	    break;
	  case OBJ_S_C_LNK:
	    err = _bfd_vms_slurp_lnk (abfd, prev_type);
	    break;
	  default:
	    err = -1;
	}
      if (err != 0)
	{
#if VMS_DEBUG
	  vms_debug (2, "slurp type %d failed with %d\n", prev_type, err);
#endif
	  goto err_wrong_format;
	}
    }
  while ((prev_type != EOBJ_S_C_EEOM) && (prev_type != OBJ_S_C_EOM) && (prev_type != OBJ_S_C_EOMW));

  if (target_vector == & vms_vax_vec)
    {
      if (! vms_fixup_sections (abfd))
	{
#if VMS_DEBUG
	  vms_debug (2, "vms_fixup_sections failed\n");
#endif
	  goto err_wrong_format;
	}

      /* Set arch_info to vax.  */

      arch = bfd_scan_arch ("vax");
      PRIV (is_vax) = TRUE;
#if VMS_DEBUG
      vms_debug (2, "arch is vax\n");
#endif
    }
  else if (target_vector == & vms_alpha_vec)
    {
      /* Set arch_info to alpha.   */

      arch = bfd_scan_arch ("alpha");
      PRIV (is_vax) = FALSE;
#if VMS_DEBUG
      vms_debug (2, "arch is alpha\n");
#endif
    }

  if (arch == NULL)
    {
#if VMS_DEBUG
      vms_debug (2, "arch not found\n");
#endif
      goto err_wrong_format;
    }
  abfd->arch_info = arch;

  return target_vector;

 err_wrong_format:
  bfd_set_error (bfd_error_wrong_format);
 error_ret:
  if (abfd->tdata.any != tdata_save && abfd->tdata.any != NULL)
    bfd_release (abfd, abfd->tdata.any);
  abfd->tdata.any = tdata_save;
  bfd_set_start_address (abfd, saddr_save);
  return NULL;
}

/* Check the format for a file being read.
   Return a (bfd_target *) if it's an archive file or zero.  */

static const struct bfd_target *
vms_archive_p (bfd * abfd ATTRIBUTE_UNUSED)
{
#if VMS_DEBUG
  vms_debug (1, "vms_archive_p (%p)\n", abfd);
#endif

  return NULL;
}

/* Set the format of a file being written.  */

static bfd_boolean
vms_mkobject (bfd * abfd)
{
#if VMS_DEBUG
  vms_debug (1, "vms_mkobject (%p)\n", abfd);
#endif

  if (!vms_initialize (abfd))
    return FALSE;

  {
#ifdef __VAX
    const bfd_arch_info_type *arch = bfd_scan_arch ("vax");
#else
    const bfd_arch_info_type *arch = bfd_scan_arch ("alpha");
#endif
    if (arch == NULL)
      {
	bfd_set_error (bfd_error_wrong_format);
	return FALSE;
      }
    abfd->arch_info = arch;
  }

  return TRUE;
}

/* Write cached information into a file being written, at bfd_close.  */

static bfd_boolean
vms_write_object_contents (bfd * abfd)
{
#if VMS_DEBUG
  vms_debug (1, "vms_write_object_contents (%p)\n", abfd);
#endif

  if (abfd->section_count > 0)			/* we have sections */
    {
      if (PRIV (is_vax))
	{
	  if (_bfd_vms_write_hdr (abfd, OBJ_S_C_HDR) != 0)
	    return FALSE;
	  if (_bfd_vms_write_gsd (abfd, OBJ_S_C_GSD) != 0)
	    return FALSE;
	  if (_bfd_vms_write_tir (abfd, OBJ_S_C_TIR) != 0)
	    return FALSE;
	  if (_bfd_vms_write_tbt (abfd, OBJ_S_C_TBT) != 0)
	    return FALSE;
	  if (_bfd_vms_write_dbg (abfd, OBJ_S_C_DBG) != 0)
	    return FALSE;
	  if (abfd->section_count > 255)
	    {
	      if (_bfd_vms_write_eom (abfd, OBJ_S_C_EOMW) != 0)
		return FALSE;
	    }
	  else
	    {
	      if (_bfd_vms_write_eom (abfd, OBJ_S_C_EOM) != 0)
		return FALSE;
	    }
	}
      else
	{
	  if (_bfd_vms_write_hdr (abfd, EOBJ_S_C_EMH) != 0)
	    return FALSE;
	  if (_bfd_vms_write_gsd (abfd, EOBJ_S_C_EGSD) != 0)
	    return FALSE;
	  if (_bfd_vms_write_tir (abfd, EOBJ_S_C_ETIR) != 0)
	    return FALSE;
	  if (_bfd_vms_write_tbt (abfd, EOBJ_S_C_ETBT) != 0)
	    return FALSE;
	  if (_bfd_vms_write_dbg (abfd, EOBJ_S_C_EDBG) != 0)
	    return FALSE;
	  if (_bfd_vms_write_eom (abfd, EOBJ_S_C_EEOM) != 0)
	    return FALSE;
	}
    }
  return TRUE;
}

/* 4.1, generic.  */

/* Called when the BFD is being closed to do any necessary cleanup.  */

static bfd_boolean
vms_close_and_cleanup (bfd * abfd)
{
#if VMS_DEBUG
  vms_debug (1, "vms_close_and_cleanup (%p)\n", abfd);
#endif
  if (abfd == NULL)
    return TRUE;

  if (PRIV (vms_buf) != NULL)
    free (PRIV (vms_buf));

  if (PRIV (sections) != NULL)
    free (PRIV (sections));

  if (PRIV (vms_symbol_table))
    bfd_hash_table_free (PRIV (vms_symbol_table));

  bfd_release (abfd, abfd->tdata.any);
  abfd->tdata.any = NULL;

  return TRUE;
}

/* Ask the BFD to free all cached information.  */

static bfd_boolean
vms_bfd_free_cached_info (bfd * abfd ATTRIBUTE_UNUSED)
{
#if VMS_DEBUG
  vms_debug (1, "vms_bfd_free_cached_info (%p)\n", abfd);
#endif
  return TRUE;
}

/* Called when a new section is created.  */

static bfd_boolean
vms_new_section_hook (bfd * abfd, asection *section)
{
  /* Count hasn't been incremented yet.  */
  unsigned int section_count = abfd->section_count + 1;

#if VMS_DEBUG
  vms_debug (1, "vms_new_section_hook (%p, [%d]%s), count %d\n",
	     abfd, section->index, section->name, section_count);
#endif
  bfd_set_section_alignment (abfd, section, 4);

  if (section_count > PRIV (section_count))
    {
      bfd_size_type amt = section_count;
      amt *= sizeof (asection *);
      PRIV (sections) = bfd_realloc (PRIV (sections), amt);
      if (PRIV (sections) == NULL)
	return FALSE;
      PRIV (section_count) = section_count;
    }
#if VMS_DEBUG
  vms_debug (6, "section_count: %d\n", PRIV (section_count));
#endif
  PRIV (sections)[section->index] = section;
#if VMS_DEBUG
  vms_debug (7, "%d: %s\n", section->index, section->name);
#endif

  return TRUE;
}

/* Read the contents of a section.
   buf points to a buffer of buf_size bytes to be filled with
   section data (starting at offset into section)  */

static bfd_boolean
vms_get_section_contents (bfd * abfd ATTRIBUTE_UNUSED,
			  asection *section ATTRIBUTE_UNUSED,
			  void * buf ATTRIBUTE_UNUSED,
			  file_ptr offset ATTRIBUTE_UNUSED,
			  bfd_size_type buf_size ATTRIBUTE_UNUSED)
{
#if VMS_DEBUG
  vms_debug (1, "vms_get_section_contents (%p, %s, %p, off %ld, size %d)\n",
		 abfd, section->name, buf, offset, (int)buf_size);
#endif

  /* Shouldn't be called, since all sections are IN_MEMORY.  */
  return FALSE;
}

/* Read the contents of a section.
   buf points to a buffer of buf_size bytes to be filled with
   section data (starting at offset into section).  */

static bfd_boolean
vms_get_section_contents_in_window (bfd * abfd ATTRIBUTE_UNUSED,
				    asection *section ATTRIBUTE_UNUSED,
				    bfd_window *w ATTRIBUTE_UNUSED,
				    file_ptr offset ATTRIBUTE_UNUSED,
				    bfd_size_type count ATTRIBUTE_UNUSED)
{
#if VMS_DEBUG
  vms_debug (1, "vms_get_section_contents_in_window (%p, %s, %p, off %ld, count %d)\n",
		 abfd, section->name, w, offset, (int)count);
#endif

  /* Shouldn't be called, since all sections are IN_MEMORY.  */
  return FALSE;
}

/* Part 4.2, copy private data.  */

/* Called to copy BFD general private data from one object file
   to another.  */

static bfd_boolean
vms_bfd_copy_private_bfd_data (bfd *src ATTRIBUTE_UNUSED,
			       bfd *dest ATTRIBUTE_UNUSED)
{
#if VMS_DEBUG
  vms_debug (1, "vms_bfd_copy_private_bfd_data (%p, %p)\n", src, dest);
#endif
  return TRUE;
}

/* Merge private BFD information from the BFD @var{ibfd} to the
   the output file BFD @var{obfd} when linking.  Return <<TRUE>>
   on success, <<FALSE>> on error.  Possible error returns are:

   o <<bfd_error_no_memory>> -
     Not enough memory exists to create private data for @var{obfd}.  */

static bfd_boolean
vms_bfd_merge_private_bfd_data (bfd * ibfd ATTRIBUTE_UNUSED,
				bfd * obfd ATTRIBUTE_UNUSED)
{
#if VMS_DEBUG
  vms_debug (1,"vms_bfd_merge_private_bfd_data (%p, %p)\n", ibfd, obfd);
#endif
  return TRUE;
}

/* Set private BFD flag information in the BFD @var{abfd}.
   Return <<TRUE>> on success, <<FALSE>> on error.  Possible error
   returns are:

   o <<bfd_error_no_memory>> -
     Not enough memory exists to create private data for @var{obfd}.  */

static bfd_boolean
vms_bfd_set_private_flags (bfd * abfd ATTRIBUTE_UNUSED,
			   flagword flags ATTRIBUTE_UNUSED)
{
#if VMS_DEBUG
  vms_debug (1,"vms_bfd_set_private_flags (%p, %lx)\n", abfd, (long)flags);
#endif
  return TRUE;
}

/* Called to copy BFD private section data from one object file
   to another.  */

static bfd_boolean
vms_bfd_copy_private_section_data (bfd *srcbfd ATTRIBUTE_UNUSED,
				   asection *srcsec ATTRIBUTE_UNUSED,
				   bfd *dstbfd ATTRIBUTE_UNUSED,
				   asection *dstsec ATTRIBUTE_UNUSED)
{
#if VMS_DEBUG
  vms_debug (1, "vms_bfd_copy_private_section_data (%p, %s, %p, %s)\n",
		 srcbfd, srcsec->name, dstbfd, dstsec->name);
#endif
  return TRUE;
}

/* Called to copy BFD private symbol data from one object file
   to another.  */

static bfd_boolean
vms_bfd_copy_private_symbol_data (bfd *ibfd ATTRIBUTE_UNUSED,
				  asymbol *isym ATTRIBUTE_UNUSED,
				  bfd *obfd ATTRIBUTE_UNUSED,
				  asymbol *osym ATTRIBUTE_UNUSED)
{
#if VMS_DEBUG
  vms_debug (1, "vms_bfd_copy_private_symbol_data (%p, %s, %p, %s)\n",
		 ibfd, isym->name, obfd, osym->name);
#endif
  return TRUE;
}

/* Part 4.3, core file.  */

/* Return a read-only string explaining which program was running
   when it failed and produced the core file abfd.  */

static char *
vms_core_file_failing_command (bfd * abfd ATTRIBUTE_UNUSED)
{
#if VMS_DEBUG
  vms_debug (1, "vms_core_file_failing_command (%p)\n", abfd);
#endif
  return NULL;
}

/* Returns the signal number which caused the core dump which
   generated the file the BFD abfd is attached to.  */

static int
vms_core_file_failing_signal (bfd * abfd ATTRIBUTE_UNUSED)
{
#if VMS_DEBUG
  vms_debug (1, "vms_core_file_failing_signal (%p)\n", abfd);
#endif
  return 0;
}

/* Return TRUE if the core file attached to core_bfd was generated
   by a run of the executable file attached to exec_bfd, FALSE otherwise.  */

static bfd_boolean
vms_core_file_matches_executable_p (bfd * abfd ATTRIBUTE_UNUSED,
				    bfd *bbfd ATTRIBUTE_UNUSED)
{
#if VMS_DEBUG
  vms_debug (1, "vms_core_file_matches_executable_p (%p, %p)\n", abfd, bbfd);
#endif
  return FALSE;
}

/* Part 4.4, archive.  */

/* ???	do something with an archive map.
   Return FALSE on error, TRUE otherwise.  */

static bfd_boolean
vms_slurp_armap (bfd * abfd ATTRIBUTE_UNUSED)
{
#if VMS_DEBUG
  vms_debug (1, "vms_slurp_armap (%p)\n", abfd);
#endif
  return FALSE;
}

/* ???	do something with an extended name table.
   Return FALSE on error, TRUE otherwise.  */

static bfd_boolean
vms_slurp_extended_name_table (bfd * abfd ATTRIBUTE_UNUSED)
{
#if VMS_DEBUG
  vms_debug (1, "vms_slurp_extended_name_table (%p)\n", abfd);
#endif
  return FALSE;
}

/* ???	do something with an extended name table.
   Return FALSE on error, TRUE otherwise.  */

static bfd_boolean
vms_construct_extended_name_table (bfd * abfd ATTRIBUTE_UNUSED,
				   char **tabloc ATTRIBUTE_UNUSED,
				   bfd_size_type *tablen ATTRIBUTE_UNUSED,
				   const char **name ATTRIBUTE_UNUSED)
{
#if VMS_DEBUG
  vms_debug (1, "vms_construct_extended_name_table (%p)\n", abfd);
#endif
  return FALSE;
}

/* Truncate the name of an archive to match system-dependent restrictions.  */

static void
vms_truncate_arname (bfd * abfd ATTRIBUTE_UNUSED,
		     const char *pathname ATTRIBUTE_UNUSED,
		     char *arhdr ATTRIBUTE_UNUSED)
{
#if VMS_DEBUG
  vms_debug (1, "vms_truncate_arname (%p, %s, %s)\n", abfd, pathname, arhdr);
#endif
  return;
}

/* ???	write archive map.  */

static bfd_boolean
vms_write_armap (bfd *arch ATTRIBUTE_UNUSED,
		 unsigned int elength ATTRIBUTE_UNUSED,
		 struct orl *map ATTRIBUTE_UNUSED,
		 unsigned int orl_count ATTRIBUTE_UNUSED,
		 int stridx ATTRIBUTE_UNUSED)
{
#if VMS_DEBUG
  vms_debug (1, "vms_write_armap (%p, %d, %p, %d %d)\n",
	arch, elength, map, orl_count, stridx);
#endif
  return TRUE;
}

/* Read archive header ???  */

static void *
vms_read_ar_hdr (bfd * abfd ATTRIBUTE_UNUSED)
{
#if VMS_DEBUG
  vms_debug (1, "vms_read_ar_hdr (%p)\n", abfd);
#endif
  return NULL;
}

/* Provided a BFD, @var{archive}, containing an archive and NULL, open
   an input BFD on the first contained element and returns that.
   Subsequent calls should pass the archive and the previous return value
   to return a created BFD to the next contained element.
   NULL is returned when there are no more.  */

static bfd *
vms_openr_next_archived_file (bfd *arch ATTRIBUTE_UNUSED,
			      bfd *prev ATTRIBUTE_UNUSED)
{
#if VMS_DEBUG
  vms_debug (1, "vms_openr_next_archived_file (%p, %p)\n", arch, prev);
#endif
  return NULL;
}

/* Return the BFD which is referenced by the symbol in ABFD indexed by
   INDEX.  INDEX should have been returned by bfd_get_next_mapent.  */

static bfd *
vms_get_elt_at_index (bfd * abfd, symindex index)
{
#if VMS_DEBUG
  vms_debug (1, "vms_get_elt_at_index (%p, %p)\n", abfd, index);
#endif
  return _bfd_generic_get_elt_at_index (abfd, index);
}

/* ???
   -> bfd_generic_stat_arch_elt.  */

static int
vms_generic_stat_arch_elt (bfd * abfd, struct stat *st)
{
#if VMS_DEBUG
  vms_debug (1, "vms_generic_stat_arch_elt (%p, %p)\n", abfd, st);
#endif
  return bfd_generic_stat_arch_elt (abfd, st);
}

/* This is a new function in bfd 2.5.  */

static bfd_boolean
vms_update_armap_timestamp (bfd * abfd ATTRIBUTE_UNUSED)
{
#if VMS_DEBUG
  vms_debug (1, "vms_update_armap_timestamp (%p)\n", abfd);
#endif
  return TRUE;
}

/* Part 4.5, symbols.  */

/* Return the number of bytes required to store a vector of pointers
   to asymbols for all the symbols in the BFD abfd, including a
   terminal NULL pointer. If there are no symbols in the BFD,
   then return 0.  If an error occurs, return -1.  */

static long
vms_get_symtab_upper_bound (bfd * abfd)
{
#if VMS_DEBUG
  vms_debug (1, "vms_get_symtab_upper_bound (%p), %d symbols\n", abfd, PRIV (gsd_sym_count));
#endif
  return (PRIV (gsd_sym_count) + 1) * sizeof (asymbol *);
}

/* Copy symbols from hash table to symbol vector

   called from bfd_hash_traverse in vms_canonicalize_symtab
   init counter to 0 if entry == 0.  */

static bfd_boolean
copy_symbols (struct bfd_hash_entry *entry, void * arg)
{
  bfd * abfd = (bfd *) arg;

  if (entry == NULL)	/* Init counter.  */
    PRIV (symnum) = 0;
  else			/* Fill vector, inc counter.  */
    PRIV (symcache)[PRIV (symnum)++] = ((vms_symbol_entry *)entry)->symbol;

  return TRUE;
}

/* Read the symbols from the BFD abfd, and fills in the vector
   location with pointers to the symbols and a trailing NULL.

   Return number of symbols read.   */

static long
vms_canonicalize_symtab (bfd * abfd, asymbol **symbols)
{
#if VMS_DEBUG
  vms_debug (1, "vms_canonicalize_symtab (%p, <ret>)\n", abfd);
#endif

  /* Init counter.  */
  copy_symbols (NULL, abfd);

  /* Traverse table and fill symbols vector.  */
  PRIV (symcache) = symbols;
  bfd_hash_traverse (PRIV (vms_symbol_table), copy_symbols, abfd);

  symbols[PRIV (gsd_sym_count)] = NULL;

  return PRIV (gsd_sym_count);
}

/* Print symbol to file according to how. how is one of
   bfd_print_symbol_name	just print the name
   bfd_print_symbol_more	print more (???)
   bfd_print_symbol_all	print all we know, which is not much right now :-).  */

static void
vms_print_symbol (bfd * abfd,
		  void * file,
		  asymbol *symbol,
		  bfd_print_symbol_type how)
{
#if VMS_DEBUG
  vms_debug (1, "vms_print_symbol (%p, %p, %p, %d)\n", abfd, file, symbol, how);
#endif

  switch (how)
    {
      case bfd_print_symbol_name:
      case bfd_print_symbol_more:
	fprintf ((FILE *)file," %s", symbol->name);
      break;

      case bfd_print_symbol_all:
	{
	  const char *section_name = symbol->section->name;

	  bfd_print_symbol_vandf (abfd, file, symbol);

	  fprintf ((FILE *) file," %-8s %s", section_name, symbol->name);
        }
      break;
    }
}

/* Return information about symbol in ret.

   fill type, value and name
   type:
	A	absolute
	B	bss segment symbol
	C	common symbol
	D	data segment symbol
	f	filename
	t	a static function symbol
	T	text segment symbol
	U	undefined
	-	debug.  */

static void
vms_get_symbol_info (bfd * abfd ATTRIBUTE_UNUSED,
		     asymbol *symbol,
		     symbol_info *ret)
{
  asection *sec;

#if VMS_DEBUG
  vms_debug (1, "vms_get_symbol_info (%p, %p, %p)\n", abfd, symbol, ret);
#endif

  sec = symbol->section;

  if (ret == NULL)
    return;

  if (bfd_is_com_section (sec))
    ret->type = 'C';
  else if (bfd_is_abs_section (sec))
    ret->type = 'A';
  else if (bfd_is_und_section (sec))
    ret->type = 'U';
  else if (bfd_is_ind_section (sec))
    ret->type = 'I';
  else if (bfd_get_section_flags (abfd, sec) & SEC_CODE)
    ret->type = 'T';
  else if (bfd_get_section_flags (abfd, sec) & SEC_DATA)
    ret->type = 'D';
  else if (bfd_get_section_flags (abfd, sec) & SEC_ALLOC)
    ret->type = 'B';
  else
    ret->type = '-';

  if (ret->type != 'U')
    ret->value = symbol->value + symbol->section->vma;
  else
    ret->value = 0;
  ret->name = symbol->name;
}

/* Return TRUE if the given symbol sym in the BFD abfd is
   a compiler generated local label, else return FALSE.  */

static bfd_boolean
vms_bfd_is_local_label_name (bfd * abfd ATTRIBUTE_UNUSED,
			     const char *name)
{
#if VMS_DEBUG
  vms_debug (1, "vms_bfd_is_local_label_name (%p, %s)\n", abfd, name);
#endif
  return name[0] == '$';
}

/* Get source line number for symbol.  */

static alent *
vms_get_lineno (bfd * abfd ATTRIBUTE_UNUSED,
		asymbol *symbol ATTRIBUTE_UNUSED)
{
#if VMS_DEBUG
  vms_debug (1, "vms_get_lineno (%p, %p)\n", abfd, symbol);
#endif
  return NULL;
}

/* Provided a BFD, a section and an offset into the section, calculate and
   return the name of the source file and the line nearest to the wanted
   location.  */

static bfd_boolean
vms_find_nearest_line (bfd * abfd ATTRIBUTE_UNUSED,
		       asection *section ATTRIBUTE_UNUSED,
		       asymbol **symbols ATTRIBUTE_UNUSED,
		       bfd_vma offset ATTRIBUTE_UNUSED,
		       const char **file ATTRIBUTE_UNUSED,
		       const char **func ATTRIBUTE_UNUSED,
		       unsigned int *line ATTRIBUTE_UNUSED)
{
#if VMS_DEBUG
  vms_debug (1, "vms_find_nearest_line (%p, %s, %p, %ld, <ret>, <ret>, <ret>)\n",
	      abfd, section->name, symbols, (long int)offset);
#endif
  return FALSE;
}

static bfd_boolean
vms_find_inliner_info (bfd * abfd ATTRIBUTE_UNUSED,
		       const char **file ATTRIBUTE_UNUSED,
		       const char **func ATTRIBUTE_UNUSED,
		       unsigned int *line ATTRIBUTE_UNUSED)
{
#if VMS_DEBUG
  vms_debug (1, "vms_find_inliner_info (%p, <ret>, <ret>, <ret>)\n",
	     abfd);
#endif
  return FALSE;
}

/* Back-door to allow format-aware applications to create debug symbols
   while using BFD for everything else.  Currently used by the assembler
   when creating COFF files.  */

static asymbol *
vms_bfd_make_debug_symbol (bfd * abfd ATTRIBUTE_UNUSED,
			   void *ptr ATTRIBUTE_UNUSED,
			   unsigned long size ATTRIBUTE_UNUSED)
{
#if VMS_DEBUG
  vms_debug (1, "vms_bfd_make_debug_symbol (%p, %p, %ld)\n", abfd, ptr, size);
#endif
  return NULL;
}

/* Read minisymbols.  For minisymbols, we use the unmodified a.out
   symbols.  The minisymbol_to_symbol function translates these into
   BFD asymbol structures.  */

static long
vms_read_minisymbols (bfd * abfd,
		      bfd_boolean dynamic,
		      void * *minisymsp,
		      unsigned int *sizep)
{
#if VMS_DEBUG
  vms_debug (1, "vms_read_minisymbols (%p, %d, %p, %d)\n", abfd, dynamic, minisymsp, *sizep);
#endif
  return _bfd_generic_read_minisymbols (abfd, dynamic, minisymsp, sizep);
}

/* Convert a minisymbol to a BFD asymbol.  A minisymbol is just an
   unmodified a.out symbol.  The SYM argument is a structure returned
   by bfd_make_empty_symbol, which we fill in here.  */

static asymbol *
vms_minisymbol_to_symbol (bfd * abfd,
			  bfd_boolean dynamic,
			  const void * minisym,
			  asymbol *sym)
{
#if VMS_DEBUG
  vms_debug (1, "vms_minisymbol_to_symbol (%p, %d, %p, %p)\n", abfd, dynamic, minisym, sym);
#endif
  return _bfd_generic_minisymbol_to_symbol (abfd, dynamic, minisym, sym);
}

/* Part 4.6, relocations.  */

/* Return the number of bytes required to store the relocation information
   associated with section sect attached to bfd abfd.
   If an error occurs, return -1.  */

static long
vms_get_reloc_upper_bound (bfd * abfd ATTRIBUTE_UNUSED,
			   asection *section ATTRIBUTE_UNUSED)
{
#if VMS_DEBUG
  vms_debug (1, "vms_get_reloc_upper_bound (%p, %s)\n", abfd, section->name);
#endif
  return -1L;
}

/* Call the back end associated with the open BFD abfd and translate the
   external form of the relocation information attached to sec into the
   internal canonical form.  Place the table into memory at loc, which has
   been preallocated, usually by a call to bfd_get_reloc_upper_bound.
   Returns the number of relocs, or -1 on error.  */

static long
vms_canonicalize_reloc (bfd * abfd ATTRIBUTE_UNUSED,
			asection *section ATTRIBUTE_UNUSED,
			arelent **location ATTRIBUTE_UNUSED,
			asymbol **symbols ATTRIBUTE_UNUSED)
{
#if VMS_DEBUG
  vms_debug (1, "vms_canonicalize_reloc (%p, %s, <ret>, <ret>)\n", abfd, section->name);
#endif
  return FALSE;
}

/* This is just copied from ecoff-alpha, needs to be fixed probably.  */

/* How to process the various reloc types.  */

static bfd_reloc_status_type
reloc_nil (bfd * abfd ATTRIBUTE_UNUSED,
	   arelent *reloc ATTRIBUTE_UNUSED,
	   asymbol *sym ATTRIBUTE_UNUSED,
	   void * data ATTRIBUTE_UNUSED,
	   asection *sec ATTRIBUTE_UNUSED,
	   bfd *output_bfd ATTRIBUTE_UNUSED,
	   char **error_message ATTRIBUTE_UNUSED)
{
#if VMS_DEBUG
  vms_debug (1, "reloc_nil (abfd %p, output_bfd %p)\n", abfd, output_bfd);
  vms_debug (2, "In section %s, symbol %s\n",
	sec->name, sym->name);
  vms_debug (2, "reloc sym %s, addr %08lx, addend %08lx, reloc is a %s\n",
		reloc->sym_ptr_ptr[0]->name,
		(unsigned long)reloc->address,
		(unsigned long)reloc->addend, reloc->howto->name);
  vms_debug (2, "data at %p\n", data);
  /*  _bfd_hexdump (2, data, bfd_get_reloc_size (reloc->howto), 0); */
#endif

  return bfd_reloc_ok;
}

/* In case we're on a 32-bit machine, construct a 64-bit "-1" value
   from smaller values.  Start with zero, widen, *then* decrement.  */
#define MINUS_ONE	(((bfd_vma)0) - 1)

static reloc_howto_type alpha_howto_table[] =
{
  HOWTO (ALPHA_R_IGNORE,	/* Type.  */
	 0,			/* Rightshift.  */
	 0,			/* Size (0 = byte, 1 = short, 2 = long).  */
	 8,			/* Bitsize.  */
	 TRUE,			/* PC relative.  */
	 0,			/* Bitpos.  */
	 complain_overflow_dont,/* Complain_on_overflow.  */
	 reloc_nil,		/* Special_function.  */
	 "IGNORE",		/* Name.  */
	 TRUE,			/* Partial_inplace.  */
	 0,			/* Source mask */
	 0,			/* Dest mask.  */
	 TRUE),			/* PC rel offset.  */

  /* A 64 bit reference to a symbol.  */
  HOWTO (ALPHA_R_REFQUAD,	/* Type.  */
	 0,			/* Rightshift.  */
	 4,			/* Size (0 = byte, 1 = short, 2 = long).  */
	 64,			/* Bitsize.  */
	 FALSE,			/* PC relative.  */
	 0,			/* Bitpos.  */
	 complain_overflow_bitfield, /* Complain_on_overflow.  */
	 reloc_nil,		/* Special_function.  */
	 "REFQUAD",		/* Name.  */
	 TRUE,			/* Partial_inplace.  */
	 MINUS_ONE,		/* Source mask.  */
	 MINUS_ONE,		/* Dest mask.  */
	 FALSE),		/* PC rel offset.  */

  /* A 21 bit branch.  The native assembler generates these for
     branches within the text segment, and also fills in the PC
     relative offset in the instruction.  */
  HOWTO (ALPHA_R_BRADDR,	/* Type.  */
	 2,			/* Rightshift.  */
	 2,			/* Size (0 = byte, 1 = short, 2 = long).  */
	 21,			/* Bitsize.  */
	 TRUE,			/* PC relative.  */
	 0,			/* Bitpos.  */
	 complain_overflow_signed, /* Complain_on_overflow.  */
	 reloc_nil,		/* Special_function.  */
	 "BRADDR",		/* Name.  */
	 TRUE,			/* Partial_inplace.  */
	 0x1fffff,		/* Source mask.  */
	 0x1fffff,		/* Dest mask.  */
	 FALSE),		/* PC rel offset.  */

  /* A hint for a jump to a register.  */
  HOWTO (ALPHA_R_HINT,		/* Type.  */
	 2,			/* Rightshift.  */
	 1,			/* Size (0 = byte, 1 = short, 2 = long).  */
	 14,			/* Bitsize.  */
	 TRUE,			/* PC relative.  */
	 0,			/* Bitpos.  */
	 complain_overflow_dont,/* Complain_on_overflow.  */
	 reloc_nil,		/* Special_function.  */
	 "HINT",		/* Name.  */
	 TRUE,			/* Partial_inplace.  */
	 0x3fff,		/* Source mask.  */
	 0x3fff,		/* Dest mask.  */
	 FALSE),		/* PC rel offset.  */

  /* 16 bit PC relative offset.  */
  HOWTO (ALPHA_R_SREL16,	/* Type.  */
	 0,			/* Rightshift.  */
	 1,			/* Size (0 = byte, 1 = short, 2 = long).  */
	 16,			/* Bitsize.  */
	 TRUE,			/* PC relative.  */
	 0,			/* Bitpos.  */
	 complain_overflow_signed, /* Complain_on_overflow.  */
	 reloc_nil,		/* Special_function.  */
	 "SREL16",		/* Name.  */
	 TRUE,			/* Partial_inplace.  */
	 0xffff,		/* Source mask.  */
	 0xffff,		/* Dest mask.  */
	 FALSE),		/* PC rel offset.  */

  /* 32 bit PC relative offset.  */
  HOWTO (ALPHA_R_SREL32,	/* Type.  */
	 0,			/* Rightshift.  */
	 2,			/* Size (0 = byte, 1 = short, 2 = long).  */
	 32,			/* Bitsize.  */
	 TRUE,			/* PC relative.  */
	 0,			/* Bitpos.  */
	 complain_overflow_signed, /* Complain_on_overflow.  */
	 reloc_nil,		/* Special_function.  */
	 "SREL32",		/* Name.  */
	 TRUE,			/* Partial_inplace.  */
	 0xffffffff,		/* Source mask.  */
	 0xffffffff,		/* Dest mask.  */
	 FALSE),		/* PC rel offset.  */

  /* A 64 bit PC relative offset.  */
  HOWTO (ALPHA_R_SREL64,	/* Type.  */
	 0,			/* Rightshift.  */
	 4,			/* Size (0 = byte, 1 = short, 2 = long).  */
	 64,			/* Bitsize.  */
	 TRUE,			/* PC relative.  */
	 0,			/* Bitpos.  */
	 complain_overflow_signed, /* Complain_on_overflow.  */
	 reloc_nil,		/* Special_function.  */
	 "SREL64",		/* Name.  */
	 TRUE,			/* Partial_inplace.  */
	 MINUS_ONE,		/* Source mask.  */
	 MINUS_ONE,		/* Dest mask.  */
	 FALSE),		/* PC rel offset.  */

  /* Push a value on the reloc evaluation stack.  */
  HOWTO (ALPHA_R_OP_PUSH,	/* Type.  */
	 0,			/* Rightshift.  */
	 0,			/* Size (0 = byte, 1 = short, 2 = long).  */
	 0,			/* Bitsize.  */
	 FALSE,			/* PC relative.  */
	 0,			/* Bitpos.  */
	 complain_overflow_dont,/* Complain_on_overflow.  */
	 reloc_nil,		/* Special_function.  */
	 "OP_PUSH",		/* Name.  */
	 FALSE,			/* Partial_inplace.  */
	 0,			/* Source mask.  */
	 0,			/* Dest mask.  */
	 FALSE),		/* PC rel offset.  */

  /* Store the value from the stack at the given address.  Store it in
     a bitfield of size r_size starting at bit position r_offset.  */
  HOWTO (ALPHA_R_OP_STORE,	/* Type.  */
	 0,			/* Rightshift.  */
	 4,			/* Size (0 = byte, 1 = short, 2 = long).  */
	 64,			/* Bitsize.  */
	 FALSE,			/* PC relative.  */
	 0,			/* Bitpos.  */
	 complain_overflow_dont,/* Complain_on_overflow.  */
	 reloc_nil,		/* Special_function.  */
	 "OP_STORE",		/* Name.  */
	 FALSE,			/* Partial_inplace.  */
	 0,			/* Source mask.  */
	 MINUS_ONE,		/* Dest mask.  */
	 FALSE),		/* PC rel offset.  */

  /* Subtract the reloc address from the value on the top of the
     relocation stack.  */
  HOWTO (ALPHA_R_OP_PSUB,	/* Type.  */
	 0,			/* Rightshift.  */
	 0,			/* Size (0 = byte, 1 = short, 2 = long).  */
	 0,			/* Bitsize.  */
	 FALSE,			/* PC relative.  */
	 0,			/* Bitpos.  */
	 complain_overflow_dont,/* Complain_on_overflow.  */
	 reloc_nil,		/* Special_function.  */
	 "OP_PSUB",		/* Name.  */
	 FALSE,			/* Partial_inplace.  */
	 0,			/* Source mask.  */
	 0,			/* Dest mask.  */
	 FALSE),		/* PC rel offset.  */

  /* Shift the value on the top of the relocation stack right by the
     given value.  */
  HOWTO (ALPHA_R_OP_PRSHIFT,	/* Type.  */
	 0,			/* Rightshift.  */
	 0,			/* Size (0 = byte, 1 = short, 2 = long).  */
	 0,			/* Bitsize.  */
	 FALSE,			/* PC relative.  */
	 0,			/* Bitpos.  */
	 complain_overflow_dont,/* Complain_on_overflow.  */
	 reloc_nil,		/* Special_function.  */
	 "OP_PRSHIFT",		/* Name.  */
	 FALSE,			/* Partial_inplace.  */
	 0,			/* Source mask.  */
	 0,			/* Dest mask.  */
	 FALSE),		/* PC rel offset.  */

  /* Hack. Linkage is done by linker.  */
  HOWTO (ALPHA_R_LINKAGE,	/* Type.  */
	 0,			/* Rightshift.  */
	 8,			/* Size (0 = byte, 1 = short, 2 = long).  */
	 256,			/* Bitsize.  */
	 FALSE,			/* PC relative.  */
	 0,			/* Bitpos.  */
	 complain_overflow_dont,/* Complain_on_overflow.  */
	 reloc_nil,		/* Special_function.  */
	 "LINKAGE",		/* Name.  */
	 FALSE,			/* Partial_inplace.  */
	 0,			/* Source mask.  */
	 0,			/* Dest mask.  */
	 FALSE),		/* PC rel offset.  */

  /* A 32 bit reference to a symbol.  */
  HOWTO (ALPHA_R_REFLONG,	/* Type.  */
	 0,			/* Rightshift.  */
	 2,			/* Size (0 = byte, 1 = short, 2 = long).  */
	 32,			/* Bitsize.  */
	 FALSE,			/* PC relative.  */
	 0,			/* Bitpos.  */
	 complain_overflow_bitfield, /* Complain_on_overflow.  */
	 reloc_nil,		/* Special_function.  */
	 "REFLONG",		/* Name.  */
	 TRUE,			/* Partial_inplace.  */
	 0xffffffff,		/* Source mask.  */
	 0xffffffff,		/* Dest mask.  */
	 FALSE),		/* PC rel offset.  */

  /* A 64 bit reference to a procedure, written as 32 bit value.  */
  HOWTO (ALPHA_R_CODEADDR,	/* Type.  */
	 0,			/* Rightshift.  */
	 4,			/* Size (0 = byte, 1 = short, 2 = long).  */
	 64,			/* Bitsize.  */
	 FALSE,			/* PC relative.  */
	 0,			/* Bitpos.  */
	 complain_overflow_signed,/* Complain_on_overflow.  */
	 reloc_nil,		/* Special_function.  */
	 "CODEADDR",		/* Name.  */
	 FALSE,			/* Partial_inplace.  */
	 0xffffffff,		/* Source mask.  */
	 0xffffffff,		/* Dest mask.  */
	 FALSE),		/* PC rel offset.  */

};

/* Return a pointer to a howto structure which, when invoked, will perform
   the relocation code on data from the architecture noted.  */

static const struct reloc_howto_struct *
vms_bfd_reloc_type_lookup (bfd * abfd ATTRIBUTE_UNUSED,
			   bfd_reloc_code_real_type code)
{
  int alpha_type;

#if VMS_DEBUG
  vms_debug (1, "vms_bfd_reloc_type_lookup (%p, %d)\t", abfd, code);
#endif

  switch (code)
    {
      case BFD_RELOC_16:		alpha_type = ALPHA_R_SREL16;	break;
      case BFD_RELOC_32:		alpha_type = ALPHA_R_REFLONG;	break;
      case BFD_RELOC_64:		alpha_type = ALPHA_R_REFQUAD;	break;
      case BFD_RELOC_CTOR:		alpha_type = ALPHA_R_REFQUAD;	break;
      case BFD_RELOC_23_PCREL_S2:	alpha_type = ALPHA_R_BRADDR;	break;
      case BFD_RELOC_ALPHA_HINT:	alpha_type = ALPHA_R_HINT;	break;
      case BFD_RELOC_16_PCREL:		alpha_type = ALPHA_R_SREL16;	break;
      case BFD_RELOC_32_PCREL:		alpha_type = ALPHA_R_SREL32;	break;
      case BFD_RELOC_64_PCREL:		alpha_type = ALPHA_R_SREL64;	break;
      case BFD_RELOC_ALPHA_LINKAGE:	alpha_type = ALPHA_R_LINKAGE;	break;
      case BFD_RELOC_ALPHA_CODEADDR:	alpha_type = ALPHA_R_CODEADDR;	break;
      default:
	(*_bfd_error_handler) ("reloc (%d) is *UNKNOWN*", code);
	return NULL;
    }
#if VMS_DEBUG
  vms_debug (2, "reloc is %s\n", alpha_howto_table[alpha_type].name);
#endif
  return & alpha_howto_table[alpha_type];
}

/* Part 4.7, writing an object file.  */

/* Set the architecture and machine type in BFD abfd to arch and mach.
   Find the correct pointer to a structure and insert it into the arch_info
   pointer.  */

static bfd_boolean
vms_set_arch_mach (bfd * abfd,
		   enum bfd_architecture arch ATTRIBUTE_UNUSED,
		   unsigned long mach ATTRIBUTE_UNUSED)
{
#if VMS_DEBUG
  vms_debug (1, "vms_set_arch_mach (%p, %d, %ld)\n", abfd, arch, mach);
#endif
  abfd->arch_info = bfd_scan_arch ("alpha");

  return TRUE;
}

/* Sets the contents of the section section in BFD abfd to the data starting
   in memory at data. The data is written to the output section starting at
   offset offset for count bytes.

   Normally TRUE is returned, else FALSE. Possible error returns are:
   o bfd_error_no_contents - The output section does not have the
	SEC_HAS_CONTENTS attribute, so nothing can be written to it.
   o and some more too  */

static bfd_boolean
vms_set_section_contents (bfd * abfd,
			  asection *section,
			  const void * location,
			  file_ptr offset,
			  bfd_size_type count)
{
#if VMS_DEBUG
  vms_debug (1, "vms_set_section_contents (%p, sec %s, loc %p, off %ld, count %d)\n",
	     abfd, section->name, location, (long int)offset, (int)count);
  vms_debug (2, "size %d\n", (int) section->size);
#endif
  return _bfd_save_vms_section (abfd, section, location, offset, count);
}

/* Part 4.8, linker.  */

/* Get the size of the section headers.  */

static int
vms_sizeof_headers (bfd * abfd ATTRIBUTE_UNUSED,
		    bfd_boolean reloc ATTRIBUTE_UNUSED)
{
#if VMS_DEBUG
  vms_debug (1, "vms_sizeof_headers (%p, %s)\n", abfd, (reloc)?"True":"False");
#endif
  return 0;
}

/* Provides default handling of relocation effort for back ends
   which can't be bothered to do it efficiently.  */

static bfd_byte *
vms_bfd_get_relocated_section_contents (bfd * abfd ATTRIBUTE_UNUSED,
					struct bfd_link_info *link_info ATTRIBUTE_UNUSED,
					struct bfd_link_order *link_order ATTRIBUTE_UNUSED,
					bfd_byte *data ATTRIBUTE_UNUSED,
					bfd_boolean relocatable ATTRIBUTE_UNUSED,
					asymbol **symbols ATTRIBUTE_UNUSED)
{
#if VMS_DEBUG
  vms_debug (1, "vms_bfd_get_relocated_section_contents (%p, %p, %p, %p, %s, %p)\n",
	     abfd, link_info, link_order, data, (relocatable)?"True":"False", symbols);
#endif
  return NULL;
}

/* ???  */

static bfd_boolean
vms_bfd_relax_section (bfd * abfd ATTRIBUTE_UNUSED,
		       asection *section ATTRIBUTE_UNUSED,
		       struct bfd_link_info *link_info ATTRIBUTE_UNUSED,
		       bfd_boolean *again ATTRIBUTE_UNUSED)
{
#if VMS_DEBUG
  vms_debug (1, "vms_bfd_relax_section (%p, %s, %p, <ret>)\n",
	     abfd, section->name, link_info);
#endif
  return TRUE;
}

static bfd_boolean
vms_bfd_gc_sections (bfd * abfd ATTRIBUTE_UNUSED,
		     struct bfd_link_info *link_info ATTRIBUTE_UNUSED)
{
#if VMS_DEBUG
  vms_debug (1, "vms_bfd_gc_sections (%p, %p)\n", abfd, link_info);
#endif
  return TRUE;
}

static bfd_boolean
vms_bfd_merge_sections (bfd * abfd ATTRIBUTE_UNUSED,
			struct bfd_link_info *link_info ATTRIBUTE_UNUSED)
{
#if VMS_DEBUG
  vms_debug (1, "vms_bfd_merge_sections (%p, %p)\n", abfd, link_info);
#endif
  return TRUE;
}

/* Create a hash table for the linker.  Different backends store
   different information in this table.  */

static struct bfd_link_hash_table *
vms_bfd_link_hash_table_create (bfd * abfd ATTRIBUTE_UNUSED)
{
#if VMS_DEBUG
  vms_debug (1, "vms_bfd_link_hash_table_create (%p)\n", abfd);
#endif
  return NULL;
}

/* Free a linker hash table.  */

static void
vms_bfd_link_hash_table_free (struct bfd_link_hash_table *hash ATTRIBUTE_UNUSED)
{
#if VMS_DEBUG
  vms_debug (1, "vms_bfd_link_hash_table_free (%p)\n", abfd);
#endif
}

/* Add symbols from this object file into the hash table.  */

static bfd_boolean
vms_bfd_link_add_symbols (bfd * abfd ATTRIBUTE_UNUSED,
			  struct bfd_link_info *link_info ATTRIBUTE_UNUSED)
{
#if VMS_DEBUG
  vms_debug (1, "vms_bfd_link_add_symbols (%p, %p)\n", abfd, link_info);
#endif
  return FALSE;
}

/* Do a link based on the link_order structures attached to each
   section of the BFD.  */

static bfd_boolean
vms_bfd_final_link (bfd * abfd ATTRIBUTE_UNUSED,
		    struct bfd_link_info *link_info ATTRIBUTE_UNUSED)
{
#if VMS_DEBUG
  vms_debug (1, "vms_bfd_final_link (%p, %p)\n", abfd, link_info);
#endif
  return TRUE;
}

/* Should this section be split up into smaller pieces during linking.  */

static bfd_boolean
vms_bfd_link_split_section (bfd * abfd ATTRIBUTE_UNUSED,
			    asection *section ATTRIBUTE_UNUSED)
{
#if VMS_DEBUG
  vms_debug (1, "vms_bfd_link_split_section (%p, %s)\n", abfd, section->name);
#endif
  return FALSE;
}

/* Part 4.9, dynamic symbols and relocations.  */

/* Get the amount of memory required to hold the dynamic symbols.  */

static long
vms_get_dynamic_symtab_upper_bound (bfd * abfd ATTRIBUTE_UNUSED)
{
#if VMS_DEBUG
  vms_debug (1, "vms_get_dynamic_symtab_upper_bound (%p)\n", abfd);
#endif
  return 0;
}

static bfd_boolean
vms_bfd_print_private_bfd_data (bfd * abfd ATTRIBUTE_UNUSED,
				void *file ATTRIBUTE_UNUSED)
{
#if VMS_DEBUG
  vms_debug (1, "vms_bfd_print_private_bfd_data (%p)\n", abfd);
#endif
  return FALSE;
}

/* Read in the dynamic symbols.  */

static long
vms_canonicalize_dynamic_symtab (bfd * abfd ATTRIBUTE_UNUSED,
				 asymbol **symbols ATTRIBUTE_UNUSED)
{
#if VMS_DEBUG
  vms_debug (1, "vms_canonicalize_dynamic_symtab (%p, <ret>)\n", abfd);
#endif
  return 0L;
}

/* Get the amount of memory required to hold the dynamic relocs.  */

static long
vms_get_dynamic_reloc_upper_bound (bfd * abfd ATTRIBUTE_UNUSED)
{
#if VMS_DEBUG
  vms_debug (1, "vms_get_dynamic_reloc_upper_bound (%p)\n", abfd);
#endif
  return 0L;
}

/* Read in the dynamic relocs.  */

static long
vms_canonicalize_dynamic_reloc (bfd * abfd ATTRIBUTE_UNUSED,
				arelent **arel ATTRIBUTE_UNUSED,
				asymbol **symbols ATTRIBUTE_UNUSED)
{
#if VMS_DEBUG
  vms_debug (1, "vms_canonicalize_dynamic_reloc (%p)\n", abfd);
#endif
  return 0L;
}

const bfd_target vms_alpha_vec =
{
  "vms-alpha",			/* Name.  */
  bfd_target_evax_flavour,
  BFD_ENDIAN_LITTLE,		/* Data byte order is little.  */
  BFD_ENDIAN_LITTLE,		/* Header byte order is little.  */

  (HAS_RELOC | HAS_SYMS
   | WP_TEXT | D_PAGED),	/* Object flags.  */
  (SEC_ALLOC | SEC_LOAD | SEC_RELOC
   | SEC_READONLY | SEC_CODE | SEC_DATA
   | SEC_HAS_CONTENTS | SEC_IN_MEMORY),		/* Sect flags.  */
  0,				/* Symbol_leading_char.  */
  ' ',				/* AR_pad_char.  */
  15,				/* AR_max_namelen.  */
  bfd_getl64, bfd_getl_signed_64, bfd_putl64,
  bfd_getl32, bfd_getl_signed_32, bfd_putl32,
  bfd_getl16, bfd_getl_signed_16, bfd_putl16,
  bfd_getl64, bfd_getl_signed_64, bfd_putl64,
  bfd_getl32, bfd_getl_signed_32, bfd_putl32,
  bfd_getl16, bfd_getl_signed_16, bfd_putl16,

  {_bfd_dummy_target, vms_object_p,		/* bfd_check_format.  */
   vms_archive_p, _bfd_dummy_target},
  {bfd_false, vms_mkobject,			/* bfd_set_format.  */
   _bfd_generic_mkarchive, bfd_false},
  {bfd_false, vms_write_object_contents,	/* bfd_write_contents.  */
   _bfd_write_archive_contents, bfd_false},

  BFD_JUMP_TABLE_GENERIC (vms),
  BFD_JUMP_TABLE_COPY (vms),
  BFD_JUMP_TABLE_CORE (vms),
  BFD_JUMP_TABLE_ARCHIVE (vms),
  BFD_JUMP_TABLE_SYMBOLS (vms),
  BFD_JUMP_TABLE_RELOCS (vms),
  BFD_JUMP_TABLE_WRITE (vms),
  BFD_JUMP_TABLE_LINK (vms),
  BFD_JUMP_TABLE_DYNAMIC (vms),

  NULL,

  NULL
};

const bfd_target vms_vax_vec =
{
  "vms-vax",			/* Name.  */
  bfd_target_ovax_flavour,
  BFD_ENDIAN_LITTLE,		/* Data byte order is little.  */
  BFD_ENDIAN_LITTLE,		/* Header byte order is little.  */

  (HAS_RELOC | HAS_SYMS 	/* Object flags.  */
   | WP_TEXT | D_PAGED
   | HAS_LINENO | HAS_DEBUG | HAS_LOCALS),

  (SEC_ALLOC | SEC_LOAD | SEC_RELOC
   | SEC_READONLY | SEC_CODE | SEC_DATA
   | SEC_HAS_CONTENTS | SEC_IN_MEMORY),		/* Sect flags.  */
  0,				/* Symbol_leading_char.  */
  ' ',				/* AR_pad_char.  */
  15,				/* AR_max_namelen.  */
  bfd_getl64, bfd_getl_signed_64, bfd_putl64,
  bfd_getl32, bfd_getl_signed_32, bfd_putl32,
  bfd_getl16, bfd_getl_signed_16, bfd_putl16, /* Data.  */
  bfd_getl64, bfd_getl_signed_64, bfd_putl64,
  bfd_getl32, bfd_getl_signed_32, bfd_putl32,
  bfd_getl16, bfd_getl_signed_16, bfd_putl16, /* Headers.  */

  {_bfd_dummy_target, vms_object_p,		/* bfd_check_format.  */
   vms_archive_p, _bfd_dummy_target},
  {bfd_false, vms_mkobject,			/* bfd_set_format.  */
   _bfd_generic_mkarchive, bfd_false},
  {bfd_false, vms_write_object_contents,	/* bfd_write_contents.  */
   _bfd_write_archive_contents, bfd_false},

  BFD_JUMP_TABLE_GENERIC (vms),
  BFD_JUMP_TABLE_COPY (vms),
  BFD_JUMP_TABLE_CORE (vms),
  BFD_JUMP_TABLE_ARCHIVE (vms),
  BFD_JUMP_TABLE_SYMBOLS (vms),
  BFD_JUMP_TABLE_RELOCS (vms),
  BFD_JUMP_TABLE_WRITE (vms),
  BFD_JUMP_TABLE_LINK (vms),
  BFD_JUMP_TABLE_DYNAMIC (vms),

  NULL,

  NULL
};
