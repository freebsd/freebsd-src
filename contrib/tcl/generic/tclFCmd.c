/*
 * tclFCmd.c
 *
 *      This file implements the generic portion of file manipulation 
 *      subcommands of the "file" command. 
 *
 * Copyright (c) 1996-1997 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tclFCmd.c 1.17 97/05/14 13:23:13
 */

#include "tclInt.h"
#include "tclPort.h"

/*
 * Declarations for local procedures defined in this file:
 */

static int		CopyRenameOneFile _ANSI_ARGS_((Tcl_Interp *interp,
			    char *source, char *dest, int copyFlag,
			    int force));
static char *		FileBasename _ANSI_ARGS_((Tcl_Interp *interp,
			    char *path, Tcl_DString *bufferPtr));
static int		FileCopyRename _ANSI_ARGS_((Tcl_Interp *interp,
			    int argc, char **argv, int copyFlag));
static int		FileForceOption _ANSI_ARGS_((Tcl_Interp *interp,
			    int argc, char **argv, int *forcePtr));

/*
 *---------------------------------------------------------------------------
 *
 * TclFileRenameCmd
 *
 *	This procedure implements the "rename" subcommand of the "file"
 *      command.  Filename arguments need to be translated to native
 *	format before being passed to platform-specific code that
 *	implements rename functionality.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *---------------------------------------------------------------------------
 */

int
TclFileRenameCmd(interp, argc, argv)
    Tcl_Interp *interp;		/* Interp for error reporting. */
    int argc;			/* Number of arguments. */
    char **argv;		/* Argument strings passed to Tcl_FileCmd. */
{
    return FileCopyRename(interp, argc, argv, 0);
}

/*
 *---------------------------------------------------------------------------
 *
 * TclFileCopyCmd
 *
 *	This procedure implements the "copy" subcommand of the "file"
 *	command.  Filename arguments need to be translated to native
 *	format before being passed to platform-specific code that
 *	implements copy functionality.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *---------------------------------------------------------------------------
 */

int
TclFileCopyCmd(interp, argc, argv)
    Tcl_Interp *interp;		/* Used for error reporting */
    int argc;			/* Number of arguments. */
    char **argv;		/* Argument strings passed to Tcl_FileCmd. */
{
    return FileCopyRename(interp, argc, argv, 1);
}

/*
 *---------------------------------------------------------------------------
 *
 * FileCopyRename --
 *
 *	Performs the work of TclFileRenameCmd and TclFileCopyCmd.
 *	See comments for those procedures.
 *
 * Results:
 *	See above.
 *
 * Side effects:
 *	See above.
 *
 *---------------------------------------------------------------------------
 */

