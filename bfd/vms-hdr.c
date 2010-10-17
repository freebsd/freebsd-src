/* vms-hdr.c -- BFD back-end for VMS/VAX (openVMS/VAX) and
   EVAX (openVMS/Alpha) files.
   Copyright 1996, 1997, 1998, 1999, 2000, 2001, 2002
   Free Software Foundation, Inc.

   HDR record handling functions
   EMH record handling functions
   and
   EOM record handling functions
   EEOM record handling functions

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
#include "bfdver.h"
#include "sysdep.h"
#include "bfdlink.h"
#include "safe-ctype.h"
#include "libbfd.h"

#include "vms.h"

#ifdef HAVE_ALLOCA_H
#include <alloca.h>
#endif

static unsigned char *get_vms_time_string PARAMS ((void));


/*---------------------------------------------------------------------------*/

/* Read & process emh record
   return 0 on success, -1 on error  */

int
_bfd_vms_slurp_hdr (abfd, objtype)
     bfd *abfd;
     int objtype;
{
  unsigned char *ptr;
  unsigned char *vms_rec;
  int subtype;

  vms_rec = PRIV(vms_rec);

#if VMS_DEBUG
  vms_debug(2, "HDR/EMH\n");
#endif

  switch (objtype)
    {
      case OBJ_S_C_HDR:
	subtype = vms_rec[1];
	break;
      case EOBJ_S_C_EMH:
	subtype = bfd_getl16 (vms_rec + 4) + EVAX_OFFSET;
	break;
      default:
	subtype = -1;
    }

#if VMS_DEBUG
  vms_debug(3, "subtype %d\n", subtype);
#endif

  switch (subtype)
    {

      case MHD_S_C_MHD:
	/*
	 * module header
	 */
	PRIV(hdr_data).hdr_b_strlvl = vms_rec[2];
	PRIV(hdr_data).hdr_l_recsiz = bfd_getl16 (vms_rec + 3);
	PRIV(hdr_data).hdr_t_name = _bfd_vms_save_counted_string (vms_rec + 5);
	ptr = vms_rec + 5 + vms_rec[5] + 1;
	PRIV(hdr_data).hdr_t_version = _bfd_vms_save_counted_string (ptr);
	ptr += *ptr + 1;
	PRIV(hdr_data).hdr_t_date = _bfd_vms_save_sized_string (ptr, 17);

      break;

      case MHD_S_C_LNM:
	/*
	 *
	 */
	PRIV(hdr_data).hdr_c_lnm = _bfd_vms_save_sized_string (vms_rec, PRIV(rec_length-2));
      break;

      case MHD_S_C_SRC:
	/*
	 *
	 */
	PRIV(hdr_data).hdr_c_src = _bfd_vms_save_sized_string (vms_rec, PRIV(rec_length-2));
      break;

      case MHD_S_C_TTL:
	/*
	 *
	 */
	PRIV(hdr_data).hdr_c_ttl = _bfd_vms_save_sized_string (vms_rec, PRIV(rec_length-2));
      break;

      case MHD_S_C_CPR:
	/*
	 *
	 */
      break;

      case MHD_S_C_MTC:
	/*
	 *
	 */
      break;

      case MHD_S_C_GTX:
	/*
	 *
	 */
      break;

      case EMH_S_C_MHD + EVAX_OFFSET:
	/*
	 * module header
	 */
	PRIV(hdr_data).hdr_b_strlvl = vms_rec[6];
	PRIV(hdr_data).hdr_l_arch1 = bfd_getl32 (vms_rec + 8);
	PRIV(hdr_data).hdr_l_arch2 = bfd_getl32 (vms_rec + 12);
	PRIV(hdr_data).hdr_l_recsiz = bfd_getl32 (vms_rec + 16);
	PRIV(hdr_data).hdr_t_name =
	  _bfd_vms_save_counted_string (vms_rec + 20);
	ptr = vms_rec + 20 + vms_rec[20] + 1;
	PRIV(hdr_data).hdr_t_version =
	  _bfd_vms_save_counted_string (ptr);
	ptr += *ptr + 1;
	PRIV(hdr_data).hdr_t_date =
	  _bfd_vms_save_sized_string (ptr, 17);

      break;

      case EMH_S_C_LNM + EVAX_OFFSET:
	/*
	 *
	 */
	PRIV(hdr_data).hdr_c_lnm =
	  _bfd_vms_save_sized_string (vms_rec, PRIV(rec_length-6));
      break;

      case EMH_S_C_SRC + EVAX_OFFSET:
	/*
	 *
	 */
	PRIV(hdr_data).hdr_c_src =
	  _bfd_vms_save_sized_string (vms_rec, PRIV(rec_length-6));
      break;

      case EMH_S_C_TTL + EVAX_OFFSET:
	/*
	 *
	 */
	PRIV(hdr_data).hdr_c_ttl =
	  _bfd_vms_save_sized_string (vms_rec, PRIV(rec_length-6));
      break;

      case EMH_S_C_CPR + EVAX_OFFSET:
	/*
	 *
	 */
      break;

      case EMH_S_C_MTC + EVAX_OFFSET:
	/*
	 *
	 */
      break;

      case EMH_S_C_GTX + EVAX_OFFSET:
	/*
	 *
	 */
      break;

      default:
	bfd_set_error (bfd_error_wrong_format);
      return -1;

    } /* switch */

  return 0;
}

