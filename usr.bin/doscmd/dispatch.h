/*
** Copyright (c) 1996
**	Michael Smith.  All rights reserved.
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
** THIS SOFTWARE IS PROVIDED BY Michael Smith ``AS IS'' AND
** ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
** IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
** ARE DISCLAIMED.  IN NO EVENT SHALL Michael Smith BE LIABLE
** FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
** DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
** OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
** HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
** LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
** OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
** SUCH DAMAGE.
**
** $FreeBSD$
*/

/*
** Interrupt dispatcher assistants.
*/


/*
** Declare a static, initialised array of these, with one
** entry per function/subfunction.
**
** The last element should be a dummy with a 'func' of -1
*/
struct intfunc_table 
{
    int		func;				/* interrupt function number */
    int		subfunc;			/* subfunction number */
    int		(* handler)(regcontext_t *REGS);/* handling function */
    const char	*desc;				/* textual description */
};
#define IFT_NOSUBFUNC	-1


/*
** Declare a static array of 256 integers to use as a fast lookup 
** into the table of handlers.
**
** Call this function to initialise the lookup.  Note that the table
** must be arranged with all handlers for a given function together, and
** that the handler listed with IFT_NOSUBFUNC should be last.
*/
static inline void
intfunc_init(struct intfunc_table table[], int idx[])
{
    int		hn;

    for (hn = 0; hn < 256; hn++)		/* initialise all no-handler state */
	idx[hn] = -1;				/* default to no handler */

    for (hn = 0; table[hn].func >= 0; hn++)	/* walk list of handlers and add references */
	if (idx[table[hn].func] == -1 )	/* reference first handler */
	    idx[table[hn].func] = hn;
}

/*
** Call this to get an index matching the function/subfunction 
** described by (sc), or -1 if none exist
*/
static inline int
intfunc_find(struct intfunc_table table[], int idx[], int func, int subfunc)
{
    int	ent = idx[func];				/* look for handler */
    
    while ((ent >= 0) &&				/* scan entries for function */
	   (table[ent].func == func)) {

	if ((table[ent].subfunc == IFT_NOSUBFUNC) ||	/* handles all */
	    (table[ent].subfunc == subfunc)) {		/* handles this one */
	    return(ent);
	}
	ent++;
    }
    return(-1);
}

/*
** A slower lookup for a set of function handlers, but one that requires
** no initialisation calls.
** Again, handlers with IFT_NOSUBFUNC should be listed after any with
** specific subfunction values.
*/
static inline int
intfunc_search(struct intfunc_table table[], int func, int subfunc)
{
    int		ent;

    for (ent = 0; table[ent].func >= 0; ent++)
	if ((table[ent].func == func) &&	/* matches required function */
	    ((table[ent].subfunc == IFT_NOSUBFUNC) || table[ent].subfunc == subfunc))
	    return(ent);
    return(-1);
}

	     
