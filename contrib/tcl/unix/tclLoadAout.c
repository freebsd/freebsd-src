/* 
 * tclLoadAout.c --
 *
 *	This procedure provides a version of the TclLoadFile that
 *	provides pseudo-static linking using version-7 compatible
 *	a.out files described in either sys/exec.h or sys/a.out.h.
 *
 * Copyright (c) 1995, by General Electric Company. All rights reserved.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * This work was supported in part by the ARPA Manufacturing Automation
 * and Design Engineering (MADE) Initiative through ARPA contract
 * F33615-94-C-4400.
 *
 * SCCS: @(#) tclLoadAout.c 1.9 97/02/22 14:05:01
 */

#include "tclInt.h"
#include <fcntl.h>
#ifdef HAVE_EXEC_AOUT_H
#   include <sys/exec_aout.h>
#endif

/*
 * Some systems describe the a.out header in sys/exec.h, and some in
 * a.out.h.
 */

#ifdef USE_SYS_EXEC_H
#include <sys/exec.h>
#endif
#ifdef USE_A_OUT_H
#include <a.out.h>
#endif
#ifdef USE_SYS_EXEC_AOUT_H
#include <sys/exec_aout.h>
#define a_magic a_midmag
#endif

/*
 * TCL_LOADSHIM is the amount by which to shim the break when loading
 */

#ifndef TCL_LOADSHIM
#define TCL_LOADSHIM 0x4000L
#endif

/*
 * TCL_LOADALIGN must be a power of 2, and is the alignment to which
 * to force the origin of load modules
 */

#ifndef TCL_LOADALIGN
#define TCL_LOADALIGN 0x4000L
#endif

/*
 * TCL_LOADMAX is the maximum size of a load module, and is used as
 * a sanity check when loading
 */

#ifndef TCL_LOADMAX
#define TCL_LOADMAX 2000000L
#endif

/*
 * Kernel calls that appear to be missing from the system .h files:
 */

extern char * brk _ANSI_ARGS_((char *));
extern char * sbrk _ANSI_ARGS_((size_t));

/*
 * The static variable SymbolTableFile contains the file name where the
 * result of the last link was stored.  The file is kept because doing so
 * allows one load module to use the symbols defined in another.
 */

static char * SymbolTableFile = NULL;

/*
 * Type of the dictionary function that begins each load module.
 */

typedef Tcl_PackageInitProc * (* DictFn) _ANSI_ARGS_ ((char * symbol));

/*
 * Prototypes for procedures referenced only in this file:
 */

static int FindLibraries _ANSI_ARGS_((Tcl_Interp * interp, char * fileName,
				      Tcl_DString * buf));
static void UnlinkSymbolTable _ANSI_ARGS_((void));

/*
 *----------------------------------------------------------------------
 *
 * TclLoadFile --
 *
 *	Dynamically loads a binary code file into memory and returns
 *	the addresses of two procedures within that file, if they
 *	are defined.
 *
 * Results:
 *	A standard Tcl completion code.  If an error occurs, an error
 *	message is left in interp->result.  *proc1Ptr and *proc2Ptr
 *	are filled in with the addresses of the symbols given by
 *	*sym1 and *sym2, or NULL if those symbols can't be found.
 *
 * Side effects:
 *	New code suddenly appears in memory.
 *
 *
 * Bugs:
 *	This function does not attempt to handle the case where the
 *	BSS segment is not executable.  It will therefore fail on
 *	Encore Multimax, Pyramid 90x, and similar machines.  The
 *	reason is that the mprotect() kernel call, which would
 *	otherwise be employed to mark the newly-loaded text segment
 *	executable, results in a system crash on BSD/386.
 *
 *	In an effort to make it fast, this function eschews the
 *	technique of linking the load module once, reading its header
 *	to determine its size, allocating memory for it, and linking
 *	it again.  Instead, it `shims out' memory allocation by
 *	placing the module TCL_LOADSHIM bytes beyond the break,
 *	and assuming that any malloc() calls required to run the
 *	linker will not advance the break beyond that point.  If
 *	the break is advanced beyonnd that point, the load will
 *	fail with an `inconsistent memory allocation' error.
 *	It perhaps ought to retry the link, but the failure has
 *	not been observed in two years of daily use of this function.
 *----------------------------------------------------------------------
 */

