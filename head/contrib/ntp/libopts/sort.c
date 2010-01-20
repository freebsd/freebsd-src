
/*
 *  sort.c  $Id: sort.c,v 4.10 2007/04/28 22:19:23 bkorb Exp $
 * Time-stamp:      "2006-10-18 11:29:04 bkorb"
 *
 *  This module implements argument sorting.
 */

/*
 *  Automated Options copyright 1992-2007 Bruce Korb
 *
 *  Automated Options is free software.
 *  You may redistribute it and/or modify it under the terms of the
 *  GNU General Public License, as published by the Free Software
 *  Foundation; either version 2, or (at your option) any later version.
 *
 *  Automated Options is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with Automated Options.  See the file "COPYING".  If not,
 *  write to:  The Free Software Foundation, Inc.,
 *             51 Franklin Street, Fifth Floor,
 *             Boston, MA  02110-1301, USA.
 *
 * As a special exception, Bruce Korb gives permission for additional
 * uses of the text contained in his release of AutoOpts.
 *
 * The exception is that, if you link the AutoOpts library with other
 * files to produce an executable, this does not by itself cause the
 * resulting executable to be covered by the GNU General Public License.
 * Your use of that executable is in no way restricted on account of
 * linking the AutoOpts library code into it.
 *
 * This exception does not however invalidate any other reasons why
 * the executable file might be covered by the GNU General Public License.
 *
 * This exception applies only to the code released by Bruce Korb under
 * the name AutoOpts.  If you copy code from other sources under the
 * General Public License into a copy of AutoOpts, as the General Public
 * License permits, the exception does not apply to the code that you add
 * in this way.  To avoid misleading anyone as to the status of such
 * modified files, you must delete this exception notice from them.
 *
 * If you write modifications of your own for AutoOpts, it is your choice
 * whether to permit this exception to apply to your modifications.
 * If you do not wish that, delete this exception notice.
 */

/* = = = START-STATIC-FORWARD = = = */
/* static forward declarations maintained by :mkfwd */
static tSuccess
mustHandleArg( tOptions* pOpts, char* pzArg, tOptState* pOS,
               char** ppzOpts, int* pOptsIdx );

static tSuccess
mayHandleArg( tOptions* pOpts, char* pzArg, tOptState* pOS,
              char** ppzOpts, int* pOptsIdx );

static tSuccess
checkShortOpts( tOptions* pOpts, char* pzArg, tOptState* pOS,
                char** ppzOpts, int* pOptsIdx );
/* = = = END-STATIC-FORWARD = = = */

/*
 *  "mustHandleArg" and "mayHandleArg" are really similar.  The biggest
 *  difference is that "may" will consume the next argument only if it
 *  does not start with a hyphen and "must" will consume it, hyphen or not.
 */
static tSuccess
mustHandleArg( tOptions* pOpts, char* pzArg, tOptState* pOS,
               char** ppzOpts, int* pOptsIdx )
{
    /*
     *  An option argument is required.  Long options can either have
     *  a separate command line argument, or an argument attached by
     *  the '=' character.  Figure out which.
     */
    switch (pOS->optType) {
    case TOPT_SHORT:
        /*
         *  See if an arg string follows the flag character.  If not,
         *  the next arg must be the option argument.
         */
        if (*pzArg != NUL)
            return SUCCESS;
        break;

    case TOPT_LONG:
        /*
         *  See if an arg string has already been assigned (glued on
         *  with an `=' character).  If not, the next is the opt arg.
         */
        if (pOS->pzOptArg != NULL)
            return SUCCESS;
        break;

    default:
        return FAILURE;
    }
    if (pOpts->curOptIdx >= pOpts->origArgCt)
        return FAILURE;

    ppzOpts[ (*pOptsIdx)++ ] = pOpts->origArgVect[ (pOpts->curOptIdx)++ ];
    return SUCCESS;
}

