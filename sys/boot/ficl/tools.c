/*******************************************************************
** t o o l s . c
** Forth Inspired Command Language - programming tools
** Author: John Sadler (john_sadler@alum.mit.edu)
** Created: 20 June 2000
** $Id: tools.c,v 1.4 2001-04-26 21:41:24-07 jsadler Exp jsadler $
*******************************************************************/
/*
** NOTES:
** SEE needs information about the addresses of functions that
** are the CFAs of colon definitions, constants, variables, DOES>
** words, and so on. It gets this information from a table and supporting
** functions in words.c.
** colonParen doDoes createParen variableParen userParen constantParen
**
** Step and break debugger for Ficl
** debug  ( xt -- )   Start debugging an xt
** Set a breakpoint
** Specify breakpoint default action
*/
/*
** Copyright (c) 1997-2001 John Sadler (john_sadler@alum.mit.edu)
** All rights reserved.
**
** Get the latest Ficl release at http://ficl.sourceforge.net
**
** L I C E N S E  and  D I S C L A I M E R
** 
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
** ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
** IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
** ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
** FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
** DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
** OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
** HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
** LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
** OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
** SUCH DAMAGE.
**
** I am interested in hearing from anyone who uses ficl. If you have
** a problem, a success story, a defect, an enhancement request, or
** if you would like to contribute to the ficl release, please send
** contact me by email at the address above.
**
** $Id: tools.c,v 1.4 2001-04-26 21:41:24-07 jsadler Exp jsadler $
*/

/* $FreeBSD$ */

#ifdef TESTMAIN
#include <stdlib.h>
#include <stdio.h>          /* sprintf */
#include <ctype.h>
#else
#include <stand.h>
#endif
#include <string.h>
#include "ficl.h"


#if 0
/*
** nBREAKPOINTS sizes the breakpoint array. One breakpoint (bp 0) is reserved
** for the STEP command. The rest are user programmable. 
*/
#define nBREAKPOINTS 32
#endif

/*
** BREAKPOINT record.
** origXT - if NULL, this breakpoint is unused. Otherwise it stores the xt 
** that the breakpoint overwrote. This is restored to the dictionary when the
** BP executes or gets cleared
** address - the location of the breakpoint (address of the instruction that
**           has been replaced with the breakpoint trap
** origXT  - The original contents of the location with the breakpoint
** Note: address is NULL when this breakpoint is empty
*/
typedef struct breakpoint
{
    void      *address;
    FICL_WORD *origXT;
} BREAKPOINT;

static BREAKPOINT bpStep = {NULL, NULL};

/*
** vmSetBreak - set a breakpoint at the current value of IP by
** storing that address in a BREAKPOINT record
*/
static void vmSetBreak(FICL_VM *pVM, BREAKPOINT *pBP)
{
    FICL_WORD *pStep = ficlLookup("step-break");
    assert(pStep);
    pBP->address = pVM->ip;
    pBP->origXT = *pVM->ip;
    *pVM->ip = pStep;
}


/*
** isAFiclWord
** Vet a candidate pointer carefully to make sure
** it's not some chunk o' inline data...
** It has to have a name, and it has to look
** like it's in the dictionary address range.
** NOTE: this excludes :noname words!
*/
int isAFiclWord(FICL_WORD *pFW)
{
    FICL_DICT *pd  = ficlGetDict();

    if (!dictIncludes(pd, pFW))
       return 0;

    if (!dictIncludes(pd, pFW->name))
        return 0;

    return ((pFW->nName > 0) && (pFW->name[pFW->nName] == '\0'));
}


static int isPrimitive(FICL_WORD *pFW)
{
    WORDKIND wk = ficlWordClassify(pFW);
    return ((wk != COLON) && (wk != DOES));
}


