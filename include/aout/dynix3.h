/* a.out specifics for Sequent Symmetry running Dynix 3.x

   Copyright 2001 Free Software Foundation, Inc.

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

#ifndef A_OUT_DYNIX3_H
#define A_OUT_DYNIX3_H

#define external_exec dynix_external_exec

/* struct exec for Dynix 3
 
   a_gdtbl and a_bootstrap are only for standalone binaries.
   Shared data fields are not supported by the kernel as of Dynix 3.1,
   but are supported by Dynix compiler programs.  */
struct dynix_external_exec
  {
    unsigned char e_info[4];
    unsigned char e_text[4];
    unsigned char e_data[4];
    unsigned char e_bss[4];
    unsigned char e_syms[4];
    unsigned char e_entry[4];
    unsigned char e_trsize[4];
    unsigned char e_drsize[4];
    unsigned char e_g_code[8];
    unsigned char e_g_data[8];
    unsigned char e_g_desc[8];
    unsigned char e_shdata[4];
    unsigned char e_shbss[4];
    unsigned char e_shdrsize[4];
    unsigned char e_bootstrap[44];
    unsigned char e_reserved[12];
    unsigned char e_version[4];
  };

#define	EXEC_BYTES_SIZE	(128)

/* All executables under Dynix are demand paged with read-only text,
   Thus no NMAGIC.
  
   ZMAGIC has a page of 0s at virtual 0,
   XMAGIC has an invalid page at virtual 0.  */
#define OMAGIC	0x12eb		/* .o */
#define ZMAGIC	0x22eb		/* zero @ 0, demand load */
#define XMAGIC	0x32eb		/* invalid @ 0, demand load */
#define SMAGIC	0x42eb		/* standalone, not supported here */

#define N_BADMAG(x) ((OMAGIC != N_MAGIC(x)) && \
		     (ZMAGIC != N_MAGIC(x)) && \
		     (XMAGIC != N_MAGIC(x)) && \
		     (SMAGIC != N_MAGIC(x)))

#define N_ADDRADJ(x) ((ZMAGIC == N_MAGIC(x) || XMAGIC == N_MAGIC(x)) ? 0x1000 : 0)

#define N_TXTOFF(x) (EXEC_BYTES_SIZE)
#define N_DATOFF(x) (N_TXTOFF(x) + N_TXTSIZE(x))
#define N_SHDATOFF(x) (N_DATOFF(x) + (x).a_data)
#define N_TRELOFF(x) (N_SHDATOFF(x) + (x).a_shdata)
#define N_DRELOFF(x) (N_TRELOFF(x) + (x).a_trsize)
#define N_SHDRELOFF(x) (N_DRELOFF(x) + (x).a_drsize)
#define N_SYMOFF(x) (N_SHDRELOFF(x) + (x).a_shdrsize)
#define N_STROFF(x) (N_SYMOFF(x) + (x).a_syms)

#define N_TXTADDR(x) \
	(((OMAGIC == N_MAGIC(x)) || (SMAGIC == N_MAGIC(x))) ? 0 \
	 : TEXT_START_ADDR + EXEC_BYTES_SIZE)

#define N_TXTSIZE(x) \
	(((OMAGIC == N_MAGIC(x)) || (SMAGIC == N_MAGIC(x))) ? ((x).a_text) \
	 : ((x).a_text - N_ADDRADJ(x) - EXEC_BYTES_SIZE))

#endif /* A_OUT_DYNIX3_H */
