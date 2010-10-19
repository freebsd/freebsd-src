/*
 * Copyright (c) 1983, 1993, 2001
 *      The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#include "gprof.h"
#include "search_list.h"
#include "source.h"
#include "symtab.h"
#include "cg_arcs.h"
#include "corefile.h"
#include "hist.h"

static int i386_iscall (unsigned char *);
void i386_find_call (Sym *, bfd_vma, bfd_vma);

static int
i386_iscall (unsigned char *ip)
{
  if (*ip == 0xe8)
    return 1;
  return 0;
}


void
i386_find_call (Sym *parent, bfd_vma p_lowpc, bfd_vma p_highpc)
{
  unsigned char *instructp;
  Sym *child;
  bfd_vma pc, destpc;

  if (core_text_space == 0)
    {
      return;
    }
  if (p_lowpc < s_lowpc)
    {
      p_lowpc = s_lowpc;
    }
  if (p_highpc > s_highpc)
    {
      p_highpc = s_highpc;
    }
  DBG (CALLDEBUG, printf ("[findcall] %s: 0x%lx to 0x%lx\n",
			  parent->name, (unsigned long) p_lowpc,
			  (unsigned long) p_highpc));

  for (pc = p_lowpc; pc < p_highpc; ++pc)
    {
      instructp = (unsigned char *) core_text_space + pc - core_text_sect->vma;
      if (i386_iscall (instructp))
	{
	  DBG (CALLDEBUG,
	       printf ("[findcall]\t0x%lx:call", (unsigned long) pc));
	  /*
	   *  regular pc relative addressing
	   *    check that this is the address of
	   *    a function.
	   */

	  destpc = bfd_get_32 (core_bfd, instructp + 1) + pc + 5;
	  if (destpc >= s_lowpc && destpc <= s_highpc)
	    {
	      child = sym_lookup (&symtab, destpc);
	      if (child && child->addr == destpc)
		{
		  /*
		   *      a hit
		   */
		  DBG (CALLDEBUG,
		       printf ("\tdestpc 0x%lx (%s)\n",
			       (unsigned long) destpc, child->name));
		  arc_add (parent, child, (unsigned long) 0);
		  instructp += 4;	/* call is a 5 byte instruction */
		  continue;
		}
	    }
	  /*
	   *  else:
	   *    it looked like a callf, but it:
	   *      a) wasn't actually a callf, or
	   *      b) didn't point to a known function in the symtab, or
	   *      c) something funny is going on.
	   */
	  DBG (CALLDEBUG, printf ("\tbut it's a botch\n"));
	}
    }
}
