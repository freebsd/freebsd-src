/* Instruction opcode table for frv.

THIS FILE IS MACHINE GENERATED WITH CGEN.

Copyright 1996-2005 Free Software Foundation, Inc.

This file is part of the GNU Binutils and/or GDB, the GNU debugger.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street - Fifth Floor, Boston, MA 02110-1301, USA.

*/

#include "sysdep.h"
#include "ansidecl.h"
#include "bfd.h"
#include "symcat.h"
#include "frv-desc.h"
#include "frv-opc.h"
#include "libiberty.h"

/* -- opc.c */
#include "elf/frv.h"
#include <stdio.h>

/* Returns TRUE if {MAJOR,MACH} is a major branch of the FRV
   development tree.  */

bfd_boolean
frv_is_branch_major (CGEN_ATTR_VALUE_ENUM_TYPE major, unsigned long mach)
{
  switch (mach)
    {
    case bfd_mach_fr400:
      if (major >= FR400_MAJOR_B_1 && major <= FR400_MAJOR_B_6)
	return TRUE;
      break;
    case bfd_mach_fr450:
      if (major >= FR450_MAJOR_B_1 && major <= FR450_MAJOR_B_6)
	return TRUE;
      break;
    default:
      if (major >= FR500_MAJOR_B_1 && major <= FR500_MAJOR_B_6)
	return TRUE;
      break;
    }

  return FALSE;
}

/* Returns TRUE if {MAJOR,MACH} supports floating point insns.  */

bfd_boolean
frv_is_float_major (CGEN_ATTR_VALUE_ENUM_TYPE major, unsigned long mach)
{
  switch (mach)
    {
    case bfd_mach_fr400:
    case bfd_mach_fr450:
      return FALSE;
    default:
      if (major >= FR500_MAJOR_F_1 && major <= FR500_MAJOR_F_8)
	return TRUE;
      break;
    }

  return FALSE;
}

/* Returns TRUE if {MAJOR,MACH} supports media insns.  */

bfd_boolean
frv_is_media_major (CGEN_ATTR_VALUE_ENUM_TYPE major, unsigned long mach)
{
  switch (mach)
    {
    case bfd_mach_fr400:
      if (major >= FR400_MAJOR_M_1 && major <= FR400_MAJOR_M_2)
	return TRUE;
      break;
    case bfd_mach_fr450:
      if (major >= FR450_MAJOR_M_1 && major <= FR450_MAJOR_M_6)
	return TRUE;
      break;
    default:
      if (major >= FR500_MAJOR_M_1 && major <= FR500_MAJOR_M_8)
	return TRUE;
      break;
    }

  return FALSE;
}

bfd_boolean
frv_is_branch_insn (const CGEN_INSN *insn)
{
  if (frv_is_branch_major (CGEN_INSN_ATTR_VALUE (insn, CGEN_INSN_FR400_MAJOR),
			   bfd_mach_fr400))
    return TRUE;
  if (frv_is_branch_major (CGEN_INSN_ATTR_VALUE (insn, CGEN_INSN_FR450_MAJOR),
			   bfd_mach_fr450))
    return TRUE;
  if (frv_is_branch_major (CGEN_INSN_ATTR_VALUE (insn, CGEN_INSN_FR500_MAJOR),
			   bfd_mach_fr500))
    return TRUE;

  return FALSE;
}

bfd_boolean
frv_is_float_insn (const CGEN_INSN *insn)
{
  if (frv_is_float_major (CGEN_INSN_ATTR_VALUE (insn, CGEN_INSN_FR400_MAJOR),
			  bfd_mach_fr400))
    return TRUE;
  if (frv_is_float_major (CGEN_INSN_ATTR_VALUE (insn, CGEN_INSN_FR450_MAJOR),
			  bfd_mach_fr450))
    return TRUE;
  if (frv_is_float_major (CGEN_INSN_ATTR_VALUE (insn, CGEN_INSN_FR500_MAJOR),
			  bfd_mach_fr500))
    return TRUE;

  return FALSE;
}

bfd_boolean
frv_is_media_insn (const CGEN_INSN *insn)
{
  if (frv_is_media_major (CGEN_INSN_ATTR_VALUE (insn, CGEN_INSN_FR400_MAJOR),
			  bfd_mach_fr400))
    return TRUE;
  if (frv_is_media_major (CGEN_INSN_ATTR_VALUE (insn, CGEN_INSN_FR450_MAJOR),
			  bfd_mach_fr450))
    return TRUE;
  if (frv_is_media_major (CGEN_INSN_ATTR_VALUE (insn, CGEN_INSN_FR500_MAJOR),
			  bfd_mach_fr500))
    return TRUE;

  return FALSE;
}

/* This table represents the allowable packing for vliw insns for the fr400.
   The fr400 has only 2 vliw slots. Represent this by not allowing any insns
   in the extra slots.
   Subsets of any given row are also allowed.  */
static VLIW_COMBO fr400_allowed_vliw[] =
{
  /*  slot0       slot1       slot2       slot3    */
  {  UNIT_I0,    UNIT_I1,    UNIT_NIL,   UNIT_NIL  PAD_VLIW_COMBO },
  {  UNIT_I0,    UNIT_FM0,   UNIT_NIL,   UNIT_NIL  PAD_VLIW_COMBO },
  {  UNIT_I0,    UNIT_B0,    UNIT_NIL,   UNIT_NIL  PAD_VLIW_COMBO },
  {  UNIT_FM0,   UNIT_FM1,   UNIT_NIL,   UNIT_NIL  PAD_VLIW_COMBO },
  {  UNIT_FM0,   UNIT_B0,    UNIT_NIL,   UNIT_NIL  PAD_VLIW_COMBO },
  {  UNIT_B0,    UNIT_NIL,   UNIT_NIL,   UNIT_NIL  PAD_VLIW_COMBO },
  {  UNIT_C,     UNIT_NIL,   UNIT_NIL,   UNIT_NIL  PAD_VLIW_COMBO },
  {  UNIT_NIL,   UNIT_NIL,   UNIT_NIL,   UNIT_NIL  PAD_VLIW_COMBO }
};

/* This table represents the allowable packing for vliw insns for the fr500.
   The fr500 has only 4 vliw slots. Represent this by not allowing any insns
   in the extra slots.
   Subsets of any given row are also allowed.  */
static VLIW_COMBO fr500_allowed_vliw[] =
{
  /*  slot0       slot1       slot2       slot3    */
  {  UNIT_I0,    UNIT_FM0,   UNIT_I1,    UNIT_FM1  PAD_VLIW_COMBO },
  {  UNIT_I0,    UNIT_FM0,   UNIT_I1,    UNIT_B0   PAD_VLIW_COMBO },
  {  UNIT_I0,    UNIT_FM0,   UNIT_FM1,   UNIT_B0   PAD_VLIW_COMBO },
  {  UNIT_I0,    UNIT_FM0,   UNIT_B0,    UNIT_B1   PAD_VLIW_COMBO },
  {  UNIT_I0,    UNIT_I1,    UNIT_B0,    UNIT_B1   PAD_VLIW_COMBO },
  {  UNIT_I0,    UNIT_B0,    UNIT_B1,    UNIT_NIL  PAD_VLIW_COMBO },
  {  UNIT_FM0,   UNIT_FM1,   UNIT_B0,    UNIT_B1   PAD_VLIW_COMBO },
  {  UNIT_FM0,   UNIT_B0,    UNIT_B1,    UNIT_NIL  PAD_VLIW_COMBO },
  {  UNIT_B0,    UNIT_B1,    UNIT_NIL,   UNIT_NIL  PAD_VLIW_COMBO },
  {  UNIT_C,     UNIT_NIL,   UNIT_NIL,   UNIT_NIL  PAD_VLIW_COMBO },
  {  UNIT_NIL,   UNIT_NIL,   UNIT_NIL,   UNIT_NIL  PAD_VLIW_COMBO }
};

/* This table represents the allowable packing for vliw insns for the fr550.
   Subsets of any given row are also allowed.  */
static VLIW_COMBO fr550_allowed_vliw[] =
{
  /*  slot0       slot1       slot2       slot3       slot4       slot5       slot6       slot7   */
  {  UNIT_I0,    UNIT_I1,    UNIT_I2,    UNIT_I3,    UNIT_B0,    UNIT_B1 ,   UNIT_NIL,   UNIT_NIL },
  {  UNIT_I0,    UNIT_I1,    UNIT_I2,    UNIT_B0,    UNIT_B1 ,   UNIT_NIL,   UNIT_NIL,   UNIT_NIL },
  {  UNIT_I0,    UNIT_I1,    UNIT_B0,    UNIT_B1 ,   UNIT_NIL,   UNIT_NIL,   UNIT_NIL,   UNIT_NIL },
  {  UNIT_I0,    UNIT_B0,    UNIT_B1 ,   UNIT_NIL,   UNIT_NIL,   UNIT_NIL,   UNIT_NIL,   UNIT_NIL },
  {  UNIT_I0,    UNIT_FM0,   UNIT_I1,    UNIT_FM1,   UNIT_I2,    UNIT_FM2,   UNIT_I3,    UNIT_FM3 },
  {  UNIT_I0,    UNIT_FM0,   UNIT_I1,    UNIT_FM1,   UNIT_I2,    UNIT_FM2,   UNIT_I3,    UNIT_B0  },
  {  UNIT_I0,    UNIT_FM0,   UNIT_I1,    UNIT_FM1,   UNIT_I2,    UNIT_FM2,   UNIT_FM3,   UNIT_B0  },
  {  UNIT_I0,    UNIT_FM0,   UNIT_I1,    UNIT_FM1,   UNIT_I2,    UNIT_FM2,   UNIT_B0,    UNIT_B1  },
  {  UNIT_I0,    UNIT_FM0,   UNIT_I1,    UNIT_FM1,   UNIT_I2,    UNIT_I3,    UNIT_B0,    UNIT_B1  },
  {  UNIT_I0,    UNIT_FM0,   UNIT_I1,    UNIT_FM1,   UNIT_I2,    UNIT_B0,    UNIT_B1,    UNIT_NIL },
  {  UNIT_I0,    UNIT_FM0,   UNIT_I1,    UNIT_FM1,   UNIT_FM2,   UNIT_FM3,   UNIT_B0,    UNIT_B1  },
  {  UNIT_I0,    UNIT_FM0,   UNIT_I1,    UNIT_FM1,   UNIT_FM2,   UNIT_FM3,   UNIT_B0,    UNIT_B1  },
  {  UNIT_I0,    UNIT_FM0,   UNIT_I1,    UNIT_FM1,   UNIT_FM2,   UNIT_B0,    UNIT_B1,    UNIT_NIL },
  {  UNIT_I0,    UNIT_FM0,   UNIT_I1,    UNIT_FM1,   UNIT_B0,    UNIT_B1,    UNIT_NIL,   UNIT_NIL },
  {  UNIT_I0,    UNIT_FM0,   UNIT_I1,    UNIT_I2,    UNIT_I3,    UNIT_B0,    UNIT_B1,    UNIT_NIL },
  {  UNIT_I0,    UNIT_FM0,   UNIT_I1,    UNIT_I2,    UNIT_B0,    UNIT_B1,    UNIT_NIL,   UNIT_NIL },
  {  UNIT_I0,    UNIT_FM0,   UNIT_I1,    UNIT_B0,    UNIT_B1,    UNIT_NIL,   UNIT_NIL,   UNIT_NIL },
  {  UNIT_I0,    UNIT_FM0,   UNIT_FM1,   UNIT_FM2,   UNIT_FM3,   UNIT_B0,    UNIT_B1,    UNIT_NIL },
  {  UNIT_I0,    UNIT_FM0,   UNIT_FM1,   UNIT_FM2,   UNIT_B0,    UNIT_B1,    UNIT_NIL,   UNIT_NIL },
  {  UNIT_I0,    UNIT_FM0,   UNIT_FM1,   UNIT_B0,    UNIT_B1,    UNIT_NIL,   UNIT_NIL,   UNIT_NIL },
  {  UNIT_I0,    UNIT_FM0,   UNIT_B0,    UNIT_B1,    UNIT_NIL,   UNIT_NIL,   UNIT_NIL,   UNIT_NIL },
  {  UNIT_B0,    UNIT_B1,    UNIT_NIL,   UNIT_NIL,   UNIT_NIL,   UNIT_NIL,   UNIT_NIL,   UNIT_NIL },
  {  UNIT_C,     UNIT_NIL,   UNIT_NIL,   UNIT_NIL,   UNIT_NIL,   UNIT_NIL,   UNIT_NIL,   UNIT_NIL },
  {  UNIT_FM0,   UNIT_FM1,   UNIT_FM2,   UNIT_FM3,   UNIT_B0,    UNIT_B1,    UNIT_NIL,   UNIT_NIL },
  {  UNIT_FM0,   UNIT_FM1,   UNIT_FM2,   UNIT_B0,    UNIT_B1,    UNIT_NIL,   UNIT_NIL,   UNIT_NIL },
  {  UNIT_FM0,   UNIT_FM1,   UNIT_B0,    UNIT_B1,    UNIT_NIL,   UNIT_NIL,   UNIT_NIL,   UNIT_NIL },
  {  UNIT_FM0,   UNIT_B0,    UNIT_B1,    UNIT_NIL,   UNIT_NIL,   UNIT_NIL,   UNIT_NIL,   UNIT_NIL },
  {  UNIT_NIL,   UNIT_NIL,   UNIT_NIL,   UNIT_NIL,   UNIT_NIL,   UNIT_NIL,   UNIT_NIL,   UNIT_NIL }
};

/* Some insns are assigned specialized implementation units which map to
   different actual implementation units on different machines.  These
   tables perform that mapping.  */
static CGEN_ATTR_VALUE_ENUM_TYPE fr400_unit_mapping[] =
{
/* unit in insn    actual unit */
/* NIL      */     UNIT_NIL,
/* I0       */     UNIT_I0,
/* I1       */     UNIT_I1,
/* I01      */     UNIT_I01, 
/* I2       */     UNIT_NIL, /* no I2 or I3 unit */
/* I3       */     UNIT_NIL,
/* IALL     */     UNIT_I01, /* only I0 and I1 units */
/* FM0      */     UNIT_FM0,
/* FM1      */     UNIT_FM1,
/* FM01     */     UNIT_FM01,
/* FM2      */     UNIT_NIL, /* no F2 or M2 units */
/* FM3      */     UNIT_NIL, /* no F3 or M3 units */
/* FMALL    */     UNIT_FM01,/* Only F0,F1,M0,M1 units */
/* FMLOW    */     UNIT_FM0, /* Only F0,M0 units */
/* B0       */     UNIT_B0,  /* branches only in B0 unit.  */
/* B1       */     UNIT_B0,
/* B01      */     UNIT_B0,
/* C        */     UNIT_C,
/* MULT-DIV */     UNIT_I0,  /* multiply and divide only in I0  unit.  */
/* IACC     */     UNIT_I01, /* iacc multiply       in I0 or I1 unit.  */
/* LOAD     */     UNIT_I0,  /* load                only in I0  unit.  */
/* STORE    */     UNIT_I0,  /* store               only in I0  unit.  */
/* SCAN     */     UNIT_I0,  /* scan                only in I0  unit.  */
/* DCPL     */     UNIT_C,   /* dcpl                only in C   unit.  */
/* MDUALACC */     UNIT_FM0, /* media dual acc insn only in FM0 unit.  */
/* MDCUTSSI */     UNIT_FM0, /* mdcutssi            only in FM0 unit.  */
/* MCLRACC-1*/     UNIT_FM0  /* mclracc,A==1   insn only in FM0 unit.  */
};

/* Some insns are assigned specialized implementation units which map to
   different actual implementation units on different machines.  These
   tables perform that mapping.  */
static CGEN_ATTR_VALUE_ENUM_TYPE fr450_unit_mapping[] =
{
/* unit in insn    actual unit */
/* NIL      */     UNIT_NIL,
/* I0       */     UNIT_I0,
/* I1       */     UNIT_I1,
/* I01      */     UNIT_I01, 
/* I2       */     UNIT_NIL, /* no I2 or I3 unit */
/* I3       */     UNIT_NIL,
/* IALL     */     UNIT_I01, /* only I0 and I1 units */
/* FM0      */     UNIT_FM0,
/* FM1      */     UNIT_FM1,
/* FM01     */     UNIT_FM01,
/* FM2      */     UNIT_NIL, /* no F2 or M2 units */
/* FM3      */     UNIT_NIL, /* no F3 or M3 units */
/* FMALL    */     UNIT_FM01,/* Only F0,F1,M0,M1 units */
/* FMLOW    */     UNIT_FM0, /* Only F0,M0 units */
/* B0       */     UNIT_B0,  /* branches only in B0 unit.  */
/* B1       */     UNIT_B0,
/* B01      */     UNIT_B0,
/* C        */     UNIT_C,
/* MULT-DIV */     UNIT_I0,  /* multiply and divide only in I0  unit.  */
/* IACC     */     UNIT_I01, /* iacc multiply       in I0 or I1 unit.  */
/* LOAD     */     UNIT_I0,  /* load                only in I0  unit.  */
/* STORE    */     UNIT_I0,  /* store               only in I0  unit.  */
/* SCAN     */     UNIT_I0,  /* scan                only in I0  unit.  */
/* DCPL     */     UNIT_I0,  /* dcpl                only in I0  unit.  */
/* MDUALACC */     UNIT_FM0, /* media dual acc insn only in FM0 unit.  */
/* MDCUTSSI */     UNIT_FM01, /* mdcutssi           in FM0 or FM1.  */
/* MCLRACC-1*/     UNIT_FM0  /* mclracc,A==1   insn only in FM0 unit.  */
};

static CGEN_ATTR_VALUE_ENUM_TYPE fr500_unit_mapping[] =
{
/* unit in insn    actual unit */
/* NIL      */     UNIT_NIL,
/* I0       */     UNIT_I0,
/* I1       */     UNIT_I1,
/* I01      */     UNIT_I01, 
/* I2       */     UNIT_NIL, /* no I2 or I3 unit */
/* I3       */     UNIT_NIL,
/* IALL     */     UNIT_I01, /* only I0 and I1 units */
/* FM0      */     UNIT_FM0,
/* FM1      */     UNIT_FM1,
/* FM01     */     UNIT_FM01,
/* FM2      */     UNIT_NIL, /* no F2 or M2 units */
/* FM3      */     UNIT_NIL, /* no F3 or M2 units */
/* FMALL    */     UNIT_FM01,/* Only F0,F1,M0,M1 units */
/* FMLOW    */     UNIT_FM0, /* Only F0,M0 units */
/* B0       */     UNIT_B0,
/* B1       */     UNIT_B1,
/* B01      */     UNIT_B01,
/* C        */     UNIT_C,
/* MULT-DIV */     UNIT_I01, /* multiply and divide in I0 or I1 unit.  */
/* IACC     */     UNIT_NIL, /* iacc multiply       not implemented */
/* LOAD     */     UNIT_I01, /* load                in I0 or I1 unit.  */
/* STORE    */     UNIT_I0,  /* store               only in I0 unit.  */
/* SCAN     */     UNIT_I01, /* scan                in I0 or I1 unit.  */
/* DCPL     */     UNIT_C,   /* dcpl                only in C unit.  */
/* MDUALACC */     UNIT_FM0, /* media dual acc insn only in FM0 unit.  */
/* MDCUTSSI */     UNIT_FM0, /* mdcutssi            only in FM0 unit.  */
/* MCLRACC-1*/     UNIT_FM01 /* mclracc,A==1 in FM0 or FM1 unit.  */
};

static CGEN_ATTR_VALUE_ENUM_TYPE fr550_unit_mapping[] =
{
/* unit in insn    actual unit */
/* NIL      */     UNIT_NIL,
/* I0       */     UNIT_I0,
/* I1       */     UNIT_I1,
/* I01      */     UNIT_I01, 
/* I2       */     UNIT_I2,
/* I3       */     UNIT_I3,
/* IALL     */     UNIT_IALL, 
/* FM0      */     UNIT_FM0,
/* FM1      */     UNIT_FM1,
/* FM01     */     UNIT_FM01,
/* FM2      */     UNIT_FM2,
/* FM3      */     UNIT_FM3,
/* FMALL    */     UNIT_FMALL,
/* FMLOW    */     UNIT_FM01, /* Only F0,F1,M0,M1 units */
/* B0       */     UNIT_B0,
/* B1       */     UNIT_B1,
/* B01      */     UNIT_B01,
/* C        */     UNIT_C,
/* MULT-DIV */     UNIT_I01,  /* multiply and divide in I0 or I1 unit.    */
/* IACC     */     UNIT_NIL,  /* iacc multiply       not implemented.     */
/* LOAD     */     UNIT_I01,  /* load                in I0 or I1 unit.    */
/* STORE    */     UNIT_I01,  /* store               in I0 or I1 unit.    */
/* SCAN     */     UNIT_IALL, /* scan                in any integer unit. */
/* DCPL     */     UNIT_I0,   /* dcpl                only in I0 unit.     */
/* MDUALACC */     UNIT_FMALL,/* media dual acc insn in all media units   */
/* MDCUTSSI */     UNIT_FM01, /* mdcutssi            in FM0 or FM1 unit.  */
/* MCLRACC-1*/     UNIT_FM01  /* mclracc,A==1 in FM0 or FM1 unit.         */
};

void
frv_vliw_reset (FRV_VLIW *vliw, unsigned long mach, unsigned long elf_flags)
{
  vliw->next_slot = 0;
  vliw->constraint_violation = 0;
  vliw->mach = mach;
  vliw->elf_flags = elf_flags;

  switch (mach)
    {
    case bfd_mach_fr400:
      vliw->current_vliw = fr400_allowed_vliw;
      vliw->unit_mapping = fr400_unit_mapping;
      break;
    case bfd_mach_fr450:
      vliw->current_vliw = fr400_allowed_vliw;
      vliw->unit_mapping = fr450_unit_mapping;
      break;
    case bfd_mach_fr550:
      vliw->current_vliw = fr550_allowed_vliw;
      vliw->unit_mapping = fr550_unit_mapping;
      break;
    default:
      vliw->current_vliw = fr500_allowed_vliw;
      vliw->unit_mapping = fr500_unit_mapping;
      break;
    }
}

/* Return TRUE if unit1 is a match for unit2.
   Unit1 comes from the insn's UNIT attribute. unit2 comes from one of the
   *_allowed_vliw tables above.  */
static bfd_boolean
match_unit (FRV_VLIW *vliw,
	    CGEN_ATTR_VALUE_ENUM_TYPE unit1, CGEN_ATTR_VALUE_ENUM_TYPE unit2)
{
  /* Map any specialized implementation units to actual ones.  */
  unit1 = vliw->unit_mapping[unit1];

  if (unit1 == unit2)
    return TRUE;
  if (unit1 < unit2)
    return FALSE;

  switch (unit1)
    {
    case UNIT_I01:
    case UNIT_FM01:
    case UNIT_B01:
      /* The 01 versions of these units are within 2 enums of the 0 or 1
	 versions.  */
      if (unit1 - unit2 <= 2)
	return TRUE;
      break;
    case UNIT_IALL:
    case UNIT_FMALL:
      /* The ALL versions of these units are within 5 enums of the 0, 1, 2 or 3
	 versions.  */
      if (unit1 - unit2 <= 5)
	return TRUE;
      break;
    default:
      break;
    }

  return FALSE;
}

/* Return TRUE if the vliws match, FALSE otherwise.  */

static bfd_boolean
match_vliw (VLIW_COMBO *vliw1, VLIW_COMBO *vliw2, int vliw_size)
{
  int i;

  for (i = 0; i < vliw_size; ++i)
    if ((*vliw1)[i] != (*vliw2)[i])
      return FALSE;

  return TRUE;
}

/* Find the next vliw vliw in the table that can accomodate the new insn.
   If one is found then return it. Otherwise return NULL.  */

static VLIW_COMBO *
add_next_to_vliw (FRV_VLIW *vliw, CGEN_ATTR_VALUE_ENUM_TYPE unit)
{
  int           next    = vliw->next_slot;
  VLIW_COMBO    *current = vliw->current_vliw;
  VLIW_COMBO    *potential;

  if (next <= 0)
    {
      fprintf (stderr, "frv-opc.c line %d: bad vliw->next_slot value.\n",
	       __LINE__);
      abort (); /* Should never happen.  */
    }

  /* The table is sorted by units allowed within slots, so vliws with
     identical starting sequences are together.  */
  potential = current;
  do
    {
      if (match_unit (vliw, unit, (*potential)[next]))
	return potential;
      ++potential;
    }
  while (match_vliw (potential, current, next));

  return NULL;
}

/* Look for the given major insn type in the given vliw.
   Returns TRUE if found, FALSE otherwise.  */

static bfd_boolean
find_major_in_vliw (FRV_VLIW *vliw, CGEN_ATTR_VALUE_ENUM_TYPE major)
{
  int i;

  for (i = 0; i < vliw->next_slot; ++i)
    if (vliw->major[i] == major)
      return TRUE;

  return FALSE;
}

/* Check for constraints between the insns in the vliw due to major insn
   types.  */

static bfd_boolean
fr400_check_insn_major_constraints (FRV_VLIW *vliw, CGEN_ATTR_VALUE_ENUM_TYPE major)
{
  /* In the cpu file, all media insns are represented as being allowed in
     both media units. This makes it easier since this is the case for fr500.
     Catch the invalid combinations here.  Insns of major class FR400_MAJOR_M_2
     cannot coexist with any other media insn in a vliw.  */
  switch (major)
    {
    case FR400_MAJOR_M_2:
      return ! find_major_in_vliw (vliw, FR400_MAJOR_M_1)
	&&   ! find_major_in_vliw (vliw, FR400_MAJOR_M_2);
    case FR400_MAJOR_M_1:
      return ! find_major_in_vliw (vliw, FR400_MAJOR_M_2);
    default:
      break;
    }
  return TRUE;
}

static bfd_boolean
fr450_check_insn_major_constraints (FRV_VLIW *vliw, CGEN_ATTR_VALUE_ENUM_TYPE major)
{
  CGEN_ATTR_VALUE_ENUM_TYPE other_major;

  /* Our caller guarantees there's at least one other instruction.  */
  other_major = CGEN_INSN_ATTR_VALUE (vliw->insn[0], CGEN_INSN_FR450_MAJOR);

  /* (M4, M5) and (M4, M6) are allowed.  */
  if (other_major == FR450_MAJOR_M_4)
    if (major == FR450_MAJOR_M_5 || major == FR450_MAJOR_M_6)
      return TRUE;

  /* Otherwise, instructions in even-numbered media categories cannot be
     executed in parallel with other media instructions.  */
  switch (major)
    {
    case FR450_MAJOR_M_2:
    case FR450_MAJOR_M_4:
    case FR450_MAJOR_M_6:
      return !(other_major >= FR450_MAJOR_M_1
	       && other_major <= FR450_MAJOR_M_6);

    case FR450_MAJOR_M_1:
    case FR450_MAJOR_M_3:
    case FR450_MAJOR_M_5:
      return !(other_major == FR450_MAJOR_M_2
	       || other_major == FR450_MAJOR_M_4
	       || other_major == FR450_MAJOR_M_6);

    default:
      return TRUE;
    }
}

static bfd_boolean
find_unit_in_vliw (FRV_VLIW *vliw, CGEN_ATTR_VALUE_ENUM_TYPE unit)
{
  int i;

  for (i = 0; i < vliw->next_slot; ++i)
    if (CGEN_INSN_ATTR_VALUE (vliw->insn[i], CGEN_INSN_UNIT) == unit)
      return TRUE;

  return FALSE; /* Not found.  */
}

static bfd_boolean
find_major_in_slot (FRV_VLIW *vliw,
		    CGEN_ATTR_VALUE_ENUM_TYPE major,
		    CGEN_ATTR_VALUE_ENUM_TYPE slot)
{
  int i;

  for (i = 0; i < vliw->next_slot; ++i)
    if (vliw->major[i] == major && (*vliw->current_vliw)[i] == slot)
      return TRUE;

  return FALSE;
}

static bfd_boolean
fr550_find_media_in_vliw (FRV_VLIW *vliw)
{
  int i;

  for (i = 0; i < vliw->next_slot; ++i)
    {
      if (vliw->major[i] < FR550_MAJOR_M_1 || vliw->major[i] > FR550_MAJOR_M_5)
	continue;

      /* Found a media insn, however, MNOP and MCLRACC don't count.  */
      if (CGEN_INSN_NUM (vliw->insn[i]) == FRV_INSN_MNOP
	  || CGEN_INSN_NUM (vliw->insn[i]) == FRV_INSN_MCLRACC_0
	  || CGEN_INSN_NUM (vliw->insn[i]) == FRV_INSN_MCLRACC_1)
	continue;

      return TRUE; /* Found one.  */
    }

  return FALSE;
}

static bfd_boolean
fr550_find_float_in_vliw (FRV_VLIW *vliw)
{
  int i;

  for (i = 0; i < vliw->next_slot; ++i)
    {
      if (vliw->major[i] < FR550_MAJOR_F_1 || vliw->major[i] > FR550_MAJOR_F_4)
	continue;

      /* Found a floating point insn, however, FNOP doesn't count.  */
      if (CGEN_INSN_NUM (vliw->insn[i]) == FRV_INSN_FNOP)
	continue;

      return TRUE; /* Found one.  */
    }

  return FALSE;
}

static bfd_boolean
fr550_check_insn_major_constraints (FRV_VLIW *vliw,
				    CGEN_ATTR_VALUE_ENUM_TYPE major,
				    const CGEN_INSN *insn)
{
  CGEN_ATTR_VALUE_ENUM_TYPE unit;
  CGEN_ATTR_VALUE_ENUM_TYPE slot = (*vliw->current_vliw)[vliw->next_slot];
  switch (slot)
    {
    case UNIT_I2:
      /* If it's a store, then there must be another store in I1 */
      unit = CGEN_INSN_ATTR_VALUE (insn, CGEN_INSN_UNIT);
      if (unit == UNIT_STORE)
	return find_unit_in_vliw (vliw, UNIT_STORE);
      break;
    case UNIT_FM2:
    case UNIT_FM3:
      /* Floating point insns other than FNOP in slot f2 or f3 cannot coexist
	 with media insns.  */
      if (major >= FR550_MAJOR_F_1 && major <= FR550_MAJOR_F_4
	  && CGEN_INSN_NUM (insn) != FRV_INSN_FNOP)
	return ! fr550_find_media_in_vliw (vliw);
      /* Media insns other than MNOP in slot m2 or m3 cannot coexist with
	 floating point insns.  */
      if (major >= FR550_MAJOR_M_1 && major <= FR550_MAJOR_M_5
	  && CGEN_INSN_NUM (insn) != FRV_INSN_MNOP)
	return ! fr550_find_float_in_vliw (vliw);
      /* F-2 in slot f2 or f3 cannot coexist with F-2 or F-4 in slot f1 or f2
	 respectively.  */
      if (major == FR550_MAJOR_F_2)
	return ! find_major_in_slot (vliw, FR550_MAJOR_F_2,
				     slot - (UNIT_FM2 - UNIT_FM0))
	  &&   ! find_major_in_slot (vliw, FR550_MAJOR_F_4,
				     slot - (UNIT_FM2 - UNIT_FM0));
      /* M-2 or M-5 in slot m2 or m3 cannot coexist with M-2 in slot m1 or m2
	 respectively.  */
      if (major == FR550_MAJOR_M_2 || major == FR550_MAJOR_M_5)
	return ! find_major_in_slot (vliw, FR550_MAJOR_M_2,
				     slot - (UNIT_FM2 - UNIT_FM0));
      /* M-4 in slot m2 or m3 cannot coexist with M-4 in slot m1 or m2
	 respectively.  */
      if (major == FR550_MAJOR_M_4)
	return ! find_major_in_slot (vliw, FR550_MAJOR_M_4,
				     slot - (UNIT_FM2 - UNIT_FM0));
      break;
    default:
      break;
    }
  return TRUE; /* All OK.  */
}