static int
FileCopyRename(interp, argc, argv, copyFlag)
    Tcl_Interp *interp;		/* Used for error reporting. */
    int argc;			/* Number of arguments. */
    char **argv;		/* Argument strings passed to Tcl_FileCmd. */
    int copyFlag;		/* If non-zero, copy source(s).  Otherwise,
				 * rename them. */
{
    int i, result, force;
    struct stat statBuf; 
    Tcl_DString targetBuffer;
    char *target;

    i = FileForceOption(interp, argc - 2, argv + 2, &force);
    if (i < 0) {
	return TCL_ERROR;
    }
    i += 2;
    if ((argc - i) < 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		" ", argv[1], " ?options? source ?source ...? target\"", 
		(char *) NULL);
	return TCL_ERROR;
    }

    /*
     * If target doesn't exist or isn't a directory, try the copy/rename.
     * More than 2 arguments is only valid if the target is an existing
     * directory.
     */

    target = Tcl_TranslateFileName(interp, argv[argc - 1], &targetBuffer);
    if (target == NULL) {
	return TCL_ERROR;
    }

    result = TCL_OK;

    /*
     * Call stat() so that if target is a symlink that points to a directory
     * we will put the sources in that directory instead of overwriting the
     * symlink.
     */

    if ((stat(target, &statBuf) != 0) || !S_ISDIR(statBuf.st_mode)) {
	if ((argc - i) > 2) {
	    errno = ENOTDIR;
	    Tcl_PosixError(interp);
	    Tcl_AppendResult(interp, "error ",
		    ((copyFlag) ? "copying" : "renaming"), ": target \"",
		    argv[argc - 1], "\" is not a directory", (char *) NULL);
	    result = TCL_ERROR;
	} else {
	    /*
	     * Even though already have target == translated(argv[i+1]),
	     * pass the original argument down, so if there's an error, the
	     * error message will reflect the original arguments.
	     */

	    result = CopyRenameOneFile(interp, argv[i], argv[i + 1], copyFlag,
		    force);
	}
	Tcl_DStringFree(&targetBuffer);
	return result;
    }
    
    /*
     * Move each source file into target directory.  Extract the basename
     * from each source, and append it to the end of the target path.
     */
    
    for ( ; i < argc - 1; i++) {
	char *jargv[2];
	char *source, *newFileName;
	Tcl_DString sourceBuffer, newFileNameBuffer;

	source = FileBasename(interp, argv[i], &sourceBuffer);
	if (source == NULL) {
	    result = TCL_ERROR;
	    break;
	}
	jargv[0] = argv[argc - 1];
	jargv[1] = source;
	Tcl_DStringInit(&newFileNameBuffer);
	newFileName = Tcl_JoinPath(2, jargv, &newFileNameBuffer);
	result = CopyRenameOneFile(interp, argv[i], newFileName, copyFlag,
		force);
	Tcl_DStringFree(&sourceBuffer);
	Tcl_DStringFree(&newFileNameBuffer);

	if (result == TCL_ERROR) {
	    break;
	}
    }
    Tcl_DStringFree(&targetBuffer);
    return result;
}

/*
 *---------------------------------------------------------------------------
 *
 * TclFileMakeDirsCmd
 *
 *	This procedure implements the "mkdir" subcommand of the "file"
 *      command.  Filename arguments need to be translated to native
 *	format before being passed to platform-specific code that
 *	implements mkdir functionality.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */
