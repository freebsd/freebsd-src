/* BFD library support routines for architectures.
   Copyright (C) 1990-1991 Free Software Foundation, Inc.
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
Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

/*

SECTION
	Architectures

	BFD's idea of an architecture is implimented in
	<<archures.c>>. BFD keeps one atom in a BFD describing the
	architecture of the data attached to the BFD;  a pointer to a
	<<bfd_arch_info_type>>.  

	Pointers to structures can be requested independently of a bfd
	so that an architecture's information can be interrogated
	without access to an open bfd.

	The arch information is provided by each architecture package.
	The set of default architectures is selected by the #define
	<<SELECT_ARCHITECTURES>>.  This is normally set up in the
	<<config/target.mt>> file of your choice.  If the name is not
	defined, then all the architectures supported are included. 

	When BFD starts up, all the architectures are called with an
	initialize method.  It is up to the architecture back end to
	insert as many items into the list of architectures as it wants to;
	generally this would be one for each machine and one for the
	default case (an item with a machine field of 0). 
*/

/*

SUBSECTION
	bfd_architecture

DESCRIPTION
	This enum gives the object file's CPU architecture, in a
	global sense --- i.e., what processor family does it belong to?
	There is another field, which indicates what processor within
	the family is in use.  The machine gives a number which
	distingushes different versions of the architecture,
	containing for example 2 and 3 for Intel i960 KA and i960 KB,
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
.
.  bfd_arch_a29k,      {* AMD 29000 *}
.  bfd_arch_sparc,     {* SPARC *}
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
.  bfd_arch_rs6000,    {* IBM RS/6000 *}
.  bfd_arch_hppa,      {* HP PA RISC *}
.  bfd_arch_z8k,       {* Zilog Z8000 *}
.#define bfd_mach_z8001		1
.#define bfd_mach_z8002		2
.  bfd_arch_h8500,     {* Hitachi H8/500 *}
.  bfd_arch_sh,        {* Hitachi SH *}
.  bfd_arch_alpha,     {* Dec Alpha *}
.  bfd_arch_last
.  };


*/

#include "bfd.h"
#include "sysdep.h"
#include "libbfd.h"

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
.  long mach;
.  char *arch_name;
.  CONST  char *printable_name;
.  unsigned int section_align_power;
. {* true if this is the default machine for the architecture *}
.  boolean the_default;	
.  CONST struct bfd_arch_info * (*compatible)
.	PARAMS ((CONST struct bfd_arch_info *a,
.	         CONST struct bfd_arch_info *b));
.
.  boolean (*scan) PARAMS ((CONST struct bfd_arch_info *, CONST char *));
.  {* How to disassemble an instruction, producing a printable
.     representation on a specified stdio stream.  This isn't
.     defined for most processors at present, because of the size
.     of the additional tables it would drag in, and because gdb
.     wants to use a different interface.  *}
.  unsigned int (*disassemble) PARAMS ((bfd_vma addr, CONST char *data,
.				        PTR stream));
.
.  struct bfd_arch_info *next;
.} bfd_arch_info_type;
*/

bfd_arch_info_type   *bfd_arch_info_list;


/*
FUNCTION
	bfd_printable_name

SYNOPSIS
	CONST char *bfd_printable_name(bfd *abfd);

DESCRIPTION
	Return a printable string representing the architecture and machine
	from the pointer to the arch info structure 

*/

CONST char *
DEFUN(bfd_printable_name, (abfd),
      bfd *abfd) 
{
  return abfd->arch_info->printable_name;
}



/*
FUNCTION
	bfd_scan_arch

SYNOPSIS
	bfd_arch_info_type *bfd_scan_arch(CONST char *);

DESCRIPTION
	This routine is provided with a string and tries to work out
	if bfd supports any cpu which could be described with the name
	provided.  The routine returns a pointer to an arch_info
	structure if a machine is found, otherwise NULL.

*/

bfd_arch_info_type *
DEFUN(bfd_scan_arch,(string),
      CONST char *string)
{
  struct bfd_arch_info *ap;

  /* Look through all the installed architectures */
  for (ap = bfd_arch_info_list;
       ap != (bfd_arch_info_type *)NULL;
       ap = ap->next) {

    if (ap->scan(ap, string)) 
      return ap;
  }
  return (bfd_arch_info_type *)NULL;
}



/*
FUNCTION
	bfd_arch_get_compatible

SYNOPSIS
	CONST bfd_arch_info_type *bfd_arch_get_compatible(
		CONST bfd *abfd,
	        CONST bfd *bbfd);

DESCRIPTION
	This routine is used to determine whether two BFDs'
	architectures and achine types are compatible.  It calculates
	the lowest common denominator between the two architectures
	and machine types implied by the BFDs and returns a pointer to
	an arch_info structure describing the compatible machine.
*/

CONST bfd_arch_info_type *
DEFUN(bfd_arch_get_compatible,(abfd, bbfd),
CONST    bfd *abfd AND
CONST    bfd *bbfd)