static bfd_boolean
fr500_check_insn_major_constraints (FRV_VLIW *vliw, CGEN_ATTR_VALUE_ENUM_TYPE major)
{
  /* TODO: A table might be faster for some of the more complex instances
     here.  */
  switch (major)
    {
    case FR500_MAJOR_I_1:
    case FR500_MAJOR_I_4:
    case FR500_MAJOR_I_5:
    case FR500_MAJOR_I_6:
    case FR500_MAJOR_B_1:
    case FR500_MAJOR_B_2:
    case FR500_MAJOR_B_3:
    case FR500_MAJOR_B_4:
    case FR500_MAJOR_B_5:
    case FR500_MAJOR_B_6:
    case FR500_MAJOR_F_4:
    case FR500_MAJOR_F_8:
    case FR500_MAJOR_M_8:
      return TRUE; /* OK */
    case FR500_MAJOR_I_2:
      /* Cannot coexist with I-3 insn.  */
      return ! find_major_in_vliw (vliw, FR500_MAJOR_I_3);
    case FR500_MAJOR_I_3:
      /* Cannot coexist with I-2 insn.  */
      return ! find_major_in_vliw (vliw, FR500_MAJOR_I_2);
    case FR500_MAJOR_F_1:
    case FR500_MAJOR_F_2:
      /* Cannot coexist with F-5, F-6, or M-7 insn.  */
      return ! find_major_in_vliw (vliw, FR500_MAJOR_F_5)
	&&   ! find_major_in_vliw (vliw, FR500_MAJOR_F_6)
	&&   ! find_major_in_vliw (vliw, FR500_MAJOR_M_7);
    case FR500_MAJOR_F_3:
      /* Cannot coexist with F-7, or M-7 insn.  */
      return ! find_major_in_vliw (vliw, FR500_MAJOR_F_7)
	&&   ! find_major_in_vliw (vliw, FR500_MAJOR_M_7);
    case FR500_MAJOR_F_5:
      /* Cannot coexist with F-1, F-2, F-6, F-7, or M-7 insn.  */
      return ! find_major_in_vliw (vliw, FR500_MAJOR_F_1)
	&&   ! find_major_in_vliw (vliw, FR500_MAJOR_F_2)
	&&   ! find_major_in_vliw (vliw, FR500_MAJOR_F_6)
	&&   ! find_major_in_vliw (vliw, FR500_MAJOR_F_7)
	&&   ! find_major_in_vliw (vliw, FR500_MAJOR_M_7);
    case FR500_MAJOR_F_6:
      /* Cannot coexist with F-1, F-2, F-5, F-6, or M-7 insn.  */
      return ! find_major_in_vliw (vliw, FR500_MAJOR_F_1)
	&&   ! find_major_in_vliw (vliw, FR500_MAJOR_F_2)
	&&   ! find_major_in_vliw (vliw, FR500_MAJOR_F_5)
	&&   ! find_major_in_vliw (vliw, FR500_MAJOR_F_6)
	&&   ! find_major_in_vliw (vliw, FR500_MAJOR_M_7);
    case FR500_MAJOR_F_7:
      /* Cannot coexist with F-3, F-5, F-7, or M-7 insn.  */
      return ! find_major_in_vliw (vliw, FR500_MAJOR_F_3)
	&&   ! find_major_in_vliw (vliw, FR500_MAJOR_F_5)
	&&   ! find_major_in_vliw (vliw, FR500_MAJOR_F_7)
	&&   ! find_major_in_vliw (vliw, FR500_MAJOR_M_7);
    case FR500_MAJOR_M_1:
      /* Cannot coexist with M-7 insn.  */
      return ! find_major_in_vliw (vliw, FR500_MAJOR_M_7);
    case FR500_MAJOR_M_2:
    case FR500_MAJOR_M_3:
      /* Cannot coexist with M-5, M-6 or M-7 insn.  */
      return ! find_major_in_vliw (vliw, FR500_MAJOR_M_5)
	&&   ! find_major_in_vliw (vliw, FR500_MAJOR_M_6)
	&&   ! find_major_in_vliw (vliw, FR500_MAJOR_M_7);
    case FR500_MAJOR_M_4:
      /* Cannot coexist with M-6 insn.  */
      return ! find_major_in_vliw (vliw, FR500_MAJOR_M_6);
    case FR500_MAJOR_M_5:
      /* Cannot coexist with M-2, M-3, M-5, M-6  or M-7 insn.  */
      return ! find_major_in_vliw (vliw, FR500_MAJOR_M_2)
	&&   ! find_major_in_vliw (vliw, FR500_MAJOR_M_3)
	&&   ! find_major_in_vliw (vliw, FR500_MAJOR_M_5)
	&&   ! find_major_in_vliw (vliw, FR500_MAJOR_M_6)
	&&   ! find_major_in_vliw (vliw, FR500_MAJOR_M_7);
    case FR500_MAJOR_M_6:
      /* Cannot coexist with M-2, M-3, M-4, M-5, M-6  or M-7 insn.  */
      return ! find_major_in_vliw (vliw, FR500_MAJOR_M_2)
	&&   ! find_major_in_vliw (vliw, FR500_MAJOR_M_3)
	&&   ! find_major_in_vliw (vliw, FR500_MAJOR_M_4)
	&&   ! find_major_in_vliw (vliw, FR500_MAJOR_M_5)
	&&   ! find_major_in_vliw (vliw, FR500_MAJOR_M_6)
	&&   ! find_major_in_vliw (vliw, FR500_MAJOR_M_7);
    case FR500_MAJOR_M_7:
      /* Cannot coexist with M-1, M-2, M-3, M-5, M-6  or M-7 insn.  */
      return ! find_major_in_vliw (vliw, FR500_MAJOR_M_1)
	&&   ! find_major_in_vliw (vliw, FR500_MAJOR_M_2)
	&&   ! find_major_in_vliw (vliw, FR500_MAJOR_M_3)
	&&   ! find_major_in_vliw (vliw, FR500_MAJOR_M_5)
	&&   ! find_major_in_vliw (vliw, FR500_MAJOR_M_6)
	&&   ! find_major_in_vliw (vliw, FR500_MAJOR_M_7)
	&&   ! find_major_in_vliw (vliw, FR500_MAJOR_F_1)
	&&   ! find_major_in_vliw (vliw, FR500_MAJOR_F_2)
	&&   ! find_major_in_vliw (vliw, FR500_MAJOR_F_3)
	&&   ! find_major_in_vliw (vliw, FR500_MAJOR_F_5)
	&&   ! find_major_in_vliw (vliw, FR500_MAJOR_F_6)
	&&   ! find_major_in_vliw (vliw, FR500_MAJOR_F_7);
    default:
      fprintf (stderr, "frv-opc.c, line %d: bad major code, aborting.\n",
	       __LINE__);
      abort ();
      break;
    }
  return TRUE;
}

static bfd_boolean
check_insn_major_constraints (FRV_VLIW *vliw,
			      CGEN_ATTR_VALUE_ENUM_TYPE major,
			      const CGEN_INSN *insn)
{
  switch (vliw->mach)
    {
    case bfd_mach_fr400:
      return fr400_check_insn_major_constraints (vliw, major);

    case bfd_mach_fr450:
      return fr450_check_insn_major_constraints (vliw, major);

    case bfd_mach_fr550:
      return fr550_check_insn_major_constraints (vliw, major, insn);

    default:
      return fr500_check_insn_major_constraints (vliw, major);
    }
}

/* Add in insn to the VLIW vliw if possible.
   Return 0 if successful, non-zero otherwise.  */

int
frv_vliw_add_insn (FRV_VLIW *vliw, const CGEN_INSN *insn)
{
  int index;
  CGEN_ATTR_VALUE_ENUM_TYPE major;
  CGEN_ATTR_VALUE_ENUM_TYPE unit;
  VLIW_COMBO *new_vliw;

  if (vliw->constraint_violation || CGEN_INSN_INVALID_P (insn))
    return 1;

  index = vliw->next_slot;
  if (index >= FRV_VLIW_SIZE)
    return 1;

  unit = CGEN_INSN_ATTR_VALUE (insn, CGEN_INSN_UNIT);
  if (unit == UNIT_NIL)
    {
      fprintf (stderr, "frv-opc.c line %d: bad insn unit.\n",
	       __LINE__);
      abort (); /* No UNIT specified for this insn in frv.cpu.  */
    }

  switch (vliw->mach)
    {
    case bfd_mach_fr400:
      major = CGEN_INSN_ATTR_VALUE (insn, CGEN_INSN_FR400_MAJOR);
      break;
    case bfd_mach_fr450:
      major = CGEN_INSN_ATTR_VALUE (insn, CGEN_INSN_FR450_MAJOR);
      break;
    case bfd_mach_fr550:
      major = CGEN_INSN_ATTR_VALUE (insn, CGEN_INSN_FR550_MAJOR);
      break;
    default:
      major = CGEN_INSN_ATTR_VALUE (insn, CGEN_INSN_FR500_MAJOR);
      break;
    }

  if (index <= 0)
    {
      /* Any insn can be added to slot 0.  */
      while (! match_unit (vliw, unit, (*vliw->current_vliw)[0]))
	++vliw->current_vliw;
      vliw->major[0] = major;
      vliw->insn[0] = insn;
      vliw->next_slot = 1;
      return 0;
    }

  /* If there are already insns in the vliw(s) check to see that
     this one can be added.  Do this by finding an allowable vliw
     combination that can accept the new insn.  */
  if (! (vliw->elf_flags & EF_FRV_NOPACK))
    {
      new_vliw = add_next_to_vliw (vliw, unit);
      if (new_vliw && check_insn_major_constraints (vliw, major, insn))
	{
	  vliw->current_vliw = new_vliw;
	  vliw->major[index] = major;
	  vliw->insn[index] = insn;
	  vliw->next_slot++;
	  return 0;
	}

      /* The frv machine supports all packing conbinations.  If we fail,
	 to add the insn, then it could not be handled as if it was the fr500.
	 Just return as if it was handled ok.  */
      if (vliw->mach == bfd_mach_frv)
	return 0;
    }

  vliw->constraint_violation = 1;
  return 1;
}

bfd_boolean
spr_valid (long regno)
{
  if (regno < 0)     return FALSE;
  if (regno <= 4095) return TRUE;
  return FALSE;
}
/* -- */
/* The hash functions are recorded here to help keep assembler code out of
   the disassembler and vice versa.  */

static int asm_hash_insn_p        (const CGEN_INSN *);
static unsigned int asm_hash_insn (const char *);
static int dis_hash_insn_p        (const CGEN_INSN *);
static unsigned int dis_hash_insn (const char *, CGEN_INSN_INT);

/* Instruction formats.  */

#if defined (__STDC__) || defined (ALMOST_STDC) || defined (HAVE_STRINGIZE)
#define F(f) & frv_cgen_ifld_table[FRV_##f]
#else
#define F(f) & frv_cgen_ifld_table[FRV_/**/f]
#endif
static const CGEN_IFMT ifmt_empty ATTRIBUTE_UNUSED = {
  0, 0, 0x0, { { 0 } }
};

static const CGEN_IFMT ifmt_add ATTRIBUTE_UNUSED = {
  32, 32, 0x1fc0fc0, { { F (F_PACK) }, { F (F_GRK) }, { F (F_OP) }, { F (F_GRI) }, { F (F_ICCI_1_NULL) }, { F (F_OPE2) }, { F (F_GRJ) }, { 0 } }
};

static const CGEN_IFMT ifmt_not ATTRIBUTE_UNUSED = {
  32, 32, 0x1ffffc0, { { F (F_PACK) }, { F (F_GRK) }, { F (F_OP) }, { F (F_RS_NULL) }, { F (F_ICCI_1_NULL) }, { F (F_OPE2) }, { F (F_GRJ) }, { 0 } }
};

static const CGEN_IFMT ifmt_smul ATTRIBUTE_UNUSED = {
  32, 32, 0x1fc0fc0, { { F (F_PACK) }, { F (F_GRK) }, { F (F_OP) }, { F (F_GRI) }, { F (F_ICCI_1_NULL) }, { F (F_OPE2) }, { F (F_GRJ) }, { 0 } }
};

static const CGEN_IFMT ifmt_smu ATTRIBUTE_UNUSED = {
  32, 32, 0x7ffc0fc0, { { F (F_PACK) }, { F (F_RD_NULL) }, { F (F_OP) }, { F (F_GRI) }, { F (F_OPE1) }, { F (F_GRJ) }, { 0 } }
};

static const CGEN_IFMT ifmt_slass ATTRIBUTE_UNUSED = {
  32, 32, 0x1fc0fc0, { { F (F_PACK) }, { F (F_GRK) }, { F (F_OP) }, { F (F_GRI) }, { F (F_OPE1) }, { F (F_GRJ) }, { 0 } }
};

static const CGEN_IFMT ifmt_scutss ATTRIBUTE_UNUSED = {
  32, 32, 0x1ffffc0, { { F (F_PACK) }, { F (F_GRK) }, { F (F_OP) }, { F (F_RS_NULL) }, { F (F_OPE1) }, { F (F_GRJ) }, { 0 } }
};

static const CGEN_IFMT ifmt_cadd ATTRIBUTE_UNUSED = {
  32, 32, 0x1fc00c0, { { F (F_PACK) }, { F (F_GRK) }, { F (F_OP) }, { F (F_GRI) }, { F (F_CCI) }, { F (F_COND) }, { F (F_OPE4) }, { F (F_GRJ) }, { 0 } }
};

static const CGEN_IFMT ifmt_cnot ATTRIBUTE_UNUSED = {
  32, 32, 0x1fff0c0, { { F (F_PACK) }, { F (F_GRK) }, { F (F_OP) }, { F (F_RS_NULL) }, { F (F_CCI) }, { F (F_COND) }, { F (F_OPE4) }, { F (F_GRJ) }, { 0 } }
};

static const CGEN_IFMT ifmt_csmul ATTRIBUTE_UNUSED = {
  32, 32, 0x1fc00c0, { { F (F_PACK) }, { F (F_GRK) }, { F (F_OP) }, { F (F_GRI) }, { F (F_CCI) }, { F (F_COND) }, { F (F_OPE4) }, { F (F_GRJ) }, { 0 } }
};

static const CGEN_IFMT ifmt_addcc ATTRIBUTE_UNUSED = {
  32, 32, 0x1fc03c0, { { F (F_PACK) }, { F (F_GRK) }, { F (F_OP) }, { F (F_GRI) }, { F (F_ICCI_1) }, { F (F_OPE2) }, { F (F_GRJ) }, { 0 } }
};

static const CGEN_IFMT ifmt_smulcc ATTRIBUTE_UNUSED = {
  32, 32, 0x1fc03c0, { { F (F_PACK) }, { F (F_GRK) }, { F (F_OP) }, { F (F_GRI) }, { F (F_ICCI_1) }, { F (F_OPE2) }, { F (F_GRJ) }, { 0 } }
};

static const CGEN_IFMT ifmt_addi ATTRIBUTE_UNUSED = {
  32, 32, 0x1fc0000, { { F (F_PACK) }, { F (F_GRK) }, { F (F_OP) }, { F (F_GRI) }, { F (F_D12) }, { 0 } }
};

static const CGEN_IFMT ifmt_smuli ATTRIBUTE_UNUSED = {
  32, 32, 0x1fc0000, { { F (F_PACK) }, { F (F_GRK) }, { F (F_OP) }, { F (F_GRI) }, { F (F_D12) }, { 0 } }
};

static const CGEN_IFMT ifmt_addicc ATTRIBUTE_UNUSED = {
  32, 32, 0x1fc0000, { { F (F_PACK) }, { F (F_GRK) }, { F (F_OP) }, { F (F_GRI) }, { F (F_ICCI_1) }, { F (F_S10) }, { 0 } }
};

static const CGEN_IFMT ifmt_smulicc ATTRIBUTE_UNUSED = {
  32, 32, 0x1fc0000, { { F (F_PACK) }, { F (F_GRK) }, { F (F_OP) }, { F (F_GRI) }, { F (F_ICCI_1) }, { F (F_S10) }, { 0 } }
};

static const CGEN_IFMT ifmt_cmpb ATTRIBUTE_UNUSED = {
  32, 32, 0x7ffc03c0, { { F (F_PACK) }, { F (F_GRK_NULL) }, { F (F_OP) }, { F (F_GRI) }, { F (F_ICCI_1) }, { F (F_OPE2) }, { F (F_GRJ) }, { 0 } }
};

static const CGEN_IFMT ifmt_setlo ATTRIBUTE_UNUSED = {
  32, 32, 0x1ff0000, { { F (F_PACK) }, { F (F_GRK) }, { F (F_OP) }, { F (F_MISC_NULL_4) }, { F (F_U16) }, { 0 } }
};

static const CGEN_IFMT ifmt_sethi ATTRIBUTE_UNUSED = {
  32, 32, 0x1ff0000, { { F (F_PACK) }, { F (F_GRK) }, { F (F_OP) }, { F (F_MISC_NULL_4) }, { F (F_U16) }, { 0 } }
};

static const CGEN_IFMT ifmt_setlos ATTRIBUTE_UNUSED = {
  32, 32, 0x1ff0000, { { F (F_PACK) }, { F (F_GRK) }, { F (F_OP) }, { F (F_MISC_NULL_4) }, { F (F_S16) }, { 0 } }
};

static const CGEN_IFMT ifmt_ldbf ATTRIBUTE_UNUSED = {
  32, 32, 0x1fc0fc0, { { F (F_PACK) }, { F (F_FRK) }, { F (F_OP) }, { F (F_GRI) }, { F (F_OPE1) }, { F (F_GRJ) }, { 0 } }
};

static const CGEN_IFMT ifmt_ldc ATTRIBUTE_UNUSED = {
  32, 32, 0x1fc0fc0, { { F (F_PACK) }, { F (F_CPRK) }, { F (F_OP) }, { F (F_GRI) }, { F (F_OPE1) }, { F (F_GRJ) }, { 0 } }
};

static const CGEN_IFMT ifmt_ldd ATTRIBUTE_UNUSED = {
  32, 32, 0x1fc0fc0, { { F (F_PACK) }, { F (F_GRK) }, { F (F_OP) }, { F (F_GRI) }, { F (F_OPE1) }, { F (F_GRJ) }, { 0 } }
};

static const CGEN_IFMT ifmt_lddf ATTRIBUTE_UNUSED = {
  32, 32, 0x1fc0fc0, { { F (F_PACK) }, { F (F_FRK) }, { F (F_OP) }, { F (F_GRI) }, { F (F_OPE1) }, { F (F_GRJ) }, { 0 } }
};

static const CGEN_IFMT ifmt_lddc ATTRIBUTE_UNUSED = {
  32, 32, 0x1fc0fc0, { { F (F_PACK) }, { F (F_CPRK) }, { F (F_OP) }, { F (F_GRI) }, { F (F_OPE1) }, { F (F_GRJ) }, { 0 } }
};

static const CGEN_IFMT ifmt_ldsbi ATTRIBUTE_UNUSED = {
  32, 32, 0x1fc0000, { { F (F_PACK) }, { F (F_GRK) }, { F (F_OP) }, { F (F_GRI) }, { F (F_D12) }, { 0 } }
};

static const CGEN_IFMT ifmt_ldbfi ATTRIBUTE_UNUSED = {
  32, 32, 0x1fc0000, { { F (F_PACK) }, { F (F_FRK) }, { F (F_OP) }, { F (F_GRI) }, { F (F_D12) }, { 0 } }
};

static const CGEN_IFMT ifmt_lddi ATTRIBUTE_UNUSED = {
  32, 32, 0x1fc0000, { { F (F_PACK) }, { F (F_GRK) }, { F (F_OP) }, { F (F_GRI) }, { F (F_D12) }, { 0 } }
};

static const CGEN_IFMT ifmt_lddfi ATTRIBUTE_UNUSED = {
  32, 32, 0x1fc0000, { { F (F_PACK) }, { F (F_FRK) }, { F (F_OP) }, { F (F_GRI) }, { F (F_D12) }, { 0 } }
};

static const CGEN_IFMT ifmt_cldbf ATTRIBUTE_UNUSED = {
  32, 32, 0x1fc00c0, { { F (F_PACK) }, { F (F_FRK) }, { F (F_OP) }, { F (F_GRI) }, { F (F_CCI) }, { F (F_COND) }, { F (F_OPE4) }, { F (F_GRJ) }, { 0 } }
};

static const CGEN_IFMT ifmt_clddf ATTRIBUTE_UNUSED = {
  32, 32, 0x1fc00c0, { { F (F_PACK) }, { F (F_FRK) }, { F (F_OP) }, { F (F_GRI) }, { F (F_CCI) }, { F (F_COND) }, { F (F_OPE4) }, { F (F_GRJ) }, { 0 } }
};

static const CGEN_IFMT ifmt_movgf ATTRIBUTE_UNUSED = {
  32, 32, 0x1ffffc0, { { F (F_PACK) }, { F (F_FRK) }, { F (F_OP) }, { F (F_RS_NULL) }, { F (F_OPE1) }, { F (F_GRJ) }, { 0 } }
};

static const CGEN_IFMT ifmt_cmovgf ATTRIBUTE_UNUSED = {
  32, 32, 0x1fff0c0, { { F (F_PACK) }, { F (F_FRK) }, { F (F_OP) }, { F (F_RS_NULL) }, { F (F_CCI) }, { F (F_COND) }, { F (F_OPE4) }, { F (F_GRJ) }, { 0 } }
};

static const CGEN_IFMT ifmt_movgs ATTRIBUTE_UNUSED = {
  32, 32, 0x1fc0fc0, { { F (F_PACK) }, { F (F_OP) }, { F (F_SPR) }, { F (F_OPE1) }, { F (F_GRJ) }, { 0 } }
};

static const CGEN_IFMT ifmt_bra ATTRIBUTE_UNUSED = {
  32, 32, 0x7ffc0000, { { F (F_PACK) }, { F (F_INT_CC) }, { F (F_ICCI_2_NULL) }, { F (F_OP) }, { F (F_HINT) }, { F (F_LABEL16) }, { 0 } }
};

static const CGEN_IFMT ifmt_bno ATTRIBUTE_UNUSED = {
  32, 32, 0x7ffcffff, { { F (F_PACK) }, { F (F_INT_CC) }, { F (F_ICCI_2_NULL) }, { F (F_OP) }, { F (F_HINT) }, { F (F_LABEL16_NULL) }, { 0 } }
};

static const CGEN_IFMT ifmt_beq ATTRIBUTE_UNUSED = {
  32, 32, 0x79fc0000, { { F (F_PACK) }, { F (F_INT_CC) }, { F (F_ICCI_2) }, { F (F_OP) }, { F (F_HINT) }, { F (F_LABEL16) }, { 0 } }
};

static const CGEN_IFMT ifmt_fbra ATTRIBUTE_UNUSED = {
  32, 32, 0x7ffc0000, { { F (F_PACK) }, { F (F_FLT_CC) }, { F (F_FCCI_2_NULL) }, { F (F_OP) }, { F (F_HINT) }, { F (F_LABEL16) }, { 0 } }
};

static const CGEN_IFMT ifmt_fbno ATTRIBUTE_UNUSED = {
  32, 32, 0x7ffcffff, { { F (F_PACK) }, { F (F_FLT_CC) }, { F (F_FCCI_2_NULL) }, { F (F_OP) }, { F (F_HINT) }, { F (F_LABEL16_NULL) }, { 0 } }
};

static const CGEN_IFMT ifmt_fbne ATTRIBUTE_UNUSED = {
  32, 32, 0x79fc0000, { { F (F_PACK) }, { F (F_FLT_CC) }, { F (F_FCCI_2) }, { F (F_OP) }, { F (F_HINT) }, { F (F_LABEL16) }, { 0 } }
};

static const CGEN_IFMT ifmt_bctrlr ATTRIBUTE_UNUSED = {
  32, 32, 0x7ffcefff, { { F (F_PACK) }, { F (F_COND_NULL) }, { F (F_ICCI_2_NULL) }, { F (F_OP) }, { F (F_HINT) }, { F (F_OPE3) }, { F (F_CCOND) }, { F (F_S12_NULL) }, { 0 } }
};

static const CGEN_IFMT ifmt_bralr ATTRIBUTE_UNUSED = {
  32, 32, 0x7ffcffff, { { F (F_PACK) }, { F (F_INT_CC) }, { F (F_ICCI_2_NULL) }, { F (F_OP) }, { F (F_HINT) }, { F (F_OPE3) }, { F (F_CCOND_NULL) }, { F (F_S12_NULL) }, { 0 } }
};

static const CGEN_IFMT ifmt_bnolr ATTRIBUTE_UNUSED = {
  32, 32, 0x7ffcffff, { { F (F_PACK) }, { F (F_INT_CC) }, { F (F_ICCI_2_NULL) }, { F (F_OP) }, { F (F_HINT) }, { F (F_OPE3) }, { F (F_CCOND_NULL) }, { F (F_S12_NULL) }, { 0 } }
};

static const CGEN_IFMT ifmt_beqlr ATTRIBUTE_UNUSED = {
  32, 32, 0x79fcffff, { { F (F_PACK) }, { F (F_INT_CC) }, { F (F_ICCI_2) }, { F (F_OP) }, { F (F_HINT) }, { F (F_OPE3) }, { F (F_CCOND_NULL) }, { F (F_S12_NULL) }, { 0 } }
};

static const CGEN_IFMT ifmt_fbralr ATTRIBUTE_UNUSED = {
  32, 32, 0x7ffcffff, { { F (F_PACK) }, { F (F_FLT_CC) }, { F (F_FCCI_2_NULL) }, { F (F_OP) }, { F (F_HINT) }, { F (F_OPE3) }, { F (F_CCOND_NULL) }, { F (F_S12_NULL) }, { 0 } }
};

static const CGEN_IFMT ifmt_fbnolr ATTRIBUTE_UNUSED = {
  32, 32, 0x7ffcffff, { { F (F_PACK) }, { F (F_FLT_CC) }, { F (F_FCCI_2_NULL) }, { F (F_OP) }, { F (F_HINT) }, { F (F_OPE3) }, { F (F_CCOND_NULL) }, { F (F_S12_NULL) }, { 0 } }
};

static const CGEN_IFMT ifmt_fbeqlr ATTRIBUTE_UNUSED = {
  32, 32, 0x79fcffff, { { F (F_PACK) }, { F (F_FLT_CC) }, { F (F_FCCI_2) }, { F (F_OP) }, { F (F_HINT) }, { F (F_OPE3) }, { F (F_CCOND_NULL) }, { F (F_S12_NULL) }, { 0 } }
};

static const CGEN_IFMT ifmt_bcralr ATTRIBUTE_UNUSED = {
  32, 32, 0x7ffcefff, { { F (F_PACK) }, { F (F_INT_CC) }, { F (F_ICCI_2_NULL) }, { F (F_OP) }, { F (F_HINT) }, { F (F_OPE3) }, { F (F_CCOND) }, { F (F_S12_NULL) }, { 0 } }
};

static const CGEN_IFMT ifmt_bceqlr ATTRIBUTE_UNUSED = {
  32, 32, 0x79fcefff, { { F (F_PACK) }, { F (F_INT_CC) }, { F (F_ICCI_2) }, { F (F_OP) }, { F (F_HINT) }, { F (F_OPE3) }, { F (F_CCOND) }, { F (F_S12_NULL) }, { 0 } }
};

static const CGEN_IFMT ifmt_fcbralr ATTRIBUTE_UNUSED = {
  32, 32, 0x7ffcefff, { { F (F_PACK) }, { F (F_FLT_CC) }, { F (F_FCCI_2_NULL) }, { F (F_OP) }, { F (F_HINT) }, { F (F_OPE3) }, { F (F_CCOND) }, { F (F_S12_NULL) }, { 0 } }
};

static const CGEN_IFMT ifmt_fcbeqlr ATTRIBUTE_UNUSED = {
  32, 32, 0x79fcefff, { { F (F_PACK) }, { F (F_FLT_CC) }, { F (F_FCCI_2) }, { F (F_OP) }, { F (F_HINT) }, { F (F_OPE3) }, { F (F_CCOND) }, { F (F_S12_NULL) }, { 0 } }
};

static const CGEN_IFMT ifmt_jmpl ATTRIBUTE_UNUSED = {
  32, 32, 0x7ffc0fc0, { { F (F_PACK) }, { F (F_MISC_NULL_1) }, { F (F_LI_OFF) }, { F (F_OP) }, { F (F_GRI) }, { F (F_MISC_NULL_2) }, { F (F_GRJ) }, { 0 } }
};

static const CGEN_IFMT ifmt_calll ATTRIBUTE_UNUSED = {
  32, 32, 0x7ffc0fc0, { { F (F_PACK) }, { F (F_MISC_NULL_1) }, { F (F_LI_ON) }, { F (F_OP) }, { F (F_GRI) }, { F (F_MISC_NULL_2) }, { F (F_GRJ) }, { 0 } }
};

static const CGEN_IFMT ifmt_jmpil ATTRIBUTE_UNUSED = {
  32, 32, 0x7ffc0000, { { F (F_PACK) }, { F (F_MISC_NULL_1) }, { F (F_LI_OFF) }, { F (F_OP) }, { F (F_GRI) }, { F (F_D12) }, { 0 } }
};

static const CGEN_IFMT ifmt_callil ATTRIBUTE_UNUSED = {
  32, 32, 0x7ffc0000, { { F (F_PACK) }, { F (F_MISC_NULL_1) }, { F (F_LI_ON) }, { F (F_OP) }, { F (F_GRI) }, { F (F_D12) }, { 0 } }
};

static const CGEN_IFMT ifmt_call ATTRIBUTE_UNUSED = {
  32, 32, 0x1fc0000, { { F (F_PACK) }, { F (F_OP) }, { F (F_LABEL24) }, { 0 } }
};

static const CGEN_IFMT ifmt_rett ATTRIBUTE_UNUSED = {
  32, 32, 0x7dffffff, { { F (F_PACK) }, { F (F_MISC_NULL_1) }, { F (F_DEBUG) }, { F (F_OP) }, { F (F_RS_NULL) }, { F (F_S12_NULL) }, { 0 } }
};

static const CGEN_IFMT ifmt_rei ATTRIBUTE_UNUSED = {
  32, 32, 0x7ffc0fff, { { F (F_PACK) }, { F (F_RD_NULL) }, { F (F_OP) }, { F (F_EIR) }, { F (F_S12_NULL) }, { 0 } }
};

static const CGEN_IFMT ifmt_tra ATTRIBUTE_UNUSED = {
  32, 32, 0x7ffc0fc0, { { F (F_PACK) }, { F (F_INT_CC) }, { F (F_ICCI_2_NULL) }, { F (F_OP) }, { F (F_GRI) }, { F (F_MISC_NULL_3) }, { F (F_OPE4) }, { F (F_GRJ) }, { 0 } }
};

static const CGEN_IFMT ifmt_tno ATTRIBUTE_UNUSED = {
  32, 32, 0x7fffffff, { { F (F_PACK) }, { F (F_INT_CC) }, { F (F_ICCI_2_NULL) }, { F (F_OP) }, { F (F_GRI_NULL) }, { F (F_MISC_NULL_3) }, { F (F_OPE4) }, { F (F_GRJ_NULL) }, { 0 } }
};

static const CGEN_IFMT ifmt_teq ATTRIBUTE_UNUSED = {
  32, 32, 0x79fc0fc0, { { F (F_PACK) }, { F (F_INT_CC) }, { F (F_ICCI_2) }, { F (F_OP) }, { F (F_GRI) }, { F (F_MISC_NULL_3) }, { F (F_OPE4) }, { F (F_GRJ) }, { 0 } }
};

static const CGEN_IFMT ifmt_ftra ATTRIBUTE_UNUSED = {
  32, 32, 0x7ffc0fc0, { { F (F_PACK) }, { F (F_FLT_CC) }, { F (F_FCCI_2_NULL) }, { F (F_OP) }, { F (F_GRI) }, { F (F_MISC_NULL_3) }, { F (F_OPE4) }, { F (F_GRJ) }, { 0 } }
};

static const CGEN_IFMT ifmt_ftno ATTRIBUTE_UNUSED = {
  32, 32, 0x7fffffff, { { F (F_PACK) }, { F (F_FLT_CC) }, { F (F_FCCI_2_NULL) }, { F (F_OP) }, { F (F_GRI_NULL) }, { F (F_MISC_NULL_3) }, { F (F_OPE4) }, { F (F_GRJ_NULL) }, { 0 } }
};

