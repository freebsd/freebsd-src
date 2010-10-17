/* vms.c -- BFD back-end for VAX (openVMS/VAX) and
   EVAX (openVMS/Alpha) files.
   Copyright 1996, 1997, 1998, 1999, 2000, 2001, 2002, 2003
   Free Software Foundation, Inc.

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
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#include "bfd.h"
#include "sysdep.h"
#include "bfdlink.h"
#include "libbfd.h"

#include "vms.h"

static bfd_boolean vms_initialize
  PARAMS ((bfd *));
static unsigned int priv_section_count;
static bfd_boolean fill_section_ptr
  PARAMS ((struct bfd_hash_entry *, PTR));
static bfd_boolean vms_fixup_sections
  PARAMS ((bfd *));
static bfd_boolean copy_symbols
  PARAMS ((struct bfd_hash_entry *, PTR));
static bfd_reloc_status_type reloc_nil
  PARAMS ((bfd *, arelent *, asymbol *, PTR, asection *, bfd *, char **));
static const struct bfd_target *vms_object_p
  PARAMS ((bfd *abfd));
static const struct bfd_target *vms_archive_p
  PARAMS ((bfd *abfd));
static bfd_boolean vms_mkobject
  PARAMS ((bfd *abfd));
static bfd_boolean vms_write_object_contents
  PARAMS ((bfd *abfd));
static bfd_boolean vms_close_and_cleanup
  PARAMS ((bfd *abfd));
static bfd_boolean vms_bfd_free_cached_info
  PARAMS ((bfd *abfd));
static bfd_boolean vms_new_section_hook
  PARAMS ((bfd *abfd, asection *section));
static bfd_boolean vms_get_section_contents
  PARAMS ((bfd *abfd, asection *section, PTR x1, file_ptr x2,
	   bfd_size_type x3));
static bfd_boolean vms_get_section_contents_in_window
  PARAMS ((bfd *abfd, asection *section, bfd_window *w, file_ptr offset,
	   bfd_size_type count));
static bfd_boolean vms_bfd_copy_private_bfd_data
  PARAMS ((bfd *src, bfd *dest));
static bfd_boolean vms_bfd_copy_private_section_data
  PARAMS ((bfd *srcbfd, asection *srcsec, bfd *dstbfd, asection *dstsec));
static bfd_boolean vms_bfd_copy_private_symbol_data
  PARAMS ((bfd *ibfd, asymbol *isym, bfd *obfd, asymbol *osym));
static bfd_boolean vms_bfd_print_private_bfd_data
  PARAMS ((bfd *abfd, void *file));
static char *vms_core_file_failing_command
  PARAMS ((bfd *abfd));
static int vms_core_file_failing_signal
  PARAMS ((bfd *abfd));
static bfd_boolean vms_core_file_matches_executable_p
  PARAMS ((bfd *abfd, bfd *bbfd));
static bfd_boolean vms_slurp_armap
  PARAMS ((bfd *abfd));
static bfd_boolean vms_slurp_extended_name_table
  PARAMS ((bfd *abfd));
static bfd_boolean vms_construct_extended_name_table
  PARAMS ((bfd *abfd, char **tabloc, bfd_size_type *tablen,
	   const char **name));
static void vms_truncate_arname
  PARAMS ((bfd *abfd, const char *pathname, char *arhdr));
static bfd_boolean vms_write_armap
  PARAMS ((bfd *arch, unsigned int elength, struct orl *map,
	   unsigned int orl_count, int stridx));
static PTR vms_read_ar_hdr
  PARAMS ((bfd *abfd));
static bfd *vms_get_elt_at_index
  PARAMS ((bfd *abfd, symindex index));
static bfd *vms_openr_next_archived_file
  PARAMS ((bfd *arch, bfd *prev));
static bfd_boolean vms_update_armap_timestamp
  PARAMS ((bfd *abfd));
static int vms_generic_stat_arch_elt
  PARAMS ((bfd *, struct stat *));
static long vms_get_symtab_upper_bound
  PARAMS ((bfd *abfd));
static long vms_canonicalize_symtab
  PARAMS ((bfd *abfd, asymbol **symbols));
static void vms_print_symbol
  PARAMS ((bfd *abfd, PTR file, asymbol *symbol, bfd_print_symbol_type how));
static void vms_get_symbol_info
  PARAMS ((bfd *abfd, asymbol *symbol, symbol_info *ret));
static bfd_boolean vms_bfd_is_local_label_name
  PARAMS ((bfd *abfd, const char *));
static alent *vms_get_lineno
  PARAMS ((bfd *abfd, asymbol *symbol));
static bfd_boolean vms_find_nearest_line
  PARAMS ((bfd *abfd, asection *section, asymbol **symbols, bfd_vma offset,
	   const char **file, const char **func, unsigned int *line));
static asymbol *vms_bfd_make_debug_symbol
  PARAMS ((bfd *abfd, void *ptr, unsigned long size));
static long vms_read_minisymbols
  PARAMS ((bfd *abfd, bfd_boolean dynamic, PTR *minisymsp,
	   unsigned int *sizep));
static asymbol *vms_minisymbol_to_symbol
  PARAMS ((bfd *abfd, bfd_boolean dynamic, const PTR minisym, asymbol *sym));
static long vms_get_reloc_upper_bound
  PARAMS ((bfd *abfd, asection *sect));
static long vms_canonicalize_reloc
  PARAMS ((bfd *abfd, asection *srcsec, arelent **location,
	   asymbol **symbols));
static const struct reloc_howto_struct *vms_bfd_reloc_type_lookup
  PARAMS ((bfd *abfd, bfd_reloc_code_real_type code));
static bfd_boolean vms_set_arch_mach
  PARAMS ((bfd *abfd, enum bfd_architecture arch, unsigned long mach));
static bfd_boolean vms_set_section_contents
  PARAMS ((bfd *abfd, asection *section, const PTR location, file_ptr offset,
	   bfd_size_type count));
static int vms_sizeof_headers
  PARAMS ((bfd *abfd, bfd_boolean reloc));
static bfd_byte *vms_bfd_get_relocated_section_contents
  PARAMS ((bfd *abfd, struct bfd_link_info *link_info,
	   struct bfd_link_order *link_order, bfd_byte *data,
	   bfd_boolean relocatable, asymbol **symbols));
static bfd_boolean vms_bfd_relax_section
  PARAMS ((bfd *abfd, asection *section, struct bfd_link_info *link_info,
	   bfd_boolean *again));
static bfd_boolean vms_bfd_gc_sections
  PARAMS ((bfd *abfd, struct bfd_link_info *link_info));
static bfd_boolean vms_bfd_merge_sections
  PARAMS ((bfd *abfd, struct bfd_link_info *link_info));
static struct bfd_link_hash_table *vms_bfd_link_hash_table_create
  PARAMS ((bfd *abfd));
static void vms_bfd_link_hash_table_free
  PARAMS ((struct bfd_link_hash_table *hash));
static bfd_boolean vms_bfd_link_add_symbols
  PARAMS ((bfd *abfd, struct bfd_link_info *link_info));
static bfd_boolean vms_bfd_final_link
  PARAMS ((bfd *abfd, struct bfd_link_info *link_info));
static bfd_boolean vms_bfd_link_split_section
  PARAMS ((bfd *abfd, asection *section));
static long vms_get_dynamic_symtab_upper_bound
  PARAMS ((bfd *abfd));
static long vms_canonicalize_dynamic_symtab
  PARAMS ((bfd *abfd, asymbol **symbols));
static long vms_get_dynamic_reloc_upper_bound
  PARAMS ((bfd *abfd));
static long vms_canonicalize_dynamic_reloc
  PARAMS ((bfd *abfd, arelent **arel, asymbol **symbols));
static bfd_boolean vms_bfd_merge_private_bfd_data
  PARAMS ((bfd *ibfd, bfd *obfd));
static bfd_boolean vms_bfd_set_private_flags
  PARAMS ((bfd *abfd, flagword flags));

#define vms_make_empty_symbol _bfd_generic_make_empty_symbol
#define vms_bfd_link_just_syms _bfd_generic_link_just_syms
#define vms_bfd_discard_group bfd_generic_discard_group

/*===========================================================================*/