int
TclFileMakeDirsCmd(interp, argc, argv)
    Tcl_Interp *interp;		/* Used for error reporting. */
    int argc;			/* Number of arguments */
    char **argv;		/* Argument strings passed to Tcl_FileCmd. */
{
    Tcl_DString nameBuffer, targetBuffer;
    char *errfile;
    int result, i, j, pargc;
    char **pargv;
    struct stat statBuf;

    pargv = NULL;
    errfile = NULL;
    Tcl_DStringInit(&nameBuffer);
    Tcl_DStringInit(&targetBuffer);

    result = TCL_OK;
    for (i = 2; i < argc; i++) {
	char *name = Tcl_TranslateFileName(interp, argv[i], &nameBuffer);
	if (name == NULL) {
	    result = TCL_ERROR;
	    break;
	}

	Tcl_SplitPath(name, &pargc, &pargv);
	if (pargc == 0) {
	    errno = ENOENT;
	    errfile = argv[i];
	    break;
	}
	for (j = 0; j < pargc; j++) {
	    char *target = Tcl_JoinPath(j + 1, pargv, &targetBuffer);

	    /*
	     * Call stat() so that if target is a symlink that points to a
	     * directory we will create subdirectories in that directory.
	     */

	    if (stat(target, &statBuf) == 0) {
		if (!S_ISDIR(statBuf.st_mode)) {
		    errno = EEXIST;
		    errfile = target;
		    goto done;
		}
	    } else if ((errno != ENOENT)
		    || (TclpCreateDirectory(target) != TCL_OK)) {
		errfile = target;
		goto done;
	    }
	    Tcl_DStringFree(&targetBuffer);
	}
	ckfree((char *) pargv);
	pargv = NULL;
	Tcl_DStringFree(&nameBuffer);
    }
	
    done:
    if (errfile != NULL) {
	Tcl_AppendResult(interp, "can't create directory \"",
		errfile, "\": ", Tcl_PosixError(interp), (char *) NULL);
	result = TCL_ERROR;
    }

    Tcl_DStringFree(&nameBuffer);
    Tcl_DStringFree(&targetBuffer);
    if (pargv != NULL) {
	ckfree((char *) pargv);
    }
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * TclFileDeleteCmd
 *
 *	This procedure implements the "delete" subcommand of the "file"
 *      command.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

int
TclFileDeleteCmd(interp, argc, argv)
    Tcl_Interp *interp;		/* Used for error reporting */
    int argc;			/* Number of arguments */
    char **argv;		/* Argument strings passed to Tcl_FileCmd. */
{
    Tcl_DString nameBuffer, errorBuffer;
    int i, force, result;
    char *errfile;
    
    i = FileForceOption(interp, argc - 2, argv + 2, &force);
    if (i < 0) {
	return TCL_ERROR;
    }
    i += 2;
    if ((argc - i) < 1) {
	Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		" ", argv[1], " ?options? file ?file ...?\"", (char *) NULL);
	return TCL_ERROR;
    }

    errfile = NULL;
    result = TCL_OK;
    Tcl_DStringInit(&errorBuffer);
    Tcl_DStringInit(&nameBuffer);

    for ( ; i < argc; i++) {
	struct stat statBuf;
	char *name;

	errfile = argv[i];
	Tcl_DStringSetLength(&nameBuffer, 0);
	name = Tcl_TranslateFileName(interp, argv[i], &nameBuffer);
	if (name == NULL) {
	    result = TCL_ERROR;
	    goto done;
	}

	/*
	 * Call lstat() to get info so can delete symbolic link itself.
	 */

	if (lstat(name, &statBuf) != 0) {
	    /*
	     * Trying to delete a file that does not exist is not
	     * considered an error, just a no-op
	     */

	    if (errno != ENOENT) {
		result = TCL_ERROR;
	    }
	} else if (S_ISDIR(statBuf.st_mode)) {
	    result = TclpRemoveDirectory(name, force, &errorBuffer);
	    if (result != TCL_OK) {
		if ((force == 0) && (errno == EEXIST)) {
		    Tcl_AppendResult(interp, "error deleting \"", argv[i],
			    "\": directory not empty", (char *) NULL);
		    Tcl_PosixError(interp);
		    goto done;
		}

		/* 
		 * If possible, use the untranslated name for the file.
		 */
		 
		errfile = Tcl_DStringValue(&errorBuffer);
		if (strcmp(name, errfile) == 0) {
		    errfile = argv[i];
		}
	    }
	} else {
	    result = TclpDeleteFile(name);
	}
	
	if (result == TCL_ERROR) {
	    break;
	}
    }
    if (result != TCL_OK) {
	Tcl_AppendResult(interp, "error deleting \"", errfile,
		"\": ", Tcl_PosixError(interp), (char *) NULL);
    } 
    done:
    Tcl_DStringFree(&errorBuffer);
    Tcl_DStringFree(&nameBuffer);
    return result;
}

/*
 *---------------------------------------------------------------------------
 *
 * CopyRenameOneFile
 *
 *	Copies or renames specified source file or directory hierarchy
 *	to the specified target.  
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Target is overwritten if the force flag is set.  Attempting to
 *	copy/rename a file onto a directory or a directory onto a file
 *	will always result in an error.  
 *
 *----------------------------------------------------------------------
 */

