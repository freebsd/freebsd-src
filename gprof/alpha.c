/*
 * Copyright (c) 1983, 1993, 1998
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

/*
 * Opcodes of the call instructions:
 */
#define OP_Jxx	0x1a
#define	OP_BSR	0x34

#define Jxx_FUNC_JMP		0
#define Jxx_FUNC_JSR		1
#define Jxx_FUNC_RET		2
#define Jxx_FUNC_JSR_COROUTINE	3

/* *INDENT-OFF* */
/* Here to document only.  We can't use this when cross compiling as
   the bitfield layout might not be the same as native.

   typedef union
     {
       struct
         {
	   unsigned other:26;
	   unsigned op_code:6;
         }
       a;				-- any format
       struct
         {
	   int disp:21;
	   unsigned ra:5;
	   unsigned op_code:6;
         }
       b;				-- branch format
       struct
         {
	   int hint:14;
	   unsigned func:2;
	   unsigned rb:5;
	   unsigned ra:5;
	   unsigned op_code:6;
         }
       j;				-- jump format
     }
    alpha_Instruction;
*/
/* *INDENT-ON* */

static Sym indirect_child;

void alpha_find_call (Sym *, bfd_vma, bfd_vma);

/*
 * On the Alpha we can only detect PC relative calls, which are
 * usually generated for calls to functions within the same
 * object file only.  This is still better than nothing, however.
 * (In particular it should be possible to find functions that
 *  potentially call integer division routines, for example.)
 */
void
alpha_find_call (Sym *parent, bfd_vma p_lowpc, bfd_vma p_highpc)
{
  bfd_vma pc, dest_pc;
  unsigned int insn;
  Sym *child;

  if (indirect_child.name == NULL)
    {
      sym_init (&indirect_child);
      indirect_child.name = _("<indirect child>");
      indirect_child.cg.prop.fract = 1.0;
      indirect_child.cg.cyc.head = &indirect_child;
    }

  DBG (CALLDEBUG, printf (_("[find_call] %s: 0x%lx to 0x%lx\n"),
			  parent->name, (unsigned long) p_lowpc,
			  (unsigned long) p_highpc));
  for (pc = (p_lowpc + 3) & ~(bfd_vma) 3; pc < p_highpc; pc += 4)
    {
      insn = bfd_get_32 (core_bfd, ((unsigned char *) core_text_space
				    + pc - core_text_sect->vma));
      switch (insn & (0x3f << 26))
	{
	case OP_Jxx << 26:
	  /*
	   * There is no simple and reliable way to determine the
	   * target of a jsr (the hint bits help, but there aren't
	   * enough bits to get a satisfactory hit rate).  Instead,
	   * for any indirect jump we simply add an arc from PARENT
	   * to INDIRECT_CHILD---that way the user it at least able
	   * to see that there are other calls as well.
	   */
	  if ((insn & (3 << 14)) == Jxx_FUNC_JSR << 14
	      || (insn & (3 << 14)) == Jxx_FUNC_JSR_COROUTINE << 14)
	    {
	      DBG (CALLDEBUG,
		   printf (_("[find_call] 0x%lx: jsr%s <indirect_child>\n"),
			   (unsigned long) pc,
			   ((insn & (3 << 14)) == Jxx_FUNC_JSR << 14
			    ? "" : "_coroutine")));
	      arc_add (parent, &indirect_child, (unsigned long) 0);
	    }
	  break;

	case OP_BSR << 26:
	  DBG (CALLDEBUG,
	       printf (_("[find_call] 0x%lx: bsr"), (unsigned long) pc));
	  /*
	   * Regular PC relative addressing.  Check that this is the
	   * address of a function.  The linker sometimes redirects
	   * the entry point by 8 bytes to skip loading the global
	   * pointer, so we allow for either address:
	   */
	  dest_pc = pc + 4 + (((bfd_signed_vma) (insn & 0x1fffff)
			       ^ 0x100000) - 0x100000);
	  if (hist_check_address (dest_pc))
	    {
	      child = sym_lookup (&symtab, dest_pc);
	      DBG (CALLDEBUG,
		   printf (" 0x%lx\t; name=%s, addr=0x%lx",
			   (unsigned long) dest_pc, child->name,
			   (unsigned long) child->addr));
	      if (child->addr == dest_pc || child->addr == dest_pc - 8)
		{
		  DBG (CALLDEBUG, printf ("\n"));
		  /* a hit:  */
		  arc_add (parent, child, (unsigned long) 0);
		  continue;
		}
	    }
	  /*
	   * Something funny going on.
	   */
	  DBG (CALLDEBUG, printf ("\tbut it's a botch\n"));
	  break;

	default:
	  break;
	}
    }
}
