/* m88k.h -- Assembler for the Motorola 88000
   Contributed by Devon Bowen of Buffalo University
   and Torbjorn Granlund of the Swedish Institute of Computer Science.
   Copyright (C) 1989-1992 Free Software Foundation, Inc.

   This file is part of GAS, the GNU Assembler.
   
   GAS is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.
   
   GAS is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with GAS; see the file COPYING.  If not, write to
   the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.  */

#define TC_M88K 1

#define NO_LISTING
#define NO_DOT_PSEUDOS
#define ALLOW_ATSIGN

#define LOCAL_LABEL(name) (name[0] == '@' \
			   && ( name[1] == 'L' || name[1] == '.' ))

#define tc_crawl_symbol_chain(a)	{;} /* not used */
#define tc_headers_hook(a)		{;} /* not used */
#define tc_aout_pre_write_hook(x)	{;} /* not used */

 /* end of tc-m88k.h */