static int
CopyRenameOneFile(interp, source, target, copyFlag, force) 
    Tcl_Interp *interp;		/* Used for error reporting. */
    char *source;		/* Pathname of file to copy.  May need to
				 * be translated. */
    char *target;		/* Pathname of file to create/overwrite.
				 * May need to be translated. */
    int copyFlag;		/* If non-zero, copy files.  Otherwise,
				 * rename them. */
    int force;			/* If non-zero, overwrite target file if it
				 * exists.  Otherwise, error if target already
				 * exists. */
{
    int result;
    Tcl_DString sourcePath, targetPath, errorBuffer;
    char *targetName, *sourceName, *errfile;
    struct stat sourceStatBuf, targetStatBuf;
	
    sourceName = Tcl_TranslateFileName(interp, source, &sourcePath);
    if (sourceName == NULL) {
	return TCL_ERROR;
    }
    targetName = Tcl_TranslateFileName(interp, target, &targetPath);
    if (targetName == NULL) {
	Tcl_DStringFree(&sourcePath);
	return TCL_ERROR;
    }
    
    errfile = NULL;
    result = TCL_ERROR;
    Tcl_DStringInit(&errorBuffer);
    
    /*
     * We want to copy/rename links and not the files they point to, so we
     * use lstat(). If target is a link, we also want to replace the 
     * link and not the file it points to, so we also use lstat() on the
     * target.
     */

    if (lstat(sourceName, &sourceStatBuf) != 0) {
	errfile = source;
	goto done;
    }
    if (lstat(targetName, &targetStatBuf) != 0) {
	if (errno != ENOENT) {
	    errfile = target;
	    goto done;
	}
    } else {
	if (force == 0) {
	    errno = EEXIST;
	    errfile = target;
	    goto done;
	}

        /* 
         * Prevent copying or renaming a file onto itself.  Under Windows, 
         * stat always returns 0 for st_ino.  However, the Windows-specific 
         * code knows how to deal with copying or renaming a file on top of
         * itself.  It might be a good idea to write a stat that worked.
         */
     
        if ((sourceStatBuf.st_ino != 0) && (targetStatBuf.st_ino != 0)) {
            if ((sourceStatBuf.st_ino == targetStatBuf.st_ino) &&
            	    (sourceStatBuf.st_dev == targetStatBuf.st_dev)) {
            	result = TCL_OK;
            	goto done;
            }
        }

	/*
	 * Prevent copying/renaming a file onto a directory and
	 * vice-versa.  This is a policy decision based on the fact that
	 * existing implementations of copy and rename on all platforms
	 * also prevent this.
	 */

	if (S_ISDIR(sourceStatBuf.st_mode)
                && !S_ISDIR(targetStatBuf.st_mode)) {
	    errno = EISDIR;
	    Tcl_AppendResult(interp, "can't overwrite file \"", target,
		    "\" with directory \"", source, "\"", (char *) NULL);
	    goto done;
	}
	if (!S_ISDIR(sourceStatBuf.st_mode)
	        && S_ISDIR(targetStatBuf.st_mode)) {
	    errno = EISDIR;
	    Tcl_AppendResult(interp, "can't overwrite directory \"", target, 
	            "\" with file \"", source, "\"", (char *) NULL);
	    goto done;
	}
    }

    if (copyFlag == 0) {
	result = TclpRenameFile(sourceName, targetName);
	if (result == TCL_OK) {
	    goto done;
	}
	    
	if (errno == EINVAL) {
	    Tcl_AppendResult(interp, "error renaming \"", source, "\" to \"",
		    target, "\": trying to rename a volume or ",
		    "move a directory into itself", (char *) NULL);
	    goto done;
	} else if (errno != EXDEV) {
	    errfile = target;
	    goto done;
	}
	
	/*
	 * The rename failed because the move was across file systems.
	 * Fall through to copy file and then remove original.  Note that
	 * the low-level TclpRenameFile is allowed to implement
	 * cross-filesystem moves itself.
	 */
    }

    if (S_ISDIR(sourceStatBuf.st_mode)) {
	result = TclpCopyDirectory(sourceName, targetName, &errorBuffer);
	if (result != TCL_OK) {
	    errfile = Tcl_DStringValue(&errorBuffer);
	    if (strcmp(errfile, sourceName) == 0) {
		errfile = source;
	    } else if (strcmp(errfile, targetName) == 0) {
		errfile = target;
	    }
	}
    } else {
	result = TclpCopyFile(sourceName, targetName);
	if (result != TCL_OK) {
	    /*
	     * Well, there really shouldn't be a problem with source,
	     * because up there we checked to see if it was ok to copy it.
	     */

	    errfile = target;
	}
    }
    if ((copyFlag == 0) && (result == TCL_OK)) {
	if (S_ISDIR(sourceStatBuf.st_mode)) {
	    result = TclpRemoveDirectory(sourceName, 1, &errorBuffer);
	    if (result != TCL_OK) {
		errfile = Tcl_DStringValue(&errorBuffer);
		if (strcmp(errfile, sourceName) == 0) {
		    errfile = source;
		}
	    }
	} else {
	    result = TclpDeleteFile(sourceName);
	    if (result != TCL_OK) {
		errfile = source;
	    }
	}
	if (result != TCL_OK) {
	    Tcl_AppendResult(interp, "can't unlink \"", errfile, "\": ",
		    Tcl_PosixError(interp), (char *) NULL);
	    errfile = NULL;
	}
    }
    
    done:
    if (errfile != NULL) {
	Tcl_AppendResult(interp, 
		((copyFlag) ? "error copying \"" : "error renaming \""),
		source, (char *) NULL);
	if (errfile != source) {
	    Tcl_AppendResult(interp, "\" to \"", target, (char *) NULL);
	    if (errfile != target) {
		Tcl_AppendResult(interp, "\": \"", errfile, (char *) NULL);
	    }
	}
	Tcl_AppendResult(interp, "\": ", Tcl_PosixError(interp),
		(char *) NULL);
    }
    Tcl_DStringFree(&errorBuffer);
    Tcl_DStringFree(&sourcePath);
    Tcl_DStringFree(&targetPath);
    return result;
}

