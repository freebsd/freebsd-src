/*	$NetBSD: targ.c,v 1.81 2020/09/01 20:54:00 rillig Exp $	*/

/*
 * Copyright (c) 1988, 1989, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
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

/*
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
 */

#ifndef MAKE_NATIVE
static char rcsid[] = "$NetBSD: targ.c,v 1.81 2020/09/01 20:54:00 rillig Exp $";
#else
#include <sys/cdefs.h>
#ifndef lint
#if 0
static char sccsid[] = "@(#)targ.c	8.2 (Berkeley) 3/19/94";
#else
__RCSID("$NetBSD: targ.c,v 1.81 2020/09/01 20:54:00 rillig Exp $");
#endif
#endif /* not lint */
#endif

/*-
 * targ.c --
 *	Functions for maintaining the Lst allTargets. Target nodes are
 *	kept in two structures: a Lst and a hash table.
 *
 * Interface:
 *	Targ_Init 	    	Initialization procedure.
 *
 *	Targ_End 	    	Cleanup the module
 *
 *	Targ_List 	    	Return the list of all targets so far.
 *
 *	Targ_NewGN	    	Create a new GNode for the passed target
 *	    	  	    	(string). The node is *not* placed in the
 *	    	  	    	hash table, though all its fields are
 *	    	  	    	initialized.
 *
 *	Targ_FindNode	    	Find the node for a given target, creating
 *	    	  	    	and storing it if it doesn't exist and the
 *	    	  	    	flags are right (TARG_CREATE)
 *
 *	Targ_FindList	    	Given a list of names, find nodes for all
 *	    	  	    	of them. If a name doesn't exist and the
 *	    	  	    	TARG_NOCREATE flag was given, an error message
 *	    	  	    	is printed. Else, if a name doesn't exist,
 *	    	  	    	its node is created.
 *
 *	Targ_Ignore	    	Return TRUE if errors should be ignored when
 *	    	  	    	creating the given target.
 *
 *	Targ_Silent	    	Return TRUE if we should be silent when
 *	    	  	    	creating the given target.
 *
 *	Targ_Precious	    	Return TRUE if the target is precious and
 *	    	  	    	should not be removed if we are interrupted.
 *
 *	Targ_Propagate		Propagate information between related
 *				nodes.	Should be called after the
 *				makefiles are parsed but before any
 *				action is taken.
 *
 * Debugging:
 *	Targ_PrintGraph	    	Print out the entire graphm all variables
 *	    	  	    	and statistics for the directory cache. Should
 *	    	  	    	print something for suffixes, too, but...
 */

#include	  <stdio.h>
#include	  <time.h>

#include	  "make.h"
#include	  "dir.h"

static Lst        allTargets;	/* the list of all targets found so far */
#ifdef CLEANUP
static Lst	  allGNs;	/* List of all the GNodes */
#endif
static Hash_Table targets;	/* a hash table of same */

static int TargPrintOnlySrc(void *, void *);
static int TargPrintName(void *, void *);
#ifdef CLEANUP
static void TargFreeGN(void *);
#endif

void
Targ_Init(void)
{
    allTargets = Lst_Init();
    Hash_InitTable(&targets, 191);
}

void
Targ_End(void)
{
    Targ_Stats();
#ifdef CLEANUP
    Lst_Free(allTargets);
    if (allGNs != NULL)
	Lst_Destroy(allGNs, TargFreeGN);
    Hash_DeleteTable(&targets);
#endif
}

void
Targ_Stats(void)
{
    Hash_DebugStats(&targets, "targets");
}

/* Return the list of all targets. */
Lst
Targ_List(void)
{
    return allTargets;
}

/* Create and initialize a new graph node. The gnode is added to the list of
 * all gnodes.
 *
 * Input:
 *	name		the name of the node, such as "clean", "src.c"
 */
