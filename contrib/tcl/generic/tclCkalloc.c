/* 
 * tclCkalloc.c --
 *
 *    Interface to malloc and free that provides support for debugging problems
 *    involving overwritten, double freeing memory and loss of memory.
 *
 * Copyright (c) 1991-1994 The Regents of the University of California.
 * Copyright (c) 1994-1996 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * This code contributed by Karl Lehenbauer and Mark Diekhans
 *
 *
 * SCCS: @(#) tclCkalloc.c 1.17 96/03/14 13:05:56
 */

#include "tclInt.h"

#define FALSE	0
#define TRUE	1

#ifdef TCL_MEM_DEBUG
#ifndef TCL_GENERIC_ONLY
#include "tclPort.h"
#endif

/*
 * One of the following structures is allocated each time the
 * "memory tag" command is invoked, to hold the current tag.
 */

typedef struct MemTag {
    int refCount;		/* Number of mem_headers referencing
				 * this tag. */
    char string[4];		/* Actual size of string will be as
				 * large as needed for actual tag.  This
				 * must be the last field in the structure. */
} MemTag;

#define TAG_SIZE(bytesInString) ((unsigned) sizeof(MemTag) + bytesInString - 3)

static MemTag *curTagPtr = NULL;/* Tag to use in all future mem_headers
				 * (set by "memory tag" command). */

/*
 * One of the following structures is allocated just before each
 * dynamically allocated chunk of memory, both to record information
 * about the chunk and to help detect chunk under-runs.
 */

#define LOW_GUARD_SIZE (8 + (32 - (sizeof(long) + sizeof(int)))%8)
struct mem_header {
    struct mem_header *flink;
    struct mem_header *blink;
    MemTag *tagPtr;		/* Tag from "memory tag" command;  may be
				 * NULL. */
    char *file;
    long length;
    int line;
    unsigned char low_guard[LOW_GUARD_SIZE];
				/* Aligns body on 8-byte boundary, plus
				 * provides at least 8 additional guard bytes
				 * to detect underruns. */
    char body[1];		/* First byte of client's space.  Actual
				 * size of this field will be larger than
				 * one. */
};

static struct mem_header *allocHead = NULL;  /* List of allocated structures */

#define GUARD_VALUE  0141

/*
 * The following macro determines the amount of guard space *above* each
 * chunk of memory.
 */

#define HIGH_GUARD_SIZE 8

/*
 * The following macro computes the offset of the "body" field within
 * mem_header.  It is used to get back to the header pointer from the
 * body pointer that's used by clients.
 */

#define BODY_OFFSET \
	((unsigned long) (&((struct mem_header *) 0)->body))

static int total_mallocs = 0;
static int total_frees = 0;
static int current_bytes_malloced = 0;
static int maximum_bytes_malloced = 0;
static int current_malloc_packets = 0;
static int maximum_malloc_packets = 0;
static int break_on_malloc = 0;
static int trace_on_at_malloc = 0;
static int  alloc_tracing = FALSE;
static int  init_malloced_bodies = TRUE;
#ifdef MEM_VALIDATE
    static int  validate_memory = TRUE;
#else
    static int  validate_memory = FALSE;
#endif

/*
 * Prototypes for procedures defined in this file:
 */

static int		MemoryCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int argc, char **argv));

/*
 *----------------------------------------------------------------------
 *
 * dump_memory_info --
 *     Display the global memory management statistics.
 *
 *----------------------------------------------------------------------
 */
static void
dump_memory_info(outFile) 
    FILE *outFile;
{
        fprintf(outFile,"total mallocs             %10d\n", 
                total_mallocs);
        fprintf(outFile,"total frees               %10d\n", 
                total_frees);
        fprintf(outFile,"current packets allocated %10d\n", 
                current_malloc_packets);
        fprintf(outFile,"current bytes allocated   %10d\n", 
                current_bytes_malloced);
        fprintf(outFile,"maximum packets allocated %10d\n", 
                maximum_malloc_packets);
        fprintf(outFile,"maximum bytes allocated   %10d\n", 
                maximum_bytes_malloced);
}

/*
 *----------------------------------------------------------------------
 *
 * ValidateMemory --
 *     Procedure to validate allocted memory guard zones.
 *
 *----------------------------------------------------------------------
 */