static const CGEN_IFMT ifmt_ftne ATTRIBUTE_UNUSED = {
  32, 32, 0x79fc0fc0, { { F (F_PACK) }, { F (F_FLT_CC) }, { F (F_FCCI_2) }, { F (F_OP) }, { F (F_GRI) }, { F (F_MISC_NULL_3) }, { F (F_OPE4) }, { F (F_GRJ) }, { 0 } }
};

static const CGEN_IFMT ifmt_tira ATTRIBUTE_UNUSED = {
  32, 32, 0x7ffc0000, { { F (F_PACK) }, { F (F_INT_CC) }, { F (F_ICCI_2_NULL) }, { F (F_OP) }, { F (F_GRI) }, { F (F_D12) }, { 0 } }
};

static const CGEN_IFMT ifmt_tino ATTRIBUTE_UNUSED = {
  32, 32, 0x7fffffff, { { F (F_PACK) }, { F (F_INT_CC) }, { F (F_ICCI_2_NULL) }, { F (F_OP) }, { F (F_GRI_NULL) }, { F (F_S12_NULL) }, { 0 } }
};

static const CGEN_IFMT ifmt_tieq ATTRIBUTE_UNUSED = {
  32, 32, 0x79fc0000, { { F (F_PACK) }, { F (F_INT_CC) }, { F (F_ICCI_2) }, { F (F_OP) }, { F (F_GRI) }, { F (F_D12) }, { 0 } }
};

static const CGEN_IFMT ifmt_ftira ATTRIBUTE_UNUSED = {
  32, 32, 0x7ffc0000, { { F (F_PACK) }, { F (F_FLT_CC) }, { F (F_ICCI_2_NULL) }, { F (F_OP) }, { F (F_GRI) }, { F (F_D12) }, { 0 } }
};

static const CGEN_IFMT ifmt_ftino ATTRIBUTE_UNUSED = {
  32, 32, 0x7fffffff, { { F (F_PACK) }, { F (F_FLT_CC) }, { F (F_FCCI_2_NULL) }, { F (F_OP) }, { F (F_GRI_NULL) }, { F (F_S12_NULL) }, { 0 } }
};

static const CGEN_IFMT ifmt_ftine ATTRIBUTE_UNUSED = {
  32, 32, 0x79fc0000, { { F (F_PACK) }, { F (F_FLT_CC) }, { F (F_FCCI_2) }, { F (F_OP) }, { F (F_GRI) }, { F (F_D12) }, { 0 } }
};

static const CGEN_IFMT ifmt_break ATTRIBUTE_UNUSED = {
  32, 32, 0x7fffffff, { { F (F_PACK) }, { F (F_RD_NULL) }, { F (F_OP) }, { F (F_RS_NULL) }, { F (F_MISC_NULL_3) }, { F (F_OPE4) }, { F (F_GRJ_NULL) }, { 0 } }
};

static const CGEN_IFMT ifmt_andcr ATTRIBUTE_UNUSED = {
  32, 32, 0x71ff8ff8, { { F (F_PACK) }, { F (F_MISC_NULL_6) }, { F (F_CRK) }, { F (F_OP) }, { F (F_MISC_NULL_7) }, { F (F_CRI) }, { F (F_OPE1) }, { F (F_MISC_NULL_8) }, { F (F_CRJ) }, { 0 } }
};

static const CGEN_IFMT ifmt_notcr ATTRIBUTE_UNUSED = {
  32, 32, 0x71fffff8, { { F (F_PACK) }, { F (F_MISC_NULL_6) }, { F (F_CRK) }, { F (F_OP) }, { F (F_RS_NULL) }, { F (F_OPE1) }, { F (F_MISC_NULL_8) }, { F (F_CRJ) }, { 0 } }
};

static const CGEN_IFMT ifmt_ckra ATTRIBUTE_UNUSED = {
  32, 32, 0x79ffffff, { { F (F_PACK) }, { F (F_INT_CC) }, { F (F_CRJ_INT) }, { F (F_OP) }, { F (F_MISC_NULL_5) }, { F (F_ICCI_3_NULL) }, { 0 } }
};

static const CGEN_IFMT ifmt_ckeq ATTRIBUTE_UNUSED = {
  32, 32, 0x79fffffc, { { F (F_PACK) }, { F (F_INT_CC) }, { F (F_CRJ_INT) }, { F (F_OP) }, { F (F_MISC_NULL_5) }, { F (F_ICCI_3) }, { 0 } }
};

static const CGEN_IFMT ifmt_fckra ATTRIBUTE_UNUSED = {
  32, 32, 0x79fffffc, { { F (F_PACK) }, { F (F_FLT_CC) }, { F (F_CRJ_FLOAT) }, { F (F_OP) }, { F (F_MISC_NULL_5) }, { F (F_FCCI_3) }, { 0 } }
};

static const CGEN_IFMT ifmt_cckra ATTRIBUTE_UNUSED = {
  32, 32, 0x79fff0ff, { { F (F_PACK) }, { F (F_INT_CC) }, { F (F_CRJ_INT) }, { F (F_OP) }, { F (F_RS_NULL) }, { F (F_CCI) }, { F (F_COND) }, { F (F_OPE4) }, { F (F_MISC_NULL_9) }, { F (F_ICCI_3_NULL) }, { 0 } }
};

static const CGEN_IFMT ifmt_cckeq ATTRIBUTE_UNUSED = {
  32, 32, 0x79fff0fc, { { F (F_PACK) }, { F (F_INT_CC) }, { F (F_CRJ_INT) }, { F (F_OP) }, { F (F_RS_NULL) }, { F (F_CCI) }, { F (F_COND) }, { F (F_OPE4) }, { F (F_MISC_NULL_9) }, { F (F_ICCI_3) }, { 0 } }
};

static const CGEN_IFMT ifmt_cfckra ATTRIBUTE_UNUSED = {
  32, 32, 0x79fff0ff, { { F (F_PACK) }, { F (F_FLT_CC) }, { F (F_CRJ_FLOAT) }, { F (F_OP) }, { F (F_RS_NULL) }, { F (F_CCI) }, { F (F_COND) }, { F (F_OPE4) }, { F (F_MISC_NULL_9) }, { F (F_FCCI_3_NULL) }, { 0 } }
};

static const CGEN_IFMT ifmt_cfckne ATTRIBUTE_UNUSED = {
  32, 32, 0x79fff0fc, { { F (F_PACK) }, { F (F_FLT_CC) }, { F (F_CRJ_FLOAT) }, { F (F_OP) }, { F (F_RS_NULL) }, { F (F_CCI) }, { F (F_COND) }, { F (F_OPE4) }, { F (F_MISC_NULL_9) }, { F (F_FCCI_3) }, { 0 } }
};

static const CGEN_IFMT ifmt_cjmpl ATTRIBUTE_UNUSED = {
  32, 32, 0x7ffc00c0, { { F (F_PACK) }, { F (F_MISC_NULL_1) }, { F (F_LI_OFF) }, { F (F_OP) }, { F (F_GRI) }, { F (F_CCI) }, { F (F_COND) }, { F (F_OPE4) }, { F (F_GRJ) }, { 0 } }
};

static const CGEN_IFMT ifmt_ccalll ATTRIBUTE_UNUSED = {
  32, 32, 0x7ffc00c0, { { F (F_PACK) }, { F (F_MISC_NULL_1) }, { F (F_LI_ON) }, { F (F_OP) }, { F (F_GRI) }, { F (F_CCI) }, { F (F_COND) }, { F (F_OPE4) }, { F (F_GRJ) }, { 0 } }
};

static const CGEN_IFMT ifmt_icei ATTRIBUTE_UNUSED = {
  32, 32, 0x7dfc0fc0, { { F (F_PACK) }, { F (F_MISC_NULL_1) }, { F (F_AE) }, { F (F_OP) }, { F (F_GRI) }, { F (F_OPE1) }, { F (F_GRJ) }, { 0 } }
};

static const CGEN_IFMT ifmt_icpl ATTRIBUTE_UNUSED = {
  32, 32, 0x7dfc0fc0, { { F (F_PACK) }, { F (F_MISC_NULL_1) }, { F (F_LOCK) }, { F (F_OP) }, { F (F_GRI) }, { F (F_OPE1) }, { F (F_GRJ) }, { 0 } }
};

static const CGEN_IFMT ifmt_icul ATTRIBUTE_UNUSED = {
  32, 32, 0x7ffc0fff, { { F (F_PACK) }, { F (F_RD_NULL) }, { F (F_OP) }, { F (F_GRI) }, { F (F_OPE1) }, { F (F_GRJ_NULL) }, { 0 } }
};

static const CGEN_IFMT ifmt_bar ATTRIBUTE_UNUSED = {
  32, 32, 0x7fffffff, { { F (F_PACK) }, { F (F_RD_NULL) }, { F (F_OP) }, { F (F_RS_NULL) }, { F (F_OPE1) }, { F (F_GRJ_NULL) }, { 0 } }
};

static const CGEN_IFMT ifmt_lrai ATTRIBUTE_UNUSED = {
  32, 32, 0x1fc0fc7, { { F (F_PACK) }, { F (F_GRK) }, { F (F_OP) }, { F (F_GRI) }, { F (F_OPE1) }, { F (F_LRAE) }, { F (F_LRAD) }, { F (F_LRAS) }, { F (F_LRA_NULL) }, { 0 } }
};

static const CGEN_IFMT ifmt_tlbpr ATTRIBUTE_UNUSED = {
  32, 32, 0x61fc0fc0, { { F (F_PACK) }, { F (F_TLBPR_NULL) }, { F (F_TLBPROPX) }, { F (F_TLBPRL) }, { F (F_OP) }, { F (F_GRI) }, { F (F_OPE1) }, { F (F_GRJ) }, { 0 } }
};

static const CGEN_IFMT ifmt_cop1 ATTRIBUTE_UNUSED = {
  32, 32, 0x1fc0000, { { F (F_PACK) }, { F (F_CPRK) }, { F (F_OP) }, { F (F_CPRI) }, { F (F_S6_1) }, { F (F_CPRJ) }, { 0 } }
};

static const CGEN_IFMT ifmt_clrgr ATTRIBUTE_UNUSED = {
  32, 32, 0x1ffffff, { { F (F_PACK) }, { F (F_GRK) }, { F (F_OP) }, { F (F_RS_NULL) }, { F (F_OPE1) }, { F (F_GRJ_NULL) }, { 0 } }
};

static const CGEN_IFMT ifmt_clrfr ATTRIBUTE_UNUSED = {
  32, 32, 0x1ffffff, { { F (F_PACK) }, { F (F_FRK) }, { F (F_OP) }, { F (F_RS_NULL) }, { F (F_OPE1) }, { F (F_GRJ_NULL) }, { 0 } }
};

static const CGEN_IFMT ifmt_fitos ATTRIBUTE_UNUSED = {
  32, 32, 0x1ffffc0, { { F (F_PACK) }, { F (F_FRK) }, { F (F_OP) }, { F (F_RS_NULL) }, { F (F_OPE1) }, { F (F_FRJ) }, { 0 } }
};

static const CGEN_IFMT ifmt_fstoi ATTRIBUTE_UNUSED = {
  32, 32, 0x1ffffc0, { { F (F_PACK) }, { F (F_FRK) }, { F (F_OP) }, { F (F_RS_NULL) }, { F (F_OPE1) }, { F (F_FRJ) }, { 0 } }
};

static const CGEN_IFMT ifmt_fitod ATTRIBUTE_UNUSED = {
  32, 32, 0x1ffffc0, { { F (F_PACK) }, { F (F_FRK) }, { F (F_OP) }, { F (F_RS_NULL) }, { F (F_OPE1) }, { F (F_FRJ) }, { 0 } }
};

static const CGEN_IFMT ifmt_fdtoi ATTRIBUTE_UNUSED = {
  32, 32, 0x1ffffc0, { { F (F_PACK) }, { F (F_FRK) }, { F (F_OP) }, { F (F_RS_NULL) }, { F (F_OPE1) }, { F (F_FRJ) }, { 0 } }
};

static const CGEN_IFMT ifmt_cfitos ATTRIBUTE_UNUSED = {
  32, 32, 0x1fff0c0, { { F (F_PACK) }, { F (F_FRK) }, { F (F_OP) }, { F (F_RS_NULL) }, { F (F_CCI) }, { F (F_COND) }, { F (F_OPE4) }, { F (F_FRJ) }, { 0 } }
};

static const CGEN_IFMT ifmt_cfstoi ATTRIBUTE_UNUSED = {
  32, 32, 0x1fff0c0, { { F (F_PACK) }, { F (F_FRK) }, { F (F_OP) }, { F (F_RS_NULL) }, { F (F_CCI) }, { F (F_COND) }, { F (F_OPE4) }, { F (F_FRJ) }, { 0 } }
};

static const CGEN_IFMT ifmt_fmovs ATTRIBUTE_UNUSED = {
  32, 32, 0x1ffffc0, { { F (F_PACK) }, { F (F_FRK) }, { F (F_OP) }, { F (F_RS_NULL) }, { F (F_OPE1) }, { F (F_FRJ) }, { 0 } }
};

static const CGEN_IFMT ifmt_fmovd ATTRIBUTE_UNUSED = {
  32, 32, 0x1ffffc0, { { F (F_PACK) }, { F (F_FRK) }, { F (F_OP) }, { F (F_RS_NULL) }, { F (F_OPE1) }, { F (F_FRJ) }, { 0 } }
};

static const CGEN_IFMT ifmt_cfmovs ATTRIBUTE_UNUSED = {
  32, 32, 0x1fff0c0, { { F (F_PACK) }, { F (F_FRK) }, { F (F_OP) }, { F (F_RS_NULL) }, { F (F_CCI) }, { F (F_COND) }, { F (F_OPE4) }, { F (F_FRJ) }, { 0 } }
};

static const CGEN_IFMT ifmt_fadds ATTRIBUTE_UNUSED = {
  32, 32, 0x1fc0fc0, { { F (F_PACK) }, { F (F_FRK) }, { F (F_OP) }, { F (F_FRI) }, { F (F_OPE1) }, { F (F_FRJ) }, { 0 } }
};

static const CGEN_IFMT ifmt_faddd ATTRIBUTE_UNUSED = {
  32, 32, 0x1fc0fc0, { { F (F_PACK) }, { F (F_FRK) }, { F (F_OP) }, { F (F_FRI) }, { F (F_OPE1) }, { F (F_FRJ) }, { 0 } }
};

static const CGEN_IFMT ifmt_cfadds ATTRIBUTE_UNUSED = {
  32, 32, 0x1fc00c0, { { F (F_PACK) }, { F (F_FRK) }, { F (F_OP) }, { F (F_FRI) }, { F (F_CCI) }, { F (F_COND) }, { F (F_OPE4) }, { F (F_FRJ) }, { 0 } }
};

static const CGEN_IFMT ifmt_fcmps ATTRIBUTE_UNUSED = {
  32, 32, 0x79fc0fc0, { { F (F_PACK) }, { F (F_COND_NULL) }, { F (F_FCCI_2) }, { F (F_OP) }, { F (F_FRI) }, { F (F_OPE1) }, { F (F_FRJ) }, { 0 } }
};

static const CGEN_IFMT ifmt_fcmpd ATTRIBUTE_UNUSED = {
  32, 32, 0x79fc0fc0, { { F (F_PACK) }, { F (F_COND_NULL) }, { F (F_FCCI_2) }, { F (F_OP) }, { F (F_FRI) }, { F (F_OPE1) }, { F (F_FRJ) }, { 0 } }
};

static const CGEN_IFMT ifmt_cfcmps ATTRIBUTE_UNUSED = {
  32, 32, 0x79fc00c0, { { F (F_PACK) }, { F (F_COND_NULL) }, { F (F_FCCI_2) }, { F (F_OP) }, { F (F_FRI) }, { F (F_CCI) }, { F (F_COND) }, { F (F_OPE4) }, { F (F_FRJ) }, { 0 } }
};

static const CGEN_IFMT ifmt_mhsetlos ATTRIBUTE_UNUSED = {
  32, 32, 0x1fc0fc0, { { F (F_PACK) }, { F (F_FRK) }, { F (F_OP) }, { F (F_OPE1) }, { F (F_U12) }, { 0 } }
};

static const CGEN_IFMT ifmt_mhsethis ATTRIBUTE_UNUSED = {
  32, 32, 0x1fc0fc0, { { F (F_PACK) }, { F (F_FRK) }, { F (F_OP) }, { F (F_OPE1) }, { F (F_U12) }, { 0 } }
};

static const CGEN_IFMT ifmt_mhdsets ATTRIBUTE_UNUSED = {
  32, 32, 0x1fc0fc0, { { F (F_PACK) }, { F (F_FRK) }, { F (F_OP) }, { F (F_OPE1) }, { F (F_U12) }, { 0 } }
};

static const CGEN_IFMT ifmt_mhsetloh ATTRIBUTE_UNUSED = {
  32, 32, 0x1ffffe0, { { F (F_PACK) }, { F (F_FRK) }, { F (F_OP) }, { F (F_FRI_NULL) }, { F (F_OPE1) }, { F (F_MISC_NULL_11) }, { F (F_S5) }, { 0 } }
};

static const CGEN_IFMT ifmt_mhsethih ATTRIBUTE_UNUSED = {
  32, 32, 0x1ffffe0, { { F (F_PACK) }, { F (F_FRK) }, { F (F_OP) }, { F (F_FRI_NULL) }, { F (F_OPE1) }, { F (F_MISC_NULL_11) }, { F (F_S5) }, { 0 } }
};

static const CGEN_IFMT ifmt_mhdseth ATTRIBUTE_UNUSED = {
  32, 32, 0x1ffffe0, { { F (F_PACK) }, { F (F_FRK) }, { F (F_OP) }, { F (F_FRI_NULL) }, { F (F_OPE1) }, { F (F_MISC_NULL_11) }, { F (F_S5) }, { 0 } }
};

static const CGEN_IFMT ifmt_mand ATTRIBUTE_UNUSED = {
  32, 32, 0x1fc0fc0, { { F (F_PACK) }, { F (F_FRK) }, { F (F_OP) }, { F (F_FRI) }, { F (F_OPE1) }, { F (F_FRJ) }, { 0 } }
};

static const CGEN_IFMT ifmt_cmand ATTRIBUTE_UNUSED = {
  32, 32, 0x1fc00c0, { { F (F_PACK) }, { F (F_FRK) }, { F (F_OP) }, { F (F_FRI) }, { F (F_CCI) }, { F (F_COND) }, { F (F_OPE4) }, { F (F_FRJ) }, { 0 } }
};

static const CGEN_IFMT ifmt_mnot ATTRIBUTE_UNUSED = {
  32, 32, 0x1ffffc0, { { F (F_PACK) }, { F (F_FRK) }, { F (F_OP) }, { F (F_RS_NULL) }, { F (F_OPE1) }, { F (F_FRJ) }, { 0 } }
};

static const CGEN_IFMT ifmt_cmnot ATTRIBUTE_UNUSED = {
  32, 32, 0x1fff0c0, { { F (F_PACK) }, { F (F_FRK) }, { F (F_OP) }, { F (F_RS_NULL) }, { F (F_CCI) }, { F (F_COND) }, { F (F_OPE4) }, { F (F_FRJ) }, { 0 } }
};

static const CGEN_IFMT ifmt_mrotli ATTRIBUTE_UNUSED = {
  32, 32, 0x1fc0fc0, { { F (F_PACK) }, { F (F_FRK) }, { F (F_OP) }, { F (F_FRI) }, { F (F_OPE1) }, { F (F_U6) }, { 0 } }
};

static const CGEN_IFMT ifmt_mcut ATTRIBUTE_UNUSED = {
  32, 32, 0x1fc0fc0, { { F (F_PACK) }, { F (F_FRK) }, { F (F_OP) }, { F (F_ACC40SI) }, { F (F_OPE1) }, { F (F_FRJ) }, { 0 } }
};

static const CGEN_IFMT ifmt_mcuti ATTRIBUTE_UNUSED = {
  32, 32, 0x1fc0fc0, { { F (F_PACK) }, { F (F_FRK) }, { F (F_OP) }, { F (F_ACC40SI) }, { F (F_OPE1) }, { F (F_S6) }, { 0 } }
};

static const CGEN_IFMT ifmt_mdcutssi ATTRIBUTE_UNUSED = {
  32, 32, 0x1fc0fc0, { { F (F_PACK) }, { F (F_FRK) }, { F (F_OP) }, { F (F_ACC40SI) }, { F (F_OPE1) }, { F (F_S6) }, { 0 } }
};

static const CGEN_IFMT ifmt_mdrotli ATTRIBUTE_UNUSED = {
  32, 32, 0x1fc0fc0, { { F (F_PACK) }, { F (F_FRK) }, { F (F_OP) }, { F (F_FRI) }, { F (F_OPE1) }, { F (F_S6) }, { 0 } }
};

static const CGEN_IFMT ifmt_mqsaths ATTRIBUTE_UNUSED = {
  32, 32, 0x1fc0fc0, { { F (F_PACK) }, { F (F_FRK) }, { F (F_OP) }, { F (F_FRI) }, { F (F_OPE1) }, { F (F_FRJ) }, { 0 } }
};

static const CGEN_IFMT ifmt_mcmpsh ATTRIBUTE_UNUSED = {
  32, 32, 0x79fc0fc0, { { F (F_PACK) }, { F (F_COND_NULL) }, { F (F_FCCK) }, { F (F_OP) }, { F (F_FRI) }, { F (F_OPE1) }, { F (F_FRJ) }, { 0 } }
};

static const CGEN_IFMT ifmt_mabshs ATTRIBUTE_UNUSED = {
  32, 32, 0x1ffffc0, { { F (F_PACK) }, { F (F_FRK) }, { F (F_OP) }, { F (F_FRI_NULL) }, { F (F_OPE1) }, { F (F_FRJ) }, { 0 } }
};

static const CGEN_IFMT ifmt_cmqaddhss ATTRIBUTE_UNUSED = {
  32, 32, 0x1fc00c0, { { F (F_PACK) }, { F (F_FRK) }, { F (F_OP) }, { F (F_FRI) }, { F (F_CCI) }, { F (F_COND) }, { F (F_OPE4) }, { F (F_FRJ) }, { 0 } }
};

static const CGEN_IFMT ifmt_mqsllhi ATTRIBUTE_UNUSED = {
  32, 32, 0x1fc0fc0, { { F (F_PACK) }, { F (F_FRK) }, { F (F_OP) }, { F (F_FRI) }, { F (F_OPE1) }, { F (F_U6) }, { 0 } }
};

static const CGEN_IFMT ifmt_maddaccs ATTRIBUTE_UNUSED = {
  32, 32, 0x1fc0fff, { { F (F_PACK) }, { F (F_ACC40SK) }, { F (F_OP) }, { F (F_ACC40SI) }, { F (F_OPE1) }, { F (F_ACCJ_NULL) }, { 0 } }
};

static const CGEN_IFMT ifmt_mmulhs ATTRIBUTE_UNUSED = {
  32, 32, 0x1fc0fc0, { { F (F_PACK) }, { F (F_ACC40SK) }, { F (F_OP) }, { F (F_FRI) }, { F (F_OPE1) }, { F (F_FRJ) }, { 0 } }
};

static const CGEN_IFMT ifmt_cmmulhs ATTRIBUTE_UNUSED = {
  32, 32, 0x1fc00c0, { { F (F_PACK) }, { F (F_ACC40SK) }, { F (F_OP) }, { F (F_FRI) }, { F (F_CCI) }, { F (F_COND) }, { F (F_OPE4) }, { F (F_FRJ) }, { 0 } }
};

static const CGEN_IFMT ifmt_mqmulhs ATTRIBUTE_UNUSED = {
  32, 32, 0x1fc0fc0, { { F (F_PACK) }, { F (F_ACC40SK) }, { F (F_OP) }, { F (F_FRI) }, { F (F_OPE1) }, { F (F_FRJ) }, { 0 } }
};

static const CGEN_IFMT ifmt_cmqmulhs ATTRIBUTE_UNUSED = {
  32, 32, 0x1fc00c0, { { F (F_PACK) }, { F (F_ACC40SK) }, { F (F_OP) }, { F (F_FRI) }, { F (F_CCI) }, { F (F_COND) }, { F (F_OPE4) }, { F (F_FRJ) }, { 0 } }
};

static const CGEN_IFMT ifmt_mmachu ATTRIBUTE_UNUSED = {
  32, 32, 0x1fc0fc0, { { F (F_PACK) }, { F (F_ACC40UK) }, { F (F_OP) }, { F (F_FRI) }, { F (F_OPE1) }, { F (F_FRJ) }, { 0 } }
};

static const CGEN_IFMT ifmt_cmmachu ATTRIBUTE_UNUSED = {
  32, 32, 0x1fc00c0, { { F (F_PACK) }, { F (F_ACC40UK) }, { F (F_OP) }, { F (F_FRI) }, { F (F_CCI) }, { F (F_COND) }, { F (F_OPE4) }, { F (F_FRJ) }, { 0 } }
};

static const CGEN_IFMT ifmt_mqmachu ATTRIBUTE_UNUSED = {
  32, 32, 0x1fc0fc0, { { F (F_PACK) }, { F (F_ACC40UK) }, { F (F_OP) }, { F (F_FRI) }, { F (F_OPE1) }, { F (F_FRJ) }, { 0 } }
};

static const CGEN_IFMT ifmt_cmqmachu ATTRIBUTE_UNUSED = {
  32, 32, 0x1fc00c0, { { F (F_PACK) }, { F (F_ACC40UK) }, { F (F_OP) }, { F (F_FRI) }, { F (F_CCI) }, { F (F_COND) }, { F (F_OPE4) }, { F (F_FRJ) }, { 0 } }
};

static const CGEN_IFMT ifmt_cmexpdhw ATTRIBUTE_UNUSED = {
  32, 32, 0x1fc00c0, { { F (F_PACK) }, { F (F_FRK) }, { F (F_OP) }, { F (F_FRI) }, { F (F_CCI) }, { F (F_COND) }, { F (F_OPE4) }, { F (F_U6) }, { 0 } }
};

static const CGEN_IFMT ifmt_mexpdhd ATTRIBUTE_UNUSED = {
  32, 32, 0x1fc0fc0, { { F (F_PACK) }, { F (F_FRK) }, { F (F_OP) }, { F (F_FRI) }, { F (F_OPE1) }, { F (F_U6) }, { 0 } }
};

static const CGEN_IFMT ifmt_cmexpdhd ATTRIBUTE_UNUSED = {
  32, 32, 0x1fc00c0, { { F (F_PACK) }, { F (F_FRK) }, { F (F_OP) }, { F (F_FRI) }, { F (F_CCI) }, { F (F_COND) }, { F (F_OPE4) }, { F (F_U6) }, { 0 } }
};

static const CGEN_IFMT ifmt_munpackh ATTRIBUTE_UNUSED = {
  32, 32, 0x1fc0fff, { { F (F_PACK) }, { F (F_FRK) }, { F (F_OP) }, { F (F_FRI) }, { F (F_OPE1) }, { F (F_FRJ_NULL) }, { 0 } }
};

static const CGEN_IFMT ifmt_mdunpackh ATTRIBUTE_UNUSED = {
  32, 32, 0x1fc0fff, { { F (F_PACK) }, { F (F_FRK) }, { F (F_OP) }, { F (F_FRI) }, { F (F_OPE1) }, { F (F_FRJ_NULL) }, { 0 } }
};

static const CGEN_IFMT ifmt_mbtoh ATTRIBUTE_UNUSED = {
  32, 32, 0x1ffffc0, { { F (F_PACK) }, { F (F_FRK) }, { F (F_OP) }, { F (F_FRI_NULL) }, { F (F_OPE1) }, { F (F_FRJ) }, { 0 } }
};

static const CGEN_IFMT ifmt_cmbtoh ATTRIBUTE_UNUSED = {
  32, 32, 0x1fff0c0, { { F (F_PACK) }, { F (F_FRK) }, { F (F_OP) }, { F (F_FRI_NULL) }, { F (F_CCI) }, { F (F_COND) }, { F (F_OPE4) }, { F (F_FRJ) }, { 0 } }
};

static const CGEN_IFMT ifmt_mhtob ATTRIBUTE_UNUSED = {
  32, 32, 0x1ffffc0, { { F (F_PACK) }, { F (F_FRK) }, { F (F_OP) }, { F (F_FRI_NULL) }, { F (F_OPE1) }, { F (F_FRJ) }, { 0 } }
};

static const CGEN_IFMT ifmt_cmhtob ATTRIBUTE_UNUSED = {
  32, 32, 0x1fff0c0, { { F (F_PACK) }, { F (F_FRK) }, { F (F_OP) }, { F (F_FRI_NULL) }, { F (F_CCI) }, { F (F_COND) }, { F (F_OPE4) }, { F (F_FRJ) }, { 0 } }
};

static const CGEN_IFMT ifmt_cmbtohe ATTRIBUTE_UNUSED = {
  32, 32, 0x1fff0c0, { { F (F_PACK) }, { F (F_FRK) }, { F (F_OP) }, { F (F_FRI_NULL) }, { F (F_CCI) }, { F (F_COND) }, { F (F_OPE4) }, { F (F_FRJ) }, { 0 } }
};

static const CGEN_IFMT ifmt_mnop ATTRIBUTE_UNUSED = {
  32, 32, 0x7fffffff, { { F (F_PACK) }, { F (F_ACC40SK) }, { F (F_OP) }, { F (F_A) }, { F (F_MISC_NULL_10) }, { F (F_OPE1) }, { F (F_FRJ_NULL) }, { 0 } }
};

static const CGEN_IFMT ifmt_mclracc_0 ATTRIBUTE_UNUSED = {
  32, 32, 0x1ffffff, { { F (F_PACK) }, { F (F_ACC40SK) }, { F (F_OP) }, { F (F_A) }, { F (F_MISC_NULL_10) }, { F (F_OPE1) }, { F (F_FRJ_NULL) }, { 0 } }
};

static const CGEN_IFMT ifmt_mrdacc ATTRIBUTE_UNUSED = {
  32, 32, 0x1fc0fff, { { F (F_PACK) }, { F (F_FRK) }, { F (F_OP) }, { F (F_ACC40SI) }, { F (F_OPE1) }, { F (F_FRJ_NULL) }, { 0 } }
};

static const CGEN_IFMT ifmt_mrdaccg ATTRIBUTE_UNUSED = {
  32, 32, 0x1fc0fff, { { F (F_PACK) }, { F (F_FRK) }, { F (F_OP) }, { F (F_ACCGI) }, { F (F_OPE1) }, { F (F_FRJ_NULL) }, { 0 } }
};

static const CGEN_IFMT ifmt_mwtacc ATTRIBUTE_UNUSED = {
  32, 32, 0x1fc0fff, { { F (F_PACK) }, { F (F_ACC40SK) }, { F (F_OP) }, { F (F_FRI) }, { F (F_OPE1) }, { F (F_FRJ_NULL) }, { 0 } }
};

static const CGEN_IFMT ifmt_mwtaccg ATTRIBUTE_UNUSED = {
  32, 32, 0x1fc0fff, { { F (F_PACK) }, { F (F_ACCGK) }, { F (F_OP) }, { F (F_FRI) }, { F (F_OPE1) }, { F (F_FRJ_NULL) }, { 0 } }
};

static const CGEN_IFMT ifmt_fnop ATTRIBUTE_UNUSED = {
  32, 32, 0x7fffffff, { { F (F_PACK) }, { F (F_RD_NULL) }, { F (F_OP) }, { F (F_FRI_NULL) }, { F (F_OPE1) }, { F (F_FRJ_NULL) }, { 0 } }
};

#undef F

#if defined (__STDC__) || defined (ALMOST_STDC) || defined (HAVE_STRINGIZE)
#define A(a) (1 << CGEN_INSN_##a)
#else
#define A(a) (1 << CGEN_INSN_/**/a)
#endif
#if defined (__STDC__) || defined (ALMOST_STDC) || defined (HAVE_STRINGIZE)
#define OPERAND(op) FRV_OPERAND_##op
#else
#define OPERAND(op) FRV_OPERAND_/**/op
#endif
#define MNEM CGEN_SYNTAX_MNEMONIC /* syntax value for mnemonic */
#define OP(field) CGEN_SYNTAX_MAKE_FIELD (OPERAND (field))

/* The instruction table.  */

