/* BFD library support routines for architectures.
   Copyright (C) 1990, 91, 92, 93, 94, 95, 1996 Free Software Foundation, Inc.
   Hacked by John Gilmore and Steve Chamberlain of Cygnus Support.

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

#include "bfd.h"
#include "sysdep.h"
#include "libbfd.h"
#include <ctype.h>

/*

SECTION
	Architectures

	BFD keeps one atom in a BFD describing the
	architecture of the data attached to the BFD: a pointer to a
	<<bfd_arch_info_type>>.  

	Pointers to structures can be requested independently of a BFD
	so that an architecture's information can be interrogated
	without access to an open BFD.

	The architecture information is provided by each architecture package.
	The set of default architectures is selected by the macro
	<<SELECT_ARCHITECTURES>>.  This is normally set up in the
	@file{config/@var{target}.mt} file of your choice.  If the name is not
	defined, then all the architectures supported are included. 

	When BFD starts up, all the architectures are called with an
	initialize method.  It is up to the architecture back end to
	insert as many items into the list of architectures as it wants to;
	generally this would be one for each machine and one for the
	default case (an item with a machine field of 0). 

	BFD's idea of an architecture is implemented in	@file{archures.c}.
*/

/*

SUBSECTION
	bfd_architecture

DESCRIPTION
	This enum gives the object file's CPU architecture, in a
	global sense---i.e., what processor family does it belong to?
	Another field indicates which processor within
	the family is in use.  The machine gives a number which
	distinguishes different versions of the architecture,
	containing, for example, 2 and 3 for Intel i960 KA and i960 KB,
	and 68020 and 68030 for Motorola 68020 and 68030. 

.enum bfd_architecture 
.{
.  bfd_arch_unknown,   {* File arch not known *}
.  bfd_arch_obscure,   {* Arch known, not one of these *}
.  bfd_arch_m68k,      {* Motorola 68xxx *}
.  bfd_arch_vax,       {* DEC Vax *}   
.  bfd_arch_i960,      {* Intel 960 *}
.    {* The order of the following is important.
.       lower number indicates a machine type that 
.       only accepts a subset of the instructions
.       available to machines with higher numbers.
.       The exception is the "ca", which is
.       incompatible with all other machines except 
.       "core". *}
.
.#define bfd_mach_i960_core      1
.#define bfd_mach_i960_ka_sa     2
.#define bfd_mach_i960_kb_sb     3
.#define bfd_mach_i960_mc        4
.#define bfd_mach_i960_xa        5
.#define bfd_mach_i960_ca        6
.#define bfd_mach_i960_jx	 7
.#define bfd_mach_i960_hx        8
.
.  bfd_arch_a29k,      {* AMD 29000 *}
.  bfd_arch_sparc,     {* SPARC *}
.#define bfd_mach_sparc			1
.{* The difference between v8plus and v9 is that v9 is a true 64 bit env.  *}
.#define bfd_mach_sparc_v8plus		2
.#define bfd_mach_sparc_v8plusa		3 {* with ultrasparc add'ns *}
.#define bfd_mach_sparc_v9		4
.#define bfd_mach_sparc_v9a		5 {* with ultrasparc add'ns *}
.{* Nonzero if MACH has the v9 instruction set.  *}
.#define bfd_mach_sparc_v9_p(mach) ((mach) != bfd_mach_sparc)
.  bfd_arch_mips,      {* MIPS Rxxxx *}
.  bfd_arch_i386,      {* Intel 386 *}
.  bfd_arch_we32k,     {* AT&T WE32xxx *}
.  bfd_arch_tahoe,     {* CCI/Harris Tahoe *}
.  bfd_arch_i860,      {* Intel 860 *}
.  bfd_arch_romp,      {* IBM ROMP PC/RT *}
.  bfd_arch_alliant,   {* Alliant *}
.  bfd_arch_convex,    {* Convex *}
.  bfd_arch_m88k,      {* Motorola 88xxx *}
.  bfd_arch_pyramid,   {* Pyramid Technology *}
.  bfd_arch_h8300,     {* Hitachi H8/300 *}
.#define bfd_mach_h8300   1
.#define bfd_mach_h8300h  2
.  bfd_arch_powerpc,   {* PowerPC *}
.  bfd_arch_rs6000,    {* IBM RS/6000 *}
.  bfd_arch_hppa,      {* HP PA RISC *}
.  bfd_arch_z8k,       {* Zilog Z8000 *}
.#define bfd_mach_z8001		1
.#define bfd_mach_z8002		2
.  bfd_arch_h8500,     {* Hitachi H8/500 *}
.  bfd_arch_sh,        {* Hitachi SH *}
.  bfd_arch_alpha,     {* Dec Alpha *}
.  bfd_arch_arm,       {* Advanced Risc Machines ARM *}
.  bfd_arch_ns32k,     {* National Semiconductors ns32000 *}
.  bfd_arch_w65,       {* WDC 65816 *}
.  bfd_arch_last
.  };


*/