/*-----------------------------------------------------------------------------*/
/* Output routines.  */

/* Manufacture a VMS like time on a unix based system.
   stolen from obj-vms.c  */

static unsigned char *
get_vms_time_string ()
{
  static unsigned char tbuf[18];
#ifndef VMS
#include <time.h>

  char *pnt;
  time_t timeb;
  time (&timeb);
  pnt = ctime (&timeb);
  pnt[3] = 0;
  pnt[7] = 0;
  pnt[10] = 0;
  pnt[16] = 0;
  pnt[24] = 0;
  sprintf (tbuf, "%2s-%3s-%s %s", pnt + 8, pnt + 4, pnt + 20, pnt + 11);
#else
#include <starlet.h>
  struct
  {
    int Size;
    unsigned char *Ptr;
  } Descriptor;
  Descriptor.Size = 17;
  Descriptor.Ptr = tbuf;
  SYS$ASCTIM (0, &Descriptor, 0, 0);
#endif /* not VMS */

#if VMS_DEBUG
  vms_debug (6, "vmstimestring:'%s'\n", tbuf);
#endif

  return tbuf;
}

/* write object header for bfd abfd  */

int
_bfd_vms_write_hdr (abfd, objtype)
     bfd *abfd;
     int objtype;
{
  asymbol *symbol;
  unsigned int symnum;
  int had_case = 0;
  int had_file = 0;

#if VMS_DEBUG
  vms_debug (2, "vms_write_hdr (%p)\n", abfd);
#endif

  _bfd_vms_output_alignment (abfd, 2);

  /* MHD */

  if (objtype == OBJ_S_C_HDR)
    {
    }
  else
    {
      _bfd_vms_output_begin (abfd, EOBJ_S_C_EMH, EMH_S_C_MHD);
      _bfd_vms_output_short (abfd, EOBJ_S_C_STRLVL);
      _bfd_vms_output_long (abfd, 0);
      _bfd_vms_output_long (abfd, 0);
      _bfd_vms_output_long (abfd, MAX_OUTREC_SIZE);
    }

  if (bfd_get_filename (abfd) != 0)
    {
      /* strip path and suffix information */

      char *fname, *fout, *fptr;

      fptr = bfd_get_filename (abfd);
      fname = (char *) alloca (strlen (fptr) + 1);
      strcpy (fname, fptr);
      fout = strrchr (fname, ']');
      if (fout == 0)
	fout = strchr (fname, ':');
      if (fout != 0)
	fout++;
      else
	fout = fname;

      /* strip .obj suffix  */

      fptr = strrchr (fname, '.');
      if ((fptr != 0)
	  && (strcasecmp (fptr, ".OBJ") == 0))
	*fptr = 0;

      fptr = fout;
      while (*fptr != 0)
	{
	  *fptr = TOUPPER (*fptr);
	  fptr++;
	  if ((*fptr == ';')
	     || ((fptr - fout) > 31))
	    *fptr = 0;
	}
      _bfd_vms_output_counted (abfd, fout);
    }
  else
    _bfd_vms_output_counted (abfd, "NONAME");

  _bfd_vms_output_counted (abfd, BFD_VERSION_STRING);
  _bfd_vms_output_dump (abfd, get_vms_time_string (), 17);
  _bfd_vms_output_fill (abfd, 0, 17);
  _bfd_vms_output_flush (abfd);

  /* LMN */

  _bfd_vms_output_begin (abfd, EOBJ_S_C_EMH, EMH_S_C_LNM);
  _bfd_vms_output_dump (abfd, (unsigned char *)"GAS proGIS", 10);
  _bfd_vms_output_flush (abfd);

  /* SRC */

  _bfd_vms_output_begin (abfd, EOBJ_S_C_EMH, EMH_S_C_SRC);

  for (symnum = 0; symnum < abfd->symcount; symnum++)
    {
      symbol = abfd->outsymbols[symnum];

      if (symbol->flags & BSF_FILE)
	{
	  if (strncmp ((char *)symbol->name, "<CASE:", 6) == 0)
	    {
	      PRIV(flag_hash_long_names) = symbol->name[6] - '0';
	      PRIV(flag_show_after_trunc) = symbol->name[7] - '0';

	      if (had_file)
		break;
	      had_case = 1;
	      continue;
	    }

	  _bfd_vms_output_dump (abfd, (unsigned char *) symbol->name,
				(int) strlen (symbol->name));
	  if (had_case)
	    break;
	  had_file = 1;
	}
    }

  if (symnum == abfd->symcount)
    _bfd_vms_output_dump (abfd, (unsigned char *)"noname", 6);

  _bfd_vms_output_flush (abfd);

  /* TTL */

  _bfd_vms_output_begin (abfd, EOBJ_S_C_EMH, EMH_S_C_TTL);
  _bfd_vms_output_dump (abfd, (unsigned char *)"TTL", 3);
  _bfd_vms_output_flush (abfd);

  /* CPR */

  _bfd_vms_output_begin (abfd, EOBJ_S_C_EMH, EMH_S_C_CPR);
  _bfd_vms_output_dump (abfd,
			 (unsigned char *)"GNU BFD ported by Klaus Kämpf 1994-1996",
			 39);
  _bfd_vms_output_flush (abfd);

  return 0;
}