/*
 *---------------------------------------------------------------------------
 *
 * FileForceOption --
 *
 *	Helps parse command line options for file commands that take
 *	the "-force" and "--" options.
 *
 * Results:
 *	The return value is how many arguments from argv were consumed
 *	by this function, or -1 if there was an error parsing the
 *	options.  If an error occurred, an error message is left in
 *	interp->result.
 *
 * Side effects:
 *	None.
 *
 *---------------------------------------------------------------------------
 */

static int
FileForceOption(interp, argc, argv, forcePtr)
    Tcl_Interp *interp;		/* Interp, for error return. */
    int argc;			/* Number of arguments. */
    char **argv;		/* Argument strings.  First command line
    option, if it exists, begins at */
    int *forcePtr;		/* If the "-force" was specified, *forcePtr
				 * is filled with 1, otherwise with 0. */
{
    int force, i;
    
    force = 0;
    for (i = 0; i < argc; i++) {
	if (argv[i][0] != '-') {
	    break;
	}
	if (strcmp(argv[i], "-force") == 0) {
	    force = 1;
	} else if (strcmp(argv[i], "--") == 0) {
	    i++;
	    break;
	} else {
	    Tcl_AppendResult(interp, "bad option \"", argv[i], 
		    "\": should be -force or --", (char *)NULL);
	    return -1;
	}
    }
    *forcePtr = force;
    return i;
}
/*
 *---------------------------------------------------------------------------
 *
 * FileBasename --
 *
 *	Given a path in either tcl format (with / separators), or in the
 *	platform-specific format for the current platform, return all the
 *	characters in the path after the last directory separator.  But,
 *	if path is the root directory, returns no characters.
 *
 * Results:
 *	Appends the string that represents the basename to the end of
 *	the specified initialized DString, returning a pointer to the
 *	resulting string.  If there is an error, an error message is left
 *	in interp, NULL is returned, and the Tcl_DString is unmodified.
 *
 * Side effects:
 *	None.
 *
 *---------------------------------------------------------------------------
 */

