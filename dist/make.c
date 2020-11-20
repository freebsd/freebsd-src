/*	$NetBSD: make.c,v 1.209 2020/11/16 22:31:42 rillig Exp $	*/

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

/* Examination of targets and their suitability for creation.
 *
 * Interface:
 *	Make_Run	Initialize things for the module. Returns TRUE if
 *			work was (or would have been) done.
 *
 *	Make_Update	After a target is made, update all its parents.
 *			Perform various bookkeeping chores like the updating
 *			of the youngestChild field of the parent, filling
 *			of the IMPSRC context variable, etc. Place the parent
 *			on the toBeMade queue if it should be.
 *
 *	GNode_UpdateYoungestChild
 *			Update the node's youngestChild field based on the
 *			child's modification time.
 *
 *	Make_DoAllVar	Set up the various local variables for a
 *			target, including the .ALLSRC variable, making
 *			sure that any variable that needs to exist
 *			at the very least has the empty value.
 *
 *	GNode_IsOODate	Determine if a target is out-of-date.
 *
 *	Make_HandleUse	See if a child is a .USE node for a parent
 *			and perform the .USE actions if so.
 *
 *	Make_ExpandUse	Expand .USE nodes
 */

#include "make.h"
#include "dir.h"
#include "job.h"

/*	"@(#)make.c	8.1 (Berkeley) 6/6/93"	*/
MAKE_RCSID("$NetBSD: make.c,v 1.209 2020/11/16 22:31:42 rillig Exp $");

/* Sequence # to detect recursion. */
static unsigned int checked_seqno = 1;

/* The current fringe of the graph.
 * These are nodes which await examination by MakeOODate.
 * It is added to by Make_Update and subtracted from by MakeStartJobs */
static GNodeList *toBeMade;

static int MakeBuildParent(void *, void *);

void
debug_printf(const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    vfprintf(opts.debug_file, fmt, args);
    va_end(args);
}

MAKE_ATTR_DEAD static void
make_abort(GNode *gn, int line)
{
    debug_printf("make_abort from line %d\n", line);
    Targ_PrintNode(gn, 2);
    Targ_PrintNodes(toBeMade, 2);
    Targ_PrintGraph(3);
    abort();
}

ENUM_VALUE_RTTI_8(GNodeMade,
		  UNMADE, DEFERRED, REQUESTED, BEINGMADE,
		  MADE, UPTODATE, ERROR, ABORTED);

ENUM_FLAGS_RTTI_31(GNodeType,
		   OP_DEPENDS, OP_FORCE, OP_DOUBLEDEP,
		   /* OP_OPMASK is omitted since it combines other flags */
		   OP_OPTIONAL, OP_USE, OP_EXEC, OP_IGNORE,
		   OP_PRECIOUS, OP_SILENT, OP_MAKE, OP_JOIN,
		   OP_MADE, OP_SPECIAL, OP_USEBEFORE, OP_INVISIBLE,
		   OP_NOTMAIN, OP_PHONY, OP_NOPATH, OP_WAIT,
		   OP_NOMETA, OP_META, OP_NOMETA_CMP, OP_SUBMAKE,
		   OP_TRANSFORM, OP_MEMBER, OP_LIB, OP_ARCHV,
		   OP_HAS_COMMANDS, OP_SAVE_CMDS, OP_DEPS_FOUND, OP_MARK);

ENUM_FLAGS_RTTI_10(GNodeFlags,
		   REMAKE, CHILDMADE, FORCE, DONE_WAIT,
		   DONE_ORDER, FROM_DEPEND, DONE_ALLSRC, CYCLE,
		   DONECYCLE, INTERNAL);

void
GNode_FprintDetails(FILE *f, const char *prefix, const GNode *gn,
		    const char *suffix)
{
    char type_buf[GNodeType_ToStringSize];
    char flags_buf[GNodeFlags_ToStringSize];

    fprintf(f, "%smade %s, type %s, flags %s%s",
	    prefix,
	    Enum_ValueToString(gn->made, GNodeMade_ToStringSpecs),
	    Enum_FlagsToString(type_buf, sizeof type_buf,
			       gn->type, GNodeType_ToStringSpecs),
	    Enum_FlagsToString(flags_buf, sizeof flags_buf,
			       gn->flags, GNodeFlags_ToStringSpecs),
	    suffix);
}

Boolean
GNode_ShouldExecute(GNode *gn)
{
    return !((gn->type & OP_MAKE) ? opts.noRecursiveExecute : opts.noExecute);
}

/* Update the youngest child of the node, according to the given child. */
void
GNode_UpdateYoungestChild(GNode *gn, GNode *cgn)
{
    if (gn->youngestChild == NULL || cgn->mtime > gn->youngestChild->mtime)
	gn->youngestChild = cgn;
}

static Boolean
IsOODateRegular(GNode *gn)
{
    /* These rules are inherited from the original Make. */

    if (gn->youngestChild != NULL) {
	if (gn->mtime < gn->youngestChild->mtime) {
	    DEBUG1(MAKE, "modified before source \"%s\"...",
		   GNode_Path(gn->youngestChild));
	    return TRUE;
	}
	return FALSE;
    }

    if (gn->mtime == 0 && !(gn->type & OP_OPTIONAL)) {
	DEBUG0(MAKE, "non-existent and no sources...");
	return TRUE;
    }

    if (gn->type & OP_DOUBLEDEP) {
	DEBUG0(MAKE, ":: operator and no sources...");
	return TRUE;
    }

    return FALSE;
}