/*

SUBSECTION
	bfd_arch_info

DESCRIPTION
	This structure contains information on architectures for use
	within BFD.

.
.typedef struct bfd_arch_info 
.{
.  int bits_per_word;
.  int bits_per_address;
.  int bits_per_byte;
.  enum bfd_architecture arch;
.  unsigned long mach;
.  const char *arch_name;
.  const char *printable_name;
.  unsigned int section_align_power;
. {* true if this is the default machine for the architecture *}
.  boolean the_default;	
.  const struct bfd_arch_info * (*compatible)
.	PARAMS ((const struct bfd_arch_info *a,
.	         const struct bfd_arch_info *b));
.
.  boolean (*scan) PARAMS ((const struct bfd_arch_info *, const char *));
.
.  const struct bfd_arch_info *next;
.} bfd_arch_info_type;
*/

extern const bfd_arch_info_type bfd_a29k_arch;
extern const bfd_arch_info_type bfd_alpha_arch;
extern const bfd_arch_info_type bfd_arm_arch;
extern const bfd_arch_info_type bfd_h8300_arch;
extern const bfd_arch_info_type bfd_h8500_arch;
extern const bfd_arch_info_type bfd_hppa_arch;
extern const bfd_arch_info_type bfd_i386_arch;
extern const bfd_arch_info_type bfd_i860_arch;
extern const bfd_arch_info_type bfd_i960_arch;
extern const bfd_arch_info_type bfd_m68k_arch;
extern const bfd_arch_info_type bfd_m88k_arch;
extern const bfd_arch_info_type bfd_mips_arch;
extern const bfd_arch_info_type bfd_powerpc_arch;
extern const bfd_arch_info_type bfd_rs6000_arch;
extern const bfd_arch_info_type bfd_sh_arch;
extern const bfd_arch_info_type bfd_sparc_arch;
extern const bfd_arch_info_type bfd_vax_arch;
extern const bfd_arch_info_type bfd_we32k_arch;
extern const bfd_arch_info_type bfd_z8k_arch;
extern const bfd_arch_info_type bfd_ns32k_arch;
extern const bfd_arch_info_type bfd_w65_arch;

static const bfd_arch_info_type * const bfd_archures_list[] =
{
#ifdef SELECT_ARCHITECTURES
  SELECT_ARCHITECTURES,
#else
  &bfd_a29k_arch,
  &bfd_alpha_arch,
  &bfd_arm_arch,
  &bfd_h8300_arch,
  &bfd_h8500_arch,
  &bfd_hppa_arch,
  &bfd_i386_arch,
  &bfd_i860_arch,
  &bfd_i960_arch,
  &bfd_m68k_arch,
  &bfd_m88k_arch,
  &bfd_mips_arch,
  &bfd_powerpc_arch,
  &bfd_rs6000_arch,
  &bfd_sh_arch,
  &bfd_sparc_arch,
  &bfd_vax_arch,
  &bfd_we32k_arch,
  &bfd_z8k_arch,
  &bfd_ns32k_arch,
  &bfd_w65_arch,
#endif
  0
};

/*
FUNCTION
	bfd_printable_name

SYNOPSIS
	const char *bfd_printable_name(bfd *abfd);

DESCRIPTION
	Return a printable string representing the architecture and machine
	from the pointer to the architecture info structure.

*/

const char *
bfd_printable_name (abfd)
     bfd *abfd;
{
  return abfd->arch_info->printable_name;
}



/*
FUNCTION
	bfd_scan_arch

SYNOPSIS
	const bfd_arch_info_type *bfd_scan_arch(const char *string);

DESCRIPTION
	Figure out if BFD supports any cpu which could be described with
	the name @var{string}.  Return a pointer to an <<arch_info>>
	structure if a machine is found, otherwise NULL.

*/

const bfd_arch_info_type *
bfd_scan_arch (string)
     const char *string;
{
  const bfd_arch_info_type * const *app, *ap;

  /* Look through all the installed architectures */
  for (app = bfd_archures_list; *app != NULL; app++)
    {
      for (ap = *app; ap != NULL; ap = ap->next)
	{
	  if (ap->scan (ap, string))
	    return ap;
	}
    }

  return NULL;
}



/*
FUNCTION
	bfd_arch_get_compatible

SYNOPSIS
	const bfd_arch_info_type *bfd_arch_get_compatible(
		const bfd *abfd,
	        const bfd *bbfd);

DESCRIPTION
	Determine whether two BFDs'
	architectures and machine types are compatible.  Calculates
	the lowest common denominator between the two architectures
	and machine types implied by the BFDs and returns a pointer to
	an <<arch_info>> structure describing the compatible machine.
*/