/**************************************************************************
                        s e e 
** TOOLS ( "<spaces>name" -- )
** Display a human-readable representation of the named word's definition.
** The source of the representation (object-code decompilation, source
** block, etc.) and the particular form of the display is implementation
** defined. 
** NOTE: these funcs come late in the file because they reference all
** of the word-builder funcs without declaring them again. Call me lazy.
**************************************************************************/
/*
** seeColon (for proctologists only)
** Walks a colon definition, decompiling
** on the fly. Knows about primitive control structures.
*/
static void seeColon(FICL_VM *pVM, CELL *pc)
{
    static FICL_WORD *pSemiParen = NULL;

    if (!pSemiParen)
        pSemiParen = ficlLookup("(;)");
    assert(pSemiParen);

    for (; pc->p != pSemiParen; pc++)
    {
        FICL_WORD *pFW = (FICL_WORD *)(pc->p);

        if (isAFiclWord(pFW))
        {
            WORDKIND kind = ficlWordClassify(pFW);
            CELL c;

            switch (kind)
            {
            case LITERAL:
                c = *++pc;
                if (isAFiclWord(c.p))
                {
                    FICL_WORD *pLit = (FICL_WORD *)c.p;
                    sprintf(pVM->pad, "    literal %.*s (%#lx)", 
                        pLit->nName, pLit->name, c.u);
                }
                else
                    sprintf(pVM->pad, "    literal %ld (%#lx)", c.i, c.u);
                break;
            case STRINGLIT:
                {
                    FICL_STRING *sp = (FICL_STRING *)(void *)++pc;
                    pc = (CELL *)alignPtr(sp->text + sp->count + 1) - 1;
                    sprintf(pVM->pad, "    s\" %.*s\"", sp->count, sp->text);
                }
                break;
            case IF:
                c = *++pc;
                if (c.i > 0)
                    sprintf(pVM->pad, "    if / while (branch rel %ld)", c.i);
                else
                    sprintf(pVM->pad, "    until (branch rel %ld)", c.i);
                break;
            case BRANCH:
                c = *++pc;
                if (c.i > 0)
                    sprintf(pVM->pad, "    else (branch rel %ld)", c.i);
                else
                    sprintf(pVM->pad, "    repeat (branch rel %ld)", c.i);
                break;

            case QDO:
                c = *++pc;
                sprintf(pVM->pad, "    ?do (leave abs %#lx)", c.u);
                break;
            case DO:
                c = *++pc;
                sprintf(pVM->pad, "    do (leave abs %#lx)", c.u);
                break;
            case LOOP:
                c = *++pc;
                sprintf(pVM->pad, "    loop (branch rel %#ld)", c.i);
                break;
            case PLOOP:
                c = *++pc;
                sprintf(pVM->pad, "    +loop (branch rel %#ld)", c.i);
                break;
            default:
                sprintf(pVM->pad, "    %.*s", pFW->nName, pFW->name);
                break;
            }
 
            vmTextOut(pVM, pVM->pad, 1);
        }
        else /* probably not a word - punt and print value */
        {
            sprintf(pVM->pad, "    %ld (%#lx)", pc->i, pc->u);
            vmTextOut(pVM, pVM->pad, 1);
        }
    }

    vmTextOut(pVM, ";", 1);
}

/*
** Here's the outer part of the decompiler. It's 
** just a big nested conditional that checks the
** CFA of the word to decompile for each kind of
** known word-builder code, and tries to do 
** something appropriate. If the CFA is not recognized,
** just indicate that it is a primitive.
*/
static void seeXT(FICL_VM *pVM)
{
    FICL_WORD *pFW;
    WORDKIND kind;

    pFW = (FICL_WORD *)stackPopPtr(pVM->pStack);
    kind = ficlWordClassify(pFW);

    switch (kind)
    {
    case COLON:
        sprintf(pVM->pad, ": %.*s", pFW->nName, pFW->name);
        vmTextOut(pVM, pVM->pad, 1);
        seeColon(pVM, pFW->param);
        break;

    case DOES:
        vmTextOut(pVM, "does>", 1);
        seeColon(pVM, (CELL *)pFW->param->p);
        break;

    case CREATE:
        vmTextOut(pVM, "create", 1);
        break;

    case VARIABLE:
        sprintf(pVM->pad, "variable = %ld (%#lx)", pFW->param->i, pFW->param->u);
        vmTextOut(pVM, pVM->pad, 1);
        break;

    case USER:
        sprintf(pVM->pad, "user variable %ld (%#lx)", pFW->param->i, pFW->param->u);
        vmTextOut(pVM, pVM->pad, 1);
        break;

    case CONSTANT:
        sprintf(pVM->pad, "constant = %ld (%#lx)", pFW->param->i, pFW->param->u);
        vmTextOut(pVM, pVM->pad, 1);

    default:
        vmTextOut(pVM, "primitive", 1);
        break;
    }

    if (pFW->flags & FW_IMMEDIATE)
    {
        vmTextOut(pVM, "immediate", 1);
    }

    if (pFW->flags & FW_COMPILE)
    {
        vmTextOut(pVM, "compile-only", 1);
    }

    return;
}


static void see(FICL_VM *pVM)
{
    ficlTick(pVM);
    seeXT(pVM);
    return;
}