/* See if the node is out of date with respect to its sources.
 *
 * Used by Make_Run when deciding which nodes to place on the
 * toBeMade queue initially and by Make_Update to screen out .USE and
 * .EXEC nodes. In the latter case, however, any other sort of node
 * must be considered out-of-date since at least one of its children
 * will have been recreated.
 *
 * The mtime field of the node and the youngestChild field of its parents
 * may be changed.
 */
Boolean
GNode_IsOODate(GNode *gn)
{
    Boolean         oodate;

    /*
     * Certain types of targets needn't even be sought as their datedness
     * doesn't depend on their modification time...
     */
    if (!(gn->type & (OP_JOIN|OP_USE|OP_USEBEFORE|OP_EXEC))) {
	Dir_UpdateMTime(gn, TRUE);
	if (DEBUG(MAKE)) {
	    if (gn->mtime != 0)
		debug_printf("modified %s...", Targ_FmtTime(gn->mtime));
	    else
		debug_printf("non-existent...");
	}
    }

    /*
     * A target is remade in one of the following circumstances:
     *	its modification time is smaller than that of its youngest child and
     *	    it would actually be run (has commands or is not GNode_IsTarget)
     *	it's the object of a force operator
     *	it has no children, was on the lhs of an operator and doesn't exist
     *	    already.
     *
     * Libraries are only considered out-of-date if the archive module says
     * they are.
     *
     * These weird rules are brought to you by Backward-Compatibility and
     * the strange people who wrote 'Make'.
     */
    if (gn->type & (OP_USE|OP_USEBEFORE)) {
	/*
	 * If the node is a USE node it is *never* out of date
	 * no matter *what*.
	 */
	DEBUG0(MAKE, ".USE node...");
	oodate = FALSE;
    } else if ((gn->type & OP_LIB) && (gn->mtime == 0 || Arch_IsLib(gn))) {
	DEBUG0(MAKE, "library...");

	/*
	 * always out of date if no children and :: target
	 * or non-existent.
	 */
	oodate = (gn->mtime == 0 || Arch_LibOODate(gn) ||
		  (gn->youngestChild == NULL && (gn->type & OP_DOUBLEDEP)));
    } else if (gn->type & OP_JOIN) {
	/*
	 * A target with the .JOIN attribute is only considered
	 * out-of-date if any of its children was out-of-date.
	 */
	DEBUG0(MAKE, ".JOIN node...");
	DEBUG1(MAKE, "source %smade...", gn->flags & CHILDMADE ? "" : "not ");
	oodate = (gn->flags & CHILDMADE) != 0;
    } else if (gn->type & (OP_FORCE|OP_EXEC|OP_PHONY)) {
	/*
	 * A node which is the object of the force (!) operator or which has
	 * the .EXEC attribute is always considered out-of-date.
	 */
	if (DEBUG(MAKE)) {
	    if (gn->type & OP_FORCE) {
		debug_printf("! operator...");
	    } else if (gn->type & OP_PHONY) {
		debug_printf(".PHONY node...");
	    } else {
		debug_printf(".EXEC node...");
	    }
	}
	oodate = TRUE;
    } else if (IsOODateRegular(gn)) {
	oodate = TRUE;
    } else {
	/*
	 * When a non-existing child with no sources
	 * (such as a typically used FORCE source) has been made and
	 * the target of the child (usually a directory) has the same
	 * timestamp as the timestamp just given to the non-existing child
	 * after it was considered made.
	 */
	if (DEBUG(MAKE)) {
	    if (gn->flags & FORCE)
		debug_printf("non existing child...");
	}
	oodate = (gn->flags & FORCE) != 0;
    }

#ifdef USE_META
    if (useMeta) {
	oodate = meta_oodate(gn, oodate);
    }
#endif

    /*
     * If the target isn't out-of-date, the parents need to know its
     * modification time. Note that targets that appear to be out-of-date
     * but aren't, because they have no commands and are GNode_IsTarget,
     * have their mtime stay below their children's mtime to keep parents from
     * thinking they're out-of-date.
     */
    if (!oodate) {
	GNodeListNode *ln;
	for (ln = gn->parents->first; ln != NULL; ln = ln->next)
	    GNode_UpdateYoungestChild(ln->datum, gn);
    }

    return oodate;
}

static void
PretendAllChildrenAreMade(GNode *pgn)
{
    GNodeListNode *ln;

    for (ln = pgn->children->first; ln != NULL; ln = ln->next) {
	GNode *cgn = ln->datum;

	Dir_UpdateMTime(cgn, FALSE);	/* cgn->path may get updated as well */
	GNode_UpdateYoungestChild(pgn, cgn);
	pgn->unmade--;
    }
}

/* Called by Make_Run and SuffApplyTransform on the downward pass to handle
 * .USE and transformation nodes, by copying the child node's commands, type
 * flags and children to the parent node.
 *
 * A .USE node is much like an explicit transformation rule, except its
 * commands are always added to the target node, even if the target already
 * has commands.
 *
 * Input:
 *	cgn		The source node, which is either a .USE/.USEBEFORE
 *			node or a transformation node (OP_TRANSFORM).
 *	pgn		The target node
 */