static const CGEN_OPCODE frv_cgen_insn_opcode_table[MAX_INSNS] =
{
  /* Special null first entry.
     A `num' value of zero is thus invalid.
     Also, the special `invalid' insn resides here.  */
  { { 0, 0, 0, 0 }, {{0}}, 0, {0}},
/* add$pack $GRi,$GRj,$GRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (GRI), ',', OP (GRJ), ',', OP (GRK), 0 } },
    & ifmt_add, { 0x0 }
  },
/* sub$pack $GRi,$GRj,$GRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (GRI), ',', OP (GRJ), ',', OP (GRK), 0 } },
    & ifmt_add, { 0x100 }
  },
/* and$pack $GRi,$GRj,$GRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (GRI), ',', OP (GRJ), ',', OP (GRK), 0 } },
    & ifmt_add, { 0x40000 }
  },
/* or$pack $GRi,$GRj,$GRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (GRI), ',', OP (GRJ), ',', OP (GRK), 0 } },
    & ifmt_add, { 0x40080 }
  },
/* xor$pack $GRi,$GRj,$GRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (GRI), ',', OP (GRJ), ',', OP (GRK), 0 } },
    & ifmt_add, { 0x40100 }
  },
/* not$pack $GRj,$GRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (GRJ), ',', OP (GRK), 0 } },
    & ifmt_not, { 0x40180 }
  },
/* sdiv$pack $GRi,$GRj,$GRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (GRI), ',', OP (GRJ), ',', OP (GRK), 0 } },
    & ifmt_add, { 0x380 }
  },
/* nsdiv$pack $GRi,$GRj,$GRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (GRI), ',', OP (GRJ), ',', OP (GRK), 0 } },
    & ifmt_add, { 0x40380 }
  },
/* udiv$pack $GRi,$GRj,$GRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (GRI), ',', OP (GRJ), ',', OP (GRK), 0 } },
    & ifmt_add, { 0x3c0 }
  },
/* nudiv$pack $GRi,$GRj,$GRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (GRI), ',', OP (GRJ), ',', OP (GRK), 0 } },
    & ifmt_add, { 0x403c0 }
  },
/* smul$pack $GRi,$GRj,$GRdoublek */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (GRI), ',', OP (GRJ), ',', OP (GRDOUBLEK), 0 } },
    & ifmt_smul, { 0x200 }
  },
/* umul$pack $GRi,$GRj,$GRdoublek */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (GRI), ',', OP (GRJ), ',', OP (GRDOUBLEK), 0 } },
    & ifmt_smul, { 0x280 }
  },
/* smu$pack $GRi,$GRj */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (GRI), ',', OP (GRJ), 0 } },
    & ifmt_smu, { 0x1180140 }
  },
/* smass$pack $GRi,$GRj */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (GRI), ',', OP (GRJ), 0 } },
    & ifmt_smu, { 0x1180180 }
  },
/* smsss$pack $GRi,$GRj */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (GRI), ',', OP (GRJ), 0 } },
    & ifmt_smu, { 0x11801c0 }
  },
/* sll$pack $GRi,$GRj,$GRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (GRI), ',', OP (GRJ), ',', OP (GRK), 0 } },
    & ifmt_add, { 0x40200 }
  },
/* srl$pack $GRi,$GRj,$GRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (GRI), ',', OP (GRJ), ',', OP (GRK), 0 } },
    & ifmt_add, { 0x40280 }
  },
/* sra$pack $GRi,$GRj,$GRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (GRI), ',', OP (GRJ), ',', OP (GRK), 0 } },
    & ifmt_add, { 0x40300 }
  },
/* slass$pack $GRi,$GRj,$GRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (GRI), ',', OP (GRJ), ',', OP (GRK), 0 } },
    & ifmt_slass, { 0x1180080 }
  },
/* scutss$pack $GRj,$GRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (GRJ), ',', OP (GRK), 0 } },
    & ifmt_scutss, { 0x1180100 }
  },
/* scan$pack $GRi,$GRj,$GRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (GRI), ',', OP (GRJ), ',', OP (GRK), 0 } },
    & ifmt_add, { 0x2c0000 }
  },
/* cadd$pack $GRi,$GRj,$GRk,$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (GRI), ',', OP (GRJ), ',', OP (GRK), ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_cadd, { 0x1600000 }
  },
/* csub$pack $GRi,$GRj,$GRk,$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (GRI), ',', OP (GRJ), ',', OP (GRK), ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_cadd, { 0x1600040 }
  },
/* cand$pack $GRi,$GRj,$GRk,$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (GRI), ',', OP (GRJ), ',', OP (GRK), ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_cadd, { 0x1680000 }
  },
/* cor$pack $GRi,$GRj,$GRk,$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (GRI), ',', OP (GRJ), ',', OP (GRK), ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_cadd, { 0x1680040 }
  },
/* cxor$pack $GRi,$GRj,$GRk,$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (GRI), ',', OP (GRJ), ',', OP (GRK), ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_cadd, { 0x1680080 }
  },
/* cnot$pack $GRj,$GRk,$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (GRJ), ',', OP (GRK), ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_cnot, { 0x16800c0 }
  },
/* csmul$pack $GRi,$GRj,$GRdoublek,$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (GRI), ',', OP (GRJ), ',', OP (GRDOUBLEK), ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_csmul, { 0x1600080 }
  },
/* csdiv$pack $GRi,$GRj,$GRk,$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (GRI), ',', OP (GRJ), ',', OP (GRK), ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_cadd, { 0x16000c0 }
  },
/* cudiv$pack $GRi,$GRj,$GRk,$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (GRI), ',', OP (GRJ), ',', OP (GRK), ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_cadd, { 0x16400c0 }
  },
/* csll$pack $GRi,$GRj,$GRk,$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (GRI), ',', OP (GRJ), ',', OP (GRK), ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_cadd, { 0x1700000 }
  },
/* csrl$pack $GRi,$GRj,$GRk,$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (GRI), ',', OP (GRJ), ',', OP (GRK), ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_cadd, { 0x1700040 }
  },
/* csra$pack $GRi,$GRj,$GRk,$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (GRI), ',', OP (GRJ), ',', OP (GRK), ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_cadd, { 0x1700080 }
  },
/* cscan$pack $GRi,$GRj,$GRk,$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (GRI), ',', OP (GRJ), ',', OP (GRK), ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_cadd, { 0x19400c0 }
  },
/* addcc$pack $GRi,$GRj,$GRk,$ICCi_1 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (GRI), ',', OP (GRJ), ',', OP (GRK), ',', OP (ICCI_1), 0 } },
    & ifmt_addcc, { 0x40 }
  },
/* subcc$pack $GRi,$GRj,$GRk,$ICCi_1 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (GRI), ',', OP (GRJ), ',', OP (GRK), ',', OP (ICCI_1), 0 } },
    & ifmt_addcc, { 0x140 }
  },
/* andcc$pack $GRi,$GRj,$GRk,$ICCi_1 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (GRI), ',', OP (GRJ), ',', OP (GRK), ',', OP (ICCI_1), 0 } },
    & ifmt_addcc, { 0x40040 }
  },
/* orcc$pack $GRi,$GRj,$GRk,$ICCi_1 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (GRI), ',', OP (GRJ), ',', OP (GRK), ',', OP (ICCI_1), 0 } },
    & ifmt_addcc, { 0x400c0 }
  },
/* xorcc$pack $GRi,$GRj,$GRk,$ICCi_1 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (GRI), ',', OP (GRJ), ',', OP (GRK), ',', OP (ICCI_1), 0 } },
    & ifmt_addcc, { 0x40140 }
  },
/* sllcc$pack $GRi,$GRj,$GRk,$ICCi_1 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (GRI), ',', OP (GRJ), ',', OP (GRK), ',', OP (ICCI_1), 0 } },
    & ifmt_addcc, { 0x40240 }
  },
/* srlcc$pack $GRi,$GRj,$GRk,$ICCi_1 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (GRI), ',', OP (GRJ), ',', OP (GRK), ',', OP (ICCI_1), 0 } },
    & ifmt_addcc, { 0x402c0 }
  },
/* sracc$pack $GRi,$GRj,$GRk,$ICCi_1 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (GRI), ',', OP (GRJ), ',', OP (GRK), ',', OP (ICCI_1), 0 } },
    & ifmt_addcc, { 0x40340 }
  },
/* smulcc$pack $GRi,$GRj,$GRdoublek,$ICCi_1 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (GRI), ',', OP (GRJ), ',', OP (GRDOUBLEK), ',', OP (ICCI_1), 0 } },
    & ifmt_smulcc, { 0x240 }
  },
/* umulcc$pack $GRi,$GRj,$GRdoublek,$ICCi_1 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (GRI), ',', OP (GRJ), ',', OP (GRDOUBLEK), ',', OP (ICCI_1), 0 } },
    & ifmt_smulcc, { 0x2c0 }
  },
/* caddcc$pack $GRi,$GRj,$GRk,$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (GRI), ',', OP (GRJ), ',', OP (GRK), ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_cadd, { 0x1640000 }
  },
/* csubcc$pack $GRi,$GRj,$GRk,$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (GRI), ',', OP (GRJ), ',', OP (GRK), ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_cadd, { 0x1640040 }
  },
/* csmulcc$pack $GRi,$GRj,$GRdoublek,$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (GRI), ',', OP (GRJ), ',', OP (GRDOUBLEK), ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_csmul, { 0x1640080 }
  },
/* candcc$pack $GRi,$GRj,$GRk,$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (GRI), ',', OP (GRJ), ',', OP (GRK), ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_cadd, { 0x16c0000 }
  },
/* corcc$pack $GRi,$GRj,$GRk,$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (GRI), ',', OP (GRJ), ',', OP (GRK), ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_cadd, { 0x16c0040 }
  },
/* cxorcc$pack $GRi,$GRj,$GRk,$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (GRI), ',', OP (GRJ), ',', OP (GRK), ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_cadd, { 0x16c0080 }
  },
/* csllcc$pack $GRi,$GRj,$GRk,$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (GRI), ',', OP (GRJ), ',', OP (GRK), ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_cadd, { 0x1740000 }
  },
/* csrlcc$pack $GRi,$GRj,$GRk,$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (GRI), ',', OP (GRJ), ',', OP (GRK), ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_cadd, { 0x1740040 }
  },
/* csracc$pack $GRi,$GRj,$GRk,$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (GRI), ',', OP (GRJ), ',', OP (GRK), ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_cadd, { 0x1740080 }
  },
/* addx$pack $GRi,$GRj,$GRk,$ICCi_1 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (GRI), ',', OP (GRJ), ',', OP (GRK), ',', OP (ICCI_1), 0 } },
    & ifmt_addcc, { 0x80 }
  },
/* subx$pack $GRi,$GRj,$GRk,$ICCi_1 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (GRI), ',', OP (GRJ), ',', OP (GRK), ',', OP (ICCI_1), 0 } },
    & ifmt_addcc, { 0x180 }
  },
/* addxcc$pack $GRi,$GRj,$GRk,$ICCi_1 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (GRI), ',', OP (GRJ), ',', OP (GRK), ',', OP (ICCI_1), 0 } },
    & ifmt_addcc, { 0xc0 }
  },
/* subxcc$pack $GRi,$GRj,$GRk,$ICCi_1 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (GRI), ',', OP (GRJ), ',', OP (GRK), ',', OP (ICCI_1), 0 } },
    & ifmt_addcc, { 0x1c0 }
  },
/* addss$pack $GRi,$GRj,$GRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (GRI), ',', OP (GRJ), ',', OP (GRK), 0 } },
    & ifmt_slass, { 0x1180000 }
  },
/* subss$pack $GRi,$GRj,$GRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (GRI), ',', OP (GRJ), ',', OP (GRK), 0 } },
    & ifmt_slass, { 0x1180040 }
  },
/* addi$pack $GRi,$s12,$GRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (GRI), ',', OP (S12), ',', OP (GRK), 0 } },
    & ifmt_addi, { 0x400000 }
  },
/* subi$pack $GRi,$s12,$GRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (GRI), ',', OP (S12), ',', OP (GRK), 0 } },
    & ifmt_addi, { 0x500000 }
  },
/* andi$pack $GRi,$s12,$GRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (GRI), ',', OP (S12), ',', OP (GRK), 0 } },
    & ifmt_addi, { 0x800000 }
  },
/* ori$pack $GRi,$s12,$GRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (GRI), ',', OP (S12), ',', OP (GRK), 0 } },
    & ifmt_addi, { 0x880000 }
  },
/* xori$pack $GRi,$s12,$GRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (GRI), ',', OP (S12), ',', OP (GRK), 0 } },
    & ifmt_addi, { 0x900000 }
  },
/* sdivi$pack $GRi,$s12,$GRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (GRI), ',', OP (S12), ',', OP (GRK), 0 } },
    & ifmt_addi, { 0x780000 }
  },
/* nsdivi$pack $GRi,$s12,$GRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (GRI), ',', OP (S12), ',', OP (GRK), 0 } },
    & ifmt_addi, { 0xb80000 }
  },
/* udivi$pack $GRi,$s12,$GRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (GRI), ',', OP (S12), ',', OP (GRK), 0 } },
    & ifmt_addi, { 0x7c0000 }
  },
/* nudivi$pack $GRi,$s12,$GRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (GRI), ',', OP (S12), ',', OP (GRK), 0 } },
    & ifmt_addi, { 0xbc0000 }
  },
/* smuli$pack $GRi,$s12,$GRdoublek */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (GRI), ',', OP (S12), ',', OP (GRDOUBLEK), 0 } },
    & ifmt_smuli, { 0x600000 }
  },
/* umuli$pack $GRi,$s12,$GRdoublek */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (GRI), ',', OP (S12), ',', OP (GRDOUBLEK), 0 } },
    & ifmt_smuli, { 0x680000 }
  },
/* slli$pack $GRi,$s12,$GRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (GRI), ',', OP (S12), ',', OP (GRK), 0 } },
    & ifmt_addi, { 0xa00000 }
  },
/* srli$pack $GRi,$s12,$GRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (GRI), ',', OP (S12), ',', OP (GRK), 0 } },
    & ifmt_addi, { 0xa80000 }
  },
/* srai$pack $GRi,$s12,$GRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (GRI), ',', OP (S12), ',', OP (GRK), 0 } },
    & ifmt_addi, { 0xb00000 }
  },
/* scani$pack $GRi,$s12,$GRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (GRI), ',', OP (S12), ',', OP (GRK), 0 } },
    & ifmt_addi, { 0x11c0000 }
  },
/* addicc$pack $GRi,$s10,$GRk,$ICCi_1 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (GRI), ',', OP (S10), ',', OP (GRK), ',', OP (ICCI_1), 0 } },
    & ifmt_addicc, { 0x440000 }
  },
/* subicc$pack $GRi,$s10,$GRk,$ICCi_1 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (GRI), ',', OP (S10), ',', OP (GRK), ',', OP (ICCI_1), 0 } },
    & ifmt_addicc, { 0x540000 }
  },
/* andicc$pack $GRi,$s10,$GRk,$ICCi_1 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (GRI), ',', OP (S10), ',', OP (GRK), ',', OP (ICCI_1), 0 } },
    & ifmt_addicc, { 0x840000 }
  },
/* oricc$pack $GRi,$s10,$GRk,$ICCi_1 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (GRI), ',', OP (S10), ',', OP (GRK), ',', OP (ICCI_1), 0 } },
    & ifmt_addicc, { 0x8c0000 }
  },
/* xoricc$pack $GRi,$s10,$GRk,$ICCi_1 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (GRI), ',', OP (S10), ',', OP (GRK), ',', OP (ICCI_1), 0 } },
    & ifmt_addicc, { 0x940000 }
  },
/* smulicc$pack $GRi,$s10,$GRdoublek,$ICCi_1 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (GRI), ',', OP (S10), ',', OP (GRDOUBLEK), ',', OP (ICCI_1), 0 } },
    & ifmt_smulicc, { 0x640000 }
  },
/* umulicc$pack $GRi,$s10,$GRdoublek,$ICCi_1 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (GRI), ',', OP (S10), ',', OP (GRDOUBLEK), ',', OP (ICCI_1), 0 } },
    & ifmt_smulicc, { 0x6c0000 }
  },
/* sllicc$pack $GRi,$s10,$GRk,$ICCi_1 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (GRI), ',', OP (S10), ',', OP (GRK), ',', OP (ICCI_1), 0 } },
    & ifmt_addicc, { 0xa40000 }
  },
/* srlicc$pack $GRi,$s10,$GRk,$ICCi_1 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (GRI), ',', OP (S10), ',', OP (GRK), ',', OP (ICCI_1), 0 } },
    & ifmt_addicc, { 0xac0000 }
  },
/* sraicc$pack $GRi,$s10,$GRk,$ICCi_1 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (GRI), ',', OP (S10), ',', OP (GRK), ',', OP (ICCI_1), 0 } },
    & ifmt_addicc, { 0xb40000 }
  },
/* addxi$pack $GRi,$s10,$GRk,$ICCi_1 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (GRI), ',', OP (S10), ',', OP (GRK), ',', OP (ICCI_1), 0 } },
    & ifmt_addicc, { 0x480000 }
  },
/* subxi$pack $GRi,$s10,$GRk,$ICCi_1 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (GRI), ',', OP (S10), ',', OP (GRK), ',', OP (ICCI_1), 0 } },
    & ifmt_addicc, { 0x580000 }
  },
/* addxicc$pack $GRi,$s10,$GRk,$ICCi_1 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (GRI), ',', OP (S10), ',', OP (GRK), ',', OP (ICCI_1), 0 } },
    & ifmt_addicc, { 0x4c0000 }
  },
/* subxicc$pack $GRi,$s10,$GRk,$ICCi_1 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (GRI), ',', OP (S10), ',', OP (GRK), ',', OP (ICCI_1), 0 } },
    & ifmt_addicc, { 0x5c0000 }
  },
/* cmpb$pack $GRi,$GRj,$ICCi_1 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (GRI), ',', OP (GRJ), ',', OP (ICCI_1), 0 } },
    & ifmt_cmpb, { 0x300 }
  },
/* cmpba$pack $GRi,$GRj,$ICCi_1 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (GRI), ',', OP (GRJ), ',', OP (ICCI_1), 0 } },
    & ifmt_cmpb, { 0x340 }
  },
/* setlo$pack $ulo16,$GRklo */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (ULO16), ',', OP (GRKLO), 0 } },
    & ifmt_setlo, { 0xf40000 }
  },
/* sethi$pack $uhi16,$GRkhi */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (UHI16), ',', OP (GRKHI), 0 } },
    & ifmt_sethi, { 0xf80000 }
  },
/* setlos$pack $slo16,$GRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (SLO16), ',', OP (GRK), 0 } },
    & ifmt_setlos, { 0xfc0000 }
  },
/* ldsb$pack @($GRi,$GRj),$GRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', '@', '(', OP (GRI), ',', OP (GRJ), ')', ',', OP (GRK), 0 } },
    & ifmt_slass, { 0x80000 }
  },
/* ldub$pack @($GRi,$GRj),$GRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', '@', '(', OP (GRI), ',', OP (GRJ), ')', ',', OP (GRK), 0 } },
    & ifmt_slass, { 0x80040 }
  },
/* ldsh$pack @($GRi,$GRj),$GRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', '@', '(', OP (GRI), ',', OP (GRJ), ')', ',', OP (GRK), 0 } },
    & ifmt_slass, { 0x80080 }
  },
/* lduh$pack @($GRi,$GRj),$GRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', '@', '(', OP (GRI), ',', OP (GRJ), ')', ',', OP (GRK), 0 } },
    & ifmt_slass, { 0x800c0 }
  },
/* ld$pack $ldann($GRi,$GRj),$GRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (LDANN), '(', OP (GRI), ',', OP (GRJ), ')', ',', OP (GRK), 0 } },
    & ifmt_slass, { 0x80100 }
  },
/* ldbf$pack @($GRi,$GRj),$FRintk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', '@', '(', OP (GRI), ',', OP (GRJ), ')', ',', OP (FRINTK), 0 } },
    & ifmt_ldbf, { 0x80200 }
  },
/* ldhf$pack @($GRi,$GRj),$FRintk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', '@', '(', OP (GRI), ',', OP (GRJ), ')', ',', OP (FRINTK), 0 } },
    & ifmt_ldbf, { 0x80240 }
  },
/* ldf$pack @($GRi,$GRj),$FRintk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', '@', '(', OP (GRI), ',', OP (GRJ), ')', ',', OP (FRINTK), 0 } },
    & ifmt_ldbf, { 0x80280 }
  },
/* ldc$pack @($GRi,$GRj),$CPRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', '@', '(', OP (GRI), ',', OP (GRJ), ')', ',', OP (CPRK), 0 } },
    & ifmt_ldc, { 0x80340 }
  },
/* nldsb$pack @($GRi,$GRj),$GRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', '@', '(', OP (GRI), ',', OP (GRJ), ')', ',', OP (GRK), 0 } },
    & ifmt_slass, { 0x80800 }
  },
/* nldub$pack @($GRi,$GRj),$GRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', '@', '(', OP (GRI), ',', OP (GRJ), ')', ',', OP (GRK), 0 } },
    & ifmt_slass, { 0x80840 }
  },
/* nldsh$pack @($GRi,$GRj),$GRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', '@', '(', OP (GRI), ',', OP (GRJ), ')', ',', OP (GRK), 0 } },
    & ifmt_slass, { 0x80880 }
  },
/* nlduh$pack @($GRi,$GRj),$GRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', '@', '(', OP (GRI), ',', OP (GRJ), ')', ',', OP (GRK), 0 } },
    & ifmt_slass, { 0x808c0 }
  },
/* nld$pack @($GRi,$GRj),$GRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', '@', '(', OP (GRI), ',', OP (GRJ), ')', ',', OP (GRK), 0 } },
    & ifmt_slass, { 0x80900 }
  },
/* nldbf$pack @($GRi,$GRj),$FRintk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', '@', '(', OP (GRI), ',', OP (GRJ), ')', ',', OP (FRINTK), 0 } },
    & ifmt_ldbf, { 0x80a00 }
  },
/* nldhf$pack @($GRi,$GRj),$FRintk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', '@', '(', OP (GRI), ',', OP (GRJ), ')', ',', OP (FRINTK), 0 } },
    & ifmt_ldbf, { 0x80a40 }
  },
/* nldf$pack @($GRi,$GRj),$FRintk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', '@', '(', OP (GRI), ',', OP (GRJ), ')', ',', OP (FRINTK), 0 } },
    & ifmt_ldbf, { 0x80a80 }
  },
/* ldd$pack $lddann($GRi,$GRj),$GRdoublek */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (LDDANN), '(', OP (GRI), ',', OP (GRJ), ')', ',', OP (GRDOUBLEK), 0 } },
    & ifmt_ldd, { 0x80140 }
  },
/* lddf$pack @($GRi,$GRj),$FRdoublek */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', '@', '(', OP (GRI), ',', OP (GRJ), ')', ',', OP (FRDOUBLEK), 0 } },
    & ifmt_lddf, { 0x802c0 }
  },
/* lddc$pack @($GRi,$GRj),$CPRdoublek */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', '@', '(', OP (GRI), ',', OP (GRJ), ')', ',', OP (CPRDOUBLEK), 0 } },
    & ifmt_lddc, { 0x80380 }
  },
/* nldd$pack @($GRi,$GRj),$GRdoublek */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', '@', '(', OP (GRI), ',', OP (GRJ), ')', ',', OP (GRDOUBLEK), 0 } },
    & ifmt_ldd, { 0x80940 }
  },
/* nlddf$pack @($GRi,$GRj),$FRdoublek */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', '@', '(', OP (GRI), ',', OP (GRJ), ')', ',', OP (FRDOUBLEK), 0 } },
    & ifmt_lddf, { 0x80ac0 }
  },
/* ldq$pack @($GRi,$GRj),$GRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', '@', '(', OP (GRI), ',', OP (GRJ), ')', ',', OP (GRK), 0 } },
    & ifmt_slass, { 0x80180 }
  },
/* ldqf$pack @($GRi,$GRj),$FRintk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', '@', '(', OP (GRI), ',', OP (GRJ), ')', ',', OP (FRINTK), 0 } },
    & ifmt_ldbf, { 0x80300 }
  },
/* ldqc$pack @($GRi,$GRj),$CPRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', '@', '(', OP (GRI), ',', OP (GRJ), ')', ',', OP (CPRK), 0 } },
    & ifmt_ldc, { 0x803c0 }
  },
/* nldq$pack @($GRi,$GRj),$GRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', '@', '(', OP (GRI), ',', OP (GRJ), ')', ',', OP (GRK), 0 } },
    & ifmt_slass, { 0x80980 }
  },
/* nldqf$pack @($GRi,$GRj),$FRintk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', '@', '(', OP (GRI), ',', OP (GRJ), ')', ',', OP (FRINTK), 0 } },
    & ifmt_ldbf, { 0x80b00 }
  },
/* ldsbu$pack @($GRi,$GRj),$GRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', '@', '(', OP (GRI), ',', OP (GRJ), ')', ',', OP (GRK), 0 } },
    & ifmt_slass, { 0x80400 }
  },
/* ldubu$pack @($GRi,$GRj),$GRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', '@', '(', OP (GRI), ',', OP (GRJ), ')', ',', OP (GRK), 0 } },
    & ifmt_slass, { 0x80440 }
  },
/* ldshu$pack @($GRi,$GRj),$GRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', '@', '(', OP (GRI), ',', OP (GRJ), ')', ',', OP (GRK), 0 } },
    & ifmt_slass, { 0x80480 }
  },
/* lduhu$pack @($GRi,$GRj),$GRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', '@', '(', OP (GRI), ',', OP (GRJ), ')', ',', OP (GRK), 0 } },
    & ifmt_slass, { 0x804c0 }
  },
/* ldu$pack @($GRi,$GRj),$GRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', '@', '(', OP (GRI), ',', OP (GRJ), ')', ',', OP (GRK), 0 } },
    & ifmt_slass, { 0x80500 }
  },
/* nldsbu$pack @($GRi,$GRj),$GRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', '@', '(', OP (GRI), ',', OP (GRJ), ')', ',', OP (GRK), 0 } },
    & ifmt_slass, { 0x80c00 }
  },
/* nldubu$pack @($GRi,$GRj),$GRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', '@', '(', OP (GRI), ',', OP (GRJ), ')', ',', OP (GRK), 0 } },
    & ifmt_slass, { 0x80c40 }
  },
/* nldshu$pack @($GRi,$GRj),$GRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', '@', '(', OP (GRI), ',', OP (GRJ), ')', ',', OP (GRK), 0 } },
    & ifmt_slass, { 0x80c80 }
  },
/* nlduhu$pack @($GRi,$GRj),$GRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', '@', '(', OP (GRI), ',', OP (GRJ), ')', ',', OP (GRK), 0 } },
    & ifmt_slass, { 0x80cc0 }
  },
/* nldu$pack @($GRi,$GRj),$GRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', '@', '(', OP (GRI), ',', OP (GRJ), ')', ',', OP (GRK), 0 } },
    & ifmt_slass, { 0x80d00 }
  },
/* ldbfu$pack @($GRi,$GRj),$FRintk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', '@', '(', OP (GRI), ',', OP (GRJ), ')', ',', OP (FRINTK), 0 } },
    & ifmt_ldbf, { 0x80600 }
  },
/* ldhfu$pack @($GRi,$GRj),$FRintk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', '@', '(', OP (GRI), ',', OP (GRJ), ')', ',', OP (FRINTK), 0 } },
    & ifmt_ldbf, { 0x80640 }
  },
/* ldfu$pack @($GRi,$GRj),$FRintk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', '@', '(', OP (GRI), ',', OP (GRJ), ')', ',', OP (FRINTK), 0 } },
    & ifmt_ldbf, { 0x80680 }
  },
/* ldcu$pack @($GRi,$GRj),$CPRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', '@', '(', OP (GRI), ',', OP (GRJ), ')', ',', OP (CPRK), 0 } },
    & ifmt_ldc, { 0x80740 }
  },
/* nldbfu$pack @($GRi,$GRj),$FRintk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', '@', '(', OP (GRI), ',', OP (GRJ), ')', ',', OP (FRINTK), 0 } },
    & ifmt_ldbf, { 0x80e00 }
  },
/* nldhfu$pack @($GRi,$GRj),$FRintk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', '@', '(', OP (GRI), ',', OP (GRJ), ')', ',', OP (FRINTK), 0 } },
    & ifmt_ldbf, { 0x80e40 }
  },
/* nldfu$pack @($GRi,$GRj),$FRintk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', '@', '(', OP (GRI), ',', OP (GRJ), ')', ',', OP (FRINTK), 0 } },
    & ifmt_ldbf, { 0x80e80 }
  },
/* lddu$pack @($GRi,$GRj),$GRdoublek */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', '@', '(', OP (GRI), ',', OP (GRJ), ')', ',', OP (GRDOUBLEK), 0 } },
    & ifmt_ldd, { 0x80540 }
  },
/* nlddu$pack @($GRi,$GRj),$GRdoublek */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', '@', '(', OP (GRI), ',', OP (GRJ), ')', ',', OP (GRDOUBLEK), 0 } },
    & ifmt_ldd, { 0x80d40 }
  },
/* lddfu$pack @($GRi,$GRj),$FRdoublek */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', '@', '(', OP (GRI), ',', OP (GRJ), ')', ',', OP (FRDOUBLEK), 0 } },
    & ifmt_lddf, { 0x806c0 }
  },
/* lddcu$pack @($GRi,$GRj),$CPRdoublek */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', '@', '(', OP (GRI), ',', OP (GRJ), ')', ',', OP (CPRDOUBLEK), 0 } },
    & ifmt_lddc, { 0x80780 }
  },
/* nlddfu$pack @($GRi,$GRj),$FRdoublek */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', '@', '(', OP (GRI), ',', OP (GRJ), ')', ',', OP (FRDOUBLEK), 0 } },
    & ifmt_lddf, { 0x80ec0 }
  },
/* ldqu$pack @($GRi,$GRj),$GRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', '@', '(', OP (GRI), ',', OP (GRJ), ')', ',', OP (GRK), 0 } },
    & ifmt_slass, { 0x80580 }
  },
/* nldqu$pack @($GRi,$GRj),$GRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', '@', '(', OP (GRI), ',', OP (GRJ), ')', ',', OP (GRK), 0 } },
    & ifmt_slass, { 0x80d80 }
  },
/* ldqfu$pack @($GRi,$GRj),$FRintk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', '@', '(', OP (GRI), ',', OP (GRJ), ')', ',', OP (FRINTK), 0 } },
    & ifmt_ldbf, { 0x80700 }
  },
/* ldqcu$pack @($GRi,$GRj),$CPRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', '@', '(', OP (GRI), ',', OP (GRJ), ')', ',', OP (CPRK), 0 } },
    & ifmt_ldc, { 0x807c0 }
  },
/* nldqfu$pack @($GRi,$GRj),$FRintk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', '@', '(', OP (GRI), ',', OP (GRJ), ')', ',', OP (FRINTK), 0 } },
    & ifmt_ldbf, { 0x80f00 }
  },
/* ldsbi$pack @($GRi,$d12),$GRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', '@', '(', OP (GRI), ',', OP (D12), ')', ',', OP (GRK), 0 } },
    & ifmt_ldsbi, { 0xc00000 }
  },
/* ldshi$pack @($GRi,$d12),$GRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', '@', '(', OP (GRI), ',', OP (D12), ')', ',', OP (GRK), 0 } },
    & ifmt_ldsbi, { 0xc40000 }
  },
/* ldi$pack @($GRi,$d12),$GRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', '@', '(', OP (GRI), ',', OP (D12), ')', ',', OP (GRK), 0 } },
    & ifmt_ldsbi, { 0xc80000 }
  },
/* ldubi$pack @($GRi,$d12),$GRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', '@', '(', OP (GRI), ',', OP (D12), ')', ',', OP (GRK), 0 } },
    & ifmt_ldsbi, { 0xd40000 }
  },
/* lduhi$pack @($GRi,$d12),$GRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', '@', '(', OP (GRI), ',', OP (D12), ')', ',', OP (GRK), 0 } },
    & ifmt_ldsbi, { 0xd80000 }
  },
/* ldbfi$pack @($GRi,$d12),$FRintk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', '@', '(', OP (GRI), ',', OP (D12), ')', ',', OP (FRINTK), 0 } },
    & ifmt_ldbfi, { 0xe00000 }
  },
/* ldhfi$pack @($GRi,$d12),$FRintk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', '@', '(', OP (GRI), ',', OP (D12), ')', ',', OP (FRINTK), 0 } },
    & ifmt_ldbfi, { 0xe40000 }
  },
/* ldfi$pack @($GRi,$d12),$FRintk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', '@', '(', OP (GRI), ',', OP (D12), ')', ',', OP (FRINTK), 0 } },
    & ifmt_ldbfi, { 0xe80000 }
  },
/* nldsbi$pack @($GRi,$d12),$GRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', '@', '(', OP (GRI), ',', OP (D12), ')', ',', OP (GRK), 0 } },
    & ifmt_ldsbi, { 0x1000000 }
  },
/* nldubi$pack @($GRi,$d12),$GRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', '@', '(', OP (GRI), ',', OP (D12), ')', ',', OP (GRK), 0 } },
    & ifmt_ldsbi, { 0x1040000 }
  },
/* nldshi$pack @($GRi,$d12),$GRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', '@', '(', OP (GRI), ',', OP (D12), ')', ',', OP (GRK), 0 } },
    & ifmt_ldsbi, { 0x1080000 }
  },
/* nlduhi$pack @($GRi,$d12),$GRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', '@', '(', OP (GRI), ',', OP (D12), ')', ',', OP (GRK), 0 } },
    & ifmt_ldsbi, { 0x10c0000 }
  },
/* nldi$pack @($GRi,$d12),$GRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', '@', '(', OP (GRI), ',', OP (D12), ')', ',', OP (GRK), 0 } },
    & ifmt_ldsbi, { 0x1100000 }
  },
/* nldbfi$pack @($GRi,$d12),$FRintk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', '@', '(', OP (GRI), ',', OP (D12), ')', ',', OP (FRINTK), 0 } },
    & ifmt_ldbfi, { 0x1200000 }
  },
/* nldhfi$pack @($GRi,$d12),$FRintk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', '@', '(', OP (GRI), ',', OP (D12), ')', ',', OP (FRINTK), 0 } },
    & ifmt_ldbfi, { 0x1240000 }
  },
/* nldfi$pack @($GRi,$d12),$FRintk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', '@', '(', OP (GRI), ',', OP (D12), ')', ',', OP (FRINTK), 0 } },
    & ifmt_ldbfi, { 0x1280000 }
  },
/* lddi$pack @($GRi,$d12),$GRdoublek */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', '@', '(', OP (GRI), ',', OP (D12), ')', ',', OP (GRDOUBLEK), 0 } },
    & ifmt_lddi, { 0xcc0000 }
  },