/**************************************************************************
                        f i c l D e b u g X T
** debug  ( xt -- )
** Given an xt of a colon definition or a word defined by DOES>, set the
** VM up to debug the word: push IP, set the xt as the next thing to execute,
** set a breakpoint at its first instruction, and run to the breakpoint.
** Note: the semantics of this word are equivalent to "step in"
**************************************************************************/
void ficlDebugXT(FICL_VM *pVM)
{
    FICL_WORD *xt    = stackPopPtr(pVM->pStack);
    WORDKIND   wk    = ficlWordClassify(xt);
    FICL_WORD *pStep = ficlLookup("step-break");

    assert(pStep);

    stackPushPtr(pVM->pStack, xt);
    seeXT(pVM);

    switch (wk)
    {
    case COLON:
    case DOES:
        /*
        ** Run the colon code and set a breakpoint at the next instruction
        */
        vmExecute(pVM, xt);
        bpStep.address = pVM->ip;
        bpStep.origXT = *pVM->ip;
        *pVM->ip = pStep;
        break;

    default:
        vmExecute(pVM, xt);
        break;
    }

    return;
}


/**************************************************************************
                        s t e p I n
** FICL 
** Execute the next instruction, stepping into it if it's a colon definition 
** or a does> word. This is the easy kind of step.
**************************************************************************/
void stepIn(FICL_VM *pVM)
{
    /*
    ** Do one step of the inner loop
    */
    { 
        M_VM_STEP(pVM) 
    }

    /*
    ** Now set a breakpoint at the next instruction
    */
    vmSetBreak(pVM, &bpStep);
    
    return;
}


/**************************************************************************
                        s t e p O v e r
** FICL 
** Execute the next instruction atomically. This requires some insight into 
** the memory layout of compiled code. Set a breakpoint at the next instruction
** in this word, and run until we hit it
**************************************************************************/
void stepOver(FICL_VM *pVM)
{
    FICL_WORD *pFW;
    WORDKIND kind;
    FICL_WORD *pStep = ficlLookup("step-break");
    assert(pStep);

    pFW = *pVM->ip;
    kind = ficlWordClassify(pFW);

    switch (kind)
    {
    case COLON: 
    case DOES:
        /*
        ** assume that the next cell holds an instruction 
        ** set a breakpoint there and return to the inner interp
        */
        bpStep.address = pVM->ip + 1;
        bpStep.origXT =  pVM->ip[1];
        pVM->ip[1] = pStep;
        break;

    default:
        stepIn(pVM);
        break;
    }

    return;
}


/**************************************************************************
                        s t e p - b r e a k
** FICL
** Handles breakpoints for stepped execution.
** Upon entry, bpStep contains the address and replaced instruction
** of the current breakpoint.
** Clear the breakpoint
** Get a command from the console. 
** i (step in) - execute the current instruction and set a new breakpoint 
**    at the IP
** o (step over) - execute the current instruction to completion and set
**    a new breakpoint at the IP
** g (go) - execute the current instruction and exit
** q (quit) - abort current word
** b (toggle breakpoint)
**************************************************************************/
void stepBreak(FICL_VM *pVM)
{
    STRINGINFO si;
    FICL_WORD *pFW;
    FICL_WORD *pOnStep;

    if (!pVM->fRestart)
    {

        assert(bpStep.address != NULL);
        /*
        ** Clear the breakpoint that caused me to run
        ** Restore the original instruction at the breakpoint, 
        ** and restore the IP
        */
        assert(bpStep.address);
        assert(bpStep.origXT);

        pVM->ip = (IPTYPE)bpStep.address;
        *pVM->ip = bpStep.origXT;

        /*
        ** If there's an onStep, do it
        */
        pOnStep = ficlLookup("on-step");
        if (pOnStep)
            ficlExecXT(pVM, pOnStep);

        /*
        ** Print the name of the next instruction
        */
        pFW = bpStep.origXT;
        sprintf(pVM->pad, "next: %.*s", pFW->nName, pFW->name);
        if (isPrimitive(pFW))
        {
            strcat(pVM->pad, " primitive");
        }

        vmTextOut(pVM, pVM->pad, 1);
    }
    else
    {
        pVM->fRestart = 0;
    }

    si = vmGetWord(pVM);

    if      (!strincmp(si.cp, "i", si.count))
    {
        stepIn(pVM);
    }
    else if (!strincmp(si.cp, "g", si.count))
    {
        return;
    }
    else if (!strincmp(si.cp, "o", si.count))
    {
        stepOver(pVM);
    }
    else if (!strincmp(si.cp, "q", si.count))
    {
        vmThrow(pVM, VM_ABORT);
    }
    else
    {
        vmTextOut(pVM, "i -- step In", 1);
        vmTextOut(pVM, "o -- step Over", 1);
        vmTextOut(pVM, "g -- Go (execute to completion)", 1);
        vmTextOut(pVM, "q -- Quit (stop debugging and abort)", 1);
        vmTextOut(pVM, "x -- eXecute a single word", 1);
        vmThrow(pVM, VM_RESTART);
    }

    return;
}


