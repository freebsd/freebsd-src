/* 
 * tclUnixTest.c --
 *
 *	Contains platform specific test commands for the Unix platform.
 *
 * Copyright (c) 1996 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tclUnixTest.c 1.1 96/03/26 12:44:30
 */

#include "tclInt.h"
#include "tclPort.h"

/*
 * The stuff below is used to keep track of file handlers created and
 * exercised by the "testfilehandler" command.
 */

typedef struct Pipe {
    Tcl_File readFile;		/* File handle for reading from the
				 * pipe.  NULL means pipe doesn't exist yet. */
    Tcl_File writeFile;		/* File handle for writing from the
				 * pipe. */
    int readCount;		/* Number of times the file handler for
				 * this file has triggered and the file
				 * was readable. */
    int writeCount;		/* Number of times the file handler for
				 * this file has triggered and the file
				 * was writable. */
} Pipe;

#define MAX_PIPES 10
static Pipe testPipes[MAX_PIPES];

/*
 * Forward declarations of procedures defined later in this file:
 */

static void		TestFileHandlerProc _ANSI_ARGS_((ClientData clientData,
			    int mask));
static int		TestfilehandlerCmd _ANSI_ARGS_((ClientData dummy,
			    Tcl_Interp *interp, int argc, char **argv));
static int		TestgetopenfileCmd _ANSI_ARGS_((ClientData dummy,
			    Tcl_Interp *interp, int argc, char **argv));
int			TclplatformtestInit _ANSI_ARGS_((Tcl_Interp *interp));

/*
 *----------------------------------------------------------------------
 *
 * TclplatformtestInit --
 *
 *	Defines commands that test platform specific functionality for
 *	Unix platforms.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Defines new commands.
 *
 *----------------------------------------------------------------------
 */