void
Make_HandleUse(GNode *cgn, GNode *pgn)
{
    GNodeListNode *ln;	/* An element in the children list */

#ifdef DEBUG_SRC
    if (!(cgn->type & (OP_USE|OP_USEBEFORE|OP_TRANSFORM))) {
	debug_printf("Make_HandleUse: called for plain node %s\n", cgn->name);
	return;		/* XXX: debug mode should not affect control flow */
    }
#endif

    if ((cgn->type & (OP_USE|OP_USEBEFORE)) || Lst_IsEmpty(pgn->commands)) {
	    if (cgn->type & OP_USEBEFORE) {
		/* .USEBEFORE */
		Lst_PrependAll(pgn->commands, cgn->commands);
	    } else {
		/* .USE, or target has no commands */
		Lst_AppendAll(pgn->commands, cgn->commands);
	    }
    }

    for (ln = cgn->children->first; ln != NULL; ln = ln->next) {
	GNode *gn = ln->datum;

	/*
	 * Expand variables in the .USE node's name
	 * and save the unexpanded form.
	 * We don't need to do this for commands.
	 * They get expanded properly when we execute.
	 */
	if (gn->uname == NULL) {
	    gn->uname = gn->name;
	} else {
	    free(gn->name);
	}
	(void)Var_Subst(gn->uname, pgn, VARE_WANTRES, &gn->name);
	/* TODO: handle errors */
	if (gn->uname && strcmp(gn->name, gn->uname) != 0) {
	    /* See if we have a target for this node. */
	    GNode *tgn = Targ_FindNode(gn->name);
	    if (tgn != NULL)
		gn = tgn;
	}

	Lst_Append(pgn->children, gn);
	Lst_Append(gn->parents, pgn);
	pgn->unmade++;
    }

    pgn->type |= cgn->type & ~(OP_OPMASK|OP_USE|OP_USEBEFORE|OP_TRANSFORM);
}

/* Used by Make_Run on the downward pass to handle .USE nodes. Should be
 * called before the children are enqueued to be looked at by MakeAddChild.
 *
 * For a .USE child, the commands, type flags and children are copied to the
 * parent node, and since the relation to the .USE node is then no longer
 * needed, that relation is removed.
 *
 * Input:
 *	cgn		the child, which may be a .USE node
 *	pgn		the current parent
 */
static void
MakeHandleUse(GNode *cgn, GNode *pgn, GNodeListNode *ln)
{
    Boolean unmarked;

    unmarked = !(cgn->type & OP_MARK);
    cgn->type |= OP_MARK;

    if (!(cgn->type & (OP_USE|OP_USEBEFORE)))
	return;

    if (unmarked)
	Make_HandleUse(cgn, pgn);

    /*
     * This child node is now "made", so we decrement the count of
     * unmade children in the parent... We also remove the child
     * from the parent's list to accurately reflect the number of decent
     * children the parent has. This is used by Make_Run to decide
     * whether to queue the parent or examine its children...
     */
    Lst_Remove(pgn->children, ln);
    pgn->unmade--;
}

static void
HandleUseNodes(GNode *gn)
{
    GNodeListNode *ln, *nln;
    for (ln = gn->children->first; ln != NULL; ln = nln) {
	nln = ln->next;
	MakeHandleUse(ln->datum, gn, ln);
    }
}


/* Check the modification time of a gnode, and update it if necessary.
 * Return 0 if the gnode does not exist, or its filesystem time if it does. */
time_t
Make_Recheck(GNode *gn)
{
    time_t mtime;

    Dir_UpdateMTime(gn, TRUE);
    mtime = gn->mtime;

#ifndef RECHECK
    /*
     * We can't re-stat the thing, but we can at least take care of rules
     * where a target depends on a source that actually creates the
     * target, but only if it has changed, e.g.
     *
     * parse.h : parse.o
     *
     * parse.o : parse.y
     *		yacc -d parse.y
     *		cc -c y.tab.c
     *		mv y.tab.o parse.o
     *		cmp -s y.tab.h parse.h || mv y.tab.h parse.h
     *
     * In this case, if the definitions produced by yacc haven't changed
     * from before, parse.h won't have been updated and gn->mtime will
     * reflect the current modification time for parse.h. This is
     * something of a kludge, I admit, but it's a useful one.
     *
     * XXX: People like to use a rule like "FRC:" to force things that
     * depend on FRC to be made, so we have to check for gn->children
     * being empty as well.
     */
    if (!Lst_IsEmpty(gn->commands) || Lst_IsEmpty(gn->children)) {
	gn->mtime = now;
    }
#else
    /*
     * This is what Make does and it's actually a good thing, as it
     * allows rules like
     *
     *	cmp -s y.tab.h parse.h || cp y.tab.h parse.h
     *
     * to function as intended. Unfortunately, thanks to the stateless
     * nature of NFS (by which I mean the loose coupling of two clients
     * using the same file from a common server), there are times
     * when the modification time of a file created on a remote
     * machine will not be modified before the local stat() implied by
     * the Dir_UpdateMTime occurs, thus leading us to believe that the file
     * is unchanged, wreaking havoc with files that depend on this one.
     *
     * I have decided it is better to make too much than to make too
     * little, so this stuff is commented out unless you're sure it's ok.
     * -- ardeb 1/12/88
     */
    /*
     * Christos, 4/9/92: If we are saving commands, pretend that
     * the target is made now. Otherwise archives with '...' rules
     * don't work!
     */
    if (!GNode_ShouldExecute(gn) || (gn->type & OP_SAVE_CMDS) ||
	    (mtime == 0 && !(gn->type & OP_WAIT))) {
	DEBUG2(MAKE, " recheck(%s): update time from %s to now\n",
	       gn->name, Targ_FmtTime(gn->mtime));
	gn->mtime = now;
    } else {
	DEBUG2(MAKE, " recheck(%s): current update time: %s\n",
	       gn->name, Targ_FmtTime(gn->mtime));
    }
#endif

    /* XXX: The returned mtime may differ from gn->mtime.
     * Intentionally? */
    return mtime;
}