static tSuccess
mayHandleArg( tOptions* pOpts, char* pzArg, tOptState* pOS,
              char** ppzOpts, int* pOptsIdx )
{
    /*
     *  An option argument is optional.
     */
    switch (pOS->optType) {
    case TOPT_SHORT:
        /*
         *  IF nothing is glued on after the current flag character,
         *  THEN see if there is another argument.  If so and if it
         *  does *NOT* start with a hyphen, then it is the option arg.
         */
        if (*pzArg != NUL)
            return SUCCESS;
        break;

    case TOPT_LONG:
        /*
         *  Look for an argument if we don't already have one (glued on
         *  with a `=' character)
         */
        if (pOS->pzOptArg != NULL)
            return SUCCESS;
        break;

    default:
        return FAILURE;
    }
    if (pOpts->curOptIdx >= pOpts->origArgCt)
        return PROBLEM;

    pzArg = pOpts->origArgVect[ pOpts->curOptIdx ];
    if (*pzArg != '-')
        ppzOpts[ (*pOptsIdx)++ ] = pOpts->origArgVect[ (pOpts->curOptIdx)++ ];
    return SUCCESS;
}

/*
 *  Process a string of short options glued together.  If the last one
 *  does or may take an argument, the do the argument processing and leave.
 */
static tSuccess
checkShortOpts( tOptions* pOpts, char* pzArg, tOptState* pOS,
                char** ppzOpts, int* pOptsIdx )
{
    while (*pzArg != NUL) {
        if (FAILED( shortOptionFind( pOpts, (tAoUC)*pzArg, pOS )))
            return FAILURE;

        /*
         *  See if we can have an arg.
         */
        if (OPTST_GET_ARGTYPE(pOS->pOD->fOptState) == OPARG_TYPE_NONE) {
            pzArg++;

        } else if (pOS->pOD->fOptState & OPTST_ARG_OPTIONAL) {
            /*
             *  Take an argument if it is not attached and it does not
             *  start with a hyphen.
             */
            if (pzArg[1] != NUL)
                return SUCCESS;

            pzArg = pOpts->origArgVect[ pOpts->curOptIdx ];
            if (*pzArg != '-')
                ppzOpts[ (*pOptsIdx)++ ] =
                    pOpts->origArgVect[ (pOpts->curOptIdx)++ ];
            return SUCCESS;

        } else {
            /*
             *  IF we need another argument, be sure it is there and
             *  take it.
             */
            if (pzArg[1] == NUL) {
                if (pOpts->curOptIdx >= pOpts->origArgCt)
                    return FAILURE;
                ppzOpts[ (*pOptsIdx)++ ] =
                    pOpts->origArgVect[ (pOpts->curOptIdx)++ ];
            }
            return SUCCESS;
        }
    }
    return SUCCESS;
}

/*
 *  If the program wants sorted options (separated operands and options),
 *  then this routine will to the trick.
 */