GNode *
Targ_NewGN(const char *name)
{
    GNode *gn;

    gn = bmake_malloc(sizeof(GNode));
    gn->name = bmake_strdup(name);
    gn->uname = NULL;
    gn->path = NULL;
    gn->type = name[0] == '-' && name[1] == 'l' ? OP_LIB : 0;
    gn->unmade =    	0;
    gn->unmade_cohorts = 0;
    gn->cohort_num[0] = 0;
    gn->centurion =    	NULL;
    gn->made = 	    	UNMADE;
    gn->flags = 	0;
    gn->checked =	0;
    gn->mtime =		0;
    gn->cmgn =		NULL;
    gn->implicitParents = Lst_Init();
    gn->cohorts =   	Lst_Init();
    gn->parents =   	Lst_Init();
    gn->children =  	Lst_Init();
    gn->order_pred =  	Lst_Init();
    gn->order_succ =  	Lst_Init();
    Hash_InitTable(&gn->context, 0);
    gn->commands =  	Lst_Init();
    gn->suffix =	NULL;
    gn->fname = 	NULL;
    gn->lineno =	0;

#ifdef CLEANUP
    if (allGNs == NULL)
	allGNs = Lst_Init();
    Lst_Append(allGNs, gn);
#endif

    return gn;
}

#ifdef CLEANUP
static void
TargFreeGN(void *gnp)
{
    GNode *gn = (GNode *)gnp;

    free(gn->name);
    free(gn->uname);
    free(gn->path);

    Lst_Free(gn->implicitParents);
    Lst_Free(gn->cohorts);
    Lst_Free(gn->parents);
    Lst_Free(gn->children);
    Lst_Free(gn->order_succ);
    Lst_Free(gn->order_pred);
    Hash_DeleteTable(&gn->context);
    Lst_Free(gn->commands);

    /* XXX: does gn->suffix need to be freed? It is reference-counted. */
    /* gn->fname points to name allocated when file was opened, don't free */

    free(gn);
}
#endif


/* Find a node in the list using the given name for matching.
 * If the node is created, it is added to the .ALLTARGETS list.
 *
 * Input:
 *	name		the name to find
 *	flags		flags governing events when target not found
 *
 * Results:
 *	The node in the list if it was. If it wasn't, return NULL if
 *	flags was TARG_NOCREATE or the newly created and initialized node
 *	if it was TARG_CREATE
 */
GNode *
Targ_FindNode(const char *name, int flags)
{
    GNode         *gn;	      /* node in that element */
    Hash_Entry	  *he = NULL; /* New or used hash entry for node */
    Boolean	  isNew;      /* Set TRUE if Hash_CreateEntry had to create */
			      /* an entry for the node */

    if (!(flags & (TARG_CREATE | TARG_NOHASH))) {
	he = Hash_FindEntry(&targets, name);
	if (he == NULL)
	    return NULL;
	return (GNode *)Hash_GetValue(he);
    }

    if (!(flags & TARG_NOHASH)) {
	he = Hash_CreateEntry(&targets, name, &isNew);
	if (!isNew)
	    return (GNode *)Hash_GetValue(he);
    }

    gn = Targ_NewGN(name);
    if (!(flags & TARG_NOHASH))
	Hash_SetValue(he, gn);
    Var_Append(".ALLTARGETS", name, VAR_GLOBAL);
    Lst_Append(allTargets, gn);
    if (doing_depend)
	gn->flags |= FROM_DEPEND;
    return gn;
}

/* Make a complete list of GNodes from the given list of names.
 * If flags is TARG_CREATE, nodes will be created for all names in
 * names which do not yet have graph nodes. If flags is TARG_NOCREATE,
 * an error message will be printed for each name which can't be found.
 *
 * Input:
 *	name		list of names to find
 *	flags		flags used if no node is found for a given name
 *
 * Results:
 *	A complete list of graph nodes corresponding to all instances of all
 *	the names in names.
 */
Lst
Targ_FindList(Lst names, int flags)
{
    Lst            nodes;	/* result list */
    LstNode	   ln;		/* name list element */
    GNode	   *gn;		/* node in tLn */
    char    	   *name;

    nodes = Lst_Init();

    Lst_Open(names);
    while ((ln = Lst_Next(names)) != NULL) {
	name = LstNode_Datum(ln);
	gn = Targ_FindNode(name, flags);
	if (gn != NULL) {
	    /*
	     * Note: Lst_Append must come before the Lst_Concat so the nodes
	     * are added to the list in the order in which they were
	     * encountered in the makefile.
	     */
	    Lst_Append(nodes, gn);
	} else if (flags == TARG_NOCREATE) {
	    Error("\"%s\" -- target unknown.", name);
	}
    }
    Lst_Close(names);
    return nodes;
}

/* Return true if should ignore errors when creating gn. */
Boolean
Targ_Ignore(GNode *gn)
{
    return ignoreErrors || gn->type & OP_IGNORE;
}