/*
 * Set the .PREFIX and .IMPSRC variables for all the implied parents
 * of this node.
 */
static void
UpdateImplicitParentsVars(GNode *cgn, const char *cname)
{
    GNodeListNode *ln;
    const char *cpref = GNode_VarPrefix(cgn);

    for (ln = cgn->implicitParents->first; ln != NULL; ln = ln->next) {
	GNode *pgn = ln->datum;
	if (pgn->flags & REMAKE) {
	    Var_Set(IMPSRC, cname, pgn);
	    if (cpref != NULL)
		Var_Set(PREFIX, cpref, pgn);
	}
    }
}

/* See if a .ORDER rule stops us from building this node. */
static Boolean
IsWaitingForOrder(GNode *gn)
{
    GNodeListNode *ln;

    for (ln = gn->order_pred->first; ln != NULL; ln = ln->next) {
	GNode *ogn = ln->datum;

	if (ogn->made >= MADE || !(ogn->flags & REMAKE))
	    continue;

	DEBUG2(MAKE, "IsWaitingForOrder: Waiting for .ORDER node \"%s%s\"\n",
	       ogn->name, ogn->cohort_num);
	return TRUE;
    }
    return FALSE;
}

/* Perform update on the parents of a node. Used by JobFinish once
 * a node has been dealt with and by MakeStartJobs if it finds an
 * up-to-date node.
 *
 * The unmade field of pgn is decremented and pgn may be placed on
 * the toBeMade queue if this field becomes 0.
 *
 * If the child was made, the parent's flag CHILDMADE field will be
 * set true.
 *
 * If the child is not up-to-date and still does not exist,
 * set the FORCE flag on the parents.
 *
 * If the child wasn't made, the youngestChild field of the parent will be
 * altered if the child's mtime is big enough.
 *
 * Finally, if the child is the implied source for the parent, the
 * parent's IMPSRC variable is set appropriately.
 */
void
Make_Update(GNode *cgn)
{
    const char *cname;		/* the child's name */
    time_t	mtime = -1;
    GNodeList *parents;
    GNodeListNode *ln;
    GNode	*centurion;

    /* It is save to re-examine any nodes again */
    checked_seqno++;

    cname = GNode_VarTarget(cgn);

    DEBUG2(MAKE, "Make_Update: %s%s\n", cgn->name, cgn->cohort_num);

    /*
     * If the child was actually made, see what its modification time is
     * now -- some rules won't actually update the file. If the file still
     * doesn't exist, make its mtime now.
     */
    if (cgn->made != UPTODATE) {
	mtime = Make_Recheck(cgn);
    }

    /*
     * If this is a `::' node, we must consult its first instance
     * which is where all parents are linked.
     */
    if ((centurion = cgn->centurion) != NULL) {
	if (!Lst_IsEmpty(cgn->parents))
		Punt("%s%s: cohort has parents", cgn->name, cgn->cohort_num);
	centurion->unmade_cohorts--;
	if (centurion->unmade_cohorts < 0)
	    Error("Graph cycles through centurion %s", centurion->name);
    } else {
	centurion = cgn;
    }
    parents = centurion->parents;

    /* If this was a .ORDER node, schedule the RHS */
    Lst_ForEachUntil(centurion->order_succ, MakeBuildParent, toBeMade->first);

    /* Now mark all the parents as having one less unmade child */
    for (ln = parents->first; ln != NULL; ln = ln->next) {
	GNode *pgn = ln->datum;

	if (DEBUG(MAKE)) {
	    debug_printf("inspect parent %s%s: ", pgn->name, pgn->cohort_num);
	    GNode_FprintDetails(opts.debug_file, "", pgn, "");
	    debug_printf(", unmade %d ", pgn->unmade - 1);
	}

	if (!(pgn->flags & REMAKE)) {
	    /* This parent isn't needed */
	    DEBUG0(MAKE, "- not needed\n");
	    continue;
	}
	if (mtime == 0 && !(cgn->type & OP_WAIT))
	    pgn->flags |= FORCE;

	/*
	 * If the parent has the .MADE attribute, its timestamp got
	 * updated to that of its newest child, and its unmade
	 * child count got set to zero in Make_ExpandUse().
	 * However other things might cause us to build one of its
	 * children - and so we mustn't do any processing here when
	 * the child build finishes.
	 */
	if (pgn->type & OP_MADE) {
	    DEBUG0(MAKE, "- .MADE\n");
	    continue;
	}

	if (!(cgn->type & (OP_EXEC | OP_USE | OP_USEBEFORE))) {
	    if (cgn->made == MADE)
		pgn->flags |= CHILDMADE;
	    GNode_UpdateYoungestChild(pgn, cgn);
	}

	/*
	 * A parent must wait for the completion of all instances
	 * of a `::' dependency.
	 */
	if (centurion->unmade_cohorts != 0 || centurion->made < MADE) {
	    DEBUG2(MAKE, "- centurion made %d, %d unmade cohorts\n",
		   centurion->made, centurion->unmade_cohorts);
	    continue;
	}

	/* One more child of this parent is now made */
	pgn->unmade--;
	if (pgn->unmade < 0) {
	    if (DEBUG(MAKE)) {
		debug_printf("Graph cycles through %s%s\n",
			     pgn->name, pgn->cohort_num);
		Targ_PrintGraph(2);
	    }
	    Error("Graph cycles through %s%s", pgn->name, pgn->cohort_num);
	}

	/* We must always rescan the parents of .WAIT and .ORDER nodes. */
	if (pgn->unmade != 0 && !(centurion->type & OP_WAIT)
		&& !(centurion->flags & DONE_ORDER)) {
	    DEBUG0(MAKE, "- unmade children\n");
	    continue;
	}
	if (pgn->made != DEFERRED) {
	    /*
	     * Either this parent is on a different branch of the tree,
	     * or it on the RHS of a .WAIT directive
	     * or it is already on the toBeMade list.
	     */
	    DEBUG0(MAKE, "- not deferred\n");
	    continue;
	}

	if (IsWaitingForOrder(pgn))
	    continue;

	if (DEBUG(MAKE)) {
	    debug_printf("- %s%s made, schedule %s%s (made %d)\n",
			 cgn->name, cgn->cohort_num,
			 pgn->name, pgn->cohort_num, pgn->made);
	    Targ_PrintNode(pgn, 2);
	}
	/* Ok, we can schedule the parent again */
	pgn->made = REQUESTED;
	Lst_Enqueue(toBeMade, pgn);
    }

    UpdateImplicitParentsVars(cgn, cname);
}