/**************************************************************************
                        b y e
** TOOLS
** Signal the system to shut down - this causes ficlExec to return
** VM_USEREXIT. The rest is up to you.
**************************************************************************/
static void bye(FICL_VM *pVM)
{
    vmThrow(pVM, VM_USEREXIT);
    return;
}


/**************************************************************************
                        d i s p l a y S t a c k
** TOOLS 
** Display the parameter stack (code for ".s")
**************************************************************************/
static void displayStack(FICL_VM *pVM)
{
    int d = stackDepth(pVM->pStack);
    int i;
    CELL *pCell;

    vmCheckStack(pVM, 0, 0);

    if (d == 0)
        vmTextOut(pVM, "(Stack Empty) ", 0);
    else
    {
        pCell = pVM->pStack->base;
        for (i = 0; i < d; i++)
        {
            vmTextOut(pVM, ltoa((*pCell++).i, pVM->pad, pVM->base), 0);
            vmTextOut(pVM, " ", 0);
        }
    }
}


static void displayRStack(FICL_VM *pVM)
{
    int d = stackDepth(pVM->rStack);
    int i;
    CELL *pCell;

    vmTextOut(pVM, "Return Stack: ", 0);
    if (d == 0)
        vmTextOut(pVM, "Empty ", 0);
    else
    {
        pCell = pVM->rStack->base;
        for (i = 0; i < d; i++)
        {
            vmTextOut(pVM, ultoa((*pCell++).i, pVM->pad, 16), 0);
            vmTextOut(pVM, " ", 0);
        }
    }
}


/**************************************************************************
                        f o r g e t - w i d
** 
**************************************************************************/
static void forgetWid(FICL_VM *pVM)
{
    FICL_DICT *pDict = ficlGetDict();
    FICL_HASH *pHash;

    pHash = (FICL_HASH *)stackPopPtr(pVM->pStack);
    hashForget(pHash, pDict->here);

    return;
}


/**************************************************************************
                        f o r g e t
** TOOLS EXT  ( "<spaces>name" -- )
** Skip leading space delimiters. Parse name delimited by a space.
** Find name, then delete name from the dictionary along with all
** words added to the dictionary after name. An ambiguous
** condition exists if name cannot be found. 
** 
** If the Search-Order word set is present, FORGET searches the
** compilation word list. An ambiguous condition exists if the
** compilation word list is deleted. 
**************************************************************************/
static void forget(FICL_VM *pVM)
{
    void *where;
    FICL_DICT *pDict = ficlGetDict();
    FICL_HASH *pHash = pDict->pCompile;

    ficlTick(pVM);
    where = ((FICL_WORD *)stackPopPtr(pVM->pStack))->name;
    hashForget(pHash, where);
    pDict->here = PTRtoCELL where;

    return;
}


/**************************************************************************
                        l i s t W o r d s
** 
**************************************************************************/
#define nCOLWIDTH 8
static void listWords(FICL_VM *pVM)
{
    FICL_DICT *dp = ficlGetDict();
    FICL_HASH *pHash = dp->pSearch[dp->nLists - 1];
    FICL_WORD *wp;
    int nChars = 0;
    int len;
    int y = 0;
    unsigned i;
    int nWords = 0;
    char *cp;
    char *pPad = pVM->pad;

    for (i = 0; i < pHash->size; i++)
    {
        for (wp = pHash->table[i]; wp != NULL; wp = wp->link, nWords++)
        {
            if (wp->nName == 0) /* ignore :noname defs */
                continue;

            cp = wp->name;
            nChars += sprintf(pPad + nChars, "%s", cp);

            if (nChars > 70)
            {
                pPad[nChars] = '\0';
                nChars = 0;
                y++;
                if(y>23) {
                        y=0;
                        vmTextOut(pVM, "--- Press Enter to continue ---",0);
                        getchar();
                        vmTextOut(pVM,"\r",0);
                }
                vmTextOut(pVM, pPad, 1);
            }
            else
            {
                len = nCOLWIDTH - nChars % nCOLWIDTH;
                while (len-- > 0)
                    pPad[nChars++] = ' ';
            }

            if (nChars > 70)
            {
                pPad[nChars] = '\0';
                nChars = 0;
                y++;
                if(y>23) {
                        y=0;
                        vmTextOut(pVM, "--- Press Enter to continue ---",0);
                        getchar();
                        vmTextOut(pVM,"\r",0);
                }
                vmTextOut(pVM, pPad, 1);
            }
        }
    }

    if (nChars > 0)
    {
        pPad[nChars] = '\0';
        nChars = 0;
        vmTextOut(pVM, pPad, 1);
    }

    sprintf(pVM->pad, "Dictionary: %d words, %ld cells used of %u total", 
        nWords, (long) (dp->here - dp->dict), dp->size);
    vmTextOut(pVM, pVM->pad, 1);
    return;
}