/* lddfi$pack @($GRi,$d12),$FRdoublek */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', '@', '(', OP (GRI), ',', OP (D12), ')', ',', OP (FRDOUBLEK), 0 } },
    & ifmt_lddfi, { 0xec0000 }
  },
/* nlddi$pack @($GRi,$d12),$GRdoublek */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', '@', '(', OP (GRI), ',', OP (D12), ')', ',', OP (GRDOUBLEK), 0 } },
    & ifmt_lddi, { 0x1140000 }
  },
/* nlddfi$pack @($GRi,$d12),$FRdoublek */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', '@', '(', OP (GRI), ',', OP (D12), ')', ',', OP (FRDOUBLEK), 0 } },
    & ifmt_lddfi, { 0x12c0000 }
  },
/* ldqi$pack @($GRi,$d12),$GRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', '@', '(', OP (GRI), ',', OP (D12), ')', ',', OP (GRK), 0 } },
    & ifmt_ldsbi, { 0xd00000 }
  },
/* ldqfi$pack @($GRi,$d12),$FRintk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', '@', '(', OP (GRI), ',', OP (D12), ')', ',', OP (FRINTK), 0 } },
    & ifmt_ldbfi, { 0xf00000 }
  },
/* nldqfi$pack @($GRi,$d12),$FRintk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', '@', '(', OP (GRI), ',', OP (D12), ')', ',', OP (FRINTK), 0 } },
    & ifmt_ldbfi, { 0x1300000 }
  },
/* stb$pack $GRk,@($GRi,$GRj) */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (GRK), ',', '@', '(', OP (GRI), ',', OP (GRJ), ')', 0 } },
    & ifmt_slass, { 0xc0000 }
  },
/* sth$pack $GRk,@($GRi,$GRj) */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (GRK), ',', '@', '(', OP (GRI), ',', OP (GRJ), ')', 0 } },
    & ifmt_slass, { 0xc0040 }
  },
/* st$pack $GRk,@($GRi,$GRj) */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (GRK), ',', '@', '(', OP (GRI), ',', OP (GRJ), ')', 0 } },
    & ifmt_slass, { 0xc0080 }
  },
/* stbf$pack $FRintk,@($GRi,$GRj) */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRINTK), ',', '@', '(', OP (GRI), ',', OP (GRJ), ')', 0 } },
    & ifmt_ldbf, { 0xc0200 }
  },
/* sthf$pack $FRintk,@($GRi,$GRj) */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRINTK), ',', '@', '(', OP (GRI), ',', OP (GRJ), ')', 0 } },
    & ifmt_ldbf, { 0xc0240 }
  },
/* stf$pack $FRintk,@($GRi,$GRj) */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRINTK), ',', '@', '(', OP (GRI), ',', OP (GRJ), ')', 0 } },
    & ifmt_ldbf, { 0xc0280 }
  },
/* stc$pack $CPRk,@($GRi,$GRj) */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (CPRK), ',', '@', '(', OP (GRI), ',', OP (GRJ), ')', 0 } },
    & ifmt_ldc, { 0xc0940 }
  },
/* std$pack $GRdoublek,@($GRi,$GRj) */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (GRDOUBLEK), ',', '@', '(', OP (GRI), ',', OP (GRJ), ')', 0 } },
    & ifmt_ldd, { 0xc00c0 }
  },
/* stdf$pack $FRdoublek,@($GRi,$GRj) */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRDOUBLEK), ',', '@', '(', OP (GRI), ',', OP (GRJ), ')', 0 } },
    & ifmt_lddf, { 0xc02c0 }
  },
/* stdc$pack $CPRdoublek,@($GRi,$GRj) */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (CPRDOUBLEK), ',', '@', '(', OP (GRI), ',', OP (GRJ), ')', 0 } },
    & ifmt_lddc, { 0xc0980 }
  },
/* stq$pack $GRk,@($GRi,$GRj) */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (GRK), ',', '@', '(', OP (GRI), ',', OP (GRJ), ')', 0 } },
    & ifmt_slass, { 0xc0100 }
  },
/* stqf$pack $FRintk,@($GRi,$GRj) */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRINTK), ',', '@', '(', OP (GRI), ',', OP (GRJ), ')', 0 } },
    & ifmt_ldbf, { 0xc0300 }
  },
/* stqc$pack $CPRk,@($GRi,$GRj) */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (CPRK), ',', '@', '(', OP (GRI), ',', OP (GRJ), ')', 0 } },
    & ifmt_ldc, { 0xc09c0 }
  },
/* stbu$pack $GRk,@($GRi,$GRj) */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (GRK), ',', '@', '(', OP (GRI), ',', OP (GRJ), ')', 0 } },
    & ifmt_slass, { 0xc0400 }
  },
/* sthu$pack $GRk,@($GRi,$GRj) */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (GRK), ',', '@', '(', OP (GRI), ',', OP (GRJ), ')', 0 } },
    & ifmt_slass, { 0xc0440 }
  },
/* stu$pack $GRk,@($GRi,$GRj) */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (GRK), ',', '@', '(', OP (GRI), ',', OP (GRJ), ')', 0 } },
    & ifmt_slass, { 0xc0480 }
  },
/* stbfu$pack $FRintk,@($GRi,$GRj) */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRINTK), ',', '@', '(', OP (GRI), ',', OP (GRJ), ')', 0 } },
    & ifmt_ldbf, { 0xc0600 }
  },
/* sthfu$pack $FRintk,@($GRi,$GRj) */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRINTK), ',', '@', '(', OP (GRI), ',', OP (GRJ), ')', 0 } },
    & ifmt_ldbf, { 0xc0640 }
  },
/* stfu$pack $FRintk,@($GRi,$GRj) */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRINTK), ',', '@', '(', OP (GRI), ',', OP (GRJ), ')', 0 } },
    & ifmt_ldbf, { 0xc0680 }
  },
/* stcu$pack $CPRk,@($GRi,$GRj) */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (CPRK), ',', '@', '(', OP (GRI), ',', OP (GRJ), ')', 0 } },
    & ifmt_ldc, { 0xc0b40 }
  },
/* stdu$pack $GRdoublek,@($GRi,$GRj) */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (GRDOUBLEK), ',', '@', '(', OP (GRI), ',', OP (GRJ), ')', 0 } },
    & ifmt_ldd, { 0xc04c0 }
  },
/* stdfu$pack $FRdoublek,@($GRi,$GRj) */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRDOUBLEK), ',', '@', '(', OP (GRI), ',', OP (GRJ), ')', 0 } },
    & ifmt_lddf, { 0xc06c0 }
  },
/* stdcu$pack $CPRdoublek,@($GRi,$GRj) */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (CPRDOUBLEK), ',', '@', '(', OP (GRI), ',', OP (GRJ), ')', 0 } },
    & ifmt_lddc, { 0xc0b80 }
  },
/* stqu$pack $GRk,@($GRi,$GRj) */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (GRK), ',', '@', '(', OP (GRI), ',', OP (GRJ), ')', 0 } },
    & ifmt_slass, { 0xc0500 }
  },
/* stqfu$pack $FRintk,@($GRi,$GRj) */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRINTK), ',', '@', '(', OP (GRI), ',', OP (GRJ), ')', 0 } },
    & ifmt_ldbf, { 0xc0700 }
  },
/* stqcu$pack $CPRk,@($GRi,$GRj) */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (CPRK), ',', '@', '(', OP (GRI), ',', OP (GRJ), ')', 0 } },
    & ifmt_ldc, { 0xc0bc0 }
  },
/* cldsb$pack @($GRi,$GRj),$GRk,$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', '@', '(', OP (GRI), ',', OP (GRJ), ')', ',', OP (GRK), ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_cadd, { 0x1780000 }
  },
/* cldub$pack @($GRi,$GRj),$GRk,$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', '@', '(', OP (GRI), ',', OP (GRJ), ')', ',', OP (GRK), ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_cadd, { 0x1780040 }
  },
/* cldsh$pack @($GRi,$GRj),$GRk,$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', '@', '(', OP (GRI), ',', OP (GRJ), ')', ',', OP (GRK), ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_cadd, { 0x1780080 }
  },
/* clduh$pack @($GRi,$GRj),$GRk,$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', '@', '(', OP (GRI), ',', OP (GRJ), ')', ',', OP (GRK), ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_cadd, { 0x17800c0 }
  },
/* cld$pack @($GRi,$GRj),$GRk,$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', '@', '(', OP (GRI), ',', OP (GRJ), ')', ',', OP (GRK), ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_cadd, { 0x17c0000 }
  },
/* cldbf$pack @($GRi,$GRj),$FRintk,$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', '@', '(', OP (GRI), ',', OP (GRJ), ')', ',', OP (FRINTK), ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_cldbf, { 0x1800000 }
  },
/* cldhf$pack @($GRi,$GRj),$FRintk,$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', '@', '(', OP (GRI), ',', OP (GRJ), ')', ',', OP (FRINTK), ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_cldbf, { 0x1800040 }
  },
/* cldf$pack @($GRi,$GRj),$FRintk,$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', '@', '(', OP (GRI), ',', OP (GRJ), ')', ',', OP (FRINTK), ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_cldbf, { 0x1800080 }
  },
/* cldd$pack @($GRi,$GRj),$GRdoublek,$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', '@', '(', OP (GRI), ',', OP (GRJ), ')', ',', OP (GRDOUBLEK), ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_csmul, { 0x17c0040 }
  },
/* clddf$pack @($GRi,$GRj),$FRdoublek,$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', '@', '(', OP (GRI), ',', OP (GRJ), ')', ',', OP (FRDOUBLEK), ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_clddf, { 0x18000c0 }
  },
/* cldq$pack @($GRi,$GRj),$GRk,$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', '@', '(', OP (GRI), ',', OP (GRJ), ')', ',', OP (GRK), ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_cadd, { 0x17c0080 }
  },
/* cldsbu$pack @($GRi,$GRj),$GRk,$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', '@', '(', OP (GRI), ',', OP (GRJ), ')', ',', OP (GRK), ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_cadd, { 0x1840000 }
  },
/* cldubu$pack @($GRi,$GRj),$GRk,$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', '@', '(', OP (GRI), ',', OP (GRJ), ')', ',', OP (GRK), ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_cadd, { 0x1840040 }
  },
/* cldshu$pack @($GRi,$GRj),$GRk,$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', '@', '(', OP (GRI), ',', OP (GRJ), ')', ',', OP (GRK), ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_cadd, { 0x1840080 }
  },
/* clduhu$pack @($GRi,$GRj),$GRk,$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', '@', '(', OP (GRI), ',', OP (GRJ), ')', ',', OP (GRK), ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_cadd, { 0x18400c0 }
  },
/* cldu$pack @($GRi,$GRj),$GRk,$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', '@', '(', OP (GRI), ',', OP (GRJ), ')', ',', OP (GRK), ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_cadd, { 0x1880000 }
  },
/* cldbfu$pack @($GRi,$GRj),$FRintk,$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', '@', '(', OP (GRI), ',', OP (GRJ), ')', ',', OP (FRINTK), ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_cldbf, { 0x18c0000 }
  },
/* cldhfu$pack @($GRi,$GRj),$FRintk,$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', '@', '(', OP (GRI), ',', OP (GRJ), ')', ',', OP (FRINTK), ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_cldbf, { 0x18c0040 }
  },
/* cldfu$pack @($GRi,$GRj),$FRintk,$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', '@', '(', OP (GRI), ',', OP (GRJ), ')', ',', OP (FRINTK), ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_cldbf, { 0x18c0080 }
  },
/* clddu$pack @($GRi,$GRj),$GRdoublek,$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', '@', '(', OP (GRI), ',', OP (GRJ), ')', ',', OP (GRDOUBLEK), ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_csmul, { 0x1880040 }
  },
/* clddfu$pack @($GRi,$GRj),$FRdoublek,$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', '@', '(', OP (GRI), ',', OP (GRJ), ')', ',', OP (FRDOUBLEK), ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_clddf, { 0x18c00c0 }
  },
/* cldqu$pack @($GRi,$GRj),$GRk,$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', '@', '(', OP (GRI), ',', OP (GRJ), ')', ',', OP (GRK), ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_cadd, { 0x1880080 }
  },
/* cstb$pack $GRk,@($GRi,$GRj),$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (GRK), ',', '@', '(', OP (GRI), ',', OP (GRJ), ')', ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_cadd, { 0x1900000 }
  },
/* csth$pack $GRk,@($GRi,$GRj),$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (GRK), ',', '@', '(', OP (GRI), ',', OP (GRJ), ')', ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_cadd, { 0x1900040 }
  },
/* cst$pack $GRk,@($GRi,$GRj),$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (GRK), ',', '@', '(', OP (GRI), ',', OP (GRJ), ')', ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_cadd, { 0x1900080 }
  },
/* cstbf$pack $FRintk,@($GRi,$GRj),$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRINTK), ',', '@', '(', OP (GRI), ',', OP (GRJ), ')', ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_cldbf, { 0x1980000 }
  },
/* csthf$pack $FRintk,@($GRi,$GRj),$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRINTK), ',', '@', '(', OP (GRI), ',', OP (GRJ), ')', ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_cldbf, { 0x1980040 }
  },
/* cstf$pack $FRintk,@($GRi,$GRj),$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRINTK), ',', '@', '(', OP (GRI), ',', OP (GRJ), ')', ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_cldbf, { 0x1980080 }
  },
/* cstd$pack $GRdoublek,@($GRi,$GRj),$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (GRDOUBLEK), ',', '@', '(', OP (GRI), ',', OP (GRJ), ')', ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_csmul, { 0x19000c0 }
  },
/* cstdf$pack $FRdoublek,@($GRi,$GRj),$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRDOUBLEK), ',', '@', '(', OP (GRI), ',', OP (GRJ), ')', ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_clddf, { 0x19800c0 }
  },
/* cstq$pack $GRk,@($GRi,$GRj),$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (GRK), ',', '@', '(', OP (GRI), ',', OP (GRJ), ')', ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_cadd, { 0x1940000 }
  },
/* cstbu$pack $GRk,@($GRi,$GRj),$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (GRK), ',', '@', '(', OP (GRI), ',', OP (GRJ), ')', ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_cadd, { 0x19c0000 }
  },
/* csthu$pack $GRk,@($GRi,$GRj),$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (GRK), ',', '@', '(', OP (GRI), ',', OP (GRJ), ')', ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_cadd, { 0x19c0040 }
  },
/* cstu$pack $GRk,@($GRi,$GRj),$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (GRK), ',', '@', '(', OP (GRI), ',', OP (GRJ), ')', ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_cadd, { 0x19c0080 }
  },
/* cstbfu$pack $FRintk,@($GRi,$GRj),$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRINTK), ',', '@', '(', OP (GRI), ',', OP (GRJ), ')', ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_cldbf, { 0x1a00000 }
  },
/* csthfu$pack $FRintk,@($GRi,$GRj),$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRINTK), ',', '@', '(', OP (GRI), ',', OP (GRJ), ')', ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_cldbf, { 0x1a00040 }
  },
/* cstfu$pack $FRintk,@($GRi,$GRj),$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRINTK), ',', '@', '(', OP (GRI), ',', OP (GRJ), ')', ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_cldbf, { 0x1a00080 }
  },
/* cstdu$pack $GRdoublek,@($GRi,$GRj),$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (GRDOUBLEK), ',', '@', '(', OP (GRI), ',', OP (GRJ), ')', ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_csmul, { 0x19c00c0 }
  },
/* cstdfu$pack $FRdoublek,@($GRi,$GRj),$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRDOUBLEK), ',', '@', '(', OP (GRI), ',', OP (GRJ), ')', ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_clddf, { 0x1a000c0 }
  },
/* stbi$pack $GRk,@($GRi,$d12) */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (GRK), ',', '@', '(', OP (GRI), ',', OP (D12), ')', 0 } },
    & ifmt_ldsbi, { 0x1400000 }
  },
/* sthi$pack $GRk,@($GRi,$d12) */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (GRK), ',', '@', '(', OP (GRI), ',', OP (D12), ')', 0 } },
    & ifmt_ldsbi, { 0x1440000 }
  },
/* sti$pack $GRk,@($GRi,$d12) */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (GRK), ',', '@', '(', OP (GRI), ',', OP (D12), ')', 0 } },
    & ifmt_ldsbi, { 0x1480000 }
  },
/* stbfi$pack $FRintk,@($GRi,$d12) */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRINTK), ',', '@', '(', OP (GRI), ',', OP (D12), ')', 0 } },
    & ifmt_ldbfi, { 0x1380000 }
  },
/* sthfi$pack $FRintk,@($GRi,$d12) */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRINTK), ',', '@', '(', OP (GRI), ',', OP (D12), ')', 0 } },
    & ifmt_ldbfi, { 0x13c0000 }
  },
/* stfi$pack $FRintk,@($GRi,$d12) */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRINTK), ',', '@', '(', OP (GRI), ',', OP (D12), ')', 0 } },
    & ifmt_ldbfi, { 0x1540000 }
  },
/* stdi$pack $GRdoublek,@($GRi,$d12) */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (GRDOUBLEK), ',', '@', '(', OP (GRI), ',', OP (D12), ')', 0 } },
    & ifmt_lddi, { 0x14c0000 }
  },
/* stdfi$pack $FRdoublek,@($GRi,$d12) */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRDOUBLEK), ',', '@', '(', OP (GRI), ',', OP (D12), ')', 0 } },
    & ifmt_lddfi, { 0x1580000 }
  },
/* stqi$pack $GRk,@($GRi,$d12) */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (GRK), ',', '@', '(', OP (GRI), ',', OP (D12), ')', 0 } },
    & ifmt_ldsbi, { 0x1500000 }
  },
/* stqfi$pack $FRintk,@($GRi,$d12) */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRINTK), ',', '@', '(', OP (GRI), ',', OP (D12), ')', 0 } },
    & ifmt_ldbfi, { 0x15c0000 }
  },
/* swap$pack @($GRi,$GRj),$GRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', '@', '(', OP (GRI), ',', OP (GRJ), ')', ',', OP (GRK), 0 } },
    & ifmt_slass, { 0xc0140 }
  },
/* swapi$pack @($GRi,$d12),$GRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', '@', '(', OP (GRI), ',', OP (D12), ')', ',', OP (GRK), 0 } },
    & ifmt_ldsbi, { 0x1340000 }
  },
/* cswap$pack @($GRi,$GRj),$GRk,$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', '@', '(', OP (GRI), ',', OP (GRJ), ')', ',', OP (GRK), ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_cadd, { 0x1940080 }
  },
/* movgf$pack $GRj,$FRintk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (GRJ), ',', OP (FRINTK), 0 } },
    & ifmt_movgf, { 0xc0540 }
  },
/* movfg$pack $FRintk,$GRj */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRINTK), ',', OP (GRJ), 0 } },
    & ifmt_movgf, { 0xc0340 }
  },
/* movgfd$pack $GRj,$FRintk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (GRJ), ',', OP (FRINTK), 0 } },
    & ifmt_movgf, { 0xc0580 }
  },
/* movfgd$pack $FRintk,$GRj */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRINTK), ',', OP (GRJ), 0 } },
    & ifmt_movgf, { 0xc0380 }
  },
/* movgfq$pack $GRj,$FRintk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (GRJ), ',', OP (FRINTK), 0 } },
    & ifmt_movgf, { 0xc05c0 }
  },
/* movfgq$pack $FRintk,$GRj */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRINTK), ',', OP (GRJ), 0 } },
    & ifmt_movgf, { 0xc03c0 }
  },
/* cmovgf$pack $GRj,$FRintk,$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (GRJ), ',', OP (FRINTK), ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_cmovgf, { 0x1a40000 }
  },
/* cmovfg$pack $FRintk,$GRj,$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRINTK), ',', OP (GRJ), ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_cmovgf, { 0x1a40080 }
  },
/* cmovgfd$pack $GRj,$FRintk,$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (GRJ), ',', OP (FRINTK), ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_cmovgf, { 0x1a40040 }
  },
/* cmovfgd$pack $FRintk,$GRj,$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRINTK), ',', OP (GRJ), ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_cmovgf, { 0x1a400c0 }
  },
/* movgs$pack $GRj,$spr */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (GRJ), ',', OP (SPR), 0 } },
    & ifmt_movgs, { 0xc0180 }
  },
/* movsg$pack $spr,$GRj */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (SPR), ',', OP (GRJ), 0 } },
    & ifmt_movgs, { 0xc01c0 }
  },
/* bra$pack $hint_taken$label16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (HINT_TAKEN), OP (LABEL16), 0 } },
    & ifmt_bra, { 0x40180000 }
  },
/* bno$pack$hint_not_taken */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), OP (HINT_NOT_TAKEN), 0 } },
    & ifmt_bno, { 0x180000 }
  },
/* beq$pack $ICCi_2,$hint,$label16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (ICCI_2), ',', OP (HINT), ',', OP (LABEL16), 0 } },
    & ifmt_beq, { 0x20180000 }
  },
/* bne$pack $ICCi_2,$hint,$label16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (ICCI_2), ',', OP (HINT), ',', OP (LABEL16), 0 } },
    & ifmt_beq, { 0x60180000 }
  },
/* ble$pack $ICCi_2,$hint,$label16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (ICCI_2), ',', OP (HINT), ',', OP (LABEL16), 0 } },
    & ifmt_beq, { 0x38180000 }
  },
/* bgt$pack $ICCi_2,$hint,$label16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (ICCI_2), ',', OP (HINT), ',', OP (LABEL16), 0 } },
    & ifmt_beq, { 0x78180000 }
  },
/* blt$pack $ICCi_2,$hint,$label16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (ICCI_2), ',', OP (HINT), ',', OP (LABEL16), 0 } },
    & ifmt_beq, { 0x18180000 }
  },
/* bge$pack $ICCi_2,$hint,$label16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (ICCI_2), ',', OP (HINT), ',', OP (LABEL16), 0 } },
    & ifmt_beq, { 0x58180000 }
  },
/* bls$pack $ICCi_2,$hint,$label16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (ICCI_2), ',', OP (HINT), ',', OP (LABEL16), 0 } },
    & ifmt_beq, { 0x28180000 }
  },
/* bhi$pack $ICCi_2,$hint,$label16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (ICCI_2), ',', OP (HINT), ',', OP (LABEL16), 0 } },
    & ifmt_beq, { 0x68180000 }
  },
/* bc$pack $ICCi_2,$hint,$label16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (ICCI_2), ',', OP (HINT), ',', OP (LABEL16), 0 } },
    & ifmt_beq, { 0x8180000 }
  },
/* bnc$pack $ICCi_2,$hint,$label16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (ICCI_2), ',', OP (HINT), ',', OP (LABEL16), 0 } },
    & ifmt_beq, { 0x48180000 }
  },
/* bn$pack $ICCi_2,$hint,$label16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (ICCI_2), ',', OP (HINT), ',', OP (LABEL16), 0 } },
    & ifmt_beq, { 0x30180000 }
  },
/* bp$pack $ICCi_2,$hint,$label16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (ICCI_2), ',', OP (HINT), ',', OP (LABEL16), 0 } },
    & ifmt_beq, { 0x70180000 }
  },
/* bv$pack $ICCi_2,$hint,$label16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (ICCI_2), ',', OP (HINT), ',', OP (LABEL16), 0 } },
    & ifmt_beq, { 0x10180000 }
  },
/* bnv$pack $ICCi_2,$hint,$label16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (ICCI_2), ',', OP (HINT), ',', OP (LABEL16), 0 } },
    & ifmt_beq, { 0x50180000 }
  },
/* fbra$pack $hint_taken$label16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (HINT_TAKEN), OP (LABEL16), 0 } },
    & ifmt_fbra, { 0x781c0000 }
  },
/* fbno$pack$hint_not_taken */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), OP (HINT_NOT_TAKEN), 0 } },
    & ifmt_fbno, { 0x1c0000 }
  },
/* fbne$pack $FCCi_2,$hint,$label16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FCCI_2), ',', OP (HINT), ',', OP (LABEL16), 0 } },
    & ifmt_fbne, { 0x381c0000 }
  },
/* fbeq$pack $FCCi_2,$hint,$label16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FCCI_2), ',', OP (HINT), ',', OP (LABEL16), 0 } },
    & ifmt_fbne, { 0x401c0000 }
  },
/* fblg$pack $FCCi_2,$hint,$label16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FCCI_2), ',', OP (HINT), ',', OP (LABEL16), 0 } },
    & ifmt_fbne, { 0x301c0000 }
  },
/* fbue$pack $FCCi_2,$hint,$label16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FCCI_2), ',', OP (HINT), ',', OP (LABEL16), 0 } },
    & ifmt_fbne, { 0x481c0000 }
  },
/* fbul$pack $FCCi_2,$hint,$label16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FCCI_2), ',', OP (HINT), ',', OP (LABEL16), 0 } },
    & ifmt_fbne, { 0x281c0000 }
  },
/* fbge$pack $FCCi_2,$hint,$label16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FCCI_2), ',', OP (HINT), ',', OP (LABEL16), 0 } },
    & ifmt_fbne, { 0x501c0000 }
  },
/* fblt$pack $FCCi_2,$hint,$label16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FCCI_2), ',', OP (HINT), ',', OP (LABEL16), 0 } },
    & ifmt_fbne, { 0x201c0000 }
  },
/* fbuge$pack $FCCi_2,$hint,$label16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FCCI_2), ',', OP (HINT), ',', OP (LABEL16), 0 } },
    & ifmt_fbne, { 0x581c0000 }
  },
/* fbug$pack $FCCi_2,$hint,$label16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FCCI_2), ',', OP (HINT), ',', OP (LABEL16), 0 } },
    & ifmt_fbne, { 0x181c0000 }
  },
/* fble$pack $FCCi_2,$hint,$label16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FCCI_2), ',', OP (HINT), ',', OP (LABEL16), 0 } },
    & ifmt_fbne, { 0x601c0000 }
  },
/* fbgt$pack $FCCi_2,$hint,$label16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FCCI_2), ',', OP (HINT), ',', OP (LABEL16), 0 } },
    & ifmt_fbne, { 0x101c0000 }
  },
/* fbule$pack $FCCi_2,$hint,$label16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FCCI_2), ',', OP (HINT), ',', OP (LABEL16), 0 } },
    & ifmt_fbne, { 0x681c0000 }
  },
/* fbu$pack $FCCi_2,$hint,$label16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FCCI_2), ',', OP (HINT), ',', OP (LABEL16), 0 } },
    & ifmt_fbne, { 0x81c0000 }
  },
/* fbo$pack $FCCi_2,$hint,$label16 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FCCI_2), ',', OP (HINT), ',', OP (LABEL16), 0 } },
    & ifmt_fbne, { 0x701c0000 }
  },
/* bctrlr$pack $ccond,$hint */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (CCOND), ',', OP (HINT), 0 } },
    & ifmt_bctrlr, { 0x382000 }
  },
/* bralr$pack$hint_taken */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), OP (HINT_TAKEN), 0 } },
    & ifmt_bralr, { 0x40384000 }
  },
/* bnolr$pack$hint_not_taken */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), OP (HINT_NOT_TAKEN), 0 } },
    & ifmt_bnolr, { 0x384000 }
  },
/* beqlr$pack $ICCi_2,$hint */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (ICCI_2), ',', OP (HINT), 0 } },
    & ifmt_beqlr, { 0x20384000 }
  },
/* bnelr$pack $ICCi_2,$hint */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (ICCI_2), ',', OP (HINT), 0 } },
    & ifmt_beqlr, { 0x60384000 }
  },
/* blelr$pack $ICCi_2,$hint */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (ICCI_2), ',', OP (HINT), 0 } },
    & ifmt_beqlr, { 0x38384000 }
  },
/* bgtlr$pack $ICCi_2,$hint */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (ICCI_2), ',', OP (HINT), 0 } },
    & ifmt_beqlr, { 0x78384000 }
  },
/* bltlr$pack $ICCi_2,$hint */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (ICCI_2), ',', OP (HINT), 0 } },
    & ifmt_beqlr, { 0x18384000 }
  },
/* bgelr$pack $ICCi_2,$hint */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (ICCI_2), ',', OP (HINT), 0 } },
    & ifmt_beqlr, { 0x58384000 }
  },
/* blslr$pack $ICCi_2,$hint */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (ICCI_2), ',', OP (HINT), 0 } },
    & ifmt_beqlr, { 0x28384000 }
  },
/* bhilr$pack $ICCi_2,$hint */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (ICCI_2), ',', OP (HINT), 0 } },
    & ifmt_beqlr, { 0x68384000 }
  },
/* bclr$pack $ICCi_2,$hint */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (ICCI_2), ',', OP (HINT), 0 } },
    & ifmt_beqlr, { 0x8384000 }
  },
/* bnclr$pack $ICCi_2,$hint */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (ICCI_2), ',', OP (HINT), 0 } },
    & ifmt_beqlr, { 0x48384000 }
  },
/* bnlr$pack $ICCi_2,$hint */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (ICCI_2), ',', OP (HINT), 0 } },
    & ifmt_beqlr, { 0x30384000 }
  },
/* bplr$pack $ICCi_2,$hint */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (ICCI_2), ',', OP (HINT), 0 } },
    & ifmt_beqlr, { 0x70384000 }
  },
/* bvlr$pack $ICCi_2,$hint */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (ICCI_2), ',', OP (HINT), 0 } },
    & ifmt_beqlr, { 0x10384000 }
  },
/* bnvlr$pack $ICCi_2,$hint */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (ICCI_2), ',', OP (HINT), 0 } },
    & ifmt_beqlr, { 0x50384000 }
  },
/* fbralr$pack$hint_taken */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), OP (HINT_TAKEN), 0 } },
    & ifmt_fbralr, { 0x7838c000 }
  },
/* fbnolr$pack$hint_not_taken */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), OP (HINT_NOT_TAKEN), 0 } },
    & ifmt_fbnolr, { 0x38c000 }
  },
