/* 
 * tclFHandle.c --
 *
 *	This file contains functions for manipulating Tcl file handles.
 *
 * Copyright (c) 1995 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tclFHandle.c 1.6 96/02/13 16:29:55
 */

#include "tcl.h"
#include "tclPort.h"

/*
 * The FileHashKey structure is used to associate the OS file handle and type
 * with the corresponding notifier data in a FileHandle.
 */

typedef struct FileHashKey {
    int type;			/* File handle type. */
    ClientData osHandle;	/* Platform specific OS file handle. */
} FileHashKey;

typedef struct FileHandle {
    FileHashKey key;		/* Hash key for a given file. */
    ClientData data;		/* Platform specific notifier data. */
    Tcl_FileFreeProc *proc;	/* Callback to invoke when file is freed. */
} FileHandle;

/*
 * Static variables used in this file:
 */

static Tcl_HashTable fileTable;	/* Hash table containing file handles. */
static int initialized = 0;	/* 1 if this module has been initialized. */

/*
 * Static procedures used in this file:
 */

static void 		FileExitProc _ANSI_ARGS_((ClientData clientData));

/*
 *----------------------------------------------------------------------
 *
 * Tcl_GetFile --
 *
 *	This function retrieves the file handle associated with a
 *	platform specific file handle of the given type.  It creates
 *	a new file handle if needed.
 *
 * Results:
 *	Returns the file handle associated with the file descriptor.
 *
 * Side effects:
 *	Initializes the file handle table if necessary.
 *
 *----------------------------------------------------------------------
 */

Tcl_File
Tcl_GetFile(osHandle, type)
    ClientData osHandle;	/* Platform specific file handle. */
    int type;			/* Type of file handle. */
{
    FileHashKey key;
    Tcl_HashEntry *entryPtr;
    int new;

    if (!initialized) {
	Tcl_InitHashTable(&fileTable, sizeof(FileHashKey)/sizeof(int));
	Tcl_CreateExitHandler(FileExitProc, 0);
	initialized = 1;
    }
    key.osHandle = osHandle;
    key.type = type;
    entryPtr = Tcl_CreateHashEntry(&fileTable, (char *) &key, &new);
    if (new) {
	FileHandle *newHandlePtr;
	newHandlePtr = (FileHandle *) ckalloc(sizeof(FileHandle));
	newHandlePtr->key = key;
	newHandlePtr->data = NULL;
	newHandlePtr->proc = NULL;
	Tcl_SetHashValue(entryPtr, newHandlePtr);
    }
    
    return (Tcl_File) Tcl_GetHashValue(entryPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_FreeFile --
 *
 *	Deallocates an entry in the file handle table.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_FreeFile(handle)
    Tcl_File handle;
{
    Tcl_HashEntry *entryPtr;
    FileHandle *handlePtr = (FileHandle *) handle;

    /*
     * Invoke free procedure, then delete the handle.
     */

    if (handlePtr->proc) {
	(*handlePtr->proc)(handlePtr->data);
    }

    entryPtr = Tcl_FindHashEntry(&fileTable, (char *) &handlePtr->key);
    if (entryPtr) {
	Tcl_DeleteHashEntry(entryPtr);
	ckfree((char *) handlePtr);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_GetFileInfo --
 *
 *	This function retrieves the platform specific file data and
 *	type from the file handle.
 *
 * Results:
 *	If typePtr is not NULL, sets *typePtr to the type of the file.
 *	Returns the platform specific file data.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

ClientData
Tcl_GetFileInfo(handle, typePtr)
    Tcl_File handle;
    int *typePtr;
{
    FileHandle *handlePtr = (FileHandle *) handle;

    if (typePtr) {
	*typePtr = handlePtr->key.type;
    }
    return handlePtr->key.osHandle;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_SetNotifierData --
 *
 *	This function is used by the notifier to associate platform
 *	specific notifier information and a deletion procedure with
 *	a file handle.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Updates the data and delProc slots in the file handle.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_SetNotifierData(handle, proc, data)
    Tcl_File handle;
    Tcl_FileFreeProc *proc;
    ClientData data;
{
    FileHandle *handlePtr = (FileHandle *) handle;
    handlePtr->proc = proc;
    handlePtr->data = data;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_GetNotifierData --
 *
 *	This function is used by the notifier to retrieve the platform
 *	specific notifier information associated with a file handle.
 *
 * Results:
 *	Returns the data stored in a file handle by a previous call to
 *	Tcl_SetNotifierData, and places a pointer to the free proc
 *	in the location referred to by procPtr.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

ClientData
Tcl_GetNotifierData(handle, procPtr)
    Tcl_File handle;
    Tcl_FileFreeProc **procPtr;
{
    FileHandle *handlePtr = (FileHandle *) handle;
    if (procPtr != NULL) {
	*procPtr = handlePtr->proc;
    }
    return handlePtr->data;
}

/*
 *----------------------------------------------------------------------
 *
 * FileExitProc --
 *
 *	This function an exit handler that frees any memory allocated
 *	for the file handle table.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Cleans up the file handle table.
 *
 *----------------------------------------------------------------------
 */

static void
FileExitProc(clientData)
    ClientData clientData;	/* Not used. */
{
    Tcl_HashSearch search;
    Tcl_HashEntry *entryPtr;

    entryPtr = Tcl_FirstHashEntry(&fileTable, &search);

    while (entryPtr) {
	ckfree(Tcl_GetHashValue(entryPtr));
	entryPtr = Tcl_NextHashEntry(&search);
    }

    Tcl_DeleteHashTable(&fileTable);
}
