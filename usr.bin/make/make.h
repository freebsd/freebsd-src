/*-
 * Copyright (c) 1988, 1989, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1989 by Berkeley Softworks
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Adam de Boor.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
 *
 *	@(#)make.h	8.3 (Berkeley) 6/13/95
 * $FreeBSD$
 */

#ifndef make_h_a91074b9
#define	make_h_a91074b9

/*-
 * make.h --
 *	The global definitions for pmake
 */

#include "sprite.h"

struct GNode;
struct Lst;

/*
 * The OP_ constants are used when parsing a dependency line as a way of
 * communicating to other parts of the program the way in which a target
 * should be made. These constants are bitwise-OR'ed together and
 * placed in the 'type' field of each node. Any node that has
 * a 'type' field which satisfies the OP_NOP function was never never on
 * the lefthand side of an operator, though it may have been on the
 * righthand side...
 */
#define	OP_DEPENDS	0x00000001  /* Execution of commands depends on
				     * kids (:) */
#define	OP_FORCE	0x00000002  /* Always execute commands (!) */
#define	OP_DOUBLEDEP	0x00000004  /* Execution of commands depends on kids
				     * per line (::) */
#define	OP_OPMASK	(OP_DEPENDS|OP_FORCE|OP_DOUBLEDEP)

#define	OP_OPTIONAL	0x00000008  /* Don't care if the target doesn't
				     * exist and can't be created */
#define	OP_USE		0x00000010  /* Use associated commands for parents */
#define	OP_EXEC	  	0x00000020  /* Target is never out of date, but always
				     * execute commands anyway. Its time
				     * doesn't matter, so it has none...sort
				     * of */
#define	OP_IGNORE	0x00000040  /* Ignore errors when creating the node */
#define	OP_PRECIOUS	0x00000080  /* Don't remove the target when
				     * interrupted */
#define	OP_SILENT	0x00000100  /* Don't echo commands when executed */
#define	OP_MAKE		0x00000200  /* Target is a recurrsive make so its
				     * commands should always be executed when
				     * it is out of date, regardless of the
				     * state of the -n or -t flags */
#define	OP_JOIN 	0x00000400  /* Target is out-of-date only if any of its
				     * children was out-of-date */
#define	OP_INVISIBLE	0x00004000  /* The node is invisible to its parents.
				     * I.e. it doesn't show up in the parents's
				     * local variables. */
#define	OP_NOTMAIN	0x00008000  /* The node is exempt from normal 'main
				     * target' processing in parse.c */
#define	OP_PHONY	0x00010000  /* Not a file target; run always */
/* Attributes applied by PMake */
#define	OP_TRANSFORM	0x80000000  /* The node is a transformation rule */
#define	OP_MEMBER 	0x40000000  /* Target is a member of an archive */
#define	OP_LIB	  	0x20000000  /* Target is a library */
#define	OP_ARCHV  	0x10000000  /* Target is an archive construct */
#define	OP_HAS_COMMANDS	0x08000000  /* Target has all the commands it should.
				     * Used when parsing to catch multiple
				     * commands for a target */
#define	OP_SAVE_CMDS	0x04000000  /* Saving commands on .END (Compat) */
#define	OP_DEPS_FOUND	0x02000000  /* Already processed by Suff_FindDeps */

/*
 * OP_NOP will return TRUE if the node with the given type was not the
 * object of a dependency operator
 */
#define	OP_NOP(t)	(((t) & OP_OPMASK) == 0x00000000)

/*
 * Error levels for parsing. PARSE_FATAL means the process cannot continue
 * once the makefile has been parsed. PARSE_WARNING means it can. Passed
 * as the first argument to Parse_Error.
 */
#define	PARSE_WARNING	2
#define	PARSE_FATAL	1

/*
 * Definitions for the "local" variables. Used only for clarity.
 */
#define	TARGET	  	  "@" 	/* Target of dependency */
#define	OODATE	  	  "?" 	/* All out-of-date sources */
#define	ALLSRC	  	  ">" 	/* All sources */
#define	IMPSRC	  	  "<" 	/* Source implied by transformation */
#define	PREFIX	  	  "*" 	/* Common prefix */
#define	ARCHIVE	  	  "!" 	/* Archive in "archive(member)" syntax */
#define	MEMBER	  	  "%" 	/* Member in "archive(member)" syntax */

#define	FTARGET           "@F"  /* file part of TARGET */
#define	DTARGET           "@D"  /* directory part of TARGET */
#define	FIMPSRC           "<F"  /* file part of IMPSRC */
#define	DIMPSRC           "<D"  /* directory part of IMPSRC */
#define	FPREFIX           "*F"  /* file part of PREFIX */
#define	DPREFIX           "*D"  /* directory part of PREFIX */

/*
 * Warning flags
 */
enum {
	WARN_DIRSYNTAX	= 0x0001,	/* syntax errors in directives */
};

int Make_TimeStamp(struct GNode *, struct GNode *);
Boolean Make_OODate(struct GNode *);
int Make_HandleUse(struct GNode *, struct GNode *);
void Make_Update(struct GNode *);
void Make_DoAllVar(struct GNode *);
Boolean Make_Run(struct Lst *);

#endif /* make_h_a91074b9 */
