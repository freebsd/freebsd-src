/* GOULD RISC target-dependent code for GDB, the GNU debugger.
   Copyright 1986, 1987, 1989, 1991 Free Software Foundation, Inc.

This file is part of GDB.

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

#include "defs.h"
#include "symtab.h"
#include "frame.h"
#include "gdbcore.h"
#if defined GOULD_PN
#include "opcode/pn.h"
#else
#include "opcode/np1.h"
#endif

/* GOULD RISC instructions are never longer than this many bytes.  */
#define MAXLEN 4

/* Number of elements in the opcode table.  */
#define NOPCODES (sizeof gld_opcodes / sizeof gld_opcodes[0])


/* Print the GOULD instruction at address MEMADDR in debugged memory,
   on STREAM.  Returns length of the instruction, in bytes.  */

int
gould_print_insn (memaddr, stream)
     CORE_ADDR memaddr;
     FILE *stream;
{
	unsigned char buffer[MAXLEN];
	register int i;
	register char *d;
	register int bestmask;
	unsigned best;
	int temp, index, bestlen;

	read_memory (memaddr, buffer, MAXLEN);

	bestmask = 0;
	index = -1;
	best = 0xffffffff;
	for (i = 0; i < NOPCODES; i++)
	{
		register unsigned int opcode = gld_opcodes[i].opcode;
		register unsigned int mask = gld_opcodes[i].mask;
		register unsigned int len = gld_opcodes[i].length;
		register unsigned int test;

		/* Get possible opcode bytes into integer */
		test = buffer[0] << 24;
		test |= buffer[1] << 16;
		test |= buffer[2] << 8;
		test |= buffer[3];

		/* Mask with opcode and see if match */
		if ((opcode & mask) == (test & mask))
		{
			/* See if second or third match */
			if (index >= 0)
			{
				/* Take new one if it looks good */
				if (bestlen == MAXLEN && len == MAXLEN)
				{
					/* See if lower bits matched */
					if (((bestmask & 3) == 0) &&
					    ((mask & 3) != 0))
					{
						bestmask = mask;
						bestlen = len;
						best = test;
						index = i;
					}
				}
			}
			else
			{
				/* First match, save it */
				bestmask = mask;
				bestlen = len;
				best = test;
				index = i;
			}
		}
	}

	/* Handle undefined instructions.  */
	if (index < 0)
	{
		fprintf (stream, "undefined   0%o",(buffer[0]<<8)+buffer[1]);
		return 2;
	}

	/* Print instruction name */
	fprintf (stream, "%-12s", gld_opcodes[index].name);

	/* Adjust if short instruction */
	if (gld_opcodes[index].length < 4)
	{
		best >>= 16;
		i = 0;
	}
	else
	{
		i = 16;
	}

	/* Dump out instruction arguments */
  	for (d = gld_opcodes[index].args; *d; ++d)
	{
	    switch (*d)
	    {
		case 'f':
		    fprintf (stream, "%d",  (best >> (7 + i)) & 7);
		    break;
		case 'r':
		    fprintf (stream, "r%d", (best >> (7 + i)) & 7);
		    break;
		case 'R':
		    fprintf (stream, "r%d", (best >> (4 + i)) & 7);
		    break;
		case 'b':
		    fprintf (stream, "b%d", (best >> (7 + i)) & 7);
		    break;
		case 'B':
		    fprintf (stream, "b%d", (best >> (4 + i)) & 7);
		    break;
		case 'v':
		    fprintf (stream, "b%d", (best >> (7 + i)) & 7);
		    break;
		case 'V':
		    fprintf (stream, "b%d", (best >> (4 + i)) & 7);
		    break;
		case 'X':
		    temp = (best >> 20) & 7;
		    if (temp)
			fprintf (stream, "r%d", temp);
		    else
			putc ('0', stream);
		    break;
		case 'A':
		    temp = (best >> 16) & 7;
		    if (temp)
			fprintf (stream, "(b%d)", temp);
		    break;
		case 'S':
		    fprintf (stream, "#%d", best & 0x1f);
		    break;
		case 'I':
		    fprintf (stream, "#%x", best & 0xffff);
		    break;
		case 'O':
		    fprintf (stream, "%x", best & 0xffff);
		    break;
		case 'h':
		    fprintf (stream, "%d", best & 0xfffe);
		    break;
		case 'd':
		    fprintf (stream, "%d", best & 0xfffc);
		    break;
		case 'T':
		    fprintf (stream, "%d", (best >> 8) & 0xff);
		    break;
		case 'N':
		    fprintf (stream, "%d", best & 0xff);
		    break;
		default:
		    putc (*d, stream);
		    break;
	    }
	}

	/* Return length of instruction */
  	return (gld_opcodes[index].length);
}

/*
 * Find the number of arguments to a function.
 */
findarg(frame)
	struct frame_info *frame;
{
	register struct symbol *func;
	register unsigned pc;

#ifdef notdef
	/* find starting address of frame function */
	pc = get_pc_function_start (frame->pc);

	/* find function symbol info */
	func = find_pc_function (pc);

	/* call blockframe code to look for match */
	if (func != NULL)
                return (func->value.block->nsyms / sizeof(int));
#endif

        return (-1);
} 

/*
 * In the case of the NPL, the frame's norminal address is Br2 and the 
 * previous routines frame is up the stack X bytes.  Finding out what
 * 'X' is can be tricky.
 *
 *    1.) stored in the code function header xA(Br1).
 *    2.) must be careful of recurssion.
 */
CORE_ADDR
findframe(thisframe)
    struct frame_info *thisframe;
{
    register CORE_ADDR pointer;
    CORE_ADDR framechain();
#if 0    
    struct frame_info *frame;

    /* Setup toplevel frame structure */
    frame->pc = read_pc();
    frame->next_frame = 0;
    frame->frame = read_register (SP_REGNUM);	/* Br2 */

    /* Search for this frame (start at current Br2) */
    do
    {
	pointer = framechain(frame);
	frame->next_frame = frame->frame;
	frame->frame = pointer;
	frame->pc = FRAME_SAVED_PC(frame);
    }
    while (frame->next_frame != thisframe);
#endif

    pointer = framechain (thisframe);

    /* stop gap for now, end at __base3 */
    if (thisframe->pc == 0)
	return 0;

    return pointer;
}

/*
 * Gdb front-end and internal framechain routine.
 * Go back up stack one level.  Tricky...
 */
CORE_ADDR
framechain(frame)
    register struct frame_info *frame;
{
    register CORE_ADDR func, prevsp;
    register unsigned value;

    /* Get real function start address from internal frame address */
    func = get_pc_function_start(frame->pc);

    /* If no stack given, read register Br1 "(sp)" */
    if (!frame->frame)
	prevsp = read_register (SP_REGNUM);
    else
	prevsp = frame->frame;

    /* Check function header, case #2 */
    value = read_memory_integer (func, 4);
    if (value)
    {
	/* 32bit call push value stored in function header */
	prevsp += value;
    }
    else
    {
	/* read half-word from suabr at start of function */
	prevsp += read_memory_integer (func + 10, 2);
    }

    return (prevsp);
}