static void
UnmarkChildren(GNode *gn)
{
    GNodeListNode *ln;

    for (ln = gn->children->first; ln != NULL; ln = ln->next) {
	GNode *child = ln->datum;
	child->type &= ~OP_MARK;
    }
}

/* Add a child's name to the ALLSRC and OODATE variables of the given
 * node, but only if it has not been given the .EXEC, .USE or .INVISIBLE
 * attributes. .EXEC and .USE children are very rarely going to be files,
 * so...
 *
 * If the child is a .JOIN node, its ALLSRC is propagated to the parent.
 *
 * A child is added to the OODATE variable if its modification time is
 * later than that of its parent, as defined by Make, except if the
 * parent is a .JOIN node. In that case, it is only added to the OODATE
 * variable if it was actually made (since .JOIN nodes don't have
 * modification times, the comparison is rather unfair...)..
 *
 * Input:
 *	cgn		The child to add
 *	pgn		The parent to whose ALLSRC variable it should
 *			be added
 */
static void
MakeAddAllSrc(GNode *cgn, GNode *pgn)
{
    if (cgn->type & OP_MARK)
	return;
    cgn->type |= OP_MARK;

    if (!(cgn->type & (OP_EXEC|OP_USE|OP_USEBEFORE|OP_INVISIBLE))) {
	const char *child, *allsrc;

	if (cgn->type & OP_ARCHV)
	    child = GNode_VarMember(cgn);
	else
	    child = GNode_Path(cgn);
	if (cgn->type & OP_JOIN) {
	    allsrc = GNode_VarAllsrc(cgn);
	} else {
	    allsrc = child;
	}
	if (allsrc != NULL)
		Var_Append(ALLSRC, allsrc, pgn);
	if (pgn->type & OP_JOIN) {
	    if (cgn->made == MADE) {
		Var_Append(OODATE, child, pgn);
	    }
	} else if ((pgn->mtime < cgn->mtime) ||
		   (cgn->mtime >= now && cgn->made == MADE))
	{
	    /*
	     * It goes in the OODATE variable if the parent is younger than the
	     * child or if the child has been modified more recently than
	     * the start of the make. This is to keep pmake from getting
	     * confused if something else updates the parent after the
	     * make starts (shouldn't happen, I know, but sometimes it
	     * does). In such a case, if we've updated the child, the parent
	     * is likely to have a modification time later than that of
	     * the child and anything that relies on the OODATE variable will
	     * be hosed.
	     *
	     * XXX: This will cause all made children to go in the OODATE
	     * variable, even if they're not touched, if RECHECK isn't defined,
	     * since cgn->mtime is set to now in Make_Update. According to
	     * some people, this is good...
	     */
	    Var_Append(OODATE, child, pgn);
	}
    }
}

/* Set up the ALLSRC and OODATE variables. Sad to say, it must be
 * done separately, rather than while traversing the graph. This is
 * because Make defined OODATE to contain all sources whose modification
 * times were later than that of the target, *not* those sources that
 * were out-of-date. Since in both compatibility and native modes,
 * the modification time of the parent isn't found until the child
 * has been dealt with, we have to wait until now to fill in the
 * variable. As for ALLSRC, the ordering is important and not
 * guaranteed when in native mode, so it must be set here, too.
 *
 * If the node is a .JOIN node, its TARGET variable will be set to
 * match its ALLSRC variable.
 */
void
Make_DoAllVar(GNode *gn)
{
    GNodeListNode *ln;

    if (gn->flags & DONE_ALLSRC)
	return;

    UnmarkChildren(gn);
    for (ln = gn->children->first; ln != NULL; ln = ln->next)
	MakeAddAllSrc(ln->datum, gn);

    if (!Var_Exists(OODATE, gn))
	Var_Set(OODATE, "", gn);
    if (!Var_Exists(ALLSRC, gn))
	Var_Set(ALLSRC, "", gn);

    if (gn->type & OP_JOIN)
	Var_Set(TARGET, GNode_VarAllsrc(gn), gn);
    gn->flags |= DONE_ALLSRC;
}