/*-----------------------------------------------------------------------------*/

/* Process EOM/EEOM record
   return 0 on success, -1 on error  */

int
_bfd_vms_slurp_eom (abfd, objtype)
     bfd *abfd;
     int objtype;
{
  unsigned char *vms_rec;

#if VMS_DEBUG
  vms_debug(2, "EOM/EEOM\n");
#endif

  vms_rec = PRIV(vms_rec);

  if ((objtype == OBJ_S_C_EOM)
     || (objtype == OBJ_S_C_EOMW))
    {
    }
  else
    {
      PRIV(eom_data).eom_l_total_lps = bfd_getl32 (vms_rec + 4);
      PRIV(eom_data).eom_b_comcod = *(vms_rec + 8);
      if (PRIV(eom_data).eom_b_comcod > 1)
	{
	  (*_bfd_error_handler) (_("Object module NOT error-free !\n"));
	  bfd_set_error (bfd_error_bad_value);
	  return -1;
	}
      PRIV(eom_data).eom_has_transfer = FALSE;
      if (PRIV(rec_size) > 10)
	{
	   PRIV(eom_data).eom_has_transfer = TRUE;
	   PRIV(eom_data).eom_b_tfrflg = *(vms_rec + 9);
	   PRIV(eom_data).eom_l_psindx = bfd_getl32 (vms_rec + 12);
	   PRIV(eom_data).eom_l_tfradr = bfd_getl32 (vms_rec + 16);

	   abfd->start_address = PRIV(eom_data).eom_l_tfradr;
	}
    }
  return 0;
}

/* Write eom record for bfd abfd  */

int
_bfd_vms_write_eom (abfd, objtype)
     bfd *abfd;
     int objtype;
{
#if VMS_DEBUG
  vms_debug (2, "vms_write_eom (%p, %d)\n", abfd, objtype);
#endif

  _bfd_vms_output_begin (abfd, objtype, -1);
  _bfd_vms_output_long (abfd, (unsigned long) (PRIV(vms_linkage_index) >> 1));
  _bfd_vms_output_byte (abfd, 0);	/* completion code */
  _bfd_vms_output_byte (abfd, 0);	/* fill byte */

  if (bfd_get_start_address (abfd) != (bfd_vma)-1)
    {
      asection *section;

      section = bfd_get_section_by_name (abfd, ".link");
      if (section == 0)
	{
	  bfd_set_error (bfd_error_nonrepresentable_section);
	  return -1;
	}
      _bfd_vms_output_short (abfd, 0);
      _bfd_vms_output_long (abfd, (unsigned long) (section->index));
      _bfd_vms_output_long (abfd,
			     (unsigned long) bfd_get_start_address (abfd));
      _bfd_vms_output_long (abfd, 0);
    }

  _bfd_vms_output_end (abfd);
  return 0;
}