/**************************************************************************
                        l i s t E n v
** Print symbols defined in the environment 
**************************************************************************/
static void listEnv(FICL_VM *pVM)
{
    FICL_DICT *dp = ficlGetEnv();
    FICL_HASH *pHash = dp->pForthWords;
    FICL_WORD *wp;
    unsigned i;
    int nWords = 0;

    for (i = 0; i < pHash->size; i++)
    {
        for (wp = pHash->table[i]; wp != NULL; wp = wp->link, nWords++)
        {
            vmTextOut(pVM, wp->name, 1);
        }
    }

    sprintf(pVM->pad, "Environment: %d words, %ld cells used of %u total", 
        nWords, (long) (dp->here - dp->dict), dp->size);
    vmTextOut(pVM, pVM->pad, 1);
    return;
}


/**************************************************************************
                        e n v C o n s t a n t
** Ficl interface to ficlSetEnv and ficlSetEnvD - allow ficl code to set
** environment constants...
**************************************************************************/
static void envConstant(FICL_VM *pVM)
{
    unsigned value;

#if FICL_ROBUST > 1
    vmCheckStack(pVM, 1, 0);
#endif

    vmGetWordToPad(pVM);
    value = POPUNS();
    ficlSetEnv(pVM->pad, (FICL_UNS)value);
    return;
}

static void env2Constant(FICL_VM *pVM)
{
    unsigned v1, v2;

#if FICL_ROBUST > 1
    vmCheckStack(pVM, 2, 0);
#endif

    vmGetWordToPad(pVM);
    v2 = POPUNS();
    v1 = POPUNS();
    ficlSetEnvD(pVM->pad, v1, v2);
    return;
}


/**************************************************************************
                        f i c l C o m p i l e T o o l s
** Builds wordset for debugger and TOOLS optional word set
**************************************************************************/

void ficlCompileTools(FICL_SYSTEM *pSys)
{
    FICL_DICT *dp = pSys->dp;
    assert (dp);

    /*
    ** TOOLS and TOOLS EXT
    */
    dictAppendWord(dp, ".r",        displayRStack,  FW_DEFAULT); /* guy carver */
    dictAppendWord(dp, ".s",        displayStack,   FW_DEFAULT);
    dictAppendWord(dp, "bye",       bye,            FW_DEFAULT);
    dictAppendWord(dp, "forget",    forget,         FW_DEFAULT);
    dictAppendWord(dp, "see",       see,            FW_DEFAULT);
    dictAppendWord(dp, "words",     listWords,      FW_DEFAULT);

    /*
    ** Set TOOLS environment query values
    */
    ficlSetEnv("tools",            FICL_TRUE);
    ficlSetEnv("tools-ext",        FICL_FALSE);

    /*
    ** Ficl extras
    */
    dictAppendWord(dp, ".env",      listEnv,        FW_DEFAULT);
    dictAppendWord(dp, "env-constant",
                                    envConstant,    FW_DEFAULT);
    dictAppendWord(dp, "env-2constant",
                                    env2Constant,   FW_DEFAULT);
    dictAppendWord(dp, "debug-xt",  ficlDebugXT,    FW_DEFAULT);
    dictAppendWord(dp, "parse-order",
                                    ficlListParseSteps,
                                                    FW_DEFAULT);
    dictAppendWord(dp, "step-break",stepBreak,      FW_DEFAULT);
    dictAppendWord(dp, "forget-wid",forgetWid,      FW_DEFAULT);
    dictAppendWord(dp, "see-xt",    seeXT,          FW_DEFAULT);
    dictAppendWord(dp, ".r",        displayRStack,  FW_DEFAULT);

    return;
}