LOCAL void
optionSort( tOptions* pOpts )
{
    char** ppzOpts;
    char** ppzOpds;
    int    optsIdx = 0;
    int    opdsIdx = 0;

    tOptState os = OPTSTATE_INITIALIZER(DEFINED);

    /*
     *  Disable for POSIX conformance, or if there are no operands.
     */
    if (  (getenv( "POSIXLY_CORRECT" ) != NULL)
       || NAMED_OPTS(pOpts))
        return;

    /*
     *  Make sure we can allocate two full-sized arg vectors.
     */
    ppzOpts = malloc( pOpts->origArgCt * sizeof( char* ));
    if (ppzOpts == NULL)
        goto exit_no_mem;

    ppzOpds = malloc( pOpts->origArgCt * sizeof( char* ));
    if (ppzOpds == NULL) {
        free( ppzOpts );
        goto exit_no_mem;
    }

    pOpts->curOptIdx = 1;
    pOpts->pzCurOpt  = NULL;

    /*
     *  Now, process all the options from our current position onward.
     *  (This allows interspersed options and arguments for the few
     *  non-standard programs that require it.)
     */
    for (;;) {
        char* pzArg;
        tSuccess res;

        /*
         *  If we're out of arguments, we're done.  Join the option and
         *  operand lists into the original argument vector.
         */
        if (pOpts->curOptIdx >= pOpts->origArgCt) {
            errno = 0;
            goto joinLists;
        }

        pzArg = pOpts->origArgVect[ pOpts->curOptIdx ];
        if (*pzArg != '-') {
            ppzOpds[ opdsIdx++ ] = pOpts->origArgVect[ (pOpts->curOptIdx)++ ];
            continue;
        }

        switch (pzArg[1]) {
        case NUL:
            /*
             *  A single hyphen is an operand.
             */
            ppzOpds[ opdsIdx++ ] = pOpts->origArgVect[ (pOpts->curOptIdx)++ ];
            continue;

        case '-':
            /*
             *  Two consecutive hypens.  Put them on the options list and then
             *  _always_ force the remainder of the arguments to be operands.
             */
            if (pzArg[2] == NUL) {
                ppzOpts[ optsIdx++ ] =
                    pOpts->origArgVect[ (pOpts->curOptIdx)++ ];
                goto restOperands;
            }
            res = longOptionFind( pOpts, pzArg+2, &os );
            break;

        default:
            /*
             *  If short options are not allowed, then do long
             *  option processing.  Otherwise the character must be a
             *  short (i.e. single character) option.
             */
            if ((pOpts->fOptSet & OPTPROC_SHORTOPT) == 0) {
                res = longOptionFind( pOpts, pzArg+1, &os );
            } else {
                res = shortOptionFind( pOpts, (tAoUC)pzArg[1], &os );
            }
            break;
        }
        if (FAILED( res )) {
            errno = EINVAL;
            goto freeTemps;
        }

        /*
         *  We've found an option.  Add the argument to the option list.
         *  Next, we have to see if we need to pull another argument to be
         *  used as the option argument.
         */
        ppzOpts[ optsIdx++ ] = pOpts->origArgVect[ (pOpts->curOptIdx)++ ];

        if (OPTST_GET_ARGTYPE(os.pOD->fOptState) == OPARG_TYPE_NONE) {
            /*
             *  No option argument.  If we have a short option here,
             *  then scan for short options until we get to the end
             *  of the argument string.
             */
            if (  (os.optType == TOPT_SHORT)
               && FAILED( checkShortOpts( pOpts, pzArg+2, &os,
                                          ppzOpts, &optsIdx )) )  {
                errno = EINVAL;
                goto freeTemps;
            }

        } else if (os.pOD->fOptState & OPTST_ARG_OPTIONAL) {
            switch (mayHandleArg( pOpts, pzArg+2, &os, ppzOpts, &optsIdx )) {
            case FAILURE: errno = EIO; goto freeTemps;
            case PROBLEM: errno = 0;   goto joinLists;
            }

        } else {
            switch (mustHandleArg( pOpts, pzArg+2, &os, ppzOpts, &optsIdx )) {
            case PROBLEM:
            case FAILURE: errno = EIO; goto freeTemps;
            }
        }
    } /* for (;;) */

 restOperands:
    while (pOpts->curOptIdx < pOpts->origArgCt)
        ppzOpds[ opdsIdx++ ] = pOpts->origArgVect[ (pOpts->curOptIdx)++ ];

 joinLists:
    if (optsIdx > 0)
        memcpy( pOpts->origArgVect + 1, ppzOpts, optsIdx * sizeof( char* ));
    if (opdsIdx > 0)
        memcpy( pOpts->origArgVect + 1 + optsIdx,
                ppzOpds, opdsIdx * sizeof( char* ));

 freeTemps:
    free( ppzOpts );
    free( ppzOpds );
    return;

 exit_no_mem:
    errno = ENOMEM;
    return;
}

/*
 * Local Variables:
 * mode: C
 * c-file-style: "stroustrup"
 * indent-tabs-mode: nil
 * End:
 * end of autoopts/sort.c */