static int
MakeBuildChild(void *v_cn, void *toBeMade_next)
{
    GNode *cn = v_cn;

    if (DEBUG(MAKE)) {
	debug_printf("MakeBuildChild: inspect %s%s, ",
	       cn->name, cn->cohort_num);
	GNode_FprintDetails(opts.debug_file, "", cn, "\n");
    }
    if (cn->made > DEFERRED)
	return 0;

    /* If this node is on the RHS of a .ORDER, check LHSs. */
    if (IsWaitingForOrder(cn)) {
	/* Can't build this (or anything else in this child list) yet */
	cn->made = DEFERRED;
	return 0;			/* but keep looking */
    }

    DEBUG2(MAKE, "MakeBuildChild: schedule %s%s\n", cn->name, cn->cohort_num);

    cn->made = REQUESTED;
    if (toBeMade_next == NULL)
	Lst_Append(toBeMade, cn);
    else
	Lst_InsertBefore(toBeMade, toBeMade_next, cn);

    if (cn->unmade_cohorts != 0)
	Lst_ForEachUntil(cn->cohorts, MakeBuildChild, toBeMade_next);

    /*
     * If this node is a .WAIT node with unmade children
     * then don't add the next sibling.
     */
    return cn->type & OP_WAIT && cn->unmade > 0;
}

/* When a .ORDER LHS node completes, we do this on each RHS. */
static int
MakeBuildParent(void *v_pn, void *toBeMade_next)
{
    GNode *pn = v_pn;

    if (pn->made != DEFERRED)
	return 0;

    if (MakeBuildChild(pn, toBeMade_next) == 0) {
	/* Mark so that when this node is built we reschedule its parents */
	pn->flags |= DONE_ORDER;
    }

    return 0;
}

/* Start as many jobs as possible, taking them from the toBeMade queue.
 *
 * If the -q option was given, no job will be started,
 * but as soon as an out-of-date target is found, this function
 * returns TRUE. In all other cases, this function returns FALSE.
 */
static Boolean
MakeStartJobs(void)
{
    GNode *gn;
    Boolean have_token = FALSE;

    while (!Lst_IsEmpty(toBeMade)) {
	/* Get token now to avoid cycling job-list when we only have 1 token */
	if (!have_token && !Job_TokenWithdraw())
	    break;
	have_token = TRUE;

	gn = Lst_Dequeue(toBeMade);
	DEBUG2(MAKE, "Examining %s%s...\n", gn->name, gn->cohort_num);

	if (gn->made != REQUESTED) {
	    DEBUG1(MAKE, "state %d\n", gn->made);

	    make_abort(gn, __LINE__);
	}

	if (gn->checked_seqno == checked_seqno) {
	    /* We've already looked at this node since a job finished... */
	    DEBUG2(MAKE, "already checked %s%s\n", gn->name, gn->cohort_num);
	    gn->made = DEFERRED;
	    continue;
	}
	gn->checked_seqno = checked_seqno;

	if (gn->unmade != 0) {
	    /*
	     * We can't build this yet, add all unmade children to toBeMade,
	     * just before the current first element.
	     */
	    gn->made = DEFERRED;
	    Lst_ForEachUntil(gn->children, MakeBuildChild, toBeMade->first);
	    /* and drop this node on the floor */
	    DEBUG2(MAKE, "dropped %s%s\n", gn->name, gn->cohort_num);
	    continue;
	}

	gn->made = BEINGMADE;
	if (GNode_IsOODate(gn)) {
	    DEBUG0(MAKE, "out-of-date\n");
	    if (opts.queryFlag)
		return TRUE;
	    Make_DoAllVar(gn);
	    Job_Make(gn);
	    have_token = FALSE;
	} else {
	    DEBUG0(MAKE, "up-to-date\n");
	    gn->made = UPTODATE;
	    if (gn->type & OP_JOIN) {
		/*
		 * Even for an up-to-date .JOIN node, we need it to have its
		 * context variables so references to it get the correct
		 * value for .TARGET when building up the context variables
		 * of its parent(s)...
		 */
		Make_DoAllVar(gn);
	    }
	    Make_Update(gn);
	}
    }

    if (have_token)
	Job_TokenReturn();

    return FALSE;
}

/* Print the status of a .ORDER node. */
static void
MakePrintStatusOrderNode(GNode *ogn, GNode *gn)
{
    if (!(ogn->flags & REMAKE) || ogn->made > REQUESTED)
	/* not waiting for this one */
	return;

    printf("    `%s%s' has .ORDER dependency against %s%s ",
	   gn->name, gn->cohort_num, ogn->name, ogn->cohort_num);
    GNode_FprintDetails(stdout, "(", ogn, ")\n");

    if (DEBUG(MAKE) && opts.debug_file != stdout) {
	debug_printf("    `%s%s' has .ORDER dependency against %s%s ",
		     gn->name, gn->cohort_num, ogn->name, ogn->cohort_num);
	GNode_FprintDetails(opts.debug_file, "(", ogn, ")\n");
    }
}

static void
MakePrintStatusOrder(GNode *gn)
{
    GNodeListNode *ln;
    for (ln = gn->order_pred->first; ln != NULL; ln = ln->next)
	MakePrintStatusOrderNode(ln->datum, gn);
}

static void MakePrintStatusList(GNodeList *, int *);

/* Print the status of a top-level node, viz. it being up-to-date already
 * or not created due to an error in a lower level.
 * Callback function for Make_Run via Lst_ForEachUntil.
 */