/* Return true if be silent when creating gn. */
Boolean
Targ_Silent(GNode *gn)
{
    return beSilent || gn->type & OP_SILENT;
}

/* See if the given target is precious. */
Boolean
Targ_Precious(GNode *gn)
{
    return allPrecious || gn->type & (OP_PRECIOUS | OP_DOUBLEDEP);
}

/******************* DEBUG INFO PRINTING ****************/

static GNode	  *mainTarg;	/* the main target, as set by Targ_SetMain */

/* Set our idea of the main target we'll be creating. Used for debugging
 * output. */
void
Targ_SetMain(GNode *gn)
{
    mainTarg = gn;
}

static int
TargPrintName(void *gnp, void *pflags MAKE_ATTR_UNUSED)
{
    GNode *gn = (GNode *)gnp;

    fprintf(debug_file, "%s%s ", gn->name, gn->cohort_num);

    return 0;
}


int
Targ_PrintCmd(void *cmd, void *dummy MAKE_ATTR_UNUSED)
{
    fprintf(debug_file, "\t%s\n", (char *)cmd);
    return 0;
}

/* Format a modification time in some reasonable way and return it.
 * The time is placed in a static area, so it is overwritten with each call. */
char *
Targ_FmtTime(time_t tm)
{
    struct tm	  	*parts;
    static char	  	buf[128];

    parts = localtime(&tm);
    (void)strftime(buf, sizeof buf, "%k:%M:%S %b %d, %Y", parts);
    return buf;
}

/* Print out a type field giving only those attributes the user can set. */
void
Targ_PrintType(int type)
{
    int    tbit;

#define PRINTBIT(attr)	case CONCAT(OP_,attr): fprintf(debug_file, "." #attr " "); break
#define PRINTDBIT(attr) case CONCAT(OP_,attr): if (DEBUG(TARG))fprintf(debug_file, "." #attr " "); break

    type &= ~OP_OPMASK;

    while (type) {
	tbit = 1 << (ffs(type) - 1);
	type &= ~tbit;

	switch(tbit) {
	    PRINTBIT(OPTIONAL);
	    PRINTBIT(USE);
	    PRINTBIT(EXEC);
	    PRINTBIT(IGNORE);
	    PRINTBIT(PRECIOUS);
	    PRINTBIT(SILENT);
	    PRINTBIT(MAKE);
	    PRINTBIT(JOIN);
	    PRINTBIT(INVISIBLE);
	    PRINTBIT(NOTMAIN);
	    PRINTDBIT(LIB);
	    /*XXX: MEMBER is defined, so CONCAT(OP_,MEMBER) gives OP_"%" */
	    case OP_MEMBER: if (DEBUG(TARG))fprintf(debug_file, ".MEMBER "); break;
	    PRINTDBIT(ARCHV);
	    PRINTDBIT(MADE);
	    PRINTDBIT(PHONY);
	}
    }
}

static const char *
made_name(GNodeMade made)
{
    switch (made) {
    case UNMADE:     return "unmade";
    case DEFERRED:   return "deferred";
    case REQUESTED:  return "requested";
    case BEINGMADE:  return "being made";
    case MADE:       return "made";
    case UPTODATE:   return "up-to-date";
    case ERROR:      return "error when made";
    case ABORTED:    return "aborted";
    default:         return "unknown enum_made value";
    }
}