const bfd_arch_info_type *
bfd_arch_get_compatible (abfd, bbfd)
     const bfd *abfd;
     const bfd *bbfd;
{
  /* If either architecture is unknown, then all we can do is assume
     the user knows what he's doing.  */
  if (abfd->arch_info->arch == bfd_arch_unknown)
  	return bbfd->arch_info;
  if (bbfd->arch_info->arch == bfd_arch_unknown)
  	return abfd->arch_info;

  /* Otherwise architecture-specific code has to decide.  */
  return abfd->arch_info->compatible (abfd->arch_info, bbfd->arch_info);
}


/*
INTERNAL_DEFINITION
	bfd_default_arch_struct

DESCRIPTION
	The <<bfd_default_arch_struct>> is an item of
	<<bfd_arch_info_type>> which has been initialized to a fairly
	generic state.  A BFD starts life by pointing to this
	structure, until the correct back end has determined the real
	architecture of the file.

.extern const bfd_arch_info_type bfd_default_arch_struct;

*/

const bfd_arch_info_type bfd_default_arch_struct =
{
    32,32,8,bfd_arch_unknown,0,"unknown","unknown",2,true,
    bfd_default_compatible,
    bfd_default_scan, 
    0,
};

/*
FUNCTION
	bfd_set_arch_info

SYNOPSIS
	void bfd_set_arch_info(bfd *abfd, const bfd_arch_info_type *arg);

DESCRIPTION
	Set the architecture info of @var{abfd} to @var{arg}.
*/

void
bfd_set_arch_info (abfd, arg)
     bfd *abfd;
     const bfd_arch_info_type *arg;
{
  abfd->arch_info = arg;
}

/*
INTERNAL_FUNCTION
	bfd_default_set_arch_mach

SYNOPSIS
	boolean bfd_default_set_arch_mach(bfd *abfd,
		enum bfd_architecture arch,
		unsigned long mach);

DESCRIPTION
	Set the architecture and machine type in BFD @var{abfd}
	to @var{arch} and @var{mach}.  Find the correct
	pointer to a structure and insert it into the <<arch_info>>
	pointer. 
*/

boolean
bfd_default_set_arch_mach (abfd, arch, mach)
     bfd *abfd;
     enum bfd_architecture arch;
     unsigned long mach;
{
  const bfd_arch_info_type * const *app, *ap;

  for (app = bfd_archures_list; *app != NULL; app++)
    {
      for (ap = *app; ap != NULL; ap = ap->next)
	{
	  if (ap->arch == arch
	      && (ap->mach == mach
		  || (mach == 0 && ap->the_default)))
	    {
	      abfd->arch_info = ap;
	      return true;
	    }
	}
    }

  abfd->arch_info = &bfd_default_arch_struct;
  bfd_set_error (bfd_error_bad_value);
  return false;
}


/*
FUNCTION
	bfd_get_arch

SYNOPSIS
	enum bfd_architecture bfd_get_arch(bfd *abfd);

DESCRIPTION
	Return the enumerated type which describes the BFD @var{abfd}'s
	architecture.

*/

enum bfd_architecture
bfd_get_arch (abfd)
     bfd *abfd;
{
    return abfd->arch_info->arch;
}

/*
FUNCTION
	bfd_get_mach

SYNOPSIS
	unsigned long bfd_get_mach(bfd *abfd);

DESCRIPTION
	Return the long type which describes the BFD @var{abfd}'s
	machine.
*/

unsigned long  
bfd_get_mach (abfd)
     bfd *abfd;
{
    return abfd->arch_info->mach;
}

/*
FUNCTION
	bfd_arch_bits_per_byte

SYNOPSIS
	unsigned int bfd_arch_bits_per_byte(bfd *abfd);

DESCRIPTION
	Return the number of bits in one of the BFD @var{abfd}'s
	architecture's bytes.

*/

unsigned int
bfd_arch_bits_per_byte (abfd)
     bfd *abfd;
{
  return abfd->arch_info->bits_per_byte;
}

/*
FUNCTION
	bfd_arch_bits_per_address

SYNOPSIS
	unsigned int bfd_arch_bits_per_address(bfd *abfd);

DESCRIPTION
	Return the number of bits in one of the BFD @var{abfd}'s
	architecture's addresses.
*/

unsigned int
bfd_arch_bits_per_address (abfd)
     bfd *abfd;
{
  return abfd->arch_info->bits_per_address;
}


/*
INTERNAL_FUNCTION 
	bfd_default_compatible

SYNOPSIS
	const bfd_arch_info_type *bfd_default_compatible
	(const bfd_arch_info_type *a,
	const bfd_arch_info_type *b);

DESCRIPTION
	The default function for testing for compatibility.
*/