{
  return  abfd->arch_info->compatible(abfd->arch_info,bbfd->arch_info);
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

.extern bfd_arch_info_type bfd_default_arch_struct;

*/

bfd_arch_info_type bfd_default_arch_struct =
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
	void bfd_set_arch_info(bfd *, bfd_arch_info_type *);

*/

void DEFUN(bfd_set_arch_info,(abfd, arg),
bfd *abfd AND
bfd_arch_info_type *arg)
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
	Set the architecture and machine type in a bfd. This finds the
	correct pointer to structure and inserts it into the arch_info
	pointer. 
*/

boolean DEFUN(bfd_default_set_arch_mach,(abfd, arch, mach),
	      bfd *abfd AND
	      enum bfd_architecture arch AND
	      unsigned    long mach)
{
  static struct bfd_arch_info *old_ptr = &bfd_default_arch_struct;
  boolean found = false;
  /* run through the table to find the one we want, we keep a little
     cache to speed things up */
  if (old_ptr == 0 || arch != old_ptr->arch || mach != old_ptr->mach) {
    bfd_arch_info_type *ptr;
    old_ptr = (bfd_arch_info_type *)NULL;
    for (ptr = bfd_arch_info_list;
	 ptr != (bfd_arch_info_type *)NULL;
	 ptr= ptr->next) {
      if (ptr->arch == arch &&
	  ((ptr->mach == mach) || (ptr->the_default && mach == 0))) {
	old_ptr = ptr;
	found = true;
	break;
      }
    }
    if (found==false) {
      /*looked for it and it wasn't there, so put in the default */
      old_ptr = &bfd_default_arch_struct;
      bfd_error = bad_value;
    }
  }
  else {
    /* it was in the cache */
    found = true;
  }

  abfd->arch_info = old_ptr;

  return found;
}





/*
FUNCTION
	bfd_get_arch

SYNOPSIS
	enum bfd_architecture bfd_get_arch(bfd *abfd);

DESCRIPTION
	Returns the enumerated type which describes the supplied bfd's
	architecture

*/

enum bfd_architecture DEFUN(bfd_get_arch, (abfd), bfd *abfd)
{
    return abfd->arch_info->arch;
}

/*
FUNCTION
	bfd_get_mach

SYNOPSIS
	unsigned long bfd_get_mach(bfd *abfd);

DESCRIPTION
	Returns the long type which describes the supplied bfd's
	machine
*/

unsigned long  
DEFUN(bfd_get_mach, (abfd), bfd *abfd)
{
    return abfd->arch_info->mach;
}

/*
FUNCTION
	bfd_arch_bits_per_byte

SYNOPSIS
	unsigned int bfd_arch_bits_per_byte(bfd *abfd);

DESCRIPTION
	Returns the number of bits in one of the architectures bytes

*/

unsigned int DEFUN(bfd_arch_bits_per_byte, (abfd), bfd *abfd)
  {
    return abfd->arch_info->bits_per_byte;
  }

/*
FUNCTION
	bfd_arch_bits_per_address

SYNOPSIS
	unsigned int bfd_arch_bits_per_address(bfd *abfd);

DESCRIPTION
	Returns the number of bits in one of the architectures addresses
*/

unsigned int DEFUN(bfd_arch_bits_per_address, (abfd), bfd *abfd)
  {
    return abfd->arch_info->bits_per_address;
  }


extern void bfd_a29k_arch PARAMS ((void));
extern void bfd_alpha_arch PARAMS ((void));
extern void bfd_h8300_arch PARAMS ((void));
extern void bfd_h8500_arch PARAMS ((void));
extern void bfd_hppa_arch PARAMS ((void));
extern void bfd_i386_arch PARAMS ((void));
extern void bfd_i960_arch PARAMS ((void));
extern void bfd_m68k_arch PARAMS ((void));
extern void bfd_m88k_arch PARAMS ((void));
extern void bfd_mips_arch PARAMS ((void));
extern void bfd_rs6000_arch PARAMS ((void));
extern void bfd_sh_arch PARAMS ((void));
extern void bfd_sparc_arch PARAMS ((void));
extern void bfd_vax_arch PARAMS ((void));
extern void bfd_we32k_arch PARAMS ((void));
extern void bfd_z8k_arch PARAMS ((void));

static void (*archures_init_table[]) PARAMS ((void)) = 
{
#ifdef SELECT_ARCHITECTURES
  SELECT_ARCHITECTURES,
#else
  bfd_a29k_arch,
  bfd_alpha_arch,
  bfd_h8300_arch,
  bfd_h8500_arch,
  bfd_hppa_arch,
  bfd_i386_arch,
  bfd_i960_arch,
  bfd_m68k_arch,
  bfd_m88k_arch,
  bfd_mips_arch,
  bfd_rs6000_arch,
  bfd_sh_arch,
  bfd_sparc_arch,
  bfd_vax_arch,
  bfd_we32k_arch,
  bfd_z8k_arch,
#endif
  0
  };



/*
INTERNAL_FUNCTION 
	bfd_arch_init

SYNOPSIS
	void  bfd_arch_init(void);

DESCRIPTION
	This routine initializes the architecture dispatch table by
	calling all installed architecture packages and getting them
	to poke around.
*/

void
DEFUN_VOID(bfd_arch_init)
{
    void (**ptable) PARAMS ((void));
    for (ptable = archures_init_table; 
	 *ptable ;
	 ptable++)
    {
	(*ptable)();
    }
}


/*
INTERNAL_FUNCTION
	bfd_arch_linkin

SYNOPSIS
	void bfd_arch_linkin(bfd_arch_info_type *);

DESCRIPTION
	Link the provided arch info structure into the list
*/

void DEFUN(bfd_arch_linkin,(ptr),
	   bfd_arch_info_type *ptr)
{
  ptr->next = bfd_arch_info_list;
  bfd_arch_info_list = ptr;
}


/*
INTERNAL_FUNCTION 
	bfd_default_compatible

SYNOPSIS
	CONST bfd_arch_info_type *bfd_default_compatible
	(CONST bfd_arch_info_type *a,
	CONST bfd_arch_info_type *b);

DESCRIPTION
	The default function for testing for compatibility.
*/

CONST bfd_arch_info_type *
DEFUN(bfd_default_compatible,(a,b),
      CONST bfd_arch_info_type *a AND
      CONST bfd_arch_info_type *b)
{
  if(a->arch != b->arch) return NULL;

  if (a->mach > b->mach) {
    return a;
  }
  if (b->mach > a->mach) {
    return b;
  }
  return a;
}


/*
INTERNAL_FUNCTION
	bfd_default_scan

SYNOPSIS
	boolean bfd_default_scan(CONST struct bfd_arch_info *, CONST char *);

DESCRIPTION
	The default function for working out whether this is an
	architecture hit and a machine hit.
*/

boolean 
DEFUN(bfd_default_scan,(info, string),
CONST struct bfd_arch_info *info AND
CONST char *string)
{
    CONST  char *ptr_src;
    CONST   char *ptr_tst;
    unsigned long number;
    enum bfd_architecture arch;
    /* First test for an exact match */
    if (strcmp(string, info->printable_name) == 0) return true;

    /* See how much of the supplied string matches with the
       architecture, eg the string m68k:68020 would match the 68k entry
       up to the :, then we get left with the machine number */

    for (ptr_src = string,
	 ptr_tst = info->arch_name; 
	 *ptr_src && *ptr_tst;
	 ptr_src++,
	 ptr_tst++) 
    {
	if (*ptr_src != *ptr_tst) break;
    }

    /* Chewed up as much of the architecture as will match, skip any
       colons */
    if (*ptr_src == ':') ptr_src++;
  
    if (*ptr_src == 0) {
	    /* nothing more, then only keep this one if it is the default
	       machine for this architecture */
	    return info->the_default;
	}
    number = 0;
    while (isdigit(*ptr_src)) {
	    number = number * 10 + *ptr_src  - '0';
	    ptr_src++;
	}

    switch (number) 
    {
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
	bfd_arch_info_type * bfd_get_arch_info(bfd *);

*/

bfd_arch_info_type *
DEFUN(bfd_get_arch_info,(abfd),
bfd *abfd)
{
  return  abfd->arch_info;
}


/*
FUNCTION
	bfd_lookup_arch

SYNOPSIS
	bfd_arch_info_type *bfd_lookup_arch
		(enum bfd_architecture
		arch,
		long machine);

DESCRIPTION
	Look for the architecure info struct which matches the
	arguments given. A machine of 0 will match the
	machine/architecture structure which marks itself as the
	default.
*/

bfd_arch_info_type * 
DEFUN(bfd_lookup_arch,(arch, machine),
enum bfd_architecture arch AND
long machine)
{
    bfd_arch_info_type *ap;
    bfd_check_init();  
    for (ap = bfd_arch_info_list; 
	 ap !=  (bfd_arch_info_type *)NULL;
	 ap = ap->next) {
	    if (ap->arch == arch &&
		((ap->mach == machine) 
		 || (ap->the_default && machine == 0))) {
		    return ap;
		}
	}
    return (bfd_arch_info_type *)NULL;
}



/*
FUNCTION
	bfd_printable_arch_mach

SYNOPSIS
	CONST char * bfd_printable_arch_mach
		(enum bfd_architecture arch, unsigned long machine);

DESCRIPTION
	Return a printable string representing the architecture and
	machine type. 

	NB. The use of this routine is depreciated.
*/

CONST char *
DEFUN(bfd_printable_arch_mach,(arch, machine),
      enum bfd_architecture arch AND
      unsigned long machine)
{
    bfd_arch_info_type *ap = bfd_lookup_arch(arch, machine);
    if(ap) return ap->printable_name;
    return "UNKNOWN!";
}