/* fbeqlr$pack $FCCi_2,$hint */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FCCI_2), ',', OP (HINT), 0 } },
    & ifmt_fbeqlr, { 0x4038c000 }
  },
/* fbnelr$pack $FCCi_2,$hint */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FCCI_2), ',', OP (HINT), 0 } },
    & ifmt_fbeqlr, { 0x3838c000 }
  },
/* fblglr$pack $FCCi_2,$hint */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FCCI_2), ',', OP (HINT), 0 } },
    & ifmt_fbeqlr, { 0x3038c000 }
  },
/* fbuelr$pack $FCCi_2,$hint */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FCCI_2), ',', OP (HINT), 0 } },
    & ifmt_fbeqlr, { 0x4838c000 }
  },
/* fbullr$pack $FCCi_2,$hint */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FCCI_2), ',', OP (HINT), 0 } },
    & ifmt_fbeqlr, { 0x2838c000 }
  },
/* fbgelr$pack $FCCi_2,$hint */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FCCI_2), ',', OP (HINT), 0 } },
    & ifmt_fbeqlr, { 0x5038c000 }
  },
/* fbltlr$pack $FCCi_2,$hint */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FCCI_2), ',', OP (HINT), 0 } },
    & ifmt_fbeqlr, { 0x2038c000 }
  },
/* fbugelr$pack $FCCi_2,$hint */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FCCI_2), ',', OP (HINT), 0 } },
    & ifmt_fbeqlr, { 0x5838c000 }
  },
/* fbuglr$pack $FCCi_2,$hint */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FCCI_2), ',', OP (HINT), 0 } },
    & ifmt_fbeqlr, { 0x1838c000 }
  },
/* fblelr$pack $FCCi_2,$hint */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FCCI_2), ',', OP (HINT), 0 } },
    & ifmt_fbeqlr, { 0x6038c000 }
  },
/* fbgtlr$pack $FCCi_2,$hint */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FCCI_2), ',', OP (HINT), 0 } },
    & ifmt_fbeqlr, { 0x1038c000 }
  },
/* fbulelr$pack $FCCi_2,$hint */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FCCI_2), ',', OP (HINT), 0 } },
    & ifmt_fbeqlr, { 0x6838c000 }
  },
/* fbulr$pack $FCCi_2,$hint */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FCCI_2), ',', OP (HINT), 0 } },
    & ifmt_fbeqlr, { 0x838c000 }
  },
/* fbolr$pack $FCCi_2,$hint */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FCCI_2), ',', OP (HINT), 0 } },
    & ifmt_fbeqlr, { 0x7038c000 }
  },
/* bcralr$pack $ccond$hint_taken */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (CCOND), OP (HINT_TAKEN), 0 } },
    & ifmt_bcralr, { 0x40386000 }
  },
/* bcnolr$pack$hint_not_taken */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), OP (HINT_NOT_TAKEN), 0 } },
    & ifmt_bnolr, { 0x386000 }
  },
/* bceqlr$pack $ICCi_2,$ccond,$hint */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (ICCI_2), ',', OP (CCOND), ',', OP (HINT), 0 } },
    & ifmt_bceqlr, { 0x20386000 }
  },
/* bcnelr$pack $ICCi_2,$ccond,$hint */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (ICCI_2), ',', OP (CCOND), ',', OP (HINT), 0 } },
    & ifmt_bceqlr, { 0x60386000 }
  },
/* bclelr$pack $ICCi_2,$ccond,$hint */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (ICCI_2), ',', OP (CCOND), ',', OP (HINT), 0 } },
    & ifmt_bceqlr, { 0x38386000 }
  },
/* bcgtlr$pack $ICCi_2,$ccond,$hint */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (ICCI_2), ',', OP (CCOND), ',', OP (HINT), 0 } },
    & ifmt_bceqlr, { 0x78386000 }
  },
/* bcltlr$pack $ICCi_2,$ccond,$hint */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (ICCI_2), ',', OP (CCOND), ',', OP (HINT), 0 } },
    & ifmt_bceqlr, { 0x18386000 }
  },
/* bcgelr$pack $ICCi_2,$ccond,$hint */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (ICCI_2), ',', OP (CCOND), ',', OP (HINT), 0 } },
    & ifmt_bceqlr, { 0x58386000 }
  },
/* bclslr$pack $ICCi_2,$ccond,$hint */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (ICCI_2), ',', OP (CCOND), ',', OP (HINT), 0 } },
    & ifmt_bceqlr, { 0x28386000 }
  },
/* bchilr$pack $ICCi_2,$ccond,$hint */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (ICCI_2), ',', OP (CCOND), ',', OP (HINT), 0 } },
    & ifmt_bceqlr, { 0x68386000 }
  },
/* bcclr$pack $ICCi_2,$ccond,$hint */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (ICCI_2), ',', OP (CCOND), ',', OP (HINT), 0 } },
    & ifmt_bceqlr, { 0x8386000 }
  },
/* bcnclr$pack $ICCi_2,$ccond,$hint */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (ICCI_2), ',', OP (CCOND), ',', OP (HINT), 0 } },
    & ifmt_bceqlr, { 0x48386000 }
  },
/* bcnlr$pack $ICCi_2,$ccond,$hint */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (ICCI_2), ',', OP (CCOND), ',', OP (HINT), 0 } },
    & ifmt_bceqlr, { 0x30386000 }
  },
/* bcplr$pack $ICCi_2,$ccond,$hint */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (ICCI_2), ',', OP (CCOND), ',', OP (HINT), 0 } },
    & ifmt_bceqlr, { 0x70386000 }
  },
/* bcvlr$pack $ICCi_2,$ccond,$hint */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (ICCI_2), ',', OP (CCOND), ',', OP (HINT), 0 } },
    & ifmt_bceqlr, { 0x10386000 }
  },
/* bcnvlr$pack $ICCi_2,$ccond,$hint */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (ICCI_2), ',', OP (CCOND), ',', OP (HINT), 0 } },
    & ifmt_bceqlr, { 0x50386000 }
  },
/* fcbralr$pack $ccond$hint_taken */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (CCOND), OP (HINT_TAKEN), 0 } },
    & ifmt_fcbralr, { 0x7838e000 }
  },
/* fcbnolr$pack$hint_not_taken */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), OP (HINT_NOT_TAKEN), 0 } },
    & ifmt_fbnolr, { 0x38e000 }
  },
/* fcbeqlr$pack $FCCi_2,$ccond,$hint */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FCCI_2), ',', OP (CCOND), ',', OP (HINT), 0 } },
    & ifmt_fcbeqlr, { 0x4038e000 }
  },
/* fcbnelr$pack $FCCi_2,$ccond,$hint */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FCCI_2), ',', OP (CCOND), ',', OP (HINT), 0 } },
    & ifmt_fcbeqlr, { 0x3838e000 }
  },
/* fcblglr$pack $FCCi_2,$ccond,$hint */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FCCI_2), ',', OP (CCOND), ',', OP (HINT), 0 } },
    & ifmt_fcbeqlr, { 0x3038e000 }
  },
/* fcbuelr$pack $FCCi_2,$ccond,$hint */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FCCI_2), ',', OP (CCOND), ',', OP (HINT), 0 } },
    & ifmt_fcbeqlr, { 0x4838e000 }
  },
/* fcbullr$pack $FCCi_2,$ccond,$hint */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FCCI_2), ',', OP (CCOND), ',', OP (HINT), 0 } },
    & ifmt_fcbeqlr, { 0x2838e000 }
  },
/* fcbgelr$pack $FCCi_2,$ccond,$hint */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FCCI_2), ',', OP (CCOND), ',', OP (HINT), 0 } },
    & ifmt_fcbeqlr, { 0x5038e000 }
  },
/* fcbltlr$pack $FCCi_2,$ccond,$hint */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FCCI_2), ',', OP (CCOND), ',', OP (HINT), 0 } },
    & ifmt_fcbeqlr, { 0x2038e000 }
  },
/* fcbugelr$pack $FCCi_2,$ccond,$hint */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FCCI_2), ',', OP (CCOND), ',', OP (HINT), 0 } },
    & ifmt_fcbeqlr, { 0x5838e000 }
  },
/* fcbuglr$pack $FCCi_2,$ccond,$hint */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FCCI_2), ',', OP (CCOND), ',', OP (HINT), 0 } },
    & ifmt_fcbeqlr, { 0x1838e000 }
  },
/* fcblelr$pack $FCCi_2,$ccond,$hint */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FCCI_2), ',', OP (CCOND), ',', OP (HINT), 0 } },
    & ifmt_fcbeqlr, { 0x6038e000 }
  },
/* fcbgtlr$pack $FCCi_2,$ccond,$hint */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FCCI_2), ',', OP (CCOND), ',', OP (HINT), 0 } },
    & ifmt_fcbeqlr, { 0x1038e000 }
  },
/* fcbulelr$pack $FCCi_2,$ccond,$hint */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FCCI_2), ',', OP (CCOND), ',', OP (HINT), 0 } },
    & ifmt_fcbeqlr, { 0x6838e000 }
  },
/* fcbulr$pack $FCCi_2,$ccond,$hint */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FCCI_2), ',', OP (CCOND), ',', OP (HINT), 0 } },
    & ifmt_fcbeqlr, { 0x838e000 }
  },
/* fcbolr$pack $FCCi_2,$ccond,$hint */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FCCI_2), ',', OP (CCOND), ',', OP (HINT), 0 } },
    & ifmt_fcbeqlr, { 0x7038e000 }
  },
/* jmpl$pack @($GRi,$GRj) */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', '@', '(', OP (GRI), ',', OP (GRJ), ')', 0 } },
    & ifmt_jmpl, { 0x300000 }
  },
/* calll$pack $callann($GRi,$GRj) */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (CALLANN), '(', OP (GRI), ',', OP (GRJ), ')', 0 } },
    & ifmt_calll, { 0x2300000 }
  },
/* jmpil$pack @($GRi,$s12) */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', '@', '(', OP (GRI), ',', OP (S12), ')', 0 } },
    & ifmt_jmpil, { 0x340000 }
  },
/* callil$pack @($GRi,$s12) */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', '@', '(', OP (GRI), ',', OP (S12), ')', 0 } },
    & ifmt_callil, { 0x2340000 }
  },
/* call$pack $label24 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (LABEL24), 0 } },
    & ifmt_call, { 0x3c0000 }
  },
/* rett$pack $debug */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (DEBUG), 0 } },
    & ifmt_rett, { 0x140000 }
  },
/* rei$pack $eir */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (EIR), 0 } },
    & ifmt_rei, { 0xdc0000 }
  },
/* tra$pack $GRi,$GRj */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (GRI), ',', OP (GRJ), 0 } },
    & ifmt_tra, { 0x40100000 }
  },
/* tno$pack */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), 0 } },
    & ifmt_tno, { 0x100000 }
  },
/* teq$pack $ICCi_2,$GRi,$GRj */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (ICCI_2), ',', OP (GRI), ',', OP (GRJ), 0 } },
    & ifmt_teq, { 0x20100000 }
  },
/* tne$pack $ICCi_2,$GRi,$GRj */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (ICCI_2), ',', OP (GRI), ',', OP (GRJ), 0 } },
    & ifmt_teq, { 0x60100000 }
  },
/* tle$pack $ICCi_2,$GRi,$GRj */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (ICCI_2), ',', OP (GRI), ',', OP (GRJ), 0 } },
    & ifmt_teq, { 0x38100000 }
  },
/* tgt$pack $ICCi_2,$GRi,$GRj */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (ICCI_2), ',', OP (GRI), ',', OP (GRJ), 0 } },
    & ifmt_teq, { 0x78100000 }
  },
/* tlt$pack $ICCi_2,$GRi,$GRj */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (ICCI_2), ',', OP (GRI), ',', OP (GRJ), 0 } },
    & ifmt_teq, { 0x18100000 }
  },
/* tge$pack $ICCi_2,$GRi,$GRj */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (ICCI_2), ',', OP (GRI), ',', OP (GRJ), 0 } },
    & ifmt_teq, { 0x58100000 }
  },
/* tls$pack $ICCi_2,$GRi,$GRj */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (ICCI_2), ',', OP (GRI), ',', OP (GRJ), 0 } },
    & ifmt_teq, { 0x28100000 }
  },
/* thi$pack $ICCi_2,$GRi,$GRj */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (ICCI_2), ',', OP (GRI), ',', OP (GRJ), 0 } },
    & ifmt_teq, { 0x68100000 }
  },
/* tc$pack $ICCi_2,$GRi,$GRj */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (ICCI_2), ',', OP (GRI), ',', OP (GRJ), 0 } },
    & ifmt_teq, { 0x8100000 }
  },
/* tnc$pack $ICCi_2,$GRi,$GRj */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (ICCI_2), ',', OP (GRI), ',', OP (GRJ), 0 } },
    & ifmt_teq, { 0x48100000 }
  },
/* tn$pack $ICCi_2,$GRi,$GRj */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (ICCI_2), ',', OP (GRI), ',', OP (GRJ), 0 } },
    & ifmt_teq, { 0x30100000 }
  },
/* tp$pack $ICCi_2,$GRi,$GRj */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (ICCI_2), ',', OP (GRI), ',', OP (GRJ), 0 } },
    & ifmt_teq, { 0x70100000 }
  },
/* tv$pack $ICCi_2,$GRi,$GRj */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (ICCI_2), ',', OP (GRI), ',', OP (GRJ), 0 } },
    & ifmt_teq, { 0x10100000 }
  },
/* tnv$pack $ICCi_2,$GRi,$GRj */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (ICCI_2), ',', OP (GRI), ',', OP (GRJ), 0 } },
    & ifmt_teq, { 0x50100000 }
  },
/* ftra$pack $GRi,$GRj */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (GRI), ',', OP (GRJ), 0 } },
    & ifmt_ftra, { 0x78100040 }
  },
/* ftno$pack */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), 0 } },
    & ifmt_ftno, { 0x100040 }
  },
/* ftne$pack $FCCi_2,$GRi,$GRj */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FCCI_2), ',', OP (GRI), ',', OP (GRJ), 0 } },
    & ifmt_ftne, { 0x38100040 }
  },
/* fteq$pack $FCCi_2,$GRi,$GRj */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FCCI_2), ',', OP (GRI), ',', OP (GRJ), 0 } },
    & ifmt_ftne, { 0x40100040 }
  },
/* ftlg$pack $FCCi_2,$GRi,$GRj */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FCCI_2), ',', OP (GRI), ',', OP (GRJ), 0 } },
    & ifmt_ftne, { 0x30100040 }
  },
/* ftue$pack $FCCi_2,$GRi,$GRj */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FCCI_2), ',', OP (GRI), ',', OP (GRJ), 0 } },
    & ifmt_ftne, { 0x48100040 }
  },
/* ftul$pack $FCCi_2,$GRi,$GRj */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FCCI_2), ',', OP (GRI), ',', OP (GRJ), 0 } },
    & ifmt_ftne, { 0x28100040 }
  },
/* ftge$pack $FCCi_2,$GRi,$GRj */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FCCI_2), ',', OP (GRI), ',', OP (GRJ), 0 } },
    & ifmt_ftne, { 0x50100040 }
  },
/* ftlt$pack $FCCi_2,$GRi,$GRj */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FCCI_2), ',', OP (GRI), ',', OP (GRJ), 0 } },
    & ifmt_ftne, { 0x20100040 }
  },
/* ftuge$pack $FCCi_2,$GRi,$GRj */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FCCI_2), ',', OP (GRI), ',', OP (GRJ), 0 } },
    & ifmt_ftne, { 0x58100040 }
  },
/* ftug$pack $FCCi_2,$GRi,$GRj */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FCCI_2), ',', OP (GRI), ',', OP (GRJ), 0 } },
    & ifmt_ftne, { 0x18100040 }
  },
/* ftle$pack $FCCi_2,$GRi,$GRj */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FCCI_2), ',', OP (GRI), ',', OP (GRJ), 0 } },
    & ifmt_ftne, { 0x60100040 }
  },
/* ftgt$pack $FCCi_2,$GRi,$GRj */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FCCI_2), ',', OP (GRI), ',', OP (GRJ), 0 } },
    & ifmt_ftne, { 0x10100040 }
  },
/* ftule$pack $FCCi_2,$GRi,$GRj */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FCCI_2), ',', OP (GRI), ',', OP (GRJ), 0 } },
    & ifmt_ftne, { 0x68100040 }
  },
/* ftu$pack $FCCi_2,$GRi,$GRj */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FCCI_2), ',', OP (GRI), ',', OP (GRJ), 0 } },
    & ifmt_ftne, { 0x8100040 }
  },
/* fto$pack $FCCi_2,$GRi,$GRj */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FCCI_2), ',', OP (GRI), ',', OP (GRJ), 0 } },
    & ifmt_ftne, { 0x70100040 }
  },
/* tira$pack $GRi,$s12 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (GRI), ',', OP (S12), 0 } },
    & ifmt_tira, { 0x40700000 }
  },
/* tino$pack */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), 0 } },
    & ifmt_tino, { 0x700000 }
  },
/* tieq$pack $ICCi_2,$GRi,$s12 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (ICCI_2), ',', OP (GRI), ',', OP (S12), 0 } },
    & ifmt_tieq, { 0x20700000 }
  },
/* tine$pack $ICCi_2,$GRi,$s12 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (ICCI_2), ',', OP (GRI), ',', OP (S12), 0 } },
    & ifmt_tieq, { 0x60700000 }
  },
/* tile$pack $ICCi_2,$GRi,$s12 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (ICCI_2), ',', OP (GRI), ',', OP (S12), 0 } },
    & ifmt_tieq, { 0x38700000 }
  },
/* tigt$pack $ICCi_2,$GRi,$s12 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (ICCI_2), ',', OP (GRI), ',', OP (S12), 0 } },
    & ifmt_tieq, { 0x78700000 }
  },
/* tilt$pack $ICCi_2,$GRi,$s12 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (ICCI_2), ',', OP (GRI), ',', OP (S12), 0 } },
    & ifmt_tieq, { 0x18700000 }
  },
/* tige$pack $ICCi_2,$GRi,$s12 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (ICCI_2), ',', OP (GRI), ',', OP (S12), 0 } },
    & ifmt_tieq, { 0x58700000 }
  },
/* tils$pack $ICCi_2,$GRi,$s12 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (ICCI_2), ',', OP (GRI), ',', OP (S12), 0 } },
    & ifmt_tieq, { 0x28700000 }
  },
/* tihi$pack $ICCi_2,$GRi,$s12 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (ICCI_2), ',', OP (GRI), ',', OP (S12), 0 } },
    & ifmt_tieq, { 0x68700000 }
  },
/* tic$pack $ICCi_2,$GRi,$s12 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (ICCI_2), ',', OP (GRI), ',', OP (S12), 0 } },
    & ifmt_tieq, { 0x8700000 }
  },
/* tinc$pack $ICCi_2,$GRi,$s12 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (ICCI_2), ',', OP (GRI), ',', OP (S12), 0 } },
    & ifmt_tieq, { 0x48700000 }
  },
/* tin$pack $ICCi_2,$GRi,$s12 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (ICCI_2), ',', OP (GRI), ',', OP (S12), 0 } },
    & ifmt_tieq, { 0x30700000 }
  },
/* tip$pack $ICCi_2,$GRi,$s12 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (ICCI_2), ',', OP (GRI), ',', OP (S12), 0 } },
    & ifmt_tieq, { 0x70700000 }
  },
/* tiv$pack $ICCi_2,$GRi,$s12 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (ICCI_2), ',', OP (GRI), ',', OP (S12), 0 } },
    & ifmt_tieq, { 0x10700000 }
  },
/* tinv$pack $ICCi_2,$GRi,$s12 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (ICCI_2), ',', OP (GRI), ',', OP (S12), 0 } },
    & ifmt_tieq, { 0x50700000 }
  },
/* ftira$pack $GRi,$s12 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (GRI), ',', OP (S12), 0 } },
    & ifmt_ftira, { 0x78740000 }
  },
/* ftino$pack */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), 0 } },
    & ifmt_ftino, { 0x740000 }
  },
/* ftine$pack $FCCi_2,$GRi,$s12 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FCCI_2), ',', OP (GRI), ',', OP (S12), 0 } },
    & ifmt_ftine, { 0x38740000 }
  },
/* ftieq$pack $FCCi_2,$GRi,$s12 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FCCI_2), ',', OP (GRI), ',', OP (S12), 0 } },
    & ifmt_ftine, { 0x40740000 }
  },
/* ftilg$pack $FCCi_2,$GRi,$s12 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FCCI_2), ',', OP (GRI), ',', OP (S12), 0 } },
    & ifmt_ftine, { 0x30740000 }
  },
/* ftiue$pack $FCCi_2,$GRi,$s12 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FCCI_2), ',', OP (GRI), ',', OP (S12), 0 } },
    & ifmt_ftine, { 0x48740000 }
  },
/* ftiul$pack $FCCi_2,$GRi,$s12 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FCCI_2), ',', OP (GRI), ',', OP (S12), 0 } },
    & ifmt_ftine, { 0x28740000 }
  },
/* ftige$pack $FCCi_2,$GRi,$s12 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FCCI_2), ',', OP (GRI), ',', OP (S12), 0 } },
    & ifmt_ftine, { 0x50740000 }
  },
/* ftilt$pack $FCCi_2,$GRi,$s12 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FCCI_2), ',', OP (GRI), ',', OP (S12), 0 } },
    & ifmt_ftine, { 0x20740000 }
  },
/* ftiuge$pack $FCCi_2,$GRi,$s12 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FCCI_2), ',', OP (GRI), ',', OP (S12), 0 } },
    & ifmt_ftine, { 0x58740000 }
  },
/* ftiug$pack $FCCi_2,$GRi,$s12 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FCCI_2), ',', OP (GRI), ',', OP (S12), 0 } },
    & ifmt_ftine, { 0x18740000 }
  },
/* ftile$pack $FCCi_2,$GRi,$s12 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FCCI_2), ',', OP (GRI), ',', OP (S12), 0 } },
    & ifmt_ftine, { 0x60740000 }
  },
/* ftigt$pack $FCCi_2,$GRi,$s12 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FCCI_2), ',', OP (GRI), ',', OP (S12), 0 } },
    & ifmt_ftine, { 0x10740000 }
  },
/* ftiule$pack $FCCi_2,$GRi,$s12 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FCCI_2), ',', OP (GRI), ',', OP (S12), 0 } },
    & ifmt_ftine, { 0x68740000 }
  },
/* ftiu$pack $FCCi_2,$GRi,$s12 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FCCI_2), ',', OP (GRI), ',', OP (S12), 0 } },
    & ifmt_ftine, { 0x8740000 }
  },
/* ftio$pack $FCCi_2,$GRi,$s12 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FCCI_2), ',', OP (GRI), ',', OP (S12), 0 } },
    & ifmt_ftine, { 0x70740000 }
  },
/* break$pack */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), 0 } },
    & ifmt_break, { 0x1000c0 }
  },
/* mtrap$pack */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), 0 } },
    & ifmt_break, { 0x100080 }
  },
/* andcr$pack $CRi,$CRj,$CRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (CRI), ',', OP (CRJ), ',', OP (CRK), 0 } },
    & ifmt_andcr, { 0x280200 }
  },
/* orcr$pack $CRi,$CRj,$CRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (CRI), ',', OP (CRJ), ',', OP (CRK), 0 } },
    & ifmt_andcr, { 0x280240 }
  },
/* xorcr$pack $CRi,$CRj,$CRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (CRI), ',', OP (CRJ), ',', OP (CRK), 0 } },
    & ifmt_andcr, { 0x280280 }
  },
/* nandcr$pack $CRi,$CRj,$CRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (CRI), ',', OP (CRJ), ',', OP (CRK), 0 } },
    & ifmt_andcr, { 0x280300 }
  },
/* norcr$pack $CRi,$CRj,$CRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (CRI), ',', OP (CRJ), ',', OP (CRK), 0 } },
    & ifmt_andcr, { 0x280340 }
  },
/* andncr$pack $CRi,$CRj,$CRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (CRI), ',', OP (CRJ), ',', OP (CRK), 0 } },
    & ifmt_andcr, { 0x280400 }
  },
/* orncr$pack $CRi,$CRj,$CRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (CRI), ',', OP (CRJ), ',', OP (CRK), 0 } },
    & ifmt_andcr, { 0x280440 }
  },
/* nandncr$pack $CRi,$CRj,$CRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (CRI), ',', OP (CRJ), ',', OP (CRK), 0 } },
    & ifmt_andcr, { 0x280500 }
  },
/* norncr$pack $CRi,$CRj,$CRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (CRI), ',', OP (CRJ), ',', OP (CRK), 0 } },
    & ifmt_andcr, { 0x280540 }
  },
/* notcr$pack $CRj,$CRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (CRJ), ',', OP (CRK), 0 } },
    & ifmt_notcr, { 0x2802c0 }
  },
/* ckra$pack $CRj_int */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (CRJ_INT), 0 } },
    & ifmt_ckra, { 0x40200000 }
  },
/* ckno$pack $CRj_int */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (CRJ_INT), 0 } },
    & ifmt_ckra, { 0x200000 }
  },
/* ckeq$pack $ICCi_3,$CRj_int */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (ICCI_3), ',', OP (CRJ_INT), 0 } },
    & ifmt_ckeq, { 0x20200000 }
  },
/* ckne$pack $ICCi_3,$CRj_int */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (ICCI_3), ',', OP (CRJ_INT), 0 } },
    & ifmt_ckeq, { 0x60200000 }
  },
/* ckle$pack $ICCi_3,$CRj_int */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (ICCI_3), ',', OP (CRJ_INT), 0 } },
    & ifmt_ckeq, { 0x38200000 }
  },
/* ckgt$pack $ICCi_3,$CRj_int */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (ICCI_3), ',', OP (CRJ_INT), 0 } },
    & ifmt_ckeq, { 0x78200000 }
  },
/* cklt$pack $ICCi_3,$CRj_int */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (ICCI_3), ',', OP (CRJ_INT), 0 } },
    & ifmt_ckeq, { 0x18200000 }
  },
/* ckge$pack $ICCi_3,$CRj_int */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (ICCI_3), ',', OP (CRJ_INT), 0 } },
    & ifmt_ckeq, { 0x58200000 }
  },
/* ckls$pack $ICCi_3,$CRj_int */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (ICCI_3), ',', OP (CRJ_INT), 0 } },
    & ifmt_ckeq, { 0x28200000 }
  },
/* ckhi$pack $ICCi_3,$CRj_int */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (ICCI_3), ',', OP (CRJ_INT), 0 } },
    & ifmt_ckeq, { 0x68200000 }
  },
/* ckc$pack $ICCi_3,$CRj_int */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (ICCI_3), ',', OP (CRJ_INT), 0 } },
    & ifmt_ckeq, { 0x8200000 }
  },
/* cknc$pack $ICCi_3,$CRj_int */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (ICCI_3), ',', OP (CRJ_INT), 0 } },
    & ifmt_ckeq, { 0x48200000 }
  },
/* ckn$pack $ICCi_3,$CRj_int */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (ICCI_3), ',', OP (CRJ_INT), 0 } },
    & ifmt_ckeq, { 0x30200000 }
  },
/* ckp$pack $ICCi_3,$CRj_int */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (ICCI_3), ',', OP (CRJ_INT), 0 } },
    & ifmt_ckeq, { 0x70200000 }
  },
/* ckv$pack $ICCi_3,$CRj_int */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (ICCI_3), ',', OP (CRJ_INT), 0 } },
    & ifmt_ckeq, { 0x10200000 }
  },
/* cknv$pack $ICCi_3,$CRj_int */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (ICCI_3), ',', OP (CRJ_INT), 0 } },
    & ifmt_ckeq, { 0x50200000 }
  },
/* fckra$pack $CRj_float */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (CRJ_FLOAT), 0 } },
    & ifmt_fckra, { 0x78240000 }
  },
/* fckno$pack $CRj_float */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (CRJ_FLOAT), 0 } },
    & ifmt_fckra, { 0x240000 }
  },
/* fckne$pack $FCCi_3,$CRj_float */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FCCI_3), ',', OP (CRJ_FLOAT), 0 } },
    & ifmt_fckra, { 0x38240000 }
  },
/* fckeq$pack $FCCi_3,$CRj_float */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FCCI_3), ',', OP (CRJ_FLOAT), 0 } },
    & ifmt_fckra, { 0x40240000 }
  },
/* fcklg$pack $FCCi_3,$CRj_float */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FCCI_3), ',', OP (CRJ_FLOAT), 0 } },
    & ifmt_fckra, { 0x30240000 }
  },
/* fckue$pack $FCCi_3,$CRj_float */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FCCI_3), ',', OP (CRJ_FLOAT), 0 } },
    & ifmt_fckra, { 0x48240000 }
  },
/* fckul$pack $FCCi_3,$CRj_float */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FCCI_3), ',', OP (CRJ_FLOAT), 0 } },
    & ifmt_fckra, { 0x28240000 }
  },
/* fckge$pack $FCCi_3,$CRj_float */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FCCI_3), ',', OP (CRJ_FLOAT), 0 } },
    & ifmt_fckra, { 0x50240000 }
  },
/* fcklt$pack $FCCi_3,$CRj_float */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FCCI_3), ',', OP (CRJ_FLOAT), 0 } },
    & ifmt_fckra, { 0x20240000 }
  },
/* fckuge$pack $FCCi_3,$CRj_float */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FCCI_3), ',', OP (CRJ_FLOAT), 0 } },
    & ifmt_fckra, { 0x58240000 }
  },
/* fckug$pack $FCCi_3,$CRj_float */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FCCI_3), ',', OP (CRJ_FLOAT), 0 } },
    & ifmt_fckra, { 0x18240000 }
  },
/* fckle$pack $FCCi_3,$CRj_float */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FCCI_3), ',', OP (CRJ_FLOAT), 0 } },
    & ifmt_fckra, { 0x60240000 }
  },
/* fckgt$pack $FCCi_3,$CRj_float */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FCCI_3), ',', OP (CRJ_FLOAT), 0 } },
    & ifmt_fckra, { 0x10240000 }
  },
/* fckule$pack $FCCi_3,$CRj_float */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FCCI_3), ',', OP (CRJ_FLOAT), 0 } },
    & ifmt_fckra, { 0x68240000 }
  },
/* fcku$pack $FCCi_3,$CRj_float */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FCCI_3), ',', OP (CRJ_FLOAT), 0 } },
    & ifmt_fckra, { 0x8240000 }
  },
/* fcko$pack $FCCi_3,$CRj_float */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FCCI_3), ',', OP (CRJ_FLOAT), 0 } },
    & ifmt_fckra, { 0x70240000 }
  },