static Boolean
MakePrintStatus(GNode *gn, int *errors)
{
    if (gn->flags & DONECYCLE)
	/* We've completely processed this node before, don't do it again. */
	return FALSE;

    if (gn->unmade == 0) {
	gn->flags |= DONECYCLE;
	switch (gn->made) {
	case UPTODATE:
	    printf("`%s%s' is up to date.\n", gn->name, gn->cohort_num);
	    break;
	case MADE:
	    break;
	case UNMADE:
	case DEFERRED:
	case REQUESTED:
	case BEINGMADE:
	    (*errors)++;
	    printf("`%s%s' was not built", gn->name, gn->cohort_num);
	    GNode_FprintDetails(stdout, " (", gn, ")!\n");
	    if (DEBUG(MAKE) && opts.debug_file != stdout) {
		debug_printf("`%s%s' was not built", gn->name, gn->cohort_num);
		GNode_FprintDetails(opts.debug_file, " (", gn, ")!\n");
	    }
	    /* Most likely problem is actually caused by .ORDER */
	    MakePrintStatusOrder(gn);
	    break;
	default:
	    /* Errors - already counted */
	    printf("`%s%s' not remade because of errors.\n",
		    gn->name, gn->cohort_num);
	    if (DEBUG(MAKE) && opts.debug_file != stdout)
		debug_printf("`%s%s' not remade because of errors.\n",
			     gn->name, gn->cohort_num);
	    break;
	}
	return FALSE;
    }

    DEBUG3(MAKE, "MakePrintStatus: %s%s has %d unmade children\n",
	   gn->name, gn->cohort_num, gn->unmade);
    /*
     * If printing cycles and came to one that has unmade children,
     * print out the cycle by recursing on its children.
     */
    if (!(gn->flags & CYCLE)) {
	/* First time we've seen this node, check all children */
	gn->flags |= CYCLE;
	MakePrintStatusList(gn->children, errors);
	/* Mark that this node needn't be processed again */
	gn->flags |= DONECYCLE;
	return FALSE;
    }

    /* Only output the error once per node */
    gn->flags |= DONECYCLE;
    Error("Graph cycles through `%s%s'", gn->name, gn->cohort_num);
    if ((*errors)++ > 100)
	/* Abandon the whole error report */
	return TRUE;

    /* Reporting for our children will give the rest of the loop */
    MakePrintStatusList(gn->children, errors);
    return FALSE;
}

static void
MakePrintStatusList(GNodeList *gnodes, int *errors)
{
    GNodeListNode *ln;
    for (ln = gnodes->first; ln != NULL; ln = ln->next)
	if (MakePrintStatus(ln->datum, errors))
	    break;
}

static void
ExamineLater(GNodeList *examine, GNodeList *toBeExamined)
{
    ListNode *ln;

    for (ln = toBeExamined->first; ln != NULL; ln = ln->next) {
	GNode *gn = ln->datum;

	if (gn->flags & REMAKE)
	    continue;
	if (gn->type & (OP_USE | OP_USEBEFORE))
	    continue;

	DEBUG2(MAKE, "ExamineLater: need to examine \"%s%s\"\n",
	       gn->name, gn->cohort_num);
	Lst_Enqueue(examine, gn);
    }
}

/* Expand .USE nodes and create a new targets list.
 *
 * Input:
 *	targs		the initial list of targets
 */
void
Make_ExpandUse(GNodeList *targs)
{
    GNodeList *examine = Lst_New();	/* Queue of targets to examine */
    Lst_AppendAll(examine, targs);

    /*
     * Make an initial downward pass over the graph, marking nodes to be made
     * as we go down. We call Suff_FindDeps to find where a node is and
     * to get some children for it if it has none and also has no commands.
     * If the node is a leaf, we stick it on the toBeMade queue to
     * be looked at in a minute, otherwise we add its children to our queue
     * and go on about our business.
     */
    while (!Lst_IsEmpty(examine)) {
	GNode *gn = Lst_Dequeue(examine);

	if (gn->flags & REMAKE)
	    /* We've looked at this one already */
	    continue;
	gn->flags |= REMAKE;
	DEBUG2(MAKE, "Make_ExpandUse: examine %s%s\n",
	       gn->name, gn->cohort_num);

	if (gn->type & OP_DOUBLEDEP)
	    Lst_PrependAll(examine, gn->cohorts);

	/*
	 * Apply any .USE rules before looking for implicit dependencies
	 * to make sure everything has commands that should...
	 * Make sure that the TARGET is set, so that we can make
	 * expansions.
	 */
	if (gn->type & OP_ARCHV) {
	    char *eoa = strchr(gn->name, '(');
	    char *eon = strchr(gn->name, ')');
	    if (eoa == NULL || eon == NULL)
		continue;
	    *eoa = '\0';
	    *eon = '\0';
	    Var_Set(MEMBER, eoa + 1, gn);
	    Var_Set(ARCHIVE, gn->name, gn);
	    *eoa = '(';
	    *eon = ')';
	}

	Dir_UpdateMTime(gn, FALSE);
	Var_Set(TARGET, GNode_Path(gn), gn);
	UnmarkChildren(gn);
	HandleUseNodes(gn);

	if (!(gn->type & OP_MADE))
	    Suff_FindDeps(gn);
	else {
	    PretendAllChildrenAreMade(gn);
	    if (gn->unmade != 0)
		printf("Warning: %s%s still has %d unmade children\n",
		       gn->name, gn->cohort_num, gn->unmade);
	}

	if (gn->unmade != 0)
	    ExamineLater(examine, gn->children);
    }

    Lst_Free(examine);
}