static char *
FileBasename(interp, path, bufferPtr)
    Tcl_Interp *interp;		/* Interp, for error return. */
    char *path;			/* Path whose basename to extract. */
    Tcl_DString *bufferPtr;	/* Initialized DString that receives
				 * basename. */
{
    int argc;
    char **argv;
    
    Tcl_SplitPath(path, &argc, &argv);
    if (argc == 0) {
	Tcl_DStringInit(bufferPtr);
    } else {
	if ((argc == 1) && (*path == '~')) {
	    Tcl_DString buffer;
	    
	    ckfree((char *) argv);
	    path = Tcl_TranslateFileName(interp, path, &buffer);
	    if (path == NULL) {
		return NULL;
	    }
	    Tcl_SplitPath(path, &argc, &argv);
	    Tcl_DStringFree(&buffer);
	}
	Tcl_DStringInit(bufferPtr);

	/*
	 * Return the last component, unless it is the only component, and it
	 * is the root of an absolute path.
	 */

	if (argc > 0) {
	    if ((argc > 1)
		    || (Tcl_GetPathType(argv[0]) == TCL_PATH_RELATIVE)) {
		Tcl_DStringAppend(bufferPtr, argv[argc - 1], -1);
	    }
	}
    }
    ckfree((char *) argv);
    return Tcl_DStringValue(bufferPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * TclFileAttrsCmd --
 *
 *      Sets or gets the platform-specific attributes of a file. The objc-objv
 *	points to the file name with the rest of the command line following.
 *	This routine uses platform-specific tables of option strings
 *	and callbacks. The callback to get the attributes take three
 *	parameters:
 *	    Tcl_Interp *interp;	    The interp to report errors with.
 *				    Since this is an object-based API,
 *				    the object form of the result should be
 *				    used.
 *	    CONST char *fileName;   This is extracted using
 *				    Tcl_TranslateFileName.
 *	    TclObj **attrObjPtrPtr; A new object to hold the attribute
 *				    is allocated and put here.
 *	The first two parameters of the callback used to write out the
 *	attributes are the same. The third parameter is:
 *	    CONST *attrObjPtr;	    A pointer to the object that has
 *				    the new attribute.
 *	They both return standard TCL errors; if the routine to get
 *	an attribute fails, no object is allocated and *attrObjPtrPtr
 *	is unchanged.
 *
 * Results:
 *      Standard TCL error.
 *
 * Side effects:
 *      May set file attributes for the file name.
 *      
 *----------------------------------------------------------------------
 */

int
TclFileAttrsCmd(interp, objc, objv)
    Tcl_Interp *interp;		/* The interpreter for error reporting. */
    int objc;			/* Number of command line arguments. */
    Tcl_Obj *CONST objv[];	/* The command line objects. */
{
    Tcl_Obj *resultPtr = Tcl_GetObjResult(interp);
    char *fileName;
    int length, index;
    Tcl_Obj *listObjPtr;
    Tcl_Obj *elementObjPtr;
    Tcl_DString buffer;

    if ((objc > 2) && ((objc % 2) == 0)) {
	Tcl_AppendStringsToObj(resultPtr, 
		"wrong # args: must be \"file attributes name ?option? ?value? ?option value? ...\"",
		(char *) NULL);
	return TCL_ERROR;
    }

    fileName = Tcl_GetStringFromObj(objv[0], &length);
    if (Tcl_TranslateFileName(interp, fileName, &buffer) == NULL) {
    	return TCL_ERROR;
    }
    fileName = Tcl_DStringValue(&buffer);
    
    if (objc == 1) {
    	listObjPtr = Tcl_NewListObj(0, (Tcl_Obj **) NULL);
    	
    	for (index = 0; tclpFileAttrStrings[index] != NULL; index++) {
    	    elementObjPtr = Tcl_NewStringObj(tclpFileAttrStrings[index], -1);
	    Tcl_ListObjAppendElement(interp, listObjPtr, elementObjPtr);
	    if ((*tclpFileAttrProcs[index].getProc)(interp, index, fileName,
	    	    &elementObjPtr) != TCL_OK) {
	    	Tcl_DecrRefCount(listObjPtr);
	    	return TCL_ERROR;
	    }
	    Tcl_ListObjAppendElement(interp, listObjPtr, elementObjPtr);
    	}
    	Tcl_SetObjResult(interp, listObjPtr);
    } else if (objc == 2) {
    	if (Tcl_GetIndexFromObj(interp, objv[1], tclpFileAttrStrings, "option",
    		0, &index) != TCL_OK) {
    	    return TCL_ERROR;
    	}
	if ((*tclpFileAttrProcs[index].getProc)(interp, index, fileName,
		&elementObjPtr) != TCL_OK) {
	    return TCL_ERROR;
	}
	Tcl_SetObjResult(interp, elementObjPtr);
    } else {
        int i;
        
    	for (i = 1; i < objc ; i += 2) {
    	    if (Tcl_GetIndexFromObj(interp, objv[i], tclpFileAttrStrings, "option",
    	    	    0, &index) != TCL_OK) {
    	    	return TCL_ERROR;
    	    }
    	    if ((*tclpFileAttrProcs[index].setProc)(interp, index, fileName,
    	    	    objv[i + 1]) != TCL_OK) {
    	    	return TCL_ERROR;
    	    }
    	}
    }
    
    Tcl_DStringFree(&buffer);
    
    return TCL_OK;
}