/* cckra$pack $CRj_int,$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (CRJ_INT), ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_cckra, { 0x41a80000 }
  },
/* cckno$pack $CRj_int,$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (CRJ_INT), ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_cckra, { 0x1a80000 }
  },
/* cckeq$pack $ICCi_3,$CRj_int,$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (ICCI_3), ',', OP (CRJ_INT), ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_cckeq, { 0x21a80000 }
  },
/* cckne$pack $ICCi_3,$CRj_int,$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (ICCI_3), ',', OP (CRJ_INT), ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_cckeq, { 0x61a80000 }
  },
/* cckle$pack $ICCi_3,$CRj_int,$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (ICCI_3), ',', OP (CRJ_INT), ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_cckeq, { 0x39a80000 }
  },
/* cckgt$pack $ICCi_3,$CRj_int,$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (ICCI_3), ',', OP (CRJ_INT), ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_cckeq, { 0x79a80000 }
  },
/* ccklt$pack $ICCi_3,$CRj_int,$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (ICCI_3), ',', OP (CRJ_INT), ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_cckeq, { 0x19a80000 }
  },
/* cckge$pack $ICCi_3,$CRj_int,$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (ICCI_3), ',', OP (CRJ_INT), ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_cckeq, { 0x59a80000 }
  },
/* cckls$pack $ICCi_3,$CRj_int,$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (ICCI_3), ',', OP (CRJ_INT), ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_cckeq, { 0x29a80000 }
  },
/* cckhi$pack $ICCi_3,$CRj_int,$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (ICCI_3), ',', OP (CRJ_INT), ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_cckeq, { 0x69a80000 }
  },
/* cckc$pack $ICCi_3,$CRj_int,$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (ICCI_3), ',', OP (CRJ_INT), ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_cckeq, { 0x9a80000 }
  },
/* ccknc$pack $ICCi_3,$CRj_int,$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (ICCI_3), ',', OP (CRJ_INT), ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_cckeq, { 0x49a80000 }
  },
/* cckn$pack $ICCi_3,$CRj_int,$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (ICCI_3), ',', OP (CRJ_INT), ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_cckeq, { 0x31a80000 }
  },
/* cckp$pack $ICCi_3,$CRj_int,$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (ICCI_3), ',', OP (CRJ_INT), ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_cckeq, { 0x71a80000 }
  },
/* cckv$pack $ICCi_3,$CRj_int,$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (ICCI_3), ',', OP (CRJ_INT), ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_cckeq, { 0x11a80000 }
  },
/* ccknv$pack $ICCi_3,$CRj_int,$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (ICCI_3), ',', OP (CRJ_INT), ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_cckeq, { 0x51a80000 }
  },
/* cfckra$pack $CRj_float,$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (CRJ_FLOAT), ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_cfckra, { 0x79a80040 }
  },
/* cfckno$pack $CRj_float,$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (CRJ_FLOAT), ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_cfckra, { 0x1a80040 }
  },
/* cfckne$pack $FCCi_3,$CRj_float,$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FCCI_3), ',', OP (CRJ_FLOAT), ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_cfckne, { 0x39a80040 }
  },
/* cfckeq$pack $FCCi_3,$CRj_float,$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FCCI_3), ',', OP (CRJ_FLOAT), ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_cfckne, { 0x41a80040 }
  },
/* cfcklg$pack $FCCi_3,$CRj_float,$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FCCI_3), ',', OP (CRJ_FLOAT), ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_cfckne, { 0x31a80040 }
  },
/* cfckue$pack $FCCi_3,$CRj_float,$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FCCI_3), ',', OP (CRJ_FLOAT), ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_cfckne, { 0x49a80040 }
  },
/* cfckul$pack $FCCi_3,$CRj_float,$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FCCI_3), ',', OP (CRJ_FLOAT), ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_cfckne, { 0x29a80040 }
  },
/* cfckge$pack $FCCi_3,$CRj_float,$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FCCI_3), ',', OP (CRJ_FLOAT), ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_cfckne, { 0x51a80040 }
  },
/* cfcklt$pack $FCCi_3,$CRj_float,$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FCCI_3), ',', OP (CRJ_FLOAT), ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_cfckne, { 0x21a80040 }
  },
/* cfckuge$pack $FCCi_3,$CRj_float,$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FCCI_3), ',', OP (CRJ_FLOAT), ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_cfckne, { 0x59a80040 }
  },
/* cfckug$pack $FCCi_3,$CRj_float,$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FCCI_3), ',', OP (CRJ_FLOAT), ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_cfckne, { 0x19a80040 }
  },
/* cfckle$pack $FCCi_3,$CRj_float,$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FCCI_3), ',', OP (CRJ_FLOAT), ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_cfckne, { 0x61a80040 }
  },
/* cfckgt$pack $FCCi_3,$CRj_float,$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FCCI_3), ',', OP (CRJ_FLOAT), ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_cfckne, { 0x11a80040 }
  },
/* cfckule$pack $FCCi_3,$CRj_float,$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FCCI_3), ',', OP (CRJ_FLOAT), ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_cfckne, { 0x69a80040 }
  },
/* cfcku$pack $FCCi_3,$CRj_float,$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FCCI_3), ',', OP (CRJ_FLOAT), ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_cfckne, { 0x9a80040 }
  },
/* cfcko$pack $FCCi_3,$CRj_float,$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FCCI_3), ',', OP (CRJ_FLOAT), ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_cfckne, { 0x71a80040 }
  },
/* cjmpl$pack @($GRi,$GRj),$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', '@', '(', OP (GRI), ',', OP (GRJ), ')', ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_cjmpl, { 0x1a80080 }
  },
/* ccalll$pack @($GRi,$GRj),$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', '@', '(', OP (GRI), ',', OP (GRJ), ')', ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_ccalll, { 0x3a80080 }
  },
/* ici$pack @($GRi,$GRj) */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', '@', '(', OP (GRI), ',', OP (GRJ), ')', 0 } },
    & ifmt_smu, { 0xc0e00 }
  },
/* dci$pack @($GRi,$GRj) */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', '@', '(', OP (GRI), ',', OP (GRJ), ')', 0 } },
    & ifmt_smu, { 0xc0f00 }
  },
/* icei$pack @($GRi,$GRj),$ae */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', '@', '(', OP (GRI), ',', OP (GRJ), ')', ',', OP (AE), 0 } },
    & ifmt_icei, { 0xc0e40 }
  },
/* dcei$pack @($GRi,$GRj),$ae */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', '@', '(', OP (GRI), ',', OP (GRJ), ')', ',', OP (AE), 0 } },
    & ifmt_icei, { 0xc0e80 }
  },
/* dcf$pack @($GRi,$GRj) */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', '@', '(', OP (GRI), ',', OP (GRJ), ')', 0 } },
    & ifmt_smu, { 0xc0f40 }
  },
/* dcef$pack @($GRi,$GRj),$ae */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', '@', '(', OP (GRI), ',', OP (GRJ), ')', ',', OP (AE), 0 } },
    & ifmt_icei, { 0xc0ec0 }
  },
/* witlb$pack $GRk,@($GRi,$GRj) */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (GRK), ',', '@', '(', OP (GRI), ',', OP (GRJ), ')', 0 } },
    & ifmt_slass, { 0xc0c80 }
  },
/* wdtlb$pack $GRk,@($GRi,$GRj) */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (GRK), ',', '@', '(', OP (GRI), ',', OP (GRJ), ')', 0 } },
    & ifmt_slass, { 0xc0d80 }
  },
/* itlbi$pack @($GRi,$GRj) */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', '@', '(', OP (GRI), ',', OP (GRJ), ')', 0 } },
    & ifmt_smu, { 0xc0cc0 }
  },
/* dtlbi$pack @($GRi,$GRj) */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', '@', '(', OP (GRI), ',', OP (GRJ), ')', 0 } },
    & ifmt_smu, { 0xc0dc0 }
  },
/* icpl$pack $GRi,$GRj,$lock */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (GRI), ',', OP (GRJ), ',', OP (LOCK), 0 } },
    & ifmt_icpl, { 0xc0c00 }
  },
/* dcpl$pack $GRi,$GRj,$lock */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (GRI), ',', OP (GRJ), ',', OP (LOCK), 0 } },
    & ifmt_icpl, { 0xc0d00 }
  },
/* icul$pack $GRi */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (GRI), 0 } },
    & ifmt_icul, { 0xc0c40 }
  },
/* dcul$pack $GRi */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (GRI), 0 } },
    & ifmt_icul, { 0xc0d40 }
  },
/* bar$pack */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), 0 } },
    & ifmt_bar, { 0xc0f80 }
  },
/* membar$pack */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), 0 } },
    & ifmt_bar, { 0xc0fc0 }
  },
/* lrai$pack $GRi,$GRk,$LRAE,$LRAD,$LRAS */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (GRI), ',', OP (GRK), ',', OP (LRAE), ',', OP (LRAD), ',', OP (LRAS), 0 } },
    & ifmt_lrai, { 0xc0800 }
  },
/* lrad$pack $GRi,$GRk,$LRAE,$LRAD,$LRAS */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (GRI), ',', OP (GRK), ',', OP (LRAE), ',', OP (LRAD), ',', OP (LRAS), 0 } },
    & ifmt_lrai, { 0xc0840 }
  },
/* tlbpr$pack $GRi,$GRj,$TLBPRopx,$TLBPRL */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (GRI), ',', OP (GRJ), ',', OP (TLBPROPX), ',', OP (TLBPRL), 0 } },
    & ifmt_tlbpr, { 0xc0900 }
  },
/* cop1$pack $s6_1,$CPRi,$CPRj,$CPRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (S6_1), ',', OP (CPRI), ',', OP (CPRJ), ',', OP (CPRK), 0 } },
    & ifmt_cop1, { 0x1f80000 }
  },
/* cop2$pack $s6_1,$CPRi,$CPRj,$CPRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (S6_1), ',', OP (CPRI), ',', OP (CPRJ), ',', OP (CPRK), 0 } },
    & ifmt_cop1, { 0x1fc0000 }
  },
/* clrgr$pack $GRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (GRK), 0 } },
    & ifmt_clrgr, { 0x280000 }
  },
/* clrfr$pack $FRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRK), 0 } },
    & ifmt_clrfr, { 0x280080 }
  },
/* clrga$pack */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), 0 } },
    & ifmt_bar, { 0x280040 }
  },
/* clrfa$pack */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), 0 } },
    & ifmt_bar, { 0x2800c0 }
  },
/* commitgr$pack $GRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (GRK), 0 } },
    & ifmt_clrgr, { 0x280100 }
  },
/* commitfr$pack $FRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRK), 0 } },
    & ifmt_clrfr, { 0x280180 }
  },
/* commitga$pack */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), 0 } },
    & ifmt_bar, { 0x280140 }
  },
/* commitfa$pack */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), 0 } },
    & ifmt_bar, { 0x2801c0 }
  },
/* fitos$pack $FRintj,$FRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRINTJ), ',', OP (FRK), 0 } },
    & ifmt_fitos, { 0x1e40000 }
  },
/* fstoi$pack $FRj,$FRintk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRJ), ',', OP (FRINTK), 0 } },
    & ifmt_fstoi, { 0x1e40040 }
  },
/* fitod$pack $FRintj,$FRdoublek */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRINTJ), ',', OP (FRDOUBLEK), 0 } },
    & ifmt_fitod, { 0x1e80000 }
  },
/* fdtoi$pack $FRdoublej,$FRintk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRDOUBLEJ), ',', OP (FRINTK), 0 } },
    & ifmt_fdtoi, { 0x1e80040 }
  },
/* fditos$pack $FRintj,$FRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRINTJ), ',', OP (FRK), 0 } },
    & ifmt_fitos, { 0x1e40400 }
  },
/* fdstoi$pack $FRj,$FRintk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRJ), ',', OP (FRINTK), 0 } },
    & ifmt_fstoi, { 0x1e40440 }
  },
/* nfditos$pack $FRintj,$FRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRINTJ), ',', OP (FRK), 0 } },
    & ifmt_fitos, { 0x1e40c00 }
  },
/* nfdstoi$pack $FRj,$FRintk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRJ), ',', OP (FRINTK), 0 } },
    & ifmt_fstoi, { 0x1e40c40 }
  },
/* cfitos$pack $FRintj,$FRk,$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRINTJ), ',', OP (FRK), ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_cfitos, { 0x1ac0000 }
  },
/* cfstoi$pack $FRj,$FRintk,$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRJ), ',', OP (FRINTK), ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_cfstoi, { 0x1ac0040 }
  },
/* nfitos$pack $FRintj,$FRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRINTJ), ',', OP (FRK), 0 } },
    & ifmt_fitos, { 0x1e40800 }
  },
/* nfstoi$pack $FRj,$FRintk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRJ), ',', OP (FRINTK), 0 } },
    & ifmt_fstoi, { 0x1e40840 }
  },
/* fmovs$pack $FRj,$FRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRJ), ',', OP (FRK), 0 } },
    & ifmt_fmovs, { 0x1e40080 }
  },
/* fmovd$pack $FRdoublej,$FRdoublek */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRDOUBLEJ), ',', OP (FRDOUBLEK), 0 } },
    & ifmt_fmovd, { 0x1e80080 }
  },
/* fdmovs$pack $FRj,$FRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRJ), ',', OP (FRK), 0 } },
    & ifmt_fmovs, { 0x1e40480 }
  },
/* cfmovs$pack $FRj,$FRk,$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRJ), ',', OP (FRK), ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_cfmovs, { 0x1b00000 }
  },
/* fnegs$pack $FRj,$FRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRJ), ',', OP (FRK), 0 } },
    & ifmt_fmovs, { 0x1e400c0 }
  },
/* fnegd$pack $FRdoublej,$FRdoublek */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRDOUBLEJ), ',', OP (FRDOUBLEK), 0 } },
    & ifmt_fmovd, { 0x1e800c0 }
  },
/* fdnegs$pack $FRj,$FRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRJ), ',', OP (FRK), 0 } },
    & ifmt_fmovs, { 0x1e404c0 }
  },
/* cfnegs$pack $FRj,$FRk,$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRJ), ',', OP (FRK), ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_cfmovs, { 0x1b00040 }
  },
/* fabss$pack $FRj,$FRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRJ), ',', OP (FRK), 0 } },
    & ifmt_fmovs, { 0x1e40100 }
  },
/* fabsd$pack $FRdoublej,$FRdoublek */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRDOUBLEJ), ',', OP (FRDOUBLEK), 0 } },
    & ifmt_fmovd, { 0x1e80100 }
  },
/* fdabss$pack $FRj,$FRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRJ), ',', OP (FRK), 0 } },
    & ifmt_fmovs, { 0x1e40500 }
  },
/* cfabss$pack $FRj,$FRk,$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRJ), ',', OP (FRK), ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_cfmovs, { 0x1b00080 }
  },
/* fsqrts$pack $FRj,$FRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRJ), ',', OP (FRK), 0 } },
    & ifmt_fmovs, { 0x1e40140 }
  },
/* fdsqrts$pack $FRj,$FRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRJ), ',', OP (FRK), 0 } },
    & ifmt_fmovs, { 0x1e40540 }
  },
/* nfdsqrts$pack $FRj,$FRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRJ), ',', OP (FRK), 0 } },
    & ifmt_fmovs, { 0x1e40d40 }
  },
/* fsqrtd$pack $FRdoublej,$FRdoublek */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRDOUBLEJ), ',', OP (FRDOUBLEK), 0 } },
    & ifmt_fmovd, { 0x1e80140 }
  },
/* cfsqrts$pack $FRj,$FRk,$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRJ), ',', OP (FRK), ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_cfmovs, { 0x1b80080 }
  },
/* nfsqrts$pack $FRj,$FRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRJ), ',', OP (FRK), 0 } },
    & ifmt_fmovs, { 0x1e40940 }
  },
/* fadds$pack $FRi,$FRj,$FRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRI), ',', OP (FRJ), ',', OP (FRK), 0 } },
    & ifmt_fadds, { 0x1e40180 }
  },
/* fsubs$pack $FRi,$FRj,$FRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRI), ',', OP (FRJ), ',', OP (FRK), 0 } },
    & ifmt_fadds, { 0x1e401c0 }
  },
/* fmuls$pack $FRi,$FRj,$FRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRI), ',', OP (FRJ), ',', OP (FRK), 0 } },
    & ifmt_fadds, { 0x1e40200 }
  },
/* fdivs$pack $FRi,$FRj,$FRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRI), ',', OP (FRJ), ',', OP (FRK), 0 } },
    & ifmt_fadds, { 0x1e40240 }
  },
/* faddd$pack $FRdoublei,$FRdoublej,$FRdoublek */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRDOUBLEI), ',', OP (FRDOUBLEJ), ',', OP (FRDOUBLEK), 0 } },
    & ifmt_faddd, { 0x1e80180 }
  },
/* fsubd$pack $FRdoublei,$FRdoublej,$FRdoublek */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRDOUBLEI), ',', OP (FRDOUBLEJ), ',', OP (FRDOUBLEK), 0 } },
    & ifmt_faddd, { 0x1e801c0 }
  },
/* fmuld$pack $FRdoublei,$FRdoublej,$FRdoublek */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRDOUBLEI), ',', OP (FRDOUBLEJ), ',', OP (FRDOUBLEK), 0 } },
    & ifmt_faddd, { 0x1e80200 }
  },
/* fdivd$pack $FRdoublei,$FRdoublej,$FRdoublek */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRDOUBLEI), ',', OP (FRDOUBLEJ), ',', OP (FRDOUBLEK), 0 } },
    & ifmt_faddd, { 0x1e80240 }
  },
/* cfadds$pack $FRi,$FRj,$FRk,$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRI), ',', OP (FRJ), ',', OP (FRK), ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_cfadds, { 0x1b40000 }
  },
/* cfsubs$pack $FRi,$FRj,$FRk,$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRI), ',', OP (FRJ), ',', OP (FRK), ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_cfadds, { 0x1b40040 }
  },
/* cfmuls$pack $FRi,$FRj,$FRk,$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRI), ',', OP (FRJ), ',', OP (FRK), ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_cfadds, { 0x1b80000 }
  },
/* cfdivs$pack $FRi,$FRj,$FRk,$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRI), ',', OP (FRJ), ',', OP (FRK), ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_cfadds, { 0x1b80040 }
  },
/* nfadds$pack $FRi,$FRj,$FRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRI), ',', OP (FRJ), ',', OP (FRK), 0 } },
    & ifmt_fadds, { 0x1e40980 }
  },
/* nfsubs$pack $FRi,$FRj,$FRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRI), ',', OP (FRJ), ',', OP (FRK), 0 } },
    & ifmt_fadds, { 0x1e409c0 }
  },
/* nfmuls$pack $FRi,$FRj,$FRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRI), ',', OP (FRJ), ',', OP (FRK), 0 } },
    & ifmt_fadds, { 0x1e40a00 }
  },
/* nfdivs$pack $FRi,$FRj,$FRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRI), ',', OP (FRJ), ',', OP (FRK), 0 } },
    & ifmt_fadds, { 0x1e40a40 }
  },
/* fcmps$pack $FRi,$FRj,$FCCi_2 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRI), ',', OP (FRJ), ',', OP (FCCI_2), 0 } },
    & ifmt_fcmps, { 0x1e40280 }
  },
/* fcmpd$pack $FRdoublei,$FRdoublej,$FCCi_2 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRDOUBLEI), ',', OP (FRDOUBLEJ), ',', OP (FCCI_2), 0 } },
    & ifmt_fcmpd, { 0x1e80280 }
  },
/* cfcmps$pack $FRi,$FRj,$FCCi_2,$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRI), ',', OP (FRJ), ',', OP (FCCI_2), ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_cfcmps, { 0x1b40080 }
  },
/* fdcmps$pack $FRi,$FRj,$FCCi_2 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRI), ',', OP (FRJ), ',', OP (FCCI_2), 0 } },
    & ifmt_fcmps, { 0x1e40680 }
  },
/* fmadds$pack $FRi,$FRj,$FRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRI), ',', OP (FRJ), ',', OP (FRK), 0 } },
    & ifmt_fadds, { 0x1e402c0 }
  },
/* fmsubs$pack $FRi,$FRj,$FRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRI), ',', OP (FRJ), ',', OP (FRK), 0 } },
    & ifmt_fadds, { 0x1e40300 }
  },
/* fmaddd$pack $FRdoublei,$FRdoublej,$FRdoublek */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRDOUBLEI), ',', OP (FRDOUBLEJ), ',', OP (FRDOUBLEK), 0 } },
    & ifmt_faddd, { 0x1e802c0 }
  },
/* fmsubd$pack $FRdoublei,$FRdoublej,$FRdoublek */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRDOUBLEI), ',', OP (FRDOUBLEJ), ',', OP (FRDOUBLEK), 0 } },
    & ifmt_faddd, { 0x1e80300 }
  },
/* fdmadds$pack $FRi,$FRj,$FRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRI), ',', OP (FRJ), ',', OP (FRK), 0 } },
    & ifmt_fadds, { 0x1e406c0 }
  },
/* nfdmadds$pack $FRi,$FRj,$FRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRI), ',', OP (FRJ), ',', OP (FRK), 0 } },
    & ifmt_fadds, { 0x1e40ec0 }
  },
/* cfmadds$pack $FRi,$FRj,$FRk,$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRI), ',', OP (FRJ), ',', OP (FRK), ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_cfadds, { 0x1bc0000 }
  },
/* cfmsubs$pack $FRi,$FRj,$FRk,$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRI), ',', OP (FRJ), ',', OP (FRK), ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_cfadds, { 0x1bc0040 }
  },
/* nfmadds$pack $FRi,$FRj,$FRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRI), ',', OP (FRJ), ',', OP (FRK), 0 } },
    & ifmt_fadds, { 0x1e40ac0 }
  },
/* nfmsubs$pack $FRi,$FRj,$FRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRI), ',', OP (FRJ), ',', OP (FRK), 0 } },
    & ifmt_fadds, { 0x1e40b00 }
  },
/* fmas$pack $FRi,$FRj,$FRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRI), ',', OP (FRJ), ',', OP (FRK), 0 } },
    & ifmt_fadds, { 0x1e40380 }
  },
/* fmss$pack $FRi,$FRj,$FRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRI), ',', OP (FRJ), ',', OP (FRK), 0 } },
    & ifmt_fadds, { 0x1e403c0 }
  },
/* fdmas$pack $FRi,$FRj,$FRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRI), ',', OP (FRJ), ',', OP (FRK), 0 } },
    & ifmt_fadds, { 0x1e40700 }
  },
/* fdmss$pack $FRi,$FRj,$FRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRI), ',', OP (FRJ), ',', OP (FRK), 0 } },
    & ifmt_fadds, { 0x1e40740 }
  },
/* nfdmas$pack $FRi,$FRj,$FRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRI), ',', OP (FRJ), ',', OP (FRK), 0 } },
    & ifmt_fadds, { 0x1e40f00 }
  },
/* nfdmss$pack $FRi,$FRj,$FRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRI), ',', OP (FRJ), ',', OP (FRK), 0 } },
    & ifmt_fadds, { 0x1e40f40 }
  },
/* cfmas$pack $FRi,$FRj,$FRk,$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRI), ',', OP (FRJ), ',', OP (FRK), ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_cfadds, { 0x1bc0080 }
  },
/* cfmss$pack $FRi,$FRj,$FRk,$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRI), ',', OP (FRJ), ',', OP (FRK), ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_cfadds, { 0x1bc00c0 }
  },
/* fmad$pack $FRi,$FRj,$FRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRI), ',', OP (FRJ), ',', OP (FRK), 0 } },
    & ifmt_fadds, { 0x1e80380 }
  },
/* fmsd$pack $FRi,$FRj,$FRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRI), ',', OP (FRJ), ',', OP (FRK), 0 } },
    & ifmt_fadds, { 0x1e803c0 }
  },
/* nfmas$pack $FRi,$FRj,$FRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRI), ',', OP (FRJ), ',', OP (FRK), 0 } },
    & ifmt_fadds, { 0x1e40b80 }
  },
/* nfmss$pack $FRi,$FRj,$FRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRI), ',', OP (FRJ), ',', OP (FRK), 0 } },
    & ifmt_fadds, { 0x1e40bc0 }
  },
/* fdadds$pack $FRi,$FRj,$FRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRI), ',', OP (FRJ), ',', OP (FRK), 0 } },
    & ifmt_fadds, { 0x1e40580 }
  },
/* fdsubs$pack $FRi,$FRj,$FRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRI), ',', OP (FRJ), ',', OP (FRK), 0 } },
    & ifmt_fadds, { 0x1e405c0 }
  },
/* fdmuls$pack $FRi,$FRj,$FRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRI), ',', OP (FRJ), ',', OP (FRK), 0 } },
    & ifmt_fadds, { 0x1e40600 }
  },
/* fddivs$pack $FRi,$FRj,$FRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRI), ',', OP (FRJ), ',', OP (FRK), 0 } },
    & ifmt_fadds, { 0x1e40640 }
  },
/* fdsads$pack $FRi,$FRj,$FRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRI), ',', OP (FRJ), ',', OP (FRK), 0 } },
    & ifmt_fadds, { 0x1e40780 }
  },
/* fdmulcs$pack $FRi,$FRj,$FRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRI), ',', OP (FRJ), ',', OP (FRK), 0 } },
    & ifmt_fadds, { 0x1e407c0 }
  },
/* nfdmulcs$pack $FRi,$FRj,$FRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRI), ',', OP (FRJ), ',', OP (FRK), 0 } },
    & ifmt_fadds, { 0x1e40fc0 }
  },
/* nfdadds$pack $FRi,$FRj,$FRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRI), ',', OP (FRJ), ',', OP (FRK), 0 } },
    & ifmt_fadds, { 0x1e40d80 }
  },
/* nfdsubs$pack $FRi,$FRj,$FRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRI), ',', OP (FRJ), ',', OP (FRK), 0 } },
    & ifmt_fadds, { 0x1e40dc0 }
  },
/* nfdmuls$pack $FRi,$FRj,$FRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRI), ',', OP (FRJ), ',', OP (FRK), 0 } },
    & ifmt_fadds, { 0x1e40e00 }
  },
/* nfddivs$pack $FRi,$FRj,$FRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRI), ',', OP (FRJ), ',', OP (FRK), 0 } },
    & ifmt_fadds, { 0x1e40e40 }
  },
/* nfdsads$pack $FRi,$FRj,$FRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRI), ',', OP (FRJ), ',', OP (FRK), 0 } },
    & ifmt_fadds, { 0x1e40f80 }
  },
/* nfdcmps$pack $FRi,$FRj,$FCCi_2 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRI), ',', OP (FRJ), ',', OP (FCCI_2), 0 } },
    & ifmt_fcmps, { 0x1e40e80 }
  },
/* mhsetlos$pack $u12,$FRklo */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (U12), ',', OP (FRKLO), 0 } },
    & ifmt_mhsetlos, { 0x1e00800 }
  },
/* mhsethis$pack $u12,$FRkhi */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (U12), ',', OP (FRKHI), 0 } },
    & ifmt_mhsethis, { 0x1e00880 }
  },
/* mhdsets$pack $u12,$FRintk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (U12), ',', OP (FRINTK), 0 } },
    & ifmt_mhdsets, { 0x1e00900 }
  },
/* mhsetloh$pack $s5,$FRklo */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (S5), ',', OP (FRKLO), 0 } },
    & ifmt_mhsetloh, { 0x1e00840 }
  },
/* mhsethih$pack $s5,$FRkhi */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (S5), ',', OP (FRKHI), 0 } },
    & ifmt_mhsethih, { 0x1e008c0 }
  },
/* mhdseth$pack $s5,$FRintk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (S5), ',', OP (FRINTK), 0 } },
    & ifmt_mhdseth, { 0x1e00940 }
  },
/* mand$pack $FRinti,$FRintj,$FRintk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRINTI), ',', OP (FRINTJ), ',', OP (FRINTK), 0 } },
    & ifmt_mand, { 0x1ec0000 }
  },
/* mor$pack $FRinti,$FRintj,$FRintk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRINTI), ',', OP (FRINTJ), ',', OP (FRINTK), 0 } },
    & ifmt_mand, { 0x1ec0040 }
  },
/* mxor$pack $FRinti,$FRintj,$FRintk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRINTI), ',', OP (FRINTJ), ',', OP (FRINTK), 0 } },
    & ifmt_mand, { 0x1ec0080 }
  },
/* cmand$pack $FRinti,$FRintj,$FRintk,$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRINTI), ',', OP (FRINTJ), ',', OP (FRINTK), ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_cmand, { 0x1c00000 }
  },
/* cmor$pack $FRinti,$FRintj,$FRintk,$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRINTI), ',', OP (FRINTJ), ',', OP (FRINTK), ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_cmand, { 0x1c00040 }
  },
/* cmxor$pack $FRinti,$FRintj,$FRintk,$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRINTI), ',', OP (FRINTJ), ',', OP (FRINTK), ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_cmand, { 0x1c00080 }
  },
/* mnot$pack $FRintj,$FRintk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRINTJ), ',', OP (FRINTK), 0 } },
    & ifmt_mnot, { 0x1ec00c0 }
  },
/* cmnot$pack $FRintj,$FRintk,$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRINTJ), ',', OP (FRINTK), ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_cmnot, { 0x1c000c0 }
  },
/* mrotli$pack $FRinti,$u6,$FRintk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRINTI), ',', OP (U6), ',', OP (FRINTK), 0 } },
    & ifmt_mrotli, { 0x1ec0100 }
  },
/* mrotri$pack $FRinti,$u6,$FRintk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRINTI), ',', OP (U6), ',', OP (FRINTK), 0 } },
    & ifmt_mrotli, { 0x1ec0140 }
  },
/* mwcut$pack $FRinti,$FRintj,$FRintk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRINTI), ',', OP (FRINTJ), ',', OP (FRINTK), 0 } },
    & ifmt_mand, { 0x1ec0180 }
  },
/* mwcuti$pack $FRinti,$u6,$FRintk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRINTI), ',', OP (U6), ',', OP (FRINTK), 0 } },
    & ifmt_mrotli, { 0x1ec01c0 }
  },
/* mcut$pack $ACC40Si,$FRintj,$FRintk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (ACC40SI), ',', OP (FRINTJ), ',', OP (FRINTK), 0 } },
    & ifmt_mcut, { 0x1ec0b00 }
  },
/* mcuti$pack $ACC40Si,$s6,$FRintk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (ACC40SI), ',', OP (S6), ',', OP (FRINTK), 0 } },
    & ifmt_mcuti, { 0x1ec0b80 }
  },
/* mcutss$pack $ACC40Si,$FRintj,$FRintk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (ACC40SI), ',', OP (FRINTJ), ',', OP (FRINTK), 0 } },
    & ifmt_mcut, { 0x1ec0b40 }
  },
/* mcutssi$pack $ACC40Si,$s6,$FRintk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (ACC40SI), ',', OP (S6), ',', OP (FRINTK), 0 } },
    & ifmt_mcuti, { 0x1ec0bc0 }
  },
/* mdcutssi$pack $ACC40Si,$s6,$FRintkeven */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (ACC40SI), ',', OP (S6), ',', OP (FRINTKEVEN), 0 } },
    & ifmt_mdcutssi, { 0x1e00380 }
  },
/* maveh$pack $FRinti,$FRintj,$FRintk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRINTI), ',', OP (FRINTJ), ',', OP (FRINTK), 0 } },
    & ifmt_mand, { 0x1ec0200 }
  },
/* msllhi$pack $FRinti,$u6,$FRintk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRINTI), ',', OP (U6), ',', OP (FRINTK), 0 } },
    & ifmt_mrotli, { 0x1ec0240 }
  },
/* msrlhi$pack $FRinti,$u6,$FRintk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRINTI), ',', OP (U6), ',', OP (FRINTK), 0 } },
    & ifmt_mrotli, { 0x1ec0280 }
  },
/* msrahi$pack $FRinti,$u6,$FRintk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRINTI), ',', OP (U6), ',', OP (FRINTK), 0 } },
    & ifmt_mrotli, { 0x1ec02c0 }
  },
/* mdrotli$pack $FRintieven,$s6,$FRintkeven */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRINTIEVEN), ',', OP (S6), ',', OP (FRINTKEVEN), 0 } },
    & ifmt_mdrotli, { 0x1e002c0 }
  },
/* mcplhi$pack $FRinti,$u6,$FRintk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRINTI), ',', OP (U6), ',', OP (FRINTK), 0 } },
    & ifmt_mrotli, { 0x1e00300 }
  },
/* mcpli$pack $FRinti,$u6,$FRintk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRINTI), ',', OP (U6), ',', OP (FRINTK), 0 } },
    & ifmt_mrotli, { 0x1e00340 }
  },
/* msaths$pack $FRinti,$FRintj,$FRintk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRINTI), ',', OP (FRINTJ), ',', OP (FRINTK), 0 } },
    & ifmt_mand, { 0x1ec0300 }
  },
/* mqsaths$pack $FRintieven,$FRintjeven,$FRintkeven */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRINTIEVEN), ',', OP (FRINTJEVEN), ',', OP (FRINTKEVEN), 0 } },
    & ifmt_mqsaths, { 0x1e003c0 }
  },