static void
ValidateMemory (memHeaderP, file, line, nukeGuards)
    struct mem_header *memHeaderP;
    char              *file;
    int                line;
    int                nukeGuards;
{
    unsigned char *hiPtr;
    int   idx;
    int   guard_failed = FALSE;
    int byte;
    
    for (idx = 0; idx < LOW_GUARD_SIZE; idx++) {
        byte = *(memHeaderP->low_guard + idx);
        if (byte != GUARD_VALUE) {
            guard_failed = TRUE;
            fflush (stdout);
	    byte &= 0xff;
            fprintf(stderr, "low guard byte %d is 0x%x  \t%c\n", idx, byte,
		    (isprint(UCHAR(byte)) ? byte : ' '));
        }
    }
    if (guard_failed) {
        dump_memory_info (stderr);
        fprintf (stderr, "low guard failed at %lx, %s %d\n",
                 (long unsigned int) memHeaderP->body, file, line);
        fflush (stderr);  /* In case name pointer is bad. */
        fprintf (stderr, "%ld bytes allocated at (%s %d)\n", memHeaderP->length,
		memHeaderP->file, memHeaderP->line);
        panic ("Memory validation failure");
    }

    hiPtr = (unsigned char *)memHeaderP->body + memHeaderP->length;
    for (idx = 0; idx < HIGH_GUARD_SIZE; idx++) {
        byte = *(hiPtr + idx);
        if (byte != GUARD_VALUE) {
            guard_failed = TRUE;
            fflush (stdout);
	    byte &= 0xff;
            fprintf(stderr, "hi guard byte %d is 0x%x  \t%c\n", idx, byte,
		    (isprint(UCHAR(byte)) ? byte : ' '));
        }
    }

    if (guard_failed) {
        dump_memory_info (stderr);
        fprintf (stderr, "high guard failed at %lx, %s %d\n",
                 (long unsigned int) memHeaderP->body, file, line);
        fflush (stderr);  /* In case name pointer is bad. */
        fprintf (stderr, "%ld bytes allocated at (%s %d)\n",
		memHeaderP->length, memHeaderP->file,
		memHeaderP->line);
        panic ("Memory validation failure");
    }

    if (nukeGuards) {
        memset ((char *) memHeaderP->low_guard, 0, LOW_GUARD_SIZE); 
        memset ((char *) hiPtr, 0, HIGH_GUARD_SIZE); 
    }

}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_ValidateAllMemory --
 *     Validates guard regions for all allocated memory.
 *
 *----------------------------------------------------------------------
 */
