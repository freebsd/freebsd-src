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
 * $FreeBSD$
 */

#ifndef GNode_h_39503bf2
#define	GNode_h_39503bf2

#include "sprite.h"
#include "lst.h"

struct _Suff;

/*
 * The structure for an individual graph node. Each node has several
 * pieces of data associated with it.
 */
typedef struct GNode {
	char	*name;	/* The target's name */
	char	*path;	/* The full pathname of the target file */

	/*
	 * The type of operator used to define the sources (qv. parse.c)
	 * See OP_ flags in make.h
	 */
	int	type;

	int	order;	/* Its wait weight */

	Boolean	make;	/* TRUE if this target needs to be remade */

	/* Set to reflect the state of processing on this node */
	enum {
		UNMADE,		/* Not examined yet */

		/*
		 * Target is already being made. Indicates a cycle in the graph.
		 * (compat mode only)
		 */
		BEINGMADE,

		MADE,		/* Was out-of-date and has been made */
		UPTODATE,	/* Was already up-to-date */

		/*
		 * An error occured while it was being
		 * made (used only in compat mode)
		 */
		ERROR,

		/*
		 * The target was aborted due to an
		 * error making an inferior (compat).
		 */
		ABORTED,

		/*
		 * Marked as potentially being part of a graph cycle.  If we
		 * come back to a node marked this way, it is printed and
		 * 'made' is changed to ENDCYCLE.
		 */
		CYCLE,

		/*
		 * The cycle has been completely printed.  Go back and
		 * unmark all its members.
		 */
		ENDCYCLE
	} made;

	/* TRUE if one of this target's children was made */
	Boolean	childMade;

	int	unmade;		/* The number of unmade children */
	int	mtime;		/* Its modification time */
	int	cmtime;		/* Modification time of its youngest child */

	/*
	 * Links to parents for which this is an implied source, if any. (nodes
	 * that depend on this, as gleaned from the transformation rules.
	 */
	Lst	iParents;

	/* List of nodes of the same name created by the :: operator */
	Lst	cohorts;

	/* Lst of nodes for which this is a source (that depend on this one) */
	Lst	parents;

 	/* List of nodes on which this depends */
	Lst	children;

	/*
	 * List of nodes that must be made (if they're made) after this node is,
	 * but that do not depend on this node, in the normal sense.
	 */
	Lst	successors;

	/*
	 * List of nodes that must be made (if they're made) before this node
	 * can be, but that do no enter into the datedness of this node.
	 */
	Lst	preds;

	/*
	 * List of ``local'' variables that are specific to this target
	 * and this target only (qv. var.c [$@ $< $?, etc.])
	 */
	Lst	context;

	/*
	 * List of strings that are commands to be given to a shell
	 * to create this target.
	 */
	Lst	commands;

	/* current command executing in compat mode */
	LstNode	*compat_command;

	/*
	 * Suffix for the node (determined by Suff_FindDeps and opaque to
	 * everyone but the Suff module)
	 */
	struct _Suff	*suffix;
} GNode;

#endif /* GNode_h_39503bf2 */
