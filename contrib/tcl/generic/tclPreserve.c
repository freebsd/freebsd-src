/* 
 * tclPreserve.c --
 *
 *	This file contains a collection of procedures that are used
 *	to make sure that widget records and other data structures
 *	aren't reallocated when there are nested procedures that
 *	depend on their existence.
 *
 * Copyright (c) 1991-1994 The Regents of the University of California.
 * Copyright (c) 1994-1995 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tclPreserve.c 1.14 96/03/20 08:24:37
 */

#include "tclInt.h"

/*
 * The following data structure is used to keep track of all the
 * Tcl_Preserve calls that are still in effect.  It grows as needed
 * to accommodate any number of calls in effect.
 */

typedef struct {
    ClientData clientData;	/* Address of preserved block. */
    int refCount;		/* Number of Tcl_Preserve calls in effect
				 * for block. */
    int mustFree;		/* Non-zero means Tcl_EventuallyFree was
				 * called while a Tcl_Preserve call was in
				 * effect, so the structure must be freed
				 * when refCount becomes zero. */
    Tcl_FreeProc *freeProc;	/* Procedure to call to free. */
} Reference;

static Reference *refArray;	/* First in array of references. */
static int spaceAvl = 0;	/* Total number of structures available
				 * at *firstRefPtr. */
static int inUse = 0;		/* Count of structures currently in use
				 * in refArray. */
#define INITIAL_SIZE 2

/*
 * Static routines in this file:
 */

static void	PreserveExitProc _ANSI_ARGS_((ClientData clientData));


/*
 *----------------------------------------------------------------------
 *
 * PreserveExitProc --
 *
 *	Called during exit processing to clean up the reference array.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Frees the storage of the reference array.
 *
 *----------------------------------------------------------------------
 */

	/* ARGSUSED */
static void
PreserveExitProc(clientData)
    ClientData clientData;		/* NULL -Unused. */
{
    if (spaceAvl != 0) {
        ckfree((char *) refArray);
        refArray = (Reference *) NULL;
        inUse = 0;
        spaceAvl = 0;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_Preserve --
 *
 *	This procedure is used by a procedure to declare its interest
 *	in a particular block of memory, so that the block will not be
 *	reallocated until a matching call to Tcl_Release has been made.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Information is retained so that the block of memory will
 *	not be freed until at least the matching call to Tcl_Release.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_Preserve(clientData)
    ClientData clientData;	/* Pointer to malloc'ed block of memory. */
{
    Reference *refPtr;
    int i;

    /*
     * See if there is already a reference for this pointer.  If so,
     * just increment its reference count.
     */

    for (i = 0, refPtr = refArray; i < inUse; i++, refPtr++) {
	if (refPtr->clientData == clientData) {
	    refPtr->refCount++;
	    return;
	}
    }

    /*
     * Make a reference array if it doesn't already exist, or make it
     * bigger if it is full.
     */

    if (inUse == spaceAvl) {
	if (spaceAvl == 0) {
            Tcl_CreateExitHandler((Tcl_ExitProc *) PreserveExitProc,
                    (ClientData) NULL);
	    refArray = (Reference *) ckalloc((unsigned)
		    (INITIAL_SIZE*sizeof(Reference)));
	    spaceAvl = INITIAL_SIZE;
	} else {
	    Reference *new;

	    new = (Reference *) ckalloc((unsigned)
		    (2*spaceAvl*sizeof(Reference)));
	    memcpy((VOID *) new, (VOID *) refArray,
                    spaceAvl*sizeof(Reference));
	    ckfree((char *) refArray);
	    refArray = new;
	    spaceAvl *= 2;
	}
    }

    /*
     * Make a new entry for the new reference.
     */

    refPtr = &refArray[inUse];
    refPtr->clientData = clientData;
    refPtr->refCount = 1;
    refPtr->mustFree = 0;
    inUse += 1;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_Release --
 *
 *	This procedure is called to cancel a previous call to
 *	Tcl_Preserve, thereby allowing a block of memory to be
 *	freed (if no one else cares about it).
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	If Tcl_EventuallyFree has been called for clientData, and if
 *	no other call to Tcl_Preserve is still in effect, the block of
 *	memory is freed.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_Release(clientData)
    ClientData clientData;	/* Pointer to malloc'ed block of memory. */
{
    Reference *refPtr;
    int mustFree;
    Tcl_FreeProc *freeProc;
    int i;

    for (i = 0, refPtr = refArray; i < inUse; i++, refPtr++) {
	if (refPtr->clientData != clientData) {
	    continue;
	}
	refPtr->refCount--;
	if (refPtr->refCount == 0) {

            /*
             * Must remove information from the slot before calling freeProc
             * to avoid reentrancy problems if the freeProc calls Tcl_Preserve
             * on the same clientData. Copy down the last reference in the
             * array to overwrite the current slot.
             */

            freeProc = refPtr->freeProc;
            mustFree = refPtr->mustFree;
	    inUse--;
	    if (i < inUse) {
		refArray[i] = refArray[inUse];
	    }
	    if (mustFree) {
		if ((freeProc == TCL_DYNAMIC) ||
                        (freeProc == (Tcl_FreeProc *) free)) {
		    ckfree((char *) clientData);
		} else {
		    (*freeProc)((char *) clientData);
		}
	    }
	}
	return;
    }

    /*
     * Reference not found.  This is a bug in the caller.
     */

    panic("Tcl_Release couldn't find reference for 0x%x", clientData);
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_EventuallyFree --
 *
 *	Free up a block of memory, unless a call to Tcl_Preserve is in
 *	effect for that block.  In this case, defer the free until all
 *	calls to Tcl_Preserve have been undone by matching calls to
 *	Tcl_Release.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Ptr may be released by calling free().
 *
 *----------------------------------------------------------------------
 */

void
Tcl_EventuallyFree(clientData, freeProc)
    ClientData clientData;	/* Pointer to malloc'ed block of memory. */
    Tcl_FreeProc *freeProc;	/* Procedure to actually do free. */
{
    Reference *refPtr;
    int i;

    /*
     * See if there is a reference for this pointer.  If so, set its
     * "mustFree" flag (the flag had better not be set already!).
     */

    for (i = 0, refPtr = refArray; i < inUse; i++, refPtr++) {
	if (refPtr->clientData != clientData) {
	    continue;
	}
	if (refPtr->mustFree) {
	    panic("Tcl_EventuallyFree called twice for 0x%x\n", clientData);
        }
        refPtr->mustFree = 1;
	refPtr->freeProc = freeProc;
        return;
    }

    /*
     * No reference for this block.  Free it now.
     */

    if ((freeProc == TCL_DYNAMIC) || (freeProc == (Tcl_FreeProc *) free)) {
	ckfree((char *) clientData);
    } else {
	(*freeProc)((char *)clientData);
    }
}