const bfd_arch_info_type *
bfd_default_compatible (a,b)
     const bfd_arch_info_type *a;
     const bfd_arch_info_type *b;
{
  if (a->arch != b->arch)
    return NULL;

  if (a->mach > b->mach)
    return a;

  if (b->mach > a->mach)
    return b;

  return a;
}


/*
INTERNAL_FUNCTION
	bfd_default_scan

SYNOPSIS
	boolean bfd_default_scan(const struct bfd_arch_info *info, const char *string);

DESCRIPTION
	The default function for working out whether this is an
	architecture hit and a machine hit.
*/

boolean 
bfd_default_scan (info, string)
     const struct bfd_arch_info *info;
     const char *string;
{
  const char *ptr_src;
  const char *ptr_tst;
  unsigned long number;
  enum bfd_architecture arch;

  /* First test for an exact match */
  if (strcmp (string, info->printable_name) == 0)
    return true;

  /* See how much of the supplied string matches with the
     architecture, eg the string m68k:68020 would match the 68k entry
     up to the :, then we get left with the machine number */

  for (ptr_src = string, ptr_tst = info->arch_name; 
       *ptr_src && *ptr_tst;
       ptr_src++, ptr_tst++) 
    {
      if (*ptr_src != *ptr_tst) break;
    }

  /* Chewed up as much of the architecture as will match, skip any
     colons */
  if (*ptr_src == ':')
    ptr_src++;
  
  if (*ptr_src == 0)
    {
      /* nothing more, then only keep this one if it is the default
	 machine for this architecture */
      return info->the_default;
    }

  number = 0;
  while (isdigit(*ptr_src))
    {
      number = number * 10 + *ptr_src  - '0';
      ptr_src++;
    }

  switch (number) 
    {
    case 65:
      arch = bfd_arch_w65;
      break;

    case 300:
      arch = bfd_arch_h8300;
      break;

    case 500:
      arch = bfd_arch_h8500;
      break;

    case 68010:
    case 68020:
    case 68030:
    case 68040:
    case 68332:
    case 68050:        
    case 68000: 
      arch = bfd_arch_m68k; 
      break;

    case 386: 
    case 80386:
    case 486:
    case 80486:
      arch = bfd_arch_i386;
      break;

    case 29000: 
      arch = bfd_arch_a29k;
      break;

    case 8000:
      arch = bfd_arch_z8k;
      break;

    case 32000:
      arch = bfd_arch_we32k;
      break;

    case 860:
    case 80860: 
      arch = bfd_arch_i860; 
      break;
    case 960:
    case 80960:
      arch = bfd_arch_i960;
      break;

    case 2000:
    case 3000:
    case 4000:
    case 4400:
      arch = bfd_arch_mips;
      break;

    case 6000:
      arch = bfd_arch_rs6000;
      break;

    default:  
      return false;
    }

  if (arch != info->arch) 
    return false;

  if (number != info->mach)
    return false;

  return true;
}


/*
FUNCTION
	bfd_get_arch_info

SYNOPSIS
	const bfd_arch_info_type * bfd_get_arch_info(bfd *abfd);

DESCRIPTION
	Return the architecture info struct in @var{abfd}.
*/

const bfd_arch_info_type *
bfd_get_arch_info (abfd)
     bfd *abfd;
{
  return abfd->arch_info;
}


/*
FUNCTION
	bfd_lookup_arch

SYNOPSIS
	const bfd_arch_info_type *bfd_lookup_arch
		(enum bfd_architecture
		arch,
		unsigned long machine);

DESCRIPTION
	Look for the architecure info structure which matches the
	arguments @var{arch} and @var{machine}. A machine of 0 matches the
	machine/architecture structure which marks itself as the
	default.
*/

const bfd_arch_info_type * 
bfd_lookup_arch (arch, machine)
     enum bfd_architecture arch;
     unsigned long machine;
{
  const bfd_arch_info_type * const *app, *ap;

  for (app = bfd_archures_list; *app != NULL; app++)
    {
      for (ap = *app; ap != NULL; ap = ap->next)
	{
	  if (ap->arch == arch
	      && (ap->mach == machine
		  || (machine == 0 && ap->the_default)))
	    return ap;
	}
    }

  return NULL;
}


/*
FUNCTION
	bfd_printable_arch_mach

SYNOPSIS
	const char *bfd_printable_arch_mach
		(enum bfd_architecture arch, unsigned long machine);

DESCRIPTION
	Return a printable string representing the architecture and
	machine type. 

	This routine is depreciated.
*/

const char *
bfd_printable_arch_mach (arch, machine)
     enum bfd_architecture arch;
     unsigned long machine;
{
    const bfd_arch_info_type *ap = bfd_lookup_arch (arch, machine);

    if (ap)
      return ap->printable_name;
    return "UNKNOWN!";
}
