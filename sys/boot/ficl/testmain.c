/*
** stub main for testing FICL under Win32
** $Id: testmain.c,v 1.6 2000-06-17 07:43:50-07 jsadler Exp jsadler $
*/

/* $FreeBSD$ */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "ficl.h"

/*
** Ficl interface to getcwd
** Prints the current working directory using the VM's 
** textOut method...
*/
static void ficlGetCWD(FICL_VM *pVM)
{
    char *cp;

   cp = getcwd(NULL, 80);
    vmTextOut(pVM, cp, 1);
    free(cp);
    return;
}

/*
** Ficl interface to chdir
** Gets a newline (or NULL) delimited string from the input
** and feeds it to chdir()
** Example:
**    cd c:\tmp
*/
static void ficlChDir(FICL_VM *pVM)
{
    FICL_STRING *pFS = (FICL_STRING *)pVM->pad;
    vmGetString(pVM, pFS, '\n');
    if (pFS->count > 0)
    {
       int err = chdir(pFS->text);
       if (err)
        {
            vmTextOut(pVM, "Error: path not found", 1);
            vmThrow(pVM, VM_QUIT);
        }
    }
    else
    {
        vmTextOut(pVM, "Warning (chdir): nothing happened", 1);
    }
    return;
}

/*
** Ficl interface to system (ANSI)
** Gets a newline (or NULL) delimited string from the input
** and feeds it to system()
** Example:
**    system del *.*
**    \ ouch!
*/
static void ficlSystem(FICL_VM *pVM)
{
    FICL_STRING *pFS = (FICL_STRING *)pVM->pad;

    vmGetString(pVM, pFS, '\n');
    if (pFS->count > 0)
    {
        int err = system(pFS->text);
        if (err)
        {
            sprintf(pVM->pad, "System call returned %d", err);
            vmTextOut(pVM, pVM->pad, 1);
            vmThrow(pVM, VM_QUIT);
        }
    }
    else
    {
        vmTextOut(pVM, "Warning (system): nothing happened", 1);
    }
    return;
}

/*
** Ficl add-in to load a text file and execute it...
** Cheesy, but illustrative.
** Line oriented... filename is newline (or NULL) delimited.
** Example:
**    load test.ficl
*/
#define nLINEBUF 256
static void ficlLoad(FICL_VM *pVM)
{
    char    cp[nLINEBUF];
    char    filename[nLINEBUF];
    FICL_STRING *pFilename = (FICL_STRING *)filename;
    int     nLine = 0;
    FILE   *fp;
    int     result;
    CELL    id;
    struct stat buf;


    vmGetString(pVM, pFilename, '\n');

    if (pFilename->count <= 0)
    {
        vmTextOut(pVM, "Warning (load): nothing happened", 1);
        return;
    }

    /*
    ** get the file's size and make sure it exists 
    */
    result = stat( pFilename->text, &buf );

    if (result != 0)
    {
        vmTextOut(pVM, "Unable to stat file: ", 0);
        vmTextOut(pVM, pFilename->text, 1);
        vmThrow(pVM, VM_QUIT);
    }

    fp = fopen(pFilename->text, "r");
    if (!fp)
    {
        vmTextOut(pVM, "Unable to open file ", 0);
        vmTextOut(pVM, pFilename->text, 1);
        vmThrow(pVM, VM_QUIT);
    }

    id = pVM->sourceID;
    pVM->sourceID.p = (void *)fp;

    /* feed each line to ficlExec */
    while (fgets(cp, nLINEBUF, fp))
    {
        int len = strlen(cp) - 1;

        nLine++;
        if (len <= 0)
            continue;

        result = ficlExecC(pVM, cp, len);
        if (result != VM_QUIT && result != VM_USEREXIT && result != VM_OUTOFTEXT )
        {
            pVM->sourceID = id;
            fclose(fp);
            vmThrowErr(pVM, "Error loading file <%s> line %d", pFilename->text, nLine);
            break; 
        }
    }
    /*
    ** Pass an empty line with SOURCE-ID == -1 to flush
    ** any pending REFILLs (as required by FILE wordset)
    */
    pVM->sourceID.i = -1;
    ficlExec(pVM, "");

    pVM->sourceID = id;
    fclose(fp);

    return;
}

/*
** Dump a tab delimited file that summarizes the contents of the
** dictionary hash table by hashcode...
*/
static void spewHash(FICL_VM *pVM)
{
    FICL_HASH *pHash = ficlGetDict()->pForthWords;
    FICL_WORD *pFW;
    FILE *pOut;
    unsigned i;
    unsigned nHash = pHash->size;

    if (!vmGetWordToPad(pVM))
        vmThrow(pVM, VM_OUTOFTEXT);

    pOut = fopen(pVM->pad, "w");
    if (!pOut)
    {
        vmTextOut(pVM, "unable to open file", 1);
        return;
    }

    for (i=0; i < nHash; i++)
    {
        int n = 0;

        pFW = pHash->table[i];
        while (pFW)
        {
            n++;
            pFW = pFW->link;
        }

        fprintf(pOut, "%d\t%d", i, n);

        pFW = pHash->table[i];
        while (pFW)
        {
            fprintf(pOut, "\t%s", pFW->name);
            pFW = pFW->link;
        }

        fprintf(pOut, "\n");
    }

    fclose(pOut);
    return;
}

static void ficlBreak(FICL_VM *pVM)
{
    pVM->state = pVM->state;
    return;
}

static void ficlClock(FICL_VM *pVM)
{
    clock_t now = clock();
    stackPushUNS(pVM->pStack, (FICL_UNS)now);
    return;
}

static void clocksPerSec(FICL_VM *pVM)
{
    stackPushUNS(pVM->pStack, CLOCKS_PER_SEC);
    return;
}


static void execxt(FICL_VM *pVM)
{
    FICL_WORD *pFW;
#if FICL_ROBUST > 1
    vmCheckStack(pVM, 1, 0);
#endif

    pFW = stackPopPtr(pVM->pStack);
    ficlExecXT(pVM, pFW);

    return;
}


void buildTestInterface(void)
{
    ficlBuild("break",    ficlBreak,    FW_DEFAULT);
    ficlBuild("clock",    ficlClock,    FW_DEFAULT);
    ficlBuild("cd",       ficlChDir,    FW_DEFAULT);
    ficlBuild("execxt",   execxt,       FW_DEFAULT);
    ficlBuild("load",     ficlLoad,     FW_DEFAULT);
    ficlBuild("pwd",      ficlGetCWD,   FW_DEFAULT);
    ficlBuild("system",   ficlSystem,   FW_DEFAULT);
    ficlBuild("spewhash", spewHash,     FW_DEFAULT);
    ficlBuild("clocks/sec", 
                          clocksPerSec, FW_DEFAULT);

    return;
}


int main(int argc, char **argv)
{
    char in[256];
    FICL_VM *pVM;

    ficlInitSystem(10000);
    buildTestInterface();
    pVM = ficlNewVM();

    ficlExec(pVM, ".ver .( " __DATE__ " ) cr quit");

    /*
    ** load file from cmd line...
    */
    if (argc  > 1)
    {
        sprintf(in, ".( loading %s ) cr load %s\n cr", argv[1], argv[1]);
        ficlExec(pVM, in);
    }

    for (;;)
    {
        int ret;
        if (fgets(in, sizeof(in) - 1, stdin) == NULL)
	    break;
        ret = ficlExec(pVM, in);
        if (ret == VM_USEREXIT)
        {
            ficlTermSystem();
            break;
        }
    }

    return 0;
}