/* Make the .WAIT node depend on the previous children */
static void
add_wait_dependency(GNodeListNode *owln, GNode *wn)
{
    GNodeListNode *cln;
    GNode *cn;

    for (cln = owln; (cn = cln->datum) != wn; cln = cln->next) {
	DEBUG3(MAKE, ".WAIT: add dependency %s%s -> %s\n",
	       cn->name, cn->cohort_num, wn->name);

	/* XXX: This pattern should be factored out, it repeats often */
	Lst_Append(wn->children, cn);
	wn->unmade++;
	Lst_Append(cn->parents, wn);
    }
}

/* Convert .WAIT nodes into dependencies. */
static void
Make_ProcessWait(GNodeList *targs)
{
    GNode  *pgn;		/* 'parent' node we are examining */
    GNodeListNode *owln;	/* Previous .WAIT node */
    GNodeList *examine;		/* List of targets to examine */

    /*
     * We need all the nodes to have a common parent in order for the
     * .WAIT and .ORDER scheduling to work.
     * Perhaps this should be done earlier...
     */

    pgn = GNode_New(".MAIN");
    pgn->flags = REMAKE;
    pgn->type = OP_PHONY | OP_DEPENDS;
    /* Get it displayed in the diag dumps */
    Lst_Prepend(Targ_List(), pgn);

    {
	GNodeListNode *ln;
	for (ln = targs->first; ln != NULL; ln = ln->next) {
	    GNode *cgn = ln->datum;

	    Lst_Append(pgn->children, cgn);
	    Lst_Append(cgn->parents, pgn);
	    pgn->unmade++;
	}
    }

    /* Start building with the 'dummy' .MAIN' node */
    MakeBuildChild(pgn, NULL);

    examine = Lst_New();
    Lst_Append(examine, pgn);

    while (!Lst_IsEmpty(examine)) {
	GNodeListNode *ln;

	pgn = Lst_Dequeue(examine);

	/* We only want to process each child-list once */
	if (pgn->flags & DONE_WAIT)
	    continue;
	pgn->flags |= DONE_WAIT;
	DEBUG1(MAKE, "Make_ProcessWait: examine %s\n", pgn->name);

	if (pgn->type & OP_DOUBLEDEP)
	    Lst_PrependAll(examine, pgn->cohorts);

	owln = pgn->children->first;
	for (ln = pgn->children->first; ln != NULL; ln = ln->next) {
	    GNode *cgn = ln->datum;
	    if (cgn->type & OP_WAIT) {
		add_wait_dependency(owln, cgn);
		owln = ln;
	    } else {
		Lst_Append(examine, cgn);
	    }
	}
    }

    Lst_Free(examine);
}

/*-
 *-----------------------------------------------------------------------
 * Make_Run --
 *	Initialize the nodes to remake and the list of nodes which are
 *	ready to be made by doing a breadth-first traversal of the graph
 *	starting from the nodes in the given list. Once this traversal
 *	is finished, all the 'leaves' of the graph are in the toBeMade
 *	queue.
 *	Using this queue and the Job module, work back up the graph,
 *	calling on MakeStartJobs to keep the job table as full as
 *	possible.
 *
 * Input:
 *	targs		the initial list of targets
 *
 * Results:
 *	TRUE if work was done. FALSE otherwise.
 *
 * Side Effects:
 *	The make field of all nodes involved in the creation of the given
 *	targets is set to 1. The toBeMade list is set to contain all the
 *	'leaves' of these subgraphs.
 *-----------------------------------------------------------------------
 */
Boolean
Make_Run(GNodeList *targs)
{
    int errors;			/* Number of errors the Job module reports */

    /* Start trying to make the current targets... */
    toBeMade = Lst_New();

    Make_ExpandUse(targs);
    Make_ProcessWait(targs);

    if (DEBUG(MAKE)) {
	 debug_printf("#***# full graph\n");
	 Targ_PrintGraph(1);
    }

    if (opts.queryFlag) {
	/*
	 * We wouldn't do any work unless we could start some jobs in the
	 * next loop... (we won't actually start any, of course, this is just
	 * to see if any of the targets was out of date)
	 */
	return MakeStartJobs();
    }
    /*
     * Initialization. At the moment, no jobs are running and until some
     * get started, nothing will happen since the remaining upward
     * traversal of the graph is performed by the routines in job.c upon
     * the finishing of a job. So we fill the Job table as much as we can
     * before going into our loop.
     */
    (void)MakeStartJobs();

    /*
     * Main Loop: The idea here is that the ending of jobs will take
     * care of the maintenance of data structures and the waiting for output
     * will cause us to be idle most of the time while our children run as
     * much as possible. Because the job table is kept as full as possible,
     * the only time when it will be empty is when all the jobs which need
     * running have been run, so that is the end condition of this loop.
     * Note that the Job module will exit if there were any errors unless the
     * keepgoing flag was given.
     */
    while (!Lst_IsEmpty(toBeMade) || jobTokensRunning > 0) {
	Job_CatchOutput();
	(void)MakeStartJobs();
    }

    errors = Job_Finish();

    /*
     * Print the final status of each target. E.g. if it wasn't made
     * because some inferior reported an error.
     */
    DEBUG1(MAKE, "done: errors %d\n", errors);
    if (errors == 0) {
	MakePrintStatusList(targs, &errors);
	if (DEBUG(MAKE)) {
	    debug_printf("done: errors %d\n", errors);
	    if (errors > 0)
		Targ_PrintGraph(4);
	}
    }
    return errors > 0;
}