/* msathu$pack $FRinti,$FRintj,$FRintk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRINTI), ',', OP (FRINTJ), ',', OP (FRINTK), 0 } },
    & ifmt_mand, { 0x1ec0340 }
  },
/* mcmpsh$pack $FRinti,$FRintj,$FCCk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRINTI), ',', OP (FRINTJ), ',', OP (FCCK), 0 } },
    & ifmt_mcmpsh, { 0x1ec0380 }
  },
/* mcmpuh$pack $FRinti,$FRintj,$FCCk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRINTI), ',', OP (FRINTJ), ',', OP (FCCK), 0 } },
    & ifmt_mcmpsh, { 0x1ec03c0 }
  },
/* mabshs$pack $FRintj,$FRintk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRINTJ), ',', OP (FRINTK), 0 } },
    & ifmt_mabshs, { 0x1e00280 }
  },
/* maddhss$pack $FRinti,$FRintj,$FRintk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRINTI), ',', OP (FRINTJ), ',', OP (FRINTK), 0 } },
    & ifmt_mand, { 0x1ec0400 }
  },
/* maddhus$pack $FRinti,$FRintj,$FRintk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRINTI), ',', OP (FRINTJ), ',', OP (FRINTK), 0 } },
    & ifmt_mand, { 0x1ec0440 }
  },
/* msubhss$pack $FRinti,$FRintj,$FRintk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRINTI), ',', OP (FRINTJ), ',', OP (FRINTK), 0 } },
    & ifmt_mand, { 0x1ec0480 }
  },
/* msubhus$pack $FRinti,$FRintj,$FRintk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRINTI), ',', OP (FRINTJ), ',', OP (FRINTK), 0 } },
    & ifmt_mand, { 0x1ec04c0 }
  },
/* cmaddhss$pack $FRinti,$FRintj,$FRintk,$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRINTI), ',', OP (FRINTJ), ',', OP (FRINTK), ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_cmand, { 0x1c40000 }
  },
/* cmaddhus$pack $FRinti,$FRintj,$FRintk,$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRINTI), ',', OP (FRINTJ), ',', OP (FRINTK), ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_cmand, { 0x1c40040 }
  },
/* cmsubhss$pack $FRinti,$FRintj,$FRintk,$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRINTI), ',', OP (FRINTJ), ',', OP (FRINTK), ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_cmand, { 0x1c40080 }
  },
/* cmsubhus$pack $FRinti,$FRintj,$FRintk,$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRINTI), ',', OP (FRINTJ), ',', OP (FRINTK), ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_cmand, { 0x1c400c0 }
  },
/* mqaddhss$pack $FRintieven,$FRintjeven,$FRintkeven */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRINTIEVEN), ',', OP (FRINTJEVEN), ',', OP (FRINTKEVEN), 0 } },
    & ifmt_mqsaths, { 0x1ec0600 }
  },
/* mqaddhus$pack $FRintieven,$FRintjeven,$FRintkeven */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRINTIEVEN), ',', OP (FRINTJEVEN), ',', OP (FRINTKEVEN), 0 } },
    & ifmt_mqsaths, { 0x1ec0640 }
  },
/* mqsubhss$pack $FRintieven,$FRintjeven,$FRintkeven */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRINTIEVEN), ',', OP (FRINTJEVEN), ',', OP (FRINTKEVEN), 0 } },
    & ifmt_mqsaths, { 0x1ec0680 }
  },
/* mqsubhus$pack $FRintieven,$FRintjeven,$FRintkeven */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRINTIEVEN), ',', OP (FRINTJEVEN), ',', OP (FRINTKEVEN), 0 } },
    & ifmt_mqsaths, { 0x1ec06c0 }
  },
/* cmqaddhss$pack $FRintieven,$FRintjeven,$FRintkeven,$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRINTIEVEN), ',', OP (FRINTJEVEN), ',', OP (FRINTKEVEN), ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_cmqaddhss, { 0x1cc0000 }
  },
/* cmqaddhus$pack $FRintieven,$FRintjeven,$FRintkeven,$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRINTIEVEN), ',', OP (FRINTJEVEN), ',', OP (FRINTKEVEN), ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_cmqaddhss, { 0x1cc0040 }
  },
/* cmqsubhss$pack $FRintieven,$FRintjeven,$FRintkeven,$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRINTIEVEN), ',', OP (FRINTJEVEN), ',', OP (FRINTKEVEN), ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_cmqaddhss, { 0x1cc0080 }
  },
/* cmqsubhus$pack $FRintieven,$FRintjeven,$FRintkeven,$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRINTIEVEN), ',', OP (FRINTJEVEN), ',', OP (FRINTKEVEN), ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_cmqaddhss, { 0x1cc00c0 }
  },
/* mqlclrhs$pack $FRintieven,$FRintjeven,$FRintkeven */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRINTIEVEN), ',', OP (FRINTJEVEN), ',', OP (FRINTKEVEN), 0 } },
    & ifmt_mqsaths, { 0x1e00400 }
  },
/* mqlmths$pack $FRintieven,$FRintjeven,$FRintkeven */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRINTIEVEN), ',', OP (FRINTJEVEN), ',', OP (FRINTKEVEN), 0 } },
    & ifmt_mqsaths, { 0x1e00500 }
  },
/* mqsllhi$pack $FRintieven,$u6,$FRintkeven */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRINTIEVEN), ',', OP (U6), ',', OP (FRINTKEVEN), 0 } },
    & ifmt_mqsllhi, { 0x1e00440 }
  },
/* mqsrahi$pack $FRintieven,$u6,$FRintkeven */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRINTIEVEN), ',', OP (U6), ',', OP (FRINTKEVEN), 0 } },
    & ifmt_mqsllhi, { 0x1e004c0 }
  },
/* maddaccs$pack $ACC40Si,$ACC40Sk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (ACC40SI), ',', OP (ACC40SK), 0 } },
    & ifmt_maddaccs, { 0x1e00100 }
  },
/* msubaccs$pack $ACC40Si,$ACC40Sk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (ACC40SI), ',', OP (ACC40SK), 0 } },
    & ifmt_maddaccs, { 0x1e00140 }
  },
/* mdaddaccs$pack $ACC40Si,$ACC40Sk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (ACC40SI), ',', OP (ACC40SK), 0 } },
    & ifmt_maddaccs, { 0x1e00180 }
  },
/* mdsubaccs$pack $ACC40Si,$ACC40Sk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (ACC40SI), ',', OP (ACC40SK), 0 } },
    & ifmt_maddaccs, { 0x1e001c0 }
  },
/* masaccs$pack $ACC40Si,$ACC40Sk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (ACC40SI), ',', OP (ACC40SK), 0 } },
    & ifmt_maddaccs, { 0x1e00200 }
  },
/* mdasaccs$pack $ACC40Si,$ACC40Sk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (ACC40SI), ',', OP (ACC40SK), 0 } },
    & ifmt_maddaccs, { 0x1e00240 }
  },
/* mmulhs$pack $FRinti,$FRintj,$ACC40Sk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRINTI), ',', OP (FRINTJ), ',', OP (ACC40SK), 0 } },
    & ifmt_mmulhs, { 0x1ec0500 }
  },
/* mmulhu$pack $FRinti,$FRintj,$ACC40Sk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRINTI), ',', OP (FRINTJ), ',', OP (ACC40SK), 0 } },
    & ifmt_mmulhs, { 0x1ec0540 }
  },
/* mmulxhs$pack $FRinti,$FRintj,$ACC40Sk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRINTI), ',', OP (FRINTJ), ',', OP (ACC40SK), 0 } },
    & ifmt_mmulhs, { 0x1ec0a00 }
  },
/* mmulxhu$pack $FRinti,$FRintj,$ACC40Sk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRINTI), ',', OP (FRINTJ), ',', OP (ACC40SK), 0 } },
    & ifmt_mmulhs, { 0x1ec0a40 }
  },
/* cmmulhs$pack $FRinti,$FRintj,$ACC40Sk,$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRINTI), ',', OP (FRINTJ), ',', OP (ACC40SK), ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_cmmulhs, { 0x1c80000 }
  },
/* cmmulhu$pack $FRinti,$FRintj,$ACC40Sk,$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRINTI), ',', OP (FRINTJ), ',', OP (ACC40SK), ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_cmmulhs, { 0x1c80040 }
  },
/* mqmulhs$pack $FRintieven,$FRintjeven,$ACC40Sk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRINTIEVEN), ',', OP (FRINTJEVEN), ',', OP (ACC40SK), 0 } },
    & ifmt_mqmulhs, { 0x1ec0700 }
  },
/* mqmulhu$pack $FRintieven,$FRintjeven,$ACC40Sk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRINTIEVEN), ',', OP (FRINTJEVEN), ',', OP (ACC40SK), 0 } },
    & ifmt_mqmulhs, { 0x1ec0740 }
  },
/* mqmulxhs$pack $FRintieven,$FRintjeven,$ACC40Sk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRINTIEVEN), ',', OP (FRINTJEVEN), ',', OP (ACC40SK), 0 } },
    & ifmt_mqmulhs, { 0x1ec0a80 }
  },
/* mqmulxhu$pack $FRintieven,$FRintjeven,$ACC40Sk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRINTIEVEN), ',', OP (FRINTJEVEN), ',', OP (ACC40SK), 0 } },
    & ifmt_mqmulhs, { 0x1ec0ac0 }
  },
/* cmqmulhs$pack $FRintieven,$FRintjeven,$ACC40Sk,$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRINTIEVEN), ',', OP (FRINTJEVEN), ',', OP (ACC40SK), ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_cmqmulhs, { 0x1d00000 }
  },
/* cmqmulhu$pack $FRintieven,$FRintjeven,$ACC40Sk,$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRINTIEVEN), ',', OP (FRINTJEVEN), ',', OP (ACC40SK), ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_cmqmulhs, { 0x1d00040 }
  },
/* mmachs$pack $FRinti,$FRintj,$ACC40Sk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRINTI), ',', OP (FRINTJ), ',', OP (ACC40SK), 0 } },
    & ifmt_mmulhs, { 0x1ec0580 }
  },
/* mmachu$pack $FRinti,$FRintj,$ACC40Uk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRINTI), ',', OP (FRINTJ), ',', OP (ACC40UK), 0 } },
    & ifmt_mmachu, { 0x1ec05c0 }
  },
/* mmrdhs$pack $FRinti,$FRintj,$ACC40Sk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRINTI), ',', OP (FRINTJ), ',', OP (ACC40SK), 0 } },
    & ifmt_mmulhs, { 0x1ec0c00 }
  },
/* mmrdhu$pack $FRinti,$FRintj,$ACC40Uk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRINTI), ',', OP (FRINTJ), ',', OP (ACC40UK), 0 } },
    & ifmt_mmachu, { 0x1ec0c40 }
  },
/* cmmachs$pack $FRinti,$FRintj,$ACC40Sk,$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRINTI), ',', OP (FRINTJ), ',', OP (ACC40SK), ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_cmmulhs, { 0x1c80080 }
  },
/* cmmachu$pack $FRinti,$FRintj,$ACC40Uk,$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRINTI), ',', OP (FRINTJ), ',', OP (ACC40UK), ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_cmmachu, { 0x1c800c0 }
  },
/* mqmachs$pack $FRintieven,$FRintjeven,$ACC40Sk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRINTIEVEN), ',', OP (FRINTJEVEN), ',', OP (ACC40SK), 0 } },
    & ifmt_mqmulhs, { 0x1ec0780 }
  },
/* mqmachu$pack $FRintieven,$FRintjeven,$ACC40Uk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRINTIEVEN), ',', OP (FRINTJEVEN), ',', OP (ACC40UK), 0 } },
    & ifmt_mqmachu, { 0x1ec07c0 }
  },
/* cmqmachs$pack $FRintieven,$FRintjeven,$ACC40Sk,$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRINTIEVEN), ',', OP (FRINTJEVEN), ',', OP (ACC40SK), ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_cmqmulhs, { 0x1d00080 }
  },
/* cmqmachu$pack $FRintieven,$FRintjeven,$ACC40Uk,$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRINTIEVEN), ',', OP (FRINTJEVEN), ',', OP (ACC40UK), ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_cmqmachu, { 0x1d000c0 }
  },
/* mqxmachs$pack $FRintieven,$FRintjeven,$ACC40Sk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRINTIEVEN), ',', OP (FRINTJEVEN), ',', OP (ACC40SK), 0 } },
    & ifmt_mqmulhs, { 0x1e00000 }
  },
/* mqxmacxhs$pack $FRintieven,$FRintjeven,$ACC40Sk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRINTIEVEN), ',', OP (FRINTJEVEN), ',', OP (ACC40SK), 0 } },
    & ifmt_mqmulhs, { 0x1e00040 }
  },
/* mqmacxhs$pack $FRintieven,$FRintjeven,$ACC40Sk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRINTIEVEN), ',', OP (FRINTJEVEN), ',', OP (ACC40SK), 0 } },
    & ifmt_mqmulhs, { 0x1e00080 }
  },
/* mcpxrs$pack $FRinti,$FRintj,$ACC40Sk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRINTI), ',', OP (FRINTJ), ',', OP (ACC40SK), 0 } },
    & ifmt_mmulhs, { 0x1ec0800 }
  },
/* mcpxru$pack $FRinti,$FRintj,$ACC40Sk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRINTI), ',', OP (FRINTJ), ',', OP (ACC40SK), 0 } },
    & ifmt_mmulhs, { 0x1ec0840 }
  },
/* mcpxis$pack $FRinti,$FRintj,$ACC40Sk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRINTI), ',', OP (FRINTJ), ',', OP (ACC40SK), 0 } },
    & ifmt_mmulhs, { 0x1ec0880 }
  },
/* mcpxiu$pack $FRinti,$FRintj,$ACC40Sk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRINTI), ',', OP (FRINTJ), ',', OP (ACC40SK), 0 } },
    & ifmt_mmulhs, { 0x1ec08c0 }
  },
/* cmcpxrs$pack $FRinti,$FRintj,$ACC40Sk,$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRINTI), ',', OP (FRINTJ), ',', OP (ACC40SK), ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_cmmulhs, { 0x1d40000 }
  },
/* cmcpxru$pack $FRinti,$FRintj,$ACC40Sk,$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRINTI), ',', OP (FRINTJ), ',', OP (ACC40SK), ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_cmmulhs, { 0x1d40040 }
  },
/* cmcpxis$pack $FRinti,$FRintj,$ACC40Sk,$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRINTI), ',', OP (FRINTJ), ',', OP (ACC40SK), ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_cmmulhs, { 0x1d40080 }
  },
/* cmcpxiu$pack $FRinti,$FRintj,$ACC40Sk,$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRINTI), ',', OP (FRINTJ), ',', OP (ACC40SK), ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_cmmulhs, { 0x1d400c0 }
  },
/* mqcpxrs$pack $FRintieven,$FRintjeven,$ACC40Sk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRINTIEVEN), ',', OP (FRINTJEVEN), ',', OP (ACC40SK), 0 } },
    & ifmt_mqmulhs, { 0x1ec0900 }
  },
/* mqcpxru$pack $FRintieven,$FRintjeven,$ACC40Sk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRINTIEVEN), ',', OP (FRINTJEVEN), ',', OP (ACC40SK), 0 } },
    & ifmt_mqmulhs, { 0x1ec0940 }
  },
/* mqcpxis$pack $FRintieven,$FRintjeven,$ACC40Sk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRINTIEVEN), ',', OP (FRINTJEVEN), ',', OP (ACC40SK), 0 } },
    & ifmt_mqmulhs, { 0x1ec0980 }
  },
/* mqcpxiu$pack $FRintieven,$FRintjeven,$ACC40Sk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRINTIEVEN), ',', OP (FRINTJEVEN), ',', OP (ACC40SK), 0 } },
    & ifmt_mqmulhs, { 0x1ec09c0 }
  },
/* mexpdhw$pack $FRinti,$u6,$FRintk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRINTI), ',', OP (U6), ',', OP (FRINTK), 0 } },
    & ifmt_mrotli, { 0x1ec0c80 }
  },
/* cmexpdhw$pack $FRinti,$u6,$FRintk,$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRINTI), ',', OP (U6), ',', OP (FRINTK), ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_cmexpdhw, { 0x1d80080 }
  },
/* mexpdhd$pack $FRinti,$u6,$FRintkeven */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRINTI), ',', OP (U6), ',', OP (FRINTKEVEN), 0 } },
    & ifmt_mexpdhd, { 0x1ec0cc0 }
  },
/* cmexpdhd$pack $FRinti,$u6,$FRintkeven,$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRINTI), ',', OP (U6), ',', OP (FRINTKEVEN), ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_cmexpdhd, { 0x1d800c0 }
  },
/* mpackh$pack $FRinti,$FRintj,$FRintk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRINTI), ',', OP (FRINTJ), ',', OP (FRINTK), 0 } },
    & ifmt_mand, { 0x1ec0d00 }
  },
/* mdpackh$pack $FRintieven,$FRintjeven,$FRintkeven */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRINTIEVEN), ',', OP (FRINTJEVEN), ',', OP (FRINTKEVEN), 0 } },
    & ifmt_mqsaths, { 0x1ec0d80 }
  },
/* munpackh$pack $FRinti,$FRintkeven */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRINTI), ',', OP (FRINTKEVEN), 0 } },
    & ifmt_munpackh, { 0x1ec0d40 }
  },
/* mdunpackh$pack $FRintieven,$FRintk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRINTIEVEN), ',', OP (FRINTK), 0 } },
    & ifmt_mdunpackh, { 0x1ec0dc0 }
  },
/* mbtoh$pack $FRintj,$FRintkeven */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRINTJ), ',', OP (FRINTKEVEN), 0 } },
    & ifmt_mbtoh, { 0x1ec0e00 }
  },
/* cmbtoh$pack $FRintj,$FRintkeven,$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRINTJ), ',', OP (FRINTKEVEN), ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_cmbtoh, { 0x1dc0000 }
  },
/* mhtob$pack $FRintjeven,$FRintk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRINTJEVEN), ',', OP (FRINTK), 0 } },
    & ifmt_mhtob, { 0x1ec0e40 }
  },
/* cmhtob$pack $FRintjeven,$FRintk,$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRINTJEVEN), ',', OP (FRINTK), ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_cmhtob, { 0x1dc0040 }
  },
/* mbtohe$pack $FRintj,$FRintk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRINTJ), ',', OP (FRINTK), 0 } },
    & ifmt_mabshs, { 0x1ec0e80 }
  },
/* cmbtohe$pack $FRintj,$FRintk,$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRINTJ), ',', OP (FRINTK), ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_cmbtohe, { 0x1dc0080 }
  },
/* mnop$pack */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), 0 } },
    & ifmt_mnop, { 0x7fee0ec0 }
  },
/* mclracc$pack $ACC40Sk,$A0 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (ACC40SK), ',', OP (A0), 0 } },
    & ifmt_mclracc_0, { 0x1ec0ec0 }
  },
/* mclracc$pack $ACC40Sk,$A1 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (ACC40SK), ',', OP (A1), 0 } },
    & ifmt_mclracc_0, { 0x1ee0ec0 }
  },
/* mrdacc$pack $ACC40Si,$FRintk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (ACC40SI), ',', OP (FRINTK), 0 } },
    & ifmt_mrdacc, { 0x1ec0f00 }
  },
/* mrdaccg$pack $ACCGi,$FRintk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (ACCGI), ',', OP (FRINTK), 0 } },
    & ifmt_mrdaccg, { 0x1ec0f80 }
  },
/* mwtacc$pack $FRinti,$ACC40Sk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRINTI), ',', OP (ACC40SK), 0 } },
    & ifmt_mwtacc, { 0x1ec0f40 }
  },
/* mwtaccg$pack $FRinti,$ACCGk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRINTI), ',', OP (ACCGK), 0 } },
    & ifmt_mwtaccg, { 0x1ec0fc0 }
  },
/* mcop1$pack $FRi,$FRj,$FRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRI), ',', OP (FRJ), ',', OP (FRK), 0 } },
    & ifmt_fadds, { 0x1f00000 }
  },
/* mcop2$pack $FRi,$FRj,$FRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (FRI), ',', OP (FRJ), ',', OP (FRK), 0 } },
    & ifmt_fadds, { 0x1f40000 }
  },
/* fnop$pack */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), 0 } },
    & ifmt_fnop, { 0x1e40340 }
  },
};

#undef A
#undef OPERAND
#undef MNEM
#undef OP

/* Formats for ALIAS macro-insns.  */

#if defined (__STDC__) || defined (ALMOST_STDC) || defined (HAVE_STRINGIZE)
#define F(f) & frv_cgen_ifld_table[FRV_##f]
#else
#define F(f) & frv_cgen_ifld_table[FRV_/**/f]
#endif
static const CGEN_IFMT ifmt_nop ATTRIBUTE_UNUSED = {
  32, 32, 0x7fffffff, { { F (F_PACK) }, { F (F_GRK) }, { F (F_OP) }, { F (F_GRI) }, { F (F_D12) }, { 0 } }
};

static const CGEN_IFMT ifmt_ret ATTRIBUTE_UNUSED = {
  32, 32, 0x7fffffff, { { F (F_PACK) }, { F (F_INT_CC) }, { F (F_ICCI_2_NULL) }, { F (F_OP) }, { F (F_HINT) }, { F (F_OPE3) }, { F (F_CCOND_NULL) }, { F (F_S12_NULL) }, { 0 } }
};

static const CGEN_IFMT ifmt_cmp ATTRIBUTE_UNUSED = {
  32, 32, 0x7ffc03c0, { { F (F_PACK) }, { F (F_GRK) }, { F (F_OP) }, { F (F_GRI) }, { F (F_ICCI_1) }, { F (F_OPE2) }, { F (F_GRJ) }, { 0 } }
};

static const CGEN_IFMT ifmt_cmpi ATTRIBUTE_UNUSED = {
  32, 32, 0x7ffc0000, { { F (F_PACK) }, { F (F_GRK) }, { F (F_OP) }, { F (F_GRI) }, { F (F_ICCI_1) }, { F (F_S10) }, { 0 } }
};

static const CGEN_IFMT ifmt_ccmp ATTRIBUTE_UNUSED = {
  32, 32, 0x7ffc00c0, { { F (F_PACK) }, { F (F_GRK) }, { F (F_OP) }, { F (F_GRI) }, { F (F_CCI) }, { F (F_COND) }, { F (F_OPE4) }, { F (F_GRJ) }, { 0 } }
};

static const CGEN_IFMT ifmt_mov ATTRIBUTE_UNUSED = {
  32, 32, 0x1fc0fff, { { F (F_PACK) }, { F (F_GRK) }, { F (F_OP) }, { F (F_GRI) }, { F (F_D12) }, { 0 } }
};

static const CGEN_IFMT ifmt_cmov ATTRIBUTE_UNUSED = {
  32, 32, 0x1fc00ff, { { F (F_PACK) }, { F (F_GRK) }, { F (F_OP) }, { F (F_GRI) }, { F (F_CCI) }, { F (F_COND) }, { F (F_OPE4) }, { F (F_GRJ) }, { 0 } }
};

#undef F

/* Each non-simple macro entry points to an array of expansion possibilities.  */

#if defined (__STDC__) || defined (ALMOST_STDC) || defined (HAVE_STRINGIZE)
#define A(a) (1 << CGEN_INSN_##a)
#else
#define A(a) (1 << CGEN_INSN_/**/a)
#endif
#if defined (__STDC__) || defined (ALMOST_STDC) || defined (HAVE_STRINGIZE)
#define OPERAND(op) FRV_OPERAND_##op
#else
#define OPERAND(op) FRV_OPERAND_/**/op
#endif
#define MNEM CGEN_SYNTAX_MNEMONIC /* syntax value for mnemonic */
#define OP(field) CGEN_SYNTAX_MAKE_FIELD (OPERAND (field))

/* The macro instruction table.  */

static const CGEN_IBASE frv_cgen_macro_insn_table[] =
{
/* nop$pack */
  {
    -1, "nop", "nop", 32,
    { 0|A(ALIAS), { { { (1<<MACH_BASE), 0 } }, { { UNIT_IALL, 0 } }, { { FR400_MAJOR_I_1, 0 } }, { { FR450_MAJOR_I_1, 0 } }, { { FR500_MAJOR_I_1, 0 } }, { { FR550_MAJOR_NONE, 0 } } } }
  },
/* ret$pack */
  {
    -1, "ret", "ret", 32,
    { 0|A(NO_DIS)|A(ALIAS), { { { (1<<MACH_BASE), 0 } }, { { UNIT_B01, 0 } }, { { FR400_MAJOR_B_3, 0 } }, { { FR450_MAJOR_B_3, 0 } }, { { FR500_MAJOR_B_3, 0 } }, { { FR550_MAJOR_NONE, 0 } } } }
  },
/* cmp$pack $GRi,$GRj,$ICCi_1 */
  {
    -1, "cmp", "cmp", 32,
    { 0|A(NO_DIS)|A(ALIAS), { { { (1<<MACH_BASE), 0 } }, { { UNIT_IALL, 0 } }, { { FR400_MAJOR_I_1, 0 } }, { { FR450_MAJOR_I_1, 0 } }, { { FR500_MAJOR_I_1, 0 } }, { { FR550_MAJOR_NONE, 0 } } } }
  },
/* cmpi$pack $GRi,$s10,$ICCi_1 */
  {
    -1, "cmpi", "cmpi", 32,
    { 0|A(NO_DIS)|A(ALIAS), { { { (1<<MACH_BASE), 0 } }, { { UNIT_IALL, 0 } }, { { FR400_MAJOR_I_1, 0 } }, { { FR450_MAJOR_I_1, 0 } }, { { FR500_MAJOR_I_1, 0 } }, { { FR550_MAJOR_NONE, 0 } } } }
  },
/* ccmp$pack $GRi,$GRj,$CCi,$cond */
  {
    -1, "ccmp", "ccmp", 32,
    { 0|A(CONDITIONAL)|A(NO_DIS)|A(ALIAS), { { { (1<<MACH_BASE), 0 } }, { { UNIT_IALL, 0 } }, { { FR400_MAJOR_I_1, 0 } }, { { FR450_MAJOR_I_1, 0 } }, { { FR500_MAJOR_I_1, 0 } }, { { FR550_MAJOR_NONE, 0 } } } }
  },
/* mov$pack $GRi,$GRk */
  {
    -1, "mov", "mov", 32,
    { 0|A(NO_DIS)|A(ALIAS), { { { (1<<MACH_BASE), 0 } }, { { UNIT_IALL, 0 } }, { { FR400_MAJOR_I_1, 0 } }, { { FR450_MAJOR_I_1, 0 } }, { { FR500_MAJOR_I_1, 0 } }, { { FR550_MAJOR_NONE, 0 } } } }
  },
/* cmov$pack $GRi,$GRk,$CCi,$cond */
  {
    -1, "cmov", "cmov", 32,
    { 0|A(CONDITIONAL)|A(NO_DIS)|A(ALIAS), { { { (1<<MACH_BASE), 0 } }, { { UNIT_IALL, 0 } }, { { FR400_MAJOR_I_1, 0 } }, { { FR450_MAJOR_I_1, 0 } }, { { FR500_MAJOR_I_1, 0 } }, { { FR550_MAJOR_NONE, 0 } } } }
  },
};

/* The macro instruction opcode table.  */

static const CGEN_OPCODE frv_cgen_macro_insn_opcode_table[] =
{
/* nop$pack */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), 0 } },
    & ifmt_nop, { 0x880000 }
  },
/* ret$pack */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), 0 } },
    & ifmt_ret, { 0x403a4000 }
  },
/* cmp$pack $GRi,$GRj,$ICCi_1 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (GRI), ',', OP (GRJ), ',', OP (ICCI_1), 0 } },
    & ifmt_cmp, { 0x140 }
  },
/* cmpi$pack $GRi,$s10,$ICCi_1 */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (GRI), ',', OP (S10), ',', OP (ICCI_1), 0 } },
    & ifmt_cmpi, { 0x540000 }
  },
/* ccmp$pack $GRi,$GRj,$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (GRI), ',', OP (GRJ), ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_ccmp, { 0x1640040 }
  },
/* mov$pack $GRi,$GRk */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (GRI), ',', OP (GRK), 0 } },
    & ifmt_mov, { 0x880000 }
  },
/* cmov$pack $GRi,$GRk,$CCi,$cond */
  {
    { 0, 0, 0, 0 },
    { { MNEM, OP (PACK), ' ', OP (GRI), ',', OP (GRK), ',', OP (CCI), ',', OP (COND), 0 } },
    & ifmt_cmov, { 0x1680040 }
  },
};

#undef A
#undef OPERAND
#undef MNEM
#undef OP

#ifndef CGEN_ASM_HASH_P
#define CGEN_ASM_HASH_P(insn) 1
#endif

#ifndef CGEN_DIS_HASH_P
#define CGEN_DIS_HASH_P(insn) 1
#endif

/* Return non-zero if INSN is to be added to the hash table.
   Targets are free to override CGEN_{ASM,DIS}_HASH_P in the .opc file.  */

static int
asm_hash_insn_p (insn)
     const CGEN_INSN *insn ATTRIBUTE_UNUSED;
{
  return CGEN_ASM_HASH_P (insn);
}

static int
dis_hash_insn_p (insn)
     const CGEN_INSN *insn;
{
  /* If building the hash table and the NO-DIS attribute is present,
     ignore.  */
  if (CGEN_INSN_ATTR_VALUE (insn, CGEN_INSN_NO_DIS))
    return 0;
  return CGEN_DIS_HASH_P (insn);
}

#ifndef CGEN_ASM_HASH
#define CGEN_ASM_HASH_SIZE 127
#ifdef CGEN_MNEMONIC_OPERANDS
#define CGEN_ASM_HASH(mnem) (*(unsigned char *) (mnem) % CGEN_ASM_HASH_SIZE)
#else
#define CGEN_ASM_HASH(mnem) (*(unsigned char *) (mnem) % CGEN_ASM_HASH_SIZE) /*FIXME*/
#endif
#endif

/* It doesn't make much sense to provide a default here,
   but while this is under development we do.
   BUFFER is a pointer to the bytes of the insn, target order.
   VALUE is the first base_insn_bitsize bits as an int in host order.  */

#ifndef CGEN_DIS_HASH
#define CGEN_DIS_HASH_SIZE 256
#define CGEN_DIS_HASH(buf, value) (*(unsigned char *) (buf))
#endif

/* The result is the hash value of the insn.
   Targets are free to override CGEN_{ASM,DIS}_HASH in the .opc file.  */

static unsigned int
asm_hash_insn (mnem)
     const char * mnem;
{
  return CGEN_ASM_HASH (mnem);
}

/* BUF is a pointer to the bytes of the insn, target order.
   VALUE is the first base_insn_bitsize bits as an int in host order.  */

static unsigned int
dis_hash_insn (buf, value)
     const char * buf ATTRIBUTE_UNUSED;
     CGEN_INSN_INT value ATTRIBUTE_UNUSED;
{
  return CGEN_DIS_HASH (buf, value);
}

/* Set the recorded length of the insn in the CGEN_FIELDS struct.  */

static void
set_fields_bitsize (CGEN_FIELDS *fields, int size)
{
  CGEN_FIELDS_BITSIZE (fields) = size;
}

/* Function to call before using the operand instance table.
   This plugs the opcode entries and macro instructions into the cpu table.  */

void
frv_cgen_init_opcode_table (CGEN_CPU_DESC cd)
{
  int i;
  int num_macros = (sizeof (frv_cgen_macro_insn_table) /
		    sizeof (frv_cgen_macro_insn_table[0]));
  const CGEN_IBASE *ib = & frv_cgen_macro_insn_table[0];
  const CGEN_OPCODE *oc = & frv_cgen_macro_insn_opcode_table[0];
  CGEN_INSN *insns = xmalloc (num_macros * sizeof (CGEN_INSN));

  memset (insns, 0, num_macros * sizeof (CGEN_INSN));
  for (i = 0; i < num_macros; ++i)
    {
      insns[i].base = &ib[i];
      insns[i].opcode = &oc[i];
      frv_cgen_build_insn_regex (& insns[i]);
    }
  cd->macro_insn_table.init_entries = insns;
  cd->macro_insn_table.entry_size = sizeof (CGEN_IBASE);
  cd->macro_insn_table.num_init_entries = num_macros;

  oc = & frv_cgen_insn_opcode_table[0];
  insns = (CGEN_INSN *) cd->insn_table.init_entries;
  for (i = 0; i < MAX_INSNS; ++i)
    {
      insns[i].opcode = &oc[i];
      frv_cgen_build_insn_regex (& insns[i]);
    }

  cd->sizeof_fields = sizeof (CGEN_FIELDS);
  cd->set_fields_bitsize = set_fields_bitsize;

  cd->asm_hash_p = asm_hash_insn_p;
  cd->asm_hash = asm_hash_insn;
  cd->asm_hash_size = CGEN_ASM_HASH_SIZE;

  cd->dis_hash_p = dis_hash_insn_p;
  cd->dis_hash = dis_hash_insn;
  cd->dis_hash_size = CGEN_DIS_HASH_SIZE;
}