/* Print the contents of a node. */
int
Targ_PrintNode(void *gnp, void *passp)
{
    GNode         *gn = (GNode *)gnp;
    int	    	  pass = passp ? *(int *)passp : 0;

    fprintf(debug_file, "# %s%s", gn->name, gn->cohort_num);
    GNode_FprintDetails(debug_file, ", ", gn, "\n");
    if (gn->flags == 0)
	return 0;

    if (!OP_NOP(gn->type)) {
	fprintf(debug_file, "#\n");
	if (gn == mainTarg) {
	    fprintf(debug_file, "# *** MAIN TARGET ***\n");
	}
	if (pass >= 2) {
	    if (gn->unmade) {
		fprintf(debug_file, "# %d unmade children\n", gn->unmade);
	    } else {
		fprintf(debug_file, "# No unmade children\n");
	    }
	    if (! (gn->type & (OP_JOIN|OP_USE|OP_USEBEFORE|OP_EXEC))) {
		if (gn->mtime != 0) {
		    fprintf(debug_file, "# last modified %s: %s\n",
			      Targ_FmtTime(gn->mtime),
			      made_name(gn->made));
		} else if (gn->made != UNMADE) {
		    fprintf(debug_file, "# non-existent (maybe): %s\n",
			      made_name(gn->made));
		} else {
		    fprintf(debug_file, "# unmade\n");
		}
	    }
	    if (!Lst_IsEmpty(gn->implicitParents)) {
		fprintf(debug_file, "# implicit parents: ");
		Lst_ForEach(gn->implicitParents, TargPrintName, NULL);
		fprintf(debug_file, "\n");
	    }
	} else {
	    if (gn->unmade)
		fprintf(debug_file, "# %d unmade children\n", gn->unmade);
	}
	if (!Lst_IsEmpty(gn->parents)) {
	    fprintf(debug_file, "# parents: ");
	    Lst_ForEach(gn->parents, TargPrintName, NULL);
	    fprintf(debug_file, "\n");
	}
	if (!Lst_IsEmpty(gn->order_pred)) {
	    fprintf(debug_file, "# order_pred: ");
	    Lst_ForEach(gn->order_pred, TargPrintName, NULL);
	    fprintf(debug_file, "\n");
	}
	if (!Lst_IsEmpty(gn->order_succ)) {
	    fprintf(debug_file, "# order_succ: ");
	    Lst_ForEach(gn->order_succ, TargPrintName, NULL);
	    fprintf(debug_file, "\n");
	}

	fprintf(debug_file, "%-16s", gn->name);
	switch (gn->type & OP_OPMASK) {
	    case OP_DEPENDS:
		fprintf(debug_file, ": "); break;
	    case OP_FORCE:
		fprintf(debug_file, "! "); break;
	    case OP_DOUBLEDEP:
		fprintf(debug_file, ":: "); break;
	}
	Targ_PrintType(gn->type);
	Lst_ForEach(gn->children, TargPrintName, NULL);
	fprintf(debug_file, "\n");
	Lst_ForEach(gn->commands, Targ_PrintCmd, NULL);
	fprintf(debug_file, "\n\n");
	if (gn->type & OP_DOUBLEDEP) {
	    Lst_ForEach(gn->cohorts, Targ_PrintNode, &pass);
	}
    }
    return 0;
}

/* Print only those targets that are just a source.
 * The name of each file is printed, preceded by #\t. */
static int
TargPrintOnlySrc(void *gnp, void *dummy MAKE_ATTR_UNUSED)
{
    GNode   	  *gn = (GNode *)gnp;
    if (!OP_NOP(gn->type))
	return 0;

    fprintf(debug_file, "#\t%s [%s] ",
	    gn->name, gn->path ? gn->path : gn->name);
    Targ_PrintType(gn->type);
    fprintf(debug_file, "\n");

    return 0;
}

/* Input:
 *	pass		1 => before processing
 *			2 => after processing
 *			3 => after processing, an error occurred
 */
void
Targ_PrintGraph(int pass)
{
    fprintf(debug_file, "#*** Input graph:\n");
    Lst_ForEach(allTargets, Targ_PrintNode, &pass);
    fprintf(debug_file, "\n\n");
    fprintf(debug_file, "#\n#   Files that are only sources:\n");
    Lst_ForEach(allTargets, TargPrintOnlySrc, NULL);
    fprintf(debug_file, "#*** Global Variables:\n");
    Var_Dump(VAR_GLOBAL);
    fprintf(debug_file, "#*** Command-line Variables:\n");
    Var_Dump(VAR_CMD);
    fprintf(debug_file, "\n");
    Dir_PrintDirectories();
    fprintf(debug_file, "\n");
    Suff_PrintAll();
}

/* Propagate some type information to cohort nodes (those from the ::
 * dependency operator).
 *
 * Should be called after the makefiles are parsed but before any action is
 * taken. */
void
Targ_Propagate(void)
{
    LstNode pn, cn;

    for (pn = Lst_First(allTargets); pn != NULL; pn = LstNode_Next(pn)) {
	GNode *pgn = LstNode_Datum(pn);

	if (!(pgn->type & OP_DOUBLEDEP))
	    continue;

	for (cn = Lst_First(pgn->cohorts); cn != NULL; cn = LstNode_Next(cn)) {
	    GNode *cgn = LstNode_Datum(cn);

	    cgn->type |= pgn->type & ~OP_OPMASK;
	}
    }
}