const bfd_target vms_alpha_vec =
{
  "vms-alpha",			/* name */
  bfd_target_evax_flavour,
  BFD_ENDIAN_LITTLE,		/* data byte order is little */
  BFD_ENDIAN_LITTLE,		/* header byte order is little */

  (HAS_RELOC | HAS_SYMS
   | WP_TEXT | D_PAGED),	/* object flags */
  (SEC_ALLOC | SEC_LOAD | SEC_RELOC
   | SEC_READONLY | SEC_CODE | SEC_DATA
   | SEC_HAS_CONTENTS | SEC_IN_MEMORY),		/* sect flags */
  0,				/* symbol_leading_char */
  ' ',				/* ar_pad_char */
  15,				/* ar_max_namelen */
  bfd_getl64, bfd_getl_signed_64, bfd_putl64,
  bfd_getl32, bfd_getl_signed_32, bfd_putl32,
  bfd_getl16, bfd_getl_signed_16, bfd_putl16,
  bfd_getl64, bfd_getl_signed_64, bfd_putl64,
  bfd_getl32, bfd_getl_signed_32, bfd_putl32,
  bfd_getl16, bfd_getl_signed_16, bfd_putl16,

  {_bfd_dummy_target, vms_object_p,		/* bfd_check_format */
   vms_archive_p, _bfd_dummy_target},
  {bfd_false, vms_mkobject,			/* bfd_set_format */
   _bfd_generic_mkarchive, bfd_false},
  {bfd_false, vms_write_object_contents,	/* bfd_write_contents */
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

  (PTR) 0
};

const bfd_target vms_vax_vec =
{
  "vms-vax",			/* name */
  bfd_target_ovax_flavour,
  BFD_ENDIAN_LITTLE,		/* data byte order is little */
  BFD_ENDIAN_LITTLE,		/* header byte order is little */

  (HAS_RELOC | HAS_SYMS 	/* object flags */
   | WP_TEXT | D_PAGED
   | HAS_LINENO | HAS_DEBUG | HAS_LOCALS),

  (SEC_ALLOC | SEC_LOAD | SEC_RELOC
   | SEC_READONLY | SEC_CODE | SEC_DATA
   | SEC_HAS_CONTENTS | SEC_IN_MEMORY),		/* sect flags */
  0,				/* symbol_leading_char */
  ' ',				/* ar_pad_char */
  15,				/* ar_max_namelen */
  bfd_getl64, bfd_getl_signed_64, bfd_putl64,
  bfd_getl32, bfd_getl_signed_32, bfd_putl32,
  bfd_getl16, bfd_getl_signed_16, bfd_putl16, /* data */
  bfd_getl64, bfd_getl_signed_64, bfd_putl64,
  bfd_getl32, bfd_getl_signed_32, bfd_putl32,
  bfd_getl16, bfd_getl_signed_16, bfd_putl16, /* hdrs */

  {_bfd_dummy_target, vms_object_p,		/* bfd_check_format */
   vms_archive_p, _bfd_dummy_target},
  {bfd_false, vms_mkobject,			/* bfd_set_format */
   _bfd_generic_mkarchive, bfd_false},
  {bfd_false, vms_write_object_contents,	/* bfd_write_contents */
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

  (PTR) 0
};

/*===========================================================================*/

/* Initialize private data  */

static bfd_boolean
vms_initialize (abfd)
     bfd *abfd;
{
  int i;
  bfd_size_type amt;

  bfd_set_start_address (abfd, (bfd_vma) -1);

  amt = sizeof (struct vms_private_data_struct);
  abfd->tdata.any = (struct vms_private_data_struct*) bfd_alloc (abfd, amt);
  if (abfd->tdata.any == 0)
    return FALSE;

#ifdef __ALPHA
  PRIV (is_vax) = 0;
#else
  PRIV (is_vax) = 1;
#endif
  PRIV (vms_buf) = 0;
  PRIV (buf_size) = 0;
  PRIV (rec_length) = 0;
  PRIV (file_format) = FF_UNKNOWN;
  PRIV (fixup_done) = FALSE;
  PRIV (sections) = NULL;

  amt = sizeof (struct stack_struct) * STACKSIZE;
  PRIV (stack) = (struct stack_struct *) bfd_alloc (abfd, amt);
  if (PRIV (stack) == 0)
    goto error_ret1;
  PRIV (stackptr) = 0;

  amt = sizeof (struct bfd_hash_table);
  PRIV (vms_symbol_table) = (struct bfd_hash_table *) bfd_alloc (abfd, amt);
  if (PRIV (vms_symbol_table) == 0)
    goto error_ret1;

  if (!bfd_hash_table_init (PRIV (vms_symbol_table), _bfd_vms_hash_newfunc))
    goto error_ret1;

  amt = sizeof (struct location_struct) * LOCATION_SAVE_SIZE;
  PRIV (location_stack) = (struct location_struct *) bfd_alloc (abfd, amt);
  if (PRIV (location_stack) == 0)
    goto error_ret2;

  for (i = 0; i < VMS_SECTION_COUNT; i++)
    PRIV (vms_section_table)[i] = NULL;

  amt = MAX_OUTREC_SIZE;
  PRIV (output_buf) = (unsigned char *) bfd_alloc (abfd, amt);
  if (PRIV (output_buf) == 0)
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
  abfd->tdata.any = 0;
  return FALSE;
}

/* Fill symbol->section with section ptr
   symbol->section is filled with the section index for defined symbols
   during reading the GSD/EGSD section. But we need the pointer to the
   bfd section later.

   It has the correct value for referenced (undefined section) symbols

   called from bfd_hash_traverse in vms_fixup_sections  */

static bfd_boolean
fill_section_ptr (entry, sections)
     struct bfd_hash_entry *entry;
     PTR sections;
{
  asection *sec;
  asymbol *sym;

  sym =  ((vms_symbol_entry *)entry)->symbol;
  sec = sym->section;

#if VMS_DEBUG
  vms_debug (6, "fill_section_ptr: sym %p, sec %p\n", sym, sec);
#endif

  /* fill forward references (these contain section number, not section ptr).  */

  if ((unsigned int) sec < priv_section_count)
    {
      sec = ((vms_symbol_entry *)entry)->symbol->section =
	((asection **)sections)[(int)sec];
    }

  if (strcmp (sym->name, sec->name) == 0)
    sym->flags |= BSF_SECTION_SYM;

  return TRUE;
}

/* Fixup sections
   set up all pointers and arrays, counters and sizes are fixed now

   we build a private sections vector for easy access since sections
   are always referenced by an index number.

   alloc PRIV(sections) according to abfd->section_count
	copy abfd->sections to PRIV(sections)  */

static bfd_boolean
vms_fixup_sections (abfd)
     bfd *abfd;
{
  if (PRIV (fixup_done))
    return TRUE;

  /*
   * traverse symbol table and fill in all section pointers
   */

  /* can't provide section count as argument to fill_section_ptr().  */
  priv_section_count = PRIV (section_count);
  bfd_hash_traverse (PRIV (vms_symbol_table), fill_section_ptr,
		    (PTR) (PRIV (sections)));

  PRIV (fixup_done) = TRUE;

  return TRUE;
}

/*===========================================================================*/

/* Check the format for a file being read.
   Return a (bfd_target *) if it's an object file or zero if not.  */

static const struct bfd_target *
vms_object_p (abfd)
     bfd *abfd;
{
  int err = 0;
  int prev_type;
  const struct bfd_target *target_vector = 0;
  const bfd_arch_info_type *arch = 0;
  PTR tdata_save = abfd->tdata.any;
  bfd_vma saddr_save = bfd_get_start_address (abfd);

#if VMS_DEBUG
  vms_debug (1, "vms_object_p(%p)\n", abfd);
#endif

  if (!vms_initialize (abfd))
    goto error_ret;

  if (bfd_seek (abfd, (file_ptr) 0, SEEK_SET))
    goto err_wrong_format;

  prev_type = -1;

  do
    {
#if VMS_DEBUG
      vms_debug (7, "reading at %08lx\n", bfd_tell(abfd));
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

      if (target_vector == 0)
	{
	  if (prev_type <= OBJ_S_C_MAXRECTYP)
	    target_vector = &vms_vax_vec;
	  else
	    target_vector = &vms_alpha_vec;
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

  if (target_vector == &vms_vax_vec)
    {
      if (! vms_fixup_sections (abfd))
	{
#if VMS_DEBUG
	  vms_debug (2, "vms_fixup_sections failed\n");
#endif
	  goto err_wrong_format;
	}

      /* set arch_info to vax  */

      arch = bfd_scan_arch ("vax");
      PRIV (is_vax) = 1;
#if VMS_DEBUG
      vms_debug (2, "arch is vax\n");
#endif
    }
  else if (target_vector == &vms_alpha_vec)
    {
      /* set arch_info to alpha  */

      arch = bfd_scan_arch ("alpha");
      PRIV (is_vax) = 0;
#if VMS_DEBUG
      vms_debug (2, "arch is alpha\n");
#endif
    }

  if (arch == 0)
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
vms_archive_p (abfd)
     bfd *abfd ATTRIBUTE_UNUSED;
{
#if VMS_DEBUG
  vms_debug (1, "vms_archive_p(%p)\n", abfd);
#endif

  return 0;
}

/* Set the format of a file being written.  */

static bfd_boolean
vms_mkobject (abfd)
     bfd *abfd;
{
#if VMS_DEBUG
  vms_debug (1, "vms_mkobject(%p)\n", abfd);
#endif

  if (!vms_initialize (abfd))
    return 0;

  {
#ifdef __VAX
    const bfd_arch_info_type *arch = bfd_scan_arch ("vax");
#else
    const bfd_arch_info_type *arch = bfd_scan_arch ("alpha");
#endif
    if (arch == 0)
      {
	bfd_set_error(bfd_error_wrong_format);
	return 0;
      }
    abfd->arch_info = arch;
  }

  return TRUE;
}

/* Write cached information into a file being written, at bfd_close.  */

static bfd_boolean
vms_write_object_contents (abfd)
     bfd *abfd;
{
#if VMS_DEBUG
  vms_debug (1, "vms_write_object_contents(%p)\n", abfd);
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

/*-- 4.1, generic -----------------------------------------------------------*/

/* Called when the BFD is being closed to do any necessary cleanup.  */

static bfd_boolean
vms_close_and_cleanup (abfd)
     bfd *abfd;
{
#if VMS_DEBUG
  vms_debug (1, "vms_close_and_cleanup(%p)\n", abfd);
#endif
  if (abfd == 0)
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
vms_bfd_free_cached_info (abfd)
     bfd *abfd ATTRIBUTE_UNUSED;
{
#if VMS_DEBUG
  vms_debug (1, "vms_bfd_free_cached_info(%p)\n", abfd);
#endif
  return TRUE;
}

/* Called when a new section is created.  */

static bfd_boolean
vms_new_section_hook (abfd, section)
     bfd *abfd;
     asection *section;
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
      PRIV (sections) = (asection **) bfd_realloc (PRIV (sections), amt);
      if (PRIV (sections) == 0)
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
vms_get_section_contents (abfd, section, buf, offset, buf_size)
     bfd *abfd ATTRIBUTE_UNUSED;
     asection *section ATTRIBUTE_UNUSED;
     PTR buf ATTRIBUTE_UNUSED;
     file_ptr offset ATTRIBUTE_UNUSED;
     bfd_size_type buf_size ATTRIBUTE_UNUSED;
{
#if VMS_DEBUG
  vms_debug (1, "vms_get_section_contents(%p, %s, %p, off %ld, size %d)\n",
		 abfd, section->name, buf, offset, (int)buf_size);
#endif

  /* shouldn't be called, since all sections are IN_MEMORY  */

  return FALSE;
}

/* Read the contents of a section.
   buf points to a buffer of buf_size bytes to be filled with
   section data (starting at offset into section)  */

static bfd_boolean
vms_get_section_contents_in_window (abfd, section, w, offset, count)
     bfd *abfd ATTRIBUTE_UNUSED;
     asection *section ATTRIBUTE_UNUSED;
     bfd_window *w ATTRIBUTE_UNUSED;
     file_ptr offset ATTRIBUTE_UNUSED;
     bfd_size_type count ATTRIBUTE_UNUSED;
{
#if VMS_DEBUG
  vms_debug (1, "vms_get_section_contents_in_window(%p, %s, %p, off %ld, count %d)\n",
		 abfd, section->name, w, offset, (int)count);
#endif

  /* shouldn't be called, since all sections are IN_MEMORY  */

  return FALSE;
}

/*-- Part 4.2, copy private data --------------------------------------------*/

/* Called to copy BFD general private data from one object file
   to another.  */

static bfd_boolean
vms_bfd_copy_private_bfd_data (src, dest)
     bfd *src ATTRIBUTE_UNUSED;
     bfd *dest ATTRIBUTE_UNUSED;
{
#if VMS_DEBUG
  vms_debug (1, "vms_bfd_copy_private_bfd_data(%p, %p)\n", src, dest);
#endif
  return TRUE;
}

/* Merge private BFD information from the BFD @var{ibfd} to the
   the output file BFD @var{obfd} when linking.  Return <<TRUE>>
   on success, <<FALSE>> on error.  Possible error returns are:

   o <<bfd_error_no_memory>> -
     Not enough memory exists to create private data for @var{obfd}.  */

static bfd_boolean
vms_bfd_merge_private_bfd_data (ibfd, obfd)
     bfd *ibfd ATTRIBUTE_UNUSED;
     bfd *obfd ATTRIBUTE_UNUSED;
{
#if VMS_DEBUG
  vms_debug (1,"vms_bfd_merge_private_bfd_data(%p, %p)\n", ibfd, obfd);
#endif
  return TRUE;
}

/* Set private BFD flag information in the BFD @var{abfd}.
   Return <<TRUE>> on success, <<FALSE>> on error.  Possible error
   returns are:

   o <<bfd_error_no_memory>> -
     Not enough memory exists to create private data for @var{obfd}.  */

static bfd_boolean
vms_bfd_set_private_flags (abfd, flags)
     bfd *abfd ATTRIBUTE_UNUSED;
     flagword flags ATTRIBUTE_UNUSED;
{
#if VMS_DEBUG
  vms_debug (1,"vms_bfd_set_private_flags(%p, %lx)\n", abfd, (long)flags);
#endif
  return TRUE;
}

/* Called to copy BFD private section data from one object file
   to another.  */

static bfd_boolean
vms_bfd_copy_private_section_data (srcbfd, srcsec, dstbfd, dstsec)
     bfd *srcbfd ATTRIBUTE_UNUSED;
     asection *srcsec ATTRIBUTE_UNUSED;
     bfd *dstbfd ATTRIBUTE_UNUSED;
     asection *dstsec ATTRIBUTE_UNUSED;
{
#if VMS_DEBUG
  vms_debug (1, "vms_bfd_copy_private_section_data(%p, %s, %p, %s)\n",
		 srcbfd, srcsec->name, dstbfd, dstsec->name);
#endif
  return TRUE;
}

/* Called to copy BFD private symbol data from one object file
   to another.  */

static bfd_boolean
vms_bfd_copy_private_symbol_data (ibfd, isym, obfd, osym)
     bfd *ibfd ATTRIBUTE_UNUSED;
     asymbol *isym ATTRIBUTE_UNUSED;
     bfd *obfd ATTRIBUTE_UNUSED;
     asymbol *osym ATTRIBUTE_UNUSED;
{
#if VMS_DEBUG
  vms_debug (1, "vms_bfd_copy_private_symbol_data(%p, %s, %p, %s)\n",
		 ibfd, isym->name, obfd, osym->name);
#endif
  return TRUE;
}

/*-- Part 4.3, core file ----------------------------------------------------*/

/* Return a read-only string explaining which program was running
   when it failed and produced the core file abfd.  */

static char *
vms_core_file_failing_command (abfd)
     bfd *abfd ATTRIBUTE_UNUSED;
{
#if VMS_DEBUG
  vms_debug (1, "vms_core_file_failing_command(%p)\n", abfd);
#endif
  return 0;
}

/* Returns the signal number which caused the core dump which
   generated the file the BFD abfd is attached to.  */

static int
vms_core_file_failing_signal (abfd)
     bfd *abfd ATTRIBUTE_UNUSED;
{
#if VMS_DEBUG
  vms_debug (1, "vms_core_file_failing_signal(%p)\n", abfd);
#endif
  return 0;
}

/* Return TRUE if the core file attached to core_bfd was generated
   by a run of the executable file attached to exec_bfd, FALSE otherwise.  */

static bfd_boolean
vms_core_file_matches_executable_p (abfd, bbfd)
     bfd *abfd ATTRIBUTE_UNUSED;
     bfd *bbfd ATTRIBUTE_UNUSED;
{
#if VMS_DEBUG
  vms_debug (1, "vms_core_file_matches_executable_p(%p, %p)\n", abfd, bbfd);
#endif
  return FALSE;
}

/*-- Part 4.4, archive ------------------------------------------------------*/

/* ???	do something with an archive map.
   Return FALSE on error, TRUE otherwise.  */

static bfd_boolean
vms_slurp_armap (abfd)
     bfd *abfd ATTRIBUTE_UNUSED;
{
#if VMS_DEBUG
  vms_debug (1, "vms_slurp_armap(%p)\n", abfd);
#endif
  return FALSE;
}

/* ???	do something with an extended name table.
   Return FALSE on error, TRUE otherwise.  */

static bfd_boolean
vms_slurp_extended_name_table (abfd)
     bfd *abfd ATTRIBUTE_UNUSED;
{
#if VMS_DEBUG
  vms_debug (1, "vms_slurp_extended_name_table(%p)\n", abfd);
#endif
  return FALSE;
}

/* ???	do something with an extended name table.
   Return FALSE on error, TRUE otherwise.  */

static bfd_boolean
vms_construct_extended_name_table (abfd, tabloc, tablen, name)
     bfd *abfd ATTRIBUTE_UNUSED;
     char **tabloc ATTRIBUTE_UNUSED;
     bfd_size_type *tablen ATTRIBUTE_UNUSED;
     const char **name ATTRIBUTE_UNUSED;
{
#if VMS_DEBUG
  vms_debug (1, "vms_construct_extended_name_table(%p)\n", abfd);
#endif
  return FALSE;
}

/* Truncate the name of an archive to match system-dependent restrictions  */

static void
vms_truncate_arname (abfd, pathname, arhdr)
     bfd *abfd ATTRIBUTE_UNUSED;
     const char *pathname ATTRIBUTE_UNUSED;
     char *arhdr ATTRIBUTE_UNUSED;
{
#if VMS_DEBUG
  vms_debug (1, "vms_truncate_arname(%p, %s, %s)\n", abfd, pathname, arhdr);
#endif
  return;
}

/* ???	write archive map  */

static bfd_boolean
vms_write_armap (arch, elength, map, orl_count, stridx)
     bfd *arch ATTRIBUTE_UNUSED;
     unsigned int elength ATTRIBUTE_UNUSED;
     struct orl *map ATTRIBUTE_UNUSED;
     unsigned int orl_count ATTRIBUTE_UNUSED;
     int stridx ATTRIBUTE_UNUSED;
{
#if VMS_DEBUG
  vms_debug (1, "vms_write_armap(%p, %d, %p, %d %d)\n",
	arch, elength, map, orl_count, stridx);
#endif
  return TRUE;
}

/* Read archive header ???  */

static PTR
vms_read_ar_hdr (abfd)
    bfd * abfd ATTRIBUTE_UNUSED;
{
#if VMS_DEBUG
  vms_debug (1, "vms_read_ar_hdr(%p)\n", abfd);
#endif
  return (PTR)0;
}

/* Provided a BFD, @var{archive}, containing an archive and NULL, open
   an input BFD on the first contained element and returns that.
   Subsequent calls should pass the archive and the previous return value
   to return a created BFD to the next contained element.
   NULL is returned when there are no more.  */

static bfd *
vms_openr_next_archived_file (arch, prev)
     bfd *arch ATTRIBUTE_UNUSED;
     bfd *prev ATTRIBUTE_UNUSED;
{
#if VMS_DEBUG
  vms_debug (1, "vms_openr_next_archived_file(%p, %p)\n", arch, prev);
#endif
  return NULL;
}

/* Return the BFD which is referenced by the symbol in ABFD indexed by
   INDEX.  INDEX should have been returned by bfd_get_next_mapent.  */

static bfd *
vms_get_elt_at_index (abfd, index)
     bfd *abfd;
     symindex index;
{
#if VMS_DEBUG
  vms_debug (1, "vms_get_elt_at_index(%p, %p)\n", abfd, index);
#endif
  return _bfd_generic_get_elt_at_index(abfd, index);
}

/* ???
   -> bfd_generic_stat_arch_elt  */

static int
vms_generic_stat_arch_elt (abfd, st)
     bfd *abfd;
     struct stat *st;
{
#if VMS_DEBUG
  vms_debug (1, "vms_generic_stat_arch_elt(%p, %p)\n", abfd, st);
#endif
  return bfd_generic_stat_arch_elt (abfd, st);
}

/* This is a new function in bfd 2.5  */

static bfd_boolean
vms_update_armap_timestamp (abfd)
     bfd *abfd ATTRIBUTE_UNUSED;
{
#if VMS_DEBUG
  vms_debug (1, "vms_update_armap_timestamp(%p)\n", abfd);
#endif
  return TRUE;
}

/*-- Part 4.5, symbols --------------------------------------------------------*/

/* Return the number of bytes required to store a vector of pointers
   to asymbols for all the symbols in the BFD abfd, including a
   terminal NULL pointer. If there are no symbols in the BFD,
   then return 0.  If an error occurs, return -1.  */

static long
vms_get_symtab_upper_bound (abfd)
     bfd *abfd;
{
#if VMS_DEBUG
  vms_debug (1, "vms_get_symtab_upper_bound(%p), %d symbols\n", abfd, PRIV (gsd_sym_count));
#endif
  return (PRIV (gsd_sym_count)+1) * sizeof (asymbol *);
}

/* Copy symbols from hash table to symbol vector

   called from bfd_hash_traverse in vms_canonicalize_symtab
   init counter to 0 if entry == 0  */

static bfd_boolean
copy_symbols (entry, arg)
     struct bfd_hash_entry *entry;
     PTR arg;
{
  bfd *abfd = (bfd *) arg;

  if (entry == NULL)	/* init counter */
    PRIV (symnum) = 0;
  else			/* fill vector, inc counter */
    PRIV (symcache)[PRIV (symnum)++] = ((vms_symbol_entry *)entry)->symbol;

  return TRUE;
}

/* Read the symbols from the BFD abfd, and fills in the vector
   location with pointers to the symbols and a trailing NULL.

   return # of symbols read  */

static long
vms_canonicalize_symtab (abfd, symbols)
     bfd *abfd;
     asymbol **symbols;
{
#if VMS_DEBUG
  vms_debug (1, "vms_canonicalize_symtab(%p, <ret>)\n", abfd);
#endif

	/* init counter */
  (void)copy_symbols((struct bfd_hash_entry *)0, abfd);

	/* traverse table and fill symbols vector */

  PRIV (symcache) = symbols;
  bfd_hash_traverse(PRIV (vms_symbol_table), copy_symbols, (PTR)abfd);

  symbols[PRIV (gsd_sym_count)] = NULL;

  return PRIV (gsd_sym_count);
}

/* Print symbol to file according to how. how is one of
   bfd_print_symbol_name	just print the name
   bfd_print_symbol_more	print more (???)
   bfd_print_symbol_all	print all we know, which is not much right now :-)  */

static void
vms_print_symbol (abfd, file, symbol, how)
     bfd *abfd;
     PTR file;
     asymbol *symbol;
     bfd_print_symbol_type how;
{
#if VMS_DEBUG
  vms_debug (1, "vms_print_symbol(%p, %p, %p, %d)\n", abfd, file, symbol, how);
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

	  bfd_print_symbol_vandf (abfd, (PTR)file, symbol);

	  fprintf ((FILE *)file," %-8s %s", section_name, symbol->name);
        }
      break;
    }
  return;
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
	-	debug  */

static void
vms_get_symbol_info (abfd, symbol, ret)
     bfd *abfd ATTRIBUTE_UNUSED;
     asymbol *symbol;
     symbol_info *ret;
{
  asection *sec;

#if VMS_DEBUG
  vms_debug (1, "vms_get_symbol_info(%p, %p, %p)\n", abfd, symbol, ret);
#endif

  sec = symbol->section;

  if (ret == 0)
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

  return;
}

/* Return TRUE if the given symbol sym in the BFD abfd is
   a compiler generated local label, else return FALSE.  */

static bfd_boolean
vms_bfd_is_local_label_name (abfd, name)
     bfd *abfd ATTRIBUTE_UNUSED;
     const char *name;
{
#if VMS_DEBUG
  vms_debug (1, "vms_bfd_is_local_label_name(%p, %s)\n", abfd, name);
#endif
  return name[0] == '$';
}

/* Get source line number for symbol  */

static alent *
vms_get_lineno (abfd, symbol)
     bfd *abfd ATTRIBUTE_UNUSED;
     asymbol *symbol ATTRIBUTE_UNUSED;
{
#if VMS_DEBUG
  vms_debug (1, "vms_get_lineno(%p, %p)\n", abfd, symbol);
#endif
  return 0;
}

/* Provided a BFD, a section and an offset into the section, calculate and
   return the name of the source file and the line nearest to the wanted
   location.  */

static bfd_boolean
vms_find_nearest_line (abfd, section, symbols, offset, file, func, line)
     bfd *abfd ATTRIBUTE_UNUSED;
     asection *section ATTRIBUTE_UNUSED;
     asymbol **symbols ATTRIBUTE_UNUSED;
     bfd_vma offset ATTRIBUTE_UNUSED;
     const char **file ATTRIBUTE_UNUSED;
     const char **func ATTRIBUTE_UNUSED;
     unsigned int *line ATTRIBUTE_UNUSED;
{
#if VMS_DEBUG
  vms_debug (1, "vms_find_nearest_line(%p, %s, %p, %ld, <ret>, <ret>, <ret>)\n",
	      abfd, section->name, symbols, (long int)offset);
#endif
  return FALSE;
}

/* Back-door to allow format-aware applications to create debug symbols
   while using BFD for everything else.  Currently used by the assembler
   when creating COFF files.  */

static asymbol *
vms_bfd_make_debug_symbol (abfd, ptr, size)
     bfd *abfd ATTRIBUTE_UNUSED;
     void *ptr ATTRIBUTE_UNUSED;
     unsigned long size ATTRIBUTE_UNUSED;
{
#if VMS_DEBUG
  vms_debug (1, "vms_bfd_make_debug_symbol(%p, %p, %ld)\n", abfd, ptr, size);
#endif
  return 0;
}

/* Read minisymbols.  For minisymbols, we use the unmodified a.out
   symbols.  The minisymbol_to_symbol function translates these into
   BFD asymbol structures.  */

static long
vms_read_minisymbols (abfd, dynamic, minisymsp, sizep)
     bfd *abfd;
     bfd_boolean dynamic;
     PTR *minisymsp;
     unsigned int *sizep;
{
#if VMS_DEBUG
  vms_debug (1, "vms_read_minisymbols(%p, %d, %p, %d)\n", abfd, dynamic, minisymsp, *sizep);
#endif
  return _bfd_generic_read_minisymbols (abfd, dynamic, minisymsp, sizep);
}

/* Convert a minisymbol to a BFD asymbol.  A minisymbol is just an
   unmodified a.out symbol.  The SYM argument is a structure returned
   by bfd_make_empty_symbol, which we fill in here.  */

static asymbol *
vms_minisymbol_to_symbol (abfd, dynamic, minisym, sym)
     bfd *abfd;
     bfd_boolean dynamic;
     const PTR minisym;
     asymbol *sym;
{
#if VMS_DEBUG
  vms_debug (1, "vms_minisymbol_to_symbol(%p, %d, %p, %p)\n", abfd, dynamic, minisym, sym);
#endif
  return _bfd_generic_minisymbol_to_symbol (abfd, dynamic, minisym, sym);
}

/*-- Part 4.6, relocations --------------------------------------------------*/

/* Return the number of bytes required to store the relocation information
   associated with section sect attached to bfd abfd.
   If an error occurs, return -1.  */

static long
vms_get_reloc_upper_bound (abfd, section)
     bfd *abfd ATTRIBUTE_UNUSED;
     asection *section ATTRIBUTE_UNUSED;
{
#if VMS_DEBUG
  vms_debug (1, "vms_get_reloc_upper_bound(%p, %s)\n", abfd, section->name);
#endif
  return -1L;
}

/* Call the back end associated with the open BFD abfd and translate the
   external form of the relocation information attached to sec into the
   internal canonical form.  Place the table into memory at loc, which has
   been preallocated, usually by a call to bfd_get_reloc_upper_bound.
   Returns the number of relocs, or -1 on error.  */

static long
vms_canonicalize_reloc (abfd, section, location, symbols)
     bfd *abfd ATTRIBUTE_UNUSED;
     asection *section ATTRIBUTE_UNUSED;
     arelent **location ATTRIBUTE_UNUSED;
     asymbol **symbols ATTRIBUTE_UNUSED;
{
#if VMS_DEBUG
  vms_debug (1, "vms_canonicalize_reloc(%p, %s, <ret>, <ret>)\n", abfd, section->name);
#endif
  return FALSE;
}

/*---------------------------------------------------------------------------*/
/* this is just copied from ecoff-alpha, needs to be fixed probably */

/* How to process the various reloc types.  */

static bfd_reloc_status_type
reloc_nil (abfd, reloc, sym, data, sec, output_bfd, error_message)
     bfd *abfd ATTRIBUTE_UNUSED;
     arelent *reloc ATTRIBUTE_UNUSED;
     asymbol *sym ATTRIBUTE_UNUSED;
     PTR data ATTRIBUTE_UNUSED;
     asection *sec ATTRIBUTE_UNUSED;
     bfd *output_bfd ATTRIBUTE_UNUSED;
     char **error_message ATTRIBUTE_UNUSED;
{
#if VMS_DEBUG
  vms_debug (1, "reloc_nil(abfd %p, output_bfd %p)\n", abfd, output_bfd);
  vms_debug (2, "In section %s, symbol %s\n",
	sec->name, sym->name);
  vms_debug (2, "reloc sym %s, addr %08lx, addend %08lx, reloc is a %s\n",
		reloc->sym_ptr_ptr[0]->name,
		(unsigned long)reloc->address,
		(unsigned long)reloc->addend, reloc->howto->name);
  vms_debug (2, "data at %p\n", data);
/*  _bfd_hexdump (2, data, bfd_get_reloc_size(reloc->howto),0); */
#endif

  return bfd_reloc_ok;
}

/* In case we're on a 32-bit machine, construct a 64-bit "-1" value
   from smaller values.  Start with zero, widen, *then* decrement.  */
#define MINUS_ONE	(((bfd_vma)0) - 1)

static reloc_howto_type alpha_howto_table[] =
{
  HOWTO (ALPHA_R_IGNORE,	/* type */
	 0,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 8,			/* bitsize */
	 TRUE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 reloc_nil,		/* special_function */
	 "IGNORE",		/* name */
	 TRUE,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 TRUE),			/* pcrel_offset */

  /* A 64 bit reference to a symbol.  */
  HOWTO (ALPHA_R_REFQUAD,	/* type */
	 0,			/* rightshift */
	 4,			/* size (0 = byte, 1 = short, 2 = long) */
	 64,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 reloc_nil,		/* special_function */
	 "REFQUAD",		/* name */
	 TRUE,			/* partial_inplace */
	 MINUS_ONE,		/* src_mask */
	 MINUS_ONE,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* A 21 bit branch.  The native assembler generates these for
     branches within the text segment, and also fills in the PC
     relative offset in the instruction.  */
  HOWTO (ALPHA_R_BRADDR,	/* type */
	 2,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 21,			/* bitsize */
	 TRUE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 reloc_nil,		/* special_function */
	 "BRADDR",		/* name */
	 TRUE,			/* partial_inplace */
	 0x1fffff,		/* src_mask */
	 0x1fffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* A hint for a jump to a register.  */
  HOWTO (ALPHA_R_HINT,		/* type */
	 2,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 14,			/* bitsize */
	 TRUE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 reloc_nil,		/* special_function */
	 "HINT",		/* name */
	 TRUE,			/* partial_inplace */
	 0x3fff,		/* src_mask */
	 0x3fff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* 16 bit PC relative offset.  */
  HOWTO (ALPHA_R_SREL16,	/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 TRUE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 reloc_nil,		/* special_function */
	 "SREL16",		/* name */
	 TRUE,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* 32 bit PC relative offset.  */
  HOWTO (ALPHA_R_SREL32,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 TRUE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 reloc_nil,		/* special_function */
	 "SREL32",		/* name */
	 TRUE,			/* partial_inplace */
	 0xffffffff,		/* src_mask */
	 0xffffffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* A 64 bit PC relative offset.  */
  HOWTO (ALPHA_R_SREL64,	/* type */
	 0,			/* rightshift */
	 4,			/* size (0 = byte, 1 = short, 2 = long) */
	 64,			/* bitsize */
	 TRUE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 reloc_nil,		/* special_function */
	 "SREL64",		/* name */
	 TRUE,			/* partial_inplace */
	 MINUS_ONE,		/* src_mask */
	 MINUS_ONE,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* Push a value on the reloc evaluation stack.  */
  HOWTO (ALPHA_R_OP_PUSH,	/* type */
	 0,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 0,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 reloc_nil,		/* special_function */
	 "OP_PUSH",		/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* Store the value from the stack at the given address.  Store it in
     a bitfield of size r_size starting at bit position r_offset.  */
  HOWTO (ALPHA_R_OP_STORE,	/* type */
	 0,			/* rightshift */
	 4,			/* size (0 = byte, 1 = short, 2 = long) */
	 64,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 reloc_nil,		/* special_function */
	 "OP_STORE",		/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 MINUS_ONE,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* Subtract the reloc address from the value on the top of the
     relocation stack.  */
  HOWTO (ALPHA_R_OP_PSUB,	/* type */
	 0,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 0,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 reloc_nil,		/* special_function */
	 "OP_PSUB",		/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* Shift the value on the top of the relocation stack right by the
     given value.  */
  HOWTO (ALPHA_R_OP_PRSHIFT,	/* type */
	 0,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 0,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 reloc_nil,		/* special_function */
	 "OP_PRSHIFT",		/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* Hack. Linkage is done by linker.  */
  HOWTO (ALPHA_R_LINKAGE,	/* type */
	 0,			/* rightshift */
	 8,			/* size (0 = byte, 1 = short, 2 = long) */
	 256,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 reloc_nil,		/* special_function */
	 "LINKAGE",		/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* A 32 bit reference to a symbol.  */
  HOWTO (ALPHA_R_REFLONG,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 reloc_nil,		/* special_function */
	 "REFLONG",		/* name */
	 TRUE,			/* partial_inplace */
	 0xffffffff,		/* src_mask */
	 0xffffffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* A 64 bit reference to a procedure, written as 32 bit value.  */
  HOWTO (ALPHA_R_CODEADDR,	/* type */
	 0,			/* rightshift */
	 4,			/* size (0 = byte, 1 = short, 2 = long) */
	 64,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed,/* complain_on_overflow */
	 reloc_nil,		/* special_function */
	 "CODEADDR",		/* name */
	 FALSE,			/* partial_inplace */
	 0xffffffff,		/* src_mask */
	 0xffffffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

};

/* Return a pointer to a howto structure which, when invoked, will perform
   the relocation code on data from the architecture noted.  */

static const struct reloc_howto_struct *
vms_bfd_reloc_type_lookup (abfd, code)
     bfd *abfd ATTRIBUTE_UNUSED;
     bfd_reloc_code_real_type code;
{
  int alpha_type;

#if VMS_DEBUG
  vms_debug (1, "vms_bfd_reloc_type_lookup(%p, %d)\t", abfd, code);
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
	return (const struct reloc_howto_struct *) NULL;
    }
#if VMS_DEBUG
  vms_debug (2, "reloc is %s\n", alpha_howto_table[alpha_type].name);
#endif
  return &alpha_howto_table[alpha_type];
}

/*-- Part 4.7, writing an object file ---------------------------------------*/

/* Set the architecture and machine type in BFD abfd to arch and mach.
   Find the correct pointer to a structure and insert it into the arch_info
   pointer.  */

static bfd_boolean
vms_set_arch_mach (abfd, arch, mach)
     bfd *abfd;
     enum bfd_architecture arch ATTRIBUTE_UNUSED;
     unsigned long mach ATTRIBUTE_UNUSED;
{
#if VMS_DEBUG
  vms_debug (1, "vms_set_arch_mach(%p, %d, %ld)\n", abfd, arch, mach);
#endif
  abfd->arch_info = bfd_scan_arch("alpha");

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
vms_set_section_contents (abfd, section, location, offset, count)
     bfd *abfd;
     asection *section;
     const PTR location;
     file_ptr offset;
     bfd_size_type count;
{
#if VMS_DEBUG
  vms_debug (1, "vms_set_section_contents(%p, sec %s, loc %p, off %ld, count %d)\n",
					abfd, section->name, location, (long int)offset, (int)count);
  vms_debug (2, "secraw %d, seccooked %d\n", (int)section->_raw_size, (int)section->_cooked_size);
#endif
  return _bfd_save_vms_section(abfd, section, location, offset, count);
}

/*-- Part 4.8, linker -------------------------------------------------------*/

/* Get the size of the section headers.  */

static int
vms_sizeof_headers (abfd, reloc)
     bfd *abfd ATTRIBUTE_UNUSED;
     bfd_boolean reloc ATTRIBUTE_UNUSED;
{
#if VMS_DEBUG
  vms_debug (1, "vms_sizeof_headers(%p, %s)\n", abfd, (reloc)?"True":"False");
#endif
  return 0;
}

/* Provides default handling of relocation effort for back ends
   which can't be bothered to do it efficiently.  */

static bfd_byte *
vms_bfd_get_relocated_section_contents (abfd, link_info, link_order, data,
					 relocatable, symbols)
     bfd *abfd ATTRIBUTE_UNUSED;
     struct bfd_link_info *link_info ATTRIBUTE_UNUSED;
     struct bfd_link_order *link_order ATTRIBUTE_UNUSED;
     bfd_byte *data ATTRIBUTE_UNUSED;
     bfd_boolean relocatable ATTRIBUTE_UNUSED;
     asymbol **symbols ATTRIBUTE_UNUSED;
{
#if VMS_DEBUG
  vms_debug (1, "vms_bfd_get_relocated_section_contents(%p, %p, %p, %p, %s, %p)\n",
			abfd, link_info, link_order, data, (relocatable)?"True":"False", symbols);
#endif
  return 0;
}

/* ???  */

static bfd_boolean
vms_bfd_relax_section (abfd, section, link_info, again)
     bfd *abfd ATTRIBUTE_UNUSED;
     asection *section ATTRIBUTE_UNUSED;
     struct bfd_link_info *link_info ATTRIBUTE_UNUSED;
     bfd_boolean *again ATTRIBUTE_UNUSED;
{
#if VMS_DEBUG
  vms_debug (1, "vms_bfd_relax_section(%p, %s, %p, <ret>)\n",
					abfd, section->name, link_info);
#endif
  return TRUE;
}

static bfd_boolean
vms_bfd_gc_sections (abfd, link_info)
     bfd *abfd ATTRIBUTE_UNUSED;
     struct bfd_link_info *link_info ATTRIBUTE_UNUSED;
{
#if VMS_DEBUG
  vms_debug (1, "vms_bfd_gc_sections(%p, %p)\n", abfd, link_info);
#endif
  return TRUE;
}

static bfd_boolean
vms_bfd_merge_sections (abfd, link_info)
     bfd *abfd ATTRIBUTE_UNUSED;
     struct bfd_link_info *link_info ATTRIBUTE_UNUSED;
{
#if VMS_DEBUG
  vms_debug (1, "vms_bfd_merge_sections(%p, %p)\n", abfd, link_info);
#endif
  return TRUE;
}

/* Create a hash table for the linker.  Different backends store
   different information in this table.  */

static struct bfd_link_hash_table *
vms_bfd_link_hash_table_create (abfd)
     bfd *abfd ATTRIBUTE_UNUSED;
{
#if VMS_DEBUG
  vms_debug (1, "vms_bfd_link_hash_table_create(%p)\n", abfd);
#endif
  return 0;
}

/* Free a linker hash table.  */

static void
vms_bfd_link_hash_table_free (hash)
     struct bfd_link_hash_table *hash ATTRIBUTE_UNUSED;
{
#if VMS_DEBUG
  vms_debug (1, "vms_bfd_link_hash_table_free(%p)\n", abfd);
#endif
}

/* Add symbols from this object file into the hash table.  */

static bfd_boolean
vms_bfd_link_add_symbols (abfd, link_info)
     bfd *abfd ATTRIBUTE_UNUSED;
     struct bfd_link_info *link_info ATTRIBUTE_UNUSED;
{
#if VMS_DEBUG
  vms_debug (1, "vms_bfd_link_add_symbols(%p, %p)\n", abfd, link_info);
#endif
  return FALSE;
}

/* Do a link based on the link_order structures attached to each
   section of the BFD.  */

static bfd_boolean
vms_bfd_final_link (abfd, link_info)
     bfd *abfd ATTRIBUTE_UNUSED;
     struct bfd_link_info *link_info ATTRIBUTE_UNUSED;
{
#if VMS_DEBUG
  vms_debug (1, "vms_bfd_final_link(%p, %p)\n", abfd, link_info);
#endif
  return TRUE;
}

/* Should this section be split up into smaller pieces during linking.  */

static bfd_boolean
vms_bfd_link_split_section (abfd, section)
     bfd *abfd ATTRIBUTE_UNUSED;
     asection *section ATTRIBUTE_UNUSED;
{
#if VMS_DEBUG
  vms_debug (1, "vms_bfd_link_split_section(%p, %s)\n", abfd, section->name);
#endif
  return FALSE;
}

/*-- Part 4.9, dynamic symbols and relocations ------------------------------*/

/* Get the amount of memory required to hold the dynamic symbols.  */

static long
vms_get_dynamic_symtab_upper_bound (abfd)
     bfd *abfd ATTRIBUTE_UNUSED;
{
#if VMS_DEBUG
  vms_debug (1, "vms_get_dynamic_symtab_upper_bound(%p)\n", abfd);
#endif
  return 0;
}

static bfd_boolean
vms_bfd_print_private_bfd_data (abfd, file)
    bfd *abfd ATTRIBUTE_UNUSED;
    void *file ATTRIBUTE_UNUSED;
{
#if VMS_DEBUG
  vms_debug (1, "vms_bfd_print_private_bfd_data(%p)\n", abfd);
#endif
  return 0;
}

/* Read in the dynamic symbols.  */

static long
vms_canonicalize_dynamic_symtab (abfd, symbols)
     bfd *abfd ATTRIBUTE_UNUSED;
     asymbol **symbols ATTRIBUTE_UNUSED;
{
#if VMS_DEBUG
  vms_debug (1, "vms_canonicalize_dynamic_symtab(%p, <ret>)\n", abfd);
#endif
  return 0L;
}

/* Get the amount of memory required to hold the dynamic relocs.  */

static long
vms_get_dynamic_reloc_upper_bound (abfd)
     bfd *abfd ATTRIBUTE_UNUSED;
{
#if VMS_DEBUG
  vms_debug (1, "vms_get_dynamic_reloc_upper_bound(%p)\n", abfd);
#endif
  return 0L;
}

/* Read in the dynamic relocs.  */

static long
vms_canonicalize_dynamic_reloc (abfd, arel, symbols)
     bfd *abfd ATTRIBUTE_UNUSED;
     arelent **arel ATTRIBUTE_UNUSED;
     asymbol **symbols ATTRIBUTE_UNUSED;
{
#if VMS_DEBUG
  vms_debug (1, "vms_canonicalize_dynamic_reloc(%p)\n", abfd);
#endif
  return 0L;
}