int
TclplatformtestInit(interp)
    Tcl_Interp *interp;		/* Interpreter to add commands to. */
{
    Tcl_CreateCommand(interp, "testfilehandler", TestfilehandlerCmd,
            (ClientData) 0, (Tcl_CmdDeleteProc *) NULL);
    Tcl_CreateCommand(interp, "testgetopenfile", TestgetopenfileCmd,
            (ClientData) 0, (Tcl_CmdDeleteProc *) NULL);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TestfilehandlerCmd --
 *
 *	This procedure implements the "testfilehandler" command. It is
 *	used to test Tcl_CreateFileHandler, Tcl_DeleteFileHandler, and
 *	TclWaitForFile.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
TestfilehandlerCmd(clientData, interp, argc, argv)
    ClientData clientData;		/* Not used. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
{
    Pipe *pipePtr;
    int i, mask, timeout;
    static int initialized = 0;
    char buffer[4000];
    Tcl_File file;

    /*
     * NOTE: When we make this code work on Windows also, the following
     * variable needs to be made Unix-only.
     */
    
    int fd;
    
    if (!initialized) {
	for (i = 0; i < MAX_PIPES; i++) {
	    testPipes[i].readFile = NULL;
	}
	initialized = 1;
    }

    if (argc < 2) {
	Tcl_AppendResult(interp, "wrong # arguments: should be \"", argv[0],
                " option ... \"", (char *) NULL);
        return TCL_ERROR;
    }
    pipePtr = NULL;
    if (argc >= 3) {
	if (Tcl_GetInt(interp, argv[2], &i) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (i >= MAX_PIPES) {
	    Tcl_AppendResult(interp, "bad index ", argv[2], (char *) NULL);
	    return TCL_ERROR;
	}
	pipePtr = &testPipes[i];
    }

    if (strcmp(argv[1], "close") == 0) {
	for (i = 0; i < MAX_PIPES; i++) {
	    if (testPipes[i].readFile != NULL) {
		Tcl_DeleteFileHandler(testPipes[i].readFile);

                /*
                 * NOTE: Unix specific code below.
                 */
                
                fd = (int) Tcl_GetFileInfo(testPipes[i].readFile, NULL);
                close(fd);
		Tcl_FreeFile(testPipes[i].readFile);

		testPipes[i].readFile = NULL;
		Tcl_DeleteFileHandler(testPipes[i].writeFile);

                /*
                 * NOTE: Unix specific code below.
                 */
                
                fd = (int) Tcl_GetFileInfo(testPipes[i].writeFile, NULL);
		Tcl_FreeFile(testPipes[i].writeFile);
                close(fd);
	    }
	}
    } else if (strcmp(argv[1], "clear") == 0) {
	if (argc != 3) {
	    Tcl_AppendResult(interp, "wrong # arguments: should be \"",
                    argv[0], " clear index\"", (char *) NULL);
	    return TCL_ERROR;
	}
	pipePtr->readCount = pipePtr->writeCount = 0;
    } else if (strcmp(argv[1], "counts") == 0) {
	if (argc != 3) {
	    Tcl_AppendResult(interp, "wrong # arguments: should be \"",
                    argv[0], " counts index\"", (char *) NULL);
	    return TCL_ERROR;
	}
	sprintf(interp->result, "%d %d", pipePtr->readCount,
		pipePtr->writeCount);
    } else if (strcmp(argv[1], "create") == 0) {
	if (argc != 5) {
	    Tcl_AppendResult(interp, "wrong # arguments: should be \"",
                    argv[0], " create index readMode writeMode\"",
                    (char *) NULL);
	    return TCL_ERROR;
	}
	if (pipePtr->readFile == NULL) {
	    if (!TclCreatePipe(&pipePtr->readFile, &pipePtr->writeFile)) {
		Tcl_AppendResult(interp, "couldn't open pipe: ",
			Tcl_PosixError(interp), (char *) NULL);
		return TCL_ERROR;
	    }
#ifdef O_NONBLOCK
	    fcntl((int)Tcl_GetFileInfo(pipePtr->readFile, NULL),
		    F_SETFL, O_NONBLOCK);
	    fcntl((int)Tcl_GetFileInfo(pipePtr->writeFile, NULL),
		    F_SETFL, O_NONBLOCK);
#else
	    interp->result = "can't make pipes non-blocking";
	    return TCL_ERROR;
#endif
	}
	pipePtr->readCount = 0;
	pipePtr->writeCount = 0;

	if (strcmp(argv[3], "readable") == 0) {
	    Tcl_CreateFileHandler(pipePtr->readFile, TCL_READABLE,
		    TestFileHandlerProc, (ClientData) pipePtr);
	} else if (strcmp(argv[3], "off") == 0) {
	    Tcl_DeleteFileHandler(pipePtr->readFile);
	} else if (strcmp(argv[3], "disabled") == 0) {
	    Tcl_CreateFileHandler(pipePtr->readFile, 0,
		    TestFileHandlerProc, (ClientData) pipePtr);
	} else {
	    Tcl_AppendResult(interp, "bad read mode \"", argv[3], "\"",
		    (char *) NULL);
	    return TCL_ERROR;
	}
	if (strcmp(argv[4], "writable") == 0) {
	    Tcl_CreateFileHandler(pipePtr->writeFile, TCL_WRITABLE,
		    TestFileHandlerProc, (ClientData) pipePtr);
	} else if (strcmp(argv[4], "off") == 0) {
	    Tcl_DeleteFileHandler(pipePtr->writeFile);
	} else if (strcmp(argv[4], "disabled") == 0) {
	    Tcl_CreateFileHandler(pipePtr->writeFile, 0,
		    TestFileHandlerProc, (ClientData) pipePtr);
	} else {
	    Tcl_AppendResult(interp, "bad read mode \"", argv[4], "\"",
		    (char *) NULL);
	    return TCL_ERROR;
	}
    } else if (strcmp(argv[1], "empty") == 0) {
	if (argc != 3) {
	    Tcl_AppendResult(interp, "wrong # arguments: should be \"",
                    argv[0], " empty index\"", (char *) NULL);
	    return TCL_ERROR;
	}

        /*
         * NOTE: Unix specific code below.
         */

        fd = (int) Tcl_GetFileInfo(pipePtr->readFile, NULL);
        while (read(fd, buffer, 4000) > 0) {
            /* Empty loop body. */
        }
    } else if (strcmp(argv[1], "fill") == 0) {
	if (argc != 3) {
	    Tcl_AppendResult(interp, "wrong # arguments: should be \"",
                    argv[0], " empty index\"", (char *) NULL);
	    return TCL_ERROR;
	}

        /*
         * NOTE: Unix specific code below.
         */

        fd = (int) Tcl_GetFileInfo(pipePtr->writeFile, NULL);
	memset((VOID *) buffer, 'a', 4000);
        while (write(fd, buffer, 4000) > 0) {
            /* Empty loop body. */
        }
    } else if (strcmp(argv[1], "fillpartial") == 0) {
	if (argc != 3) {
	    Tcl_AppendResult(interp, "wrong # arguments: should be \"",
                    argv[0], " empty index\"", (char *) NULL);
	    return TCL_ERROR;
	}

        /*
         * NOTE: Unix specific code below.
         */

        fd = (int) Tcl_GetFileInfo(pipePtr->writeFile, NULL);
	memset((VOID *) buffer, 'b', 10);
	sprintf(interp->result, "%d", write(fd, buffer, 10));
    } else if (strcmp(argv[1], "oneevent") == 0) {
	Tcl_DoOneEvent(TCL_FILE_EVENTS|TCL_DONT_WAIT);
    } else if (strcmp(argv[1], "wait") == 0) {
	if (argc != 5) {
	    Tcl_AppendResult(interp, "wrong # arguments: should be \"",
                    argv[0], " wait index readable/writable timeout\"",
                    (char *) NULL);
	    return TCL_ERROR;
	}
	if (pipePtr->readFile == NULL) {
	    Tcl_AppendResult(interp, "pipe ", argv[2], " doesn't exist",
		    (char *) NULL);
	    return TCL_ERROR;
	}
	if (strcmp(argv[3], "readable") == 0) {
	    mask = TCL_READABLE;
	    file = pipePtr->readFile;
	} else {
	    mask = TCL_WRITABLE;
	    file = pipePtr->writeFile;
	}
	if (Tcl_GetInt(interp, argv[4], &timeout) != TCL_OK) {
	    return TCL_ERROR;
	}
	i = TclWaitForFile(file, mask, timeout);
	if (i & TCL_READABLE) {
	    Tcl_AppendElement(interp, "readable");
	}
	if (i & TCL_WRITABLE) {
	    Tcl_AppendElement(interp, "writable");
	}
    } else if (strcmp(argv[1], "windowevent") == 0) {
	Tcl_DoOneEvent(TCL_WINDOW_EVENTS|TCL_DONT_WAIT);
    } else {
	Tcl_AppendResult(interp, "bad option \"", argv[1],
		"\": must be close, clear, counts, create, empty, fill, ",
		"fillpartial, oneevent, wait, or windowevent",
		(char *) NULL);
	return TCL_ERROR;
    }
    return TCL_OK;
}

static void TestFileHandlerProc(clientData, mask)
    ClientData clientData;	/* Points to a Pipe structure. */
    int mask;			/* Indicates which events happened:
				 * TCL_READABLE or TCL_WRITABLE. */
{
    Pipe *pipePtr = (Pipe *) clientData;

    if (mask & TCL_READABLE) {
	pipePtr->readCount++;
    }
    if (mask & TCL_WRITABLE) {
	pipePtr->writeCount++;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TestgetopenfileCmd --
 *
 *	This procedure implements the "testgetopenfile" command. It is
 *	used to get a FILE * value from a registered channel.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
TestgetopenfileCmd(clientData, interp, argc, argv)
    ClientData clientData;		/* Not used. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
{
    ClientData filePtr;

    if (argc != 3) {
        Tcl_AppendResult(interp,
                "wrong # args: should be \"", argv[0],
                " channelName forWriting\"",
                (char *) NULL);
        return TCL_ERROR;
    }
    if (Tcl_GetOpenFile(interp, argv[1], atoi(argv[2]), 1, &filePtr)
            == TCL_ERROR) {
        return TCL_ERROR;
    }
    if (filePtr == (ClientData) NULL) {
        Tcl_AppendResult(interp,
                "Tcl_GetOpenFile succeeded but FILE * NULL!", (char *) NULL);
        return TCL_ERROR;
    }
    return TCL_OK;
}