void
Tcl_ValidateAllMemory (file, line)
    char  *file;
    int    line;
{
    struct mem_header *memScanP;

    for (memScanP = allocHead; memScanP != NULL; memScanP = memScanP->flink)
        ValidateMemory (memScanP, file, line, FALSE);

}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_DumpActiveMemory --
 *     Displays all allocated memory to stderr.
 *
 * Results:
 *     Return TCL_ERROR if an error accessing the file occures, `errno' 
 *     will have the file error number left in it.
 *----------------------------------------------------------------------
 */
int
Tcl_DumpActiveMemory (fileName)
    char *fileName;
{
    FILE              *fileP;
    struct mem_header *memScanP;
    char              *address;

    fileP = fopen(fileName, "w");
    if (fileP == NULL)
        return TCL_ERROR;

    for (memScanP = allocHead; memScanP != NULL; memScanP = memScanP->flink) {
        address = &memScanP->body [0];
        fprintf (fileP, "%8lx - %8lx  %7ld @ %s %d %s",
		(long unsigned int) address,
                 (long unsigned int) address + memScanP->length - 1,
		 memScanP->length, memScanP->file, memScanP->line,
		 (memScanP->tagPtr == NULL) ? "" : memScanP->tagPtr->string);
	(void) fputc('\n', fileP);
    }
    fclose (fileP);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_DbCkalloc - debugging ckalloc
 *
 *        Allocate the requested amount of space plus some extra for
 *        guard bands at both ends of the request, plus a size, panicing 
 *        if there isn't enough space, then write in the guard bands
 *        and return the address of the space in the middle that the
 *        user asked for.
 *
 *        The second and third arguments are file and line, these contain
 *        the filename and line number corresponding to the caller.
 *        These are sent by the ckalloc macro; it uses the preprocessor
 *        autodefines __FILE__ and __LINE__.
 *
 *----------------------------------------------------------------------
 */
char *
Tcl_DbCkalloc(size, file, line)
    unsigned int size;
    char        *file;
    int          line;
{
    struct mem_header *result;

    if (validate_memory)
        Tcl_ValidateAllMemory (file, line);

    result = (struct mem_header *)malloc((unsigned)size + 
                              sizeof(struct mem_header) + HIGH_GUARD_SIZE);
    if (result == NULL) {
        fflush(stdout);
        dump_memory_info(stderr);
        panic("unable to alloc %d bytes, %s line %d", size, file, 
              line);
    }

    /*
     * Fill in guard zones and size.  Also initialize the contents of
     * the block with bogus bytes to detect uses of initialized data.
     * Link into allocated list.
     */
    if (init_malloced_bodies) {
        memset ((VOID *) result, GUARD_VALUE,
		size + sizeof(struct mem_header) + HIGH_GUARD_SIZE);
    } else {
	memset ((char *) result->low_guard, GUARD_VALUE, LOW_GUARD_SIZE);
	memset (result->body + size, GUARD_VALUE, HIGH_GUARD_SIZE);
    }
    result->length = size;
    result->tagPtr = curTagPtr;
    if (curTagPtr != NULL) {
	curTagPtr->refCount++;
    }
    result->file = file;
    result->line = line;
    result->flink = allocHead;
    result->blink = NULL;
    if (allocHead != NULL)
        allocHead->blink = result;
    allocHead = result;

    total_mallocs++;
    if (trace_on_at_malloc && (total_mallocs >= trace_on_at_malloc)) {
        (void) fflush(stdout);
        fprintf(stderr, "reached malloc trace enable point (%d)\n",
                total_mallocs);
        fflush(stderr);
        alloc_tracing = TRUE;
        trace_on_at_malloc = 0;
    }

    if (alloc_tracing)
        fprintf(stderr,"ckalloc %lx %d %s %d\n",
		(long unsigned int) result->body, size, file, line);

    if (break_on_malloc && (total_mallocs >= break_on_malloc)) {
        break_on_malloc = 0;
        (void) fflush(stdout);
        fprintf(stderr,"reached malloc break limit (%d)\n", 
                total_mallocs);
        fprintf(stderr, "program will now enter C debugger\n");
        (void) fflush(stderr);
	abort();
    }

    current_malloc_packets++;
    if (current_malloc_packets > maximum_malloc_packets)
        maximum_malloc_packets = current_malloc_packets;
    current_bytes_malloced += size;
    if (current_bytes_malloced > maximum_bytes_malloced)
        maximum_bytes_malloced = current_bytes_malloced;

    return result->body;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_DbCkfree - debugging ckfree
 *
 *        Verify that the low and high guards are intact, and if so
 *        then free the buffer else panic.
 *
 *        The guards are erased after being checked to catch duplicate
 *        frees.
 *
 *        The second and third arguments are file and line, these contain
 *        the filename and line number corresponding to the caller.
 *        These are sent by the ckfree macro; it uses the preprocessor
 *        autodefines __FILE__ and __LINE__.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_DbCkfree(ptr, file, line)
    char *  ptr;
    char     *file;
    int       line;
{
    /*
     * The following cast is *very* tricky.  Must convert the pointer
     * to an integer before doing arithmetic on it, because otherwise
     * the arithmetic will be done differently (and incorrectly) on
     * word-addressed machines such as Crays (will subtract only bytes,
     * even though BODY_OFFSET is in words on these machines).
     */

    struct mem_header *memp = (struct mem_header *)
	    (((unsigned long) ptr) - BODY_OFFSET);

    if (alloc_tracing)
        fprintf(stderr, "ckfree %lx %ld %s %d\n",
		(long unsigned int) memp->body, memp->length, file, line);

    if (validate_memory)
        Tcl_ValidateAllMemory (file, line);

    ValidateMemory (memp, file, line, TRUE);
    if (init_malloced_bodies) {
	memset((VOID *) ptr, GUARD_VALUE, (size_t) memp->length);
    }

    total_frees++;
    current_malloc_packets--;
    current_bytes_malloced -= memp->length;

    if (memp->tagPtr != NULL) {
	memp->tagPtr->refCount--;
	if ((memp->tagPtr->refCount == 0) && (curTagPtr != memp->tagPtr)) {
	    free((char *) memp->tagPtr);
	}
    }

    /*
     * Delink from allocated list
     */
    if (memp->flink != NULL)
        memp->flink->blink = memp->blink;
    if (memp->blink != NULL)
        memp->blink->flink = memp->flink;
    if (allocHead == memp)
        allocHead = memp->flink;
    free((char *) memp);
    return 0;
}

/*
 *--------------------------------------------------------------------
 *
 * Tcl_DbCkrealloc - debugging ckrealloc
 *
 *	Reallocate a chunk of memory by allocating a new one of the
 *	right size, copying the old data to the new location, and then
 *	freeing the old memory space, using all the memory checking
 *	features of this package.
 *
 *--------------------------------------------------------------------
 */
char *
Tcl_DbCkrealloc(ptr, size, file, line)
    char *ptr;
    unsigned int size;
    char *file;
    int line;
{
    char *new;
    unsigned int copySize;

    /*
     * See comment from Tcl_DbCkfree before you change the following
     * line.
     */

    struct mem_header *memp = (struct mem_header *)
	    (((unsigned long) ptr) - BODY_OFFSET);

    copySize = size;
    if (copySize > memp->length) {
	copySize = memp->length;
    }
    new = Tcl_DbCkalloc(size, file, line);
    memcpy((VOID *) new, (VOID *) ptr, (size_t) copySize);
    Tcl_DbCkfree(ptr, file, line);
    return(new);
}

/*
 *----------------------------------------------------------------------
 *
 * MemoryCmd --
 *     Implements the TCL memory command:
 *       memory info
 *       memory display
 *       break_on_malloc count
 *       trace_on_at_malloc count
 *       trace on|off
 *       validate on|off
 *
 * Results:
 *     Standard TCL results.
 *
 *----------------------------------------------------------------------
 */
	/* ARGSUSED */
static int
MemoryCmd (clientData, interp, argc, argv)
    ClientData  clientData;
    Tcl_Interp *interp;
    int         argc;
    char      **argv;
{
    char *fileName;
    Tcl_DString buffer;
    int result;

    if (argc < 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"",
		argv[0], " option [args..]\"", (char *) NULL);
	return TCL_ERROR;
    }

    if (strcmp(argv[1],"active") == 0) {
        if (argc != 3) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " active file\"", (char *) NULL);
	    return TCL_ERROR;
	}
	fileName = Tcl_TranslateFileName(interp, argv[2], &buffer);
	if (fileName == NULL) {
	    return TCL_ERROR;
	}
	result = Tcl_DumpActiveMemory (fileName);
	Tcl_DStringFree(&buffer);
	if (result != TCL_OK) {
	    Tcl_AppendResult(interp, "error accessing ", argv[2], 
		    (char *) NULL);
	    return TCL_ERROR;
	}
	return TCL_OK;
    }
    if (strcmp(argv[1],"break_on_malloc") == 0) {
        if (argc != 3) 
            goto argError;
        if (Tcl_GetInt(interp, argv[2], &break_on_malloc) != TCL_OK)
                return TCL_ERROR;
        return TCL_OK;
    }
    if (strcmp(argv[1],"info") == 0) {
        dump_memory_info(stdout);
        return TCL_OK;
    }
    if (strcmp(argv[1],"init") == 0) {
        if (argc != 3)
            goto bad_suboption;
        init_malloced_bodies = (strcmp(argv[2],"on") == 0);
        return TCL_OK;
    }
    if (strcmp(argv[1],"tag") == 0) {
	if (argc != 3) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		    " tag string\"", (char *) NULL);
	    return TCL_ERROR;
	}
	if ((curTagPtr != NULL) && (curTagPtr->refCount == 0)) {
	    free((char *) curTagPtr);
	}
	curTagPtr = (MemTag *) malloc(TAG_SIZE(strlen(argv[2])));
	curTagPtr->refCount = 0;
	strcpy(curTagPtr->string, argv[2]);
	return TCL_OK;
    }
    if (strcmp(argv[1],"trace") == 0) {
        if (argc != 3) 
            goto bad_suboption;
        alloc_tracing = (strcmp(argv[2],"on") == 0);
        return TCL_OK;
    }

    if (strcmp(argv[1],"trace_on_at_malloc") == 0) {
        if (argc != 3) 
            goto argError;
        if (Tcl_GetInt(interp, argv[2], &trace_on_at_malloc) != TCL_OK)
                return TCL_ERROR;
         return TCL_OK;
    }
    if (strcmp(argv[1],"validate") == 0) {
        if (argc != 3)
             goto bad_suboption;
        validate_memory = (strcmp(argv[2],"on") == 0);
        return TCL_OK;
    }

    Tcl_AppendResult(interp, "bad option \"", argv[1],
	    "\": should be active, break_on_malloc, info, init, ",
	    "tag, trace, trace_on_at_malloc, or validate", (char *) NULL);
    return TCL_ERROR;

argError:
    Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
	    " ", argv[1], " count\"", (char *) NULL);
    return TCL_ERROR;

bad_suboption:
    Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
	    " ", argv[1], " on|off\"", (char *) NULL);
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_InitMemory --
 *     Initialize the memory command.
 *
 *----------------------------------------------------------------------
 */
void
Tcl_InitMemory(interp)
    Tcl_Interp *interp;
{
Tcl_CreateCommand (interp, "memory", MemoryCmd, (ClientData) NULL, 
                  (Tcl_CmdDeleteProc *) NULL);
}

#else


/*
 *----------------------------------------------------------------------
 *
 * Tcl_Ckalloc --
 *     Interface to malloc when TCL_MEM_DEBUG is disabled.  It does check
 *     that memory was actually allocated.
 *
 *----------------------------------------------------------------------
 */
VOID *
Tcl_Ckalloc (size)
    unsigned int size;
{
        char *result;

        result = malloc(size);
        if (result == NULL) 
                panic("unable to alloc %d bytes", size);
        return result;
}


char *
Tcl_DbCkalloc(size, file, line)
    unsigned int size;
    char        *file;
    int          line;
{
    char *result;

    result = (char *) malloc(size);

    if (result == NULL) {
        fflush(stdout);
        panic("unable to alloc %d bytes, %s line %d", size, file, 
              line);
    }
    return result;
}

char *
Tcl_DbCkrealloc(ptr, size, file, line)
    char *ptr;
    unsigned int size;
    char *file;
    int line;
{
    char *result;

    result = (char *) realloc(ptr, size);

    if (result == NULL) {
        fflush(stdout);
        panic("unable to realloc %d bytes, %s line %d", size, file, 
              line);
    }
    return result;
}
/*
 *----------------------------------------------------------------------
 *
 * TckCkfree --
 *     Interface to free when TCL_MEM_DEBUG is disabled.  Done here rather
 *     in the macro to keep some modules from being compiled with 
 *     TCL_MEM_DEBUG enabled and some with it disabled.
 *
 *----------------------------------------------------------------------
 */
void
Tcl_Ckfree (ptr)
    char *ptr;
{
        free (ptr);
}

int
Tcl_DbCkfree(ptr, file, line)
    char *  ptr;
    char     *file;
    int       line;
{
    free (ptr);
    return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_InitMemory --
 *     Dummy initialization for memory command, which is only available 
 *     if TCL_MEM_DEBUG is on.
 *
 *----------------------------------------------------------------------
 */
	/* ARGSUSED */
void
Tcl_InitMemory(interp)
    Tcl_Interp *interp;
{
}

#undef Tcl_DumpActiveMemory
#undef Tcl_ValidateAllMemory

extern int		Tcl_DumpActiveMemory _ANSI_ARGS_((char *fileName));
extern void		Tcl_ValidateAllMemory _ANSI_ARGS_((char *file,
			    int line));

int
Tcl_DumpActiveMemory (fileName)
    char *fileName;
{
    return TCL_OK;
}

void
Tcl_ValidateAllMemory (file, line)
    char  *file;
    int    line;
{
}

#endif