int
TclLoadFile(interp, fileName, sym1, sym2, proc1Ptr, proc2Ptr)
    Tcl_Interp *interp;		/* Used for error reporting. */
    char *fileName;		/* Name of the file containing the desired
				 * code. */
    char *sym1, *sym2;		/* Names of two procedures to look up in
				 * the file's symbol table. */
    Tcl_PackageInitProc **proc1Ptr, **proc2Ptr;
				/* Where to return the addresses corresponding
				 * to sym1 and sym2. */
{
  char * inputSymbolTable;	/* Name of the file containing the 
				 * symbol table from the last link. */
  Tcl_DString linkCommandBuf;	/* Command to do the run-time relocation
				 * of the module.*/
  char * linkCommand;
  char relocatedFileName [L_tmpnam];
				/* Name of the file holding the relocated */
				/* text of the module */
  int relocatedFd;		/* File descriptor of the file holding
				 * relocated text */
  struct exec relocatedHead;	/* Header of the relocated text */
  unsigned long relocatedSize;	/* Size of the relocated text */
  char * startAddress;		/* Starting address of the module */
  DictFn dictionary;		/* Dictionary function in the load module */
  int status;			/* Status return from Tcl_ calls */
  char * p;

  /* Find the file that contains the symbols for the run-time link. */

  if (SymbolTableFile != NULL) {
    inputSymbolTable = SymbolTableFile;
  } else if (tclExecutableName == NULL) {
    Tcl_SetResult (interp, "can't find the tclsh executable", TCL_STATIC);
    return TCL_ERROR;
  } else {
    inputSymbolTable = tclExecutableName;
  }

  /* Construct the `ld' command that builds the relocated module */

  tmpnam (relocatedFileName);
  Tcl_DStringInit (&linkCommandBuf);
  Tcl_DStringAppend (&linkCommandBuf, "exec ld -o ", -1);
  Tcl_DStringAppend (&linkCommandBuf, relocatedFileName, -1);
#if defined(__mips) || defined(mips)
  Tcl_DStringAppend (&linkCommandBuf, " -G 0 ", -1);
#endif
  Tcl_DStringAppend (&linkCommandBuf, " -u TclLoadDictionary_", -1);
  TclGuessPackageName(fileName, &linkCommandBuf);
  Tcl_DStringAppend (&linkCommandBuf, " -A ", -1);
  Tcl_DStringAppend (&linkCommandBuf, inputSymbolTable, -1);
  Tcl_DStringAppend (&linkCommandBuf, " -N -T XXXXXXXX ", -1);
  Tcl_DStringAppend (&linkCommandBuf, fileName, -1);
  Tcl_DStringAppend (&linkCommandBuf, " ", -1);
  if (FindLibraries (interp, fileName, &linkCommandBuf) != TCL_OK) {
    Tcl_DStringFree (&linkCommandBuf);
    return TCL_ERROR;
  }
  linkCommand = Tcl_DStringValue (&linkCommandBuf);

  /* Determine the starting address, and plug it into the command */
  
  startAddress = (char *) (((unsigned long) sbrk (0)
			    + TCL_LOADSHIM + TCL_LOADALIGN - 1)
			   & (- TCL_LOADALIGN));
  p = strstr (linkCommand, "-T") + 3;
  sprintf (p, "%08lx", (long) startAddress);
  p [8] = ' ';

  /* Run the linker */

  status = Tcl_Eval (interp, linkCommand);
  Tcl_DStringFree (&linkCommandBuf);
  if (status != 0) {
    return TCL_ERROR;
  }

  /* Open the linker's result file and read the header */

  relocatedFd = open (relocatedFileName, O_RDONLY);
  if (relocatedFd < 0) {
    goto ioError;
  }
  status= read (relocatedFd, (char *) & relocatedHead, sizeof relocatedHead);
  if (status < sizeof relocatedHead) {
    goto ioError;
  }

  /* Check the magic number */

  if (relocatedHead.a_magic != OMAGIC) {
    Tcl_AppendResult (interp, "bad magic number in intermediate file \"",
		      relocatedFileName, "\"", (char *) NULL);
    goto failure;
  }

  /* Make sure that memory allocation is still consistent */

  if ((unsigned long) sbrk (0) > (unsigned long) startAddress) {
    Tcl_SetResult (interp, "can't load, memory allocation is inconsistent.",
		   TCL_STATIC);
    goto failure;
  }

  /* Make sure that the relocated module's size is reasonable */

  relocatedSize = relocatedHead.a_text + relocatedHead.a_data
    + relocatedHead.a_bss;
  if (relocatedSize > TCL_LOADMAX) {
    Tcl_SetResult (interp, "module too big to load", TCL_STATIC);
    goto failure;
  }

  /* Advance the break to protect the loaded module */

  (void) brk (startAddress + relocatedSize);

  /* Seek to the start of the module's text */

#if defined(__mips) || defined(mips)
  status = lseek (relocatedFd,
		  N_TXTOFF (relocatedHead.ex_f, relocatedHead.ex_o),
		  SEEK_SET);
#else
  status = lseek (relocatedFd, N_TXTOFF (relocatedHead), SEEK_SET);
#endif
  if (status < 0) {
    goto ioError;
  }

  /* Read in the module's text and data */

  relocatedSize = relocatedHead.a_text + relocatedHead.a_data;
  if (read (relocatedFd, startAddress, relocatedSize) < relocatedSize) {
    brk (startAddress);
  ioError:
    Tcl_AppendResult (interp, "error on intermediate file \"",
		      relocatedFileName, "\": ", Tcl_PosixError (interp),
		      (char *) NULL);
  failure:
    (void) unlink (relocatedFileName);
    return TCL_ERROR;
  }

  /* Close the intermediate file. */

  (void) close (relocatedFd);

  /* Arrange things so that intermediate symbol tables eventually get
   * deleted. */

  if (SymbolTableFile != NULL) {
    UnlinkSymbolTable ();
  } else {
    atexit (UnlinkSymbolTable);
  }
  SymbolTableFile = ckalloc (strlen (relocatedFileName) + 1);
  strcpy (SymbolTableFile, relocatedFileName);
  
  /* Look up the entry points in the load module's dictionary. */

  dictionary = (DictFn) startAddress;
  *proc1Ptr = dictionary (sym1);
  *proc2Ptr = dictionary (sym2);

  return TCL_OK;
}

/*
 *------------------------------------------------------------------------
 *
 * FindLibraries --
 *
 *	Find the libraries needed to link a load module at run time.
 *
 * Results:
 *	A standard Tcl completion code.  If an error occurs,
 *	an error message is left in interp->result.  The -l and -L flags
 *	are concatenated onto the dynamic string `buf'.
 *
 *------------------------------------------------------------------------
 */

static int
FindLibraries (interp, fileName, buf)
     Tcl_Interp * interp;	/* Used for error reporting */
     char * fileName;		/* Name of the load module */
     Tcl_DString * buf;		/* Buffer where the -l an -L flags */
{
  FILE * f;			/* The load module */
  int c;			/* Byte from the load module */
  char * p;

  /* Open the load module */

  if ((f = fopen (fileName, "rb")) == NULL) {
    Tcl_AppendResult (interp, "couldn't open \"", fileName, "\": ",
		      Tcl_PosixError (interp), (char *) NULL);
    return TCL_ERROR;
  }

  /* Search for the library list in the load module */

  p = "@LIBS: ";
  while (*p != '\0' && (c = getc (f)) != EOF) {
    if (c == *p) {
      ++p;
    }
    else {
      p = "@LIBS: ";
      if (c == *p) {
	++p;
      }
    }
  }

  /* No library list -- this must be an ill-formed module */

  if (c == EOF) {
    Tcl_AppendResult (interp, "File \"", fileName,
		      "\" is not a Tcl load module.", (char *) NULL);
    (void) fclose (f);
    return TCL_ERROR;
  }

  /* Accumulate the library list */

  while ((c = getc (f)) != '\0' && c != EOF) {
    char cc = c;
    Tcl_DStringAppend (buf, &cc, 1);
  }
  (void) fclose (f);

  if (c == EOF) {
    Tcl_AppendResult (interp, "Library directory in \"", fileName,
		      "\" ends prematurely.", (char *) NULL);
    return TCL_ERROR;
  }

  return TCL_OK;
}

/*
 *------------------------------------------------------------------------
 *
 * UnlinkSymbolTable --
 *
 *	Remove the symbol table file from the last dynamic link.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The symbol table file from the last dynamic link is removed.
 *	This function is called when (a) a new symbol table is present
 *	because another dynamic link is complete, or (b) the process
 *	is exiting.
 *------------------------------------------------------------------------
 */

static void
UnlinkSymbolTable ()
{
  (void) unlink (SymbolTableFile);
  ckfree (SymbolTableFile);
  SymbolTableFile = NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * TclGuessPackageName --
 *
 *	If the "load" command is invoked without providing a package
 *	name, this procedure is invoked to try to figure it out.
 *
 * Results:
 *	Always returns 0 to indicate that we couldn't figure out a
 *	package name;  generic code will then try to guess the package
 *	from the file name.  A return value of 1 would have meant that
 *	we figured out the package name and put it in bufPtr.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TclGuessPackageName(fileName, bufPtr)
    char *fileName;		/* Name of file containing package (already
				 * translated to local form if needed). */
    Tcl_DString *bufPtr;	/* Initialized empty dstring.  Append
				 * package name to this if possible. */
{
    char *p, *q, *r;

    if (q = strrchr(fileName,'/')) {
	q++;
    } else {
	q = fileName;
    }
    if (!strncmp(q,"lib",3)) {
	q+=3;
    }
    p = q;
    while ((*p) && (*p != '.') && ((*p<'0') || (*p>'9'))) {
	p++;
    }
    if ((p>q+2) && !strncmp(p-2,"_G0.",4)) {
	p-=2;
    }
    if (p<q) {
	return 0;
    }

    Tcl_DStringAppend(bufPtr,q, p-q);

    r = Tcl_DStringValue(bufPtr);
    r += strlen(r) - (p-q);

    if (islower(UCHAR(*r))) {
	*r = (char) toupper(UCHAR(*r));
    }
    while (*(++r)) {
	if (isupper(UCHAR(*r))) {
	    *r = (char) tolower(UCHAR(*r));
	}
    }

    return 1;
}
