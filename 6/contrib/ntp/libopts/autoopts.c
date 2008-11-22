
/*
 *  $Id: autoopts.c,v 4.25 2007/04/15 19:01:18 bkorb Exp $
 *  Time-stamp:      "2007-04-15 11:10:40 bkorb"
 *
 *  This file contains all of the routines that must be linked into
 *  an executable to use the generated option processing.  The optional
 *  routines are in separately compiled modules so that they will not
 *  necessarily be linked in.
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

static char const zNil[] = "";

/* = = = START-STATIC-FORWARD = = = */
/* static forward declarations maintained by :mkfwd */
static tSuccess
findOptDesc( tOptions* pOpts, tOptState* pOptState );

static tSuccess
nextOption( tOptions* pOpts, tOptState* pOptState );

static tSuccess
doPresets( tOptions* pOpts );

static int
checkConsistency( tOptions* pOpts );
/* = = = END-STATIC-FORWARD = = = */

LOCAL void *
ao_malloc( size_t sz )
{
    void * res = malloc(sz);
    if (res == NULL) {
        fprintf( stderr, "malloc of %d bytes failed\n", (int)sz );
        exit( EXIT_FAILURE );
    }
    return res;
}
#undef  malloc
#define malloc(_s) ao_malloc(_s)

LOCAL void *
ao_realloc( void *p, size_t sz )
{
    void * res = realloc(p, sz);
    if (res == NULL) {
        fprintf( stderr, "realloc of %d bytes at 0x%p failed\n", (int)sz, p );
        exit( EXIT_FAILURE );
    }
    return res;
}
#undef  realloc
#define realloc(_p,_s) ao_realloc(_p,_s)


LOCAL void
ao_free( void *p )
{
    if (p != NULL)
        free(p);
}
#undef  free
#define free(_p) ao_free(_p)


LOCAL char *
ao_strdup( char const *str )
{
    char * res = strdup(str);
    if (res == NULL) {
        fprintf( stderr, "strdup of %d byte string failed\n", (int)strlen(str) );
        exit( EXIT_FAILURE );
    }
    return res;
}
#undef  strdup
#define strdup(_p) ao_strdup(_p)

#ifndef HAVE_PATHFIND
#  include "compat/pathfind.c"
#endif

#ifndef HAVE_SNPRINTF
#  include "compat/snprintf.c"
#endif

#ifndef HAVE_STRDUP
#  include "compat/strdup.c"
#endif

#ifndef HAVE_STRCHR
#  include "compat/strchr.c"
#endif

/*
 *  handleOption
 *
 *  This routine handles equivalencing, sets the option state flags and
 *  invokes the handler procedure, if any.
 */
LOCAL tSuccess
handleOption( tOptions* pOpts, tOptState* pOptState )
{
    /*
     *  Save a copy of the option procedure pointer.
     *  If this is an equivalence class option, we still want this proc.
     */
    tOptDesc* pOD = pOptState->pOD;
    tOptProc* pOP = pOD->pOptProc;
    if (pOD->fOptState & OPTST_ALLOC_ARG)
        AGFREE(pOD->optArg.argString);

    pOD->optArg.argString = pOptState->pzOptArg;

    /*
     *  IF we are presetting options, then we will ignore any un-presettable
     *  options.  They are the ones either marked as such.
     */
    if (  ((pOpts->fOptSet & OPTPROC_PRESETTING) != 0)
       && ((pOD->fOptState & OPTST_NO_INIT) != 0)
       )
        return PROBLEM;

    /*
     *  IF this is an equivalence class option,
     *  THEN
     *      Save the option value that got us to this option
     *      entry.  (It may not be pOD->optChar[0], if this is an
     *      equivalence entry.)
     *      set the pointer to the equivalence class base
     */
    if (pOD->optEquivIndex != NO_EQUIVALENT) {
        tOptDesc* p = pOpts->pOptDesc + pOD->optEquivIndex;

        /*
         * IF the current option state has not been defined (set on the
         *    command line), THEN we will allow continued resetting of
         *    the value.  Once "defined", then it must not change.
         */
        if ((pOD->fOptState & OPTST_DEFINED) != 0) {
            /*
             *  The equivalenced-to option has been found on the command
             *  line before.  Make sure new occurrences are the same type.
             *
             *  IF this option has been previously equivalenced and
             *     it was not the same equivalenced-to option,
             *  THEN we have a usage problem.
             */
            if (p->optActualIndex != pOD->optIndex) {
                fprintf( stderr, (char*)zMultiEquiv, p->pz_Name, pOD->pz_Name,
                         (pOpts->pOptDesc + p->optActualIndex)->pz_Name);
                return FAILURE;
            }
        } else {
            /*
             *  Set the equivalenced-to actual option index to no-equivalent
             *  so that we set all the entries below.  This option may either
             *  never have been selected before, or else it was selected by
             *  some sort of "presetting" mechanism.
             */
            p->optActualIndex = NO_EQUIVALENT;
        }

        if (p->optActualIndex != pOD->optIndex) {
            /*
             *  First time through, copy over the state
             *  and add in the equivalence flag
             */
            p->optActualValue = pOD->optValue;
            p->optActualIndex = pOD->optIndex;
            pOptState->flags |= OPTST_EQUIVALENCE;
        }

        /*
         *  Copy the most recent option argument.  set membership state
         *  is kept in ``p->optCookie''.  Do not overwrite.
         */
        p->optArg.argString = pOD->optArg.argString;
        pOD = p;

    } else {
        pOD->optActualValue = pOD->optValue;
        pOD->optActualIndex = pOD->optIndex;
    }

    pOD->fOptState &= OPTST_PERSISTENT_MASK;
    pOD->fOptState |= (pOptState->flags & ~OPTST_PERSISTENT_MASK);

    /*
     *  Keep track of count only for DEFINED (command line) options.
     *  IF we have too many, build up an error message and bail.
     */
    if (  (pOD->fOptState & OPTST_DEFINED)
       && (++pOD->optOccCt > pOD->optMaxCt)  )  {

        if ((pOpts->fOptSet & OPTPROC_ERRSTOP) != 0) {
            char const * pzEqv =
                (pOD->optEquivIndex != NO_EQUIVALENT) ? zEquiv : zNil;

            fputs( zErrOnly, stderr );

            if (pOD->optMaxCt > 1)
                fprintf(stderr, zAtMost, pOD->optMaxCt, pOD->pz_Name, pzEqv);
            else
                fprintf(stderr, zOnlyOne, pOD->pz_Name, pzEqv);
        }

        return FAILURE;
    }

    /*
     *  If provided a procedure to call, call it
     */
    if (pOP != (tpOptProc)NULL)
        (*pOP)( pOpts, pOD );

    return SUCCESS;
}


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *
 *  HUNT FOR OPTIONS IN THE ARGUMENT LIST
 *
 *  The next four procedures are "private" to nextOption().
 *  nextOption() uses findOptDesc() to find the next descriptor and it, in
 *  turn, uses longOptionFind() and shortOptionFind() to actually do the hunt.
 *
 *  longOptionFind
 *
 *  Find the long option descriptor for the current option
 */
LOCAL tSuccess
longOptionFind( tOptions* pOpts, char* pzOptName, tOptState* pOptState )
{
    ag_bool    disable  = AG_FALSE;
    char*      pzEq     = strchr( pzOptName, '=' );
    tOptDesc*  pOD      = pOpts->pOptDesc;
    int        idx      = 0;
    int        idxLim   = pOpts->optCt;
    int        matchCt  = 0;
    int        matchIdx = 0;
    int        nameLen;

    /*
     *  IF the value is attached to the name,
     *  THEN clip it off.
     *  Either way, figure out how long our name is
     */
    if (pzEq != NULL) {
        nameLen = (int)(pzEq - pzOptName);
        *pzEq = NUL;
    } else nameLen = strlen( pzOptName );

    do  {
        if (SKIP_OPT(pOD))
            continue;

        if (strneqvcmp( pzOptName, pOD->pz_Name, nameLen ) == 0) {
            /*
             *  IF we have a complete match
             *  THEN it takes priority over any already located partial
             */
            if (pOD->pz_Name[ nameLen ] == NUL) {
                matchCt  = 1;
                matchIdx = idx;
                break;
            }
        }

        /*
         *  IF       there is a disable name
         *     *AND* no argument value has been supplied
         *              (disabled options may have no argument)
         *     *AND* the option name matches the disable name
         *  THEN ...
         */
        else if (  (pOD->pz_DisableName != NULL)
                && (strneqvcmp(pzOptName, pOD->pz_DisableName, nameLen) == 0)
                )  {
            disable  = AG_TRUE;

            /*
             *  IF we have a complete match
             *  THEN it takes priority over any already located partial
             */
            if (pOD->pz_DisableName[ nameLen ] == NUL) {
                matchCt  = 1;
                matchIdx = idx;
                break;
            }
        }

        else
            continue;

        /*
         *  We found a partial match, either regular or disabling.
         *  Remember the index for later.
         */
        matchIdx = idx;

        if (++matchCt > 1)
            break;

    } while (pOD++, (++idx < idxLim));

    if (pzEq != NULL)
        *(pzEq++) = '=';

    /*
     *  Make sure we either found an exact match or found only one partial
     */
    if (matchCt == 1) {
        /*
         *  IF we found a disablement name,
         *  THEN set the bit in the callers' flag word
         */
        if (disable)
            pOptState->flags |= OPTST_DISABLED;

        pOptState->pOD      = pOpts->pOptDesc + matchIdx;
        pOptState->pzOptArg = pzEq;
        pOptState->optType  = TOPT_LONG;
        return SUCCESS;
    }

    /*
     *  IF there is no equal sign
     *     *AND* we are using named arguments
     *     *AND* there is a default named option,
     *  THEN return that option.
     */
    if (  (pzEq == NULL)
       && NAMED_OPTS(pOpts)
       && (pOpts->specOptIdx.default_opt != NO_EQUIVALENT)) {
        pOptState->pOD = pOpts->pOptDesc + pOpts->specOptIdx.default_opt;

        pOptState->pzOptArg = pzOptName;
        pOptState->optType  = TOPT_DEFAULT;
        return SUCCESS;
    }

    /*
     *  IF we are to stop on errors (the default, actually)
     *  THEN call the usage procedure.
     */
    if ((pOpts->fOptSet & OPTPROC_ERRSTOP) != 0) {
        fprintf( stderr, zIllOptStr, pOpts->pzProgPath,
                 (matchCt == 0) ? zIllegal : zAmbiguous, pzOptName );
        (*pOpts->pUsageProc)( pOpts, EXIT_FAILURE );
    }

    return FAILURE;
}


/*
 *  shortOptionFind
 *
 *  Find the short option descriptor for the current option
 */
LOCAL tSuccess
shortOptionFind( tOptions* pOpts, uint_t optValue, tOptState* pOptState )
{
    tOptDesc*  pRes = pOpts->pOptDesc;
    int        ct   = pOpts->optCt;

    /*
     *  Search the option list
     */
    for (;;) {
        /*
         *  IF the values match,
         *  THEN we stop here
         */
        if ((! SKIP_OPT(pRes)) && (optValue == pRes->optValue)) {
            pOptState->pOD     = pRes;
            pOptState->optType = TOPT_SHORT;
            return SUCCESS;
        }

        /*
         *  Advance to next option description
         */
        pRes++;

        /*
         *  IF we have searched everything, ...
         */
        if (--ct <= 0)
            break;
    }

    /*
     *  IF    the character value is a digit
     *    AND there is a special number option ("-n")
     *  THEN the result is the "option" itself and the
     *       option is the specially marked "number" option.
     */
    if (  isdigit( optValue )
       && (pOpts->specOptIdx.number_option != NO_EQUIVALENT) ) {
        pOptState->pOD = \
        pRes           = pOpts->pOptDesc + pOpts->specOptIdx.number_option;
        (pOpts->pzCurOpt)--;
        pOptState->optType = TOPT_SHORT;
        return SUCCESS;
    }

    /*
     *  IF we are to stop on errors (the default, actually)
     *  THEN call the usage procedure.
     */
    if ((pOpts->fOptSet & OPTPROC_ERRSTOP) != 0) {
        fprintf( stderr, zIllOptChr, pOpts->pzProgPath, optValue );
        (*pOpts->pUsageProc)( pOpts, EXIT_FAILURE );
    }

    return FAILURE;
}


/*
 *  findOptDesc
 *
 *  Find the option descriptor for the current option
 */
static tSuccess
findOptDesc( tOptions* pOpts, tOptState* pOptState )
{
    /*
     *  IF we are continuing a short option list (e.g. -xyz...)
     *  THEN continue a single flag option.
     *  OTHERWISE see if there is room to advance and then do so.
     */
    if ((pOpts->pzCurOpt != NULL) && (*pOpts->pzCurOpt != NUL))
        return shortOptionFind( pOpts, (tAoUC)*(pOpts->pzCurOpt), pOptState );

    if (pOpts->curOptIdx >= pOpts->origArgCt)
        return PROBLEM; /* NORMAL COMPLETION */

    pOpts->pzCurOpt = pOpts->origArgVect[ pOpts->curOptIdx ];

    /*
     *  IF all arguments must be named options, ...
     */
    if (NAMED_OPTS(pOpts)) {
        char* pz = pOpts->pzCurOpt;
        pOpts->curOptIdx++;

        /*
         *  Skip over any flag/option markers.
         *  In this mode, they are not required.
         */
        while (*pz == '-') pz++;

        return longOptionFind( pOpts, pz, pOptState );
    }

    /*
     *  Note the kind of flag/option marker
     */
    if (*((pOpts->pzCurOpt)++) != '-')
        return PROBLEM; /* NORMAL COMPLETION - this + rest are operands */

    /*
     *  Special hack for a hyphen by itself
     */
    if (*(pOpts->pzCurOpt) == NUL)
        return PROBLEM; /* NORMAL COMPLETION - this + rest are operands */

    /*
     *  The current argument is to be processed as an option argument
     */
    pOpts->curOptIdx++;

    /*
     *  We have an option marker.
     *  Test the next character for long option indication
     */
    if (pOpts->pzCurOpt[0] == '-') {
        if (*++(pOpts->pzCurOpt) == NUL)
            /*
             *  NORMAL COMPLETION - NOT this arg, but rest are operands
             */
            return PROBLEM;

        /*
         *  We do not allow the hyphen to be used as a flag value.
         *  Therefore, if long options are not to be accepted, we punt.
         */
        if ((pOpts->fOptSet & OPTPROC_LONGOPT) == 0) {
            fprintf( stderr, zIllOptStr, pOpts->pzProgPath,
                     zIllegal, pOpts->pzCurOpt-2 );
            return FAILURE;
        }

        return longOptionFind( pOpts, pOpts->pzCurOpt, pOptState );
    }

    /*
     *  If short options are not allowed, then do long
     *  option processing.  Otherwise the character must be a
     *  short (i.e. single character) option.
     */
    if ((pOpts->fOptSet & OPTPROC_SHORTOPT) != 0)
        return shortOptionFind( pOpts, (tAoUC)*(pOpts->pzCurOpt), pOptState );

    return longOptionFind( pOpts, pOpts->pzCurOpt, pOptState );
}


/*
 *  nextOption
 *
 *  Find the option descriptor and option argument (if any) for the
 *  next command line argument.  DO NOT modify the descriptor.  Put
 *  all the state in the state argument so that the option can be skipped
 *  without consequence (side effect).
 */
static tSuccess
nextOption( tOptions* pOpts, tOptState* pOptState )
{
    tSuccess res;
    enum { ARG_NONE, ARG_MAY, ARG_MUST } arg_type = ARG_NONE;
    teOptArgType at;

    res = findOptDesc( pOpts, pOptState );
    if (! SUCCESSFUL( res ))
        return res;
    pOptState->flags |= (pOptState->pOD->fOptState & OPTST_PERSISTENT_MASK);
    at = OPTST_GET_ARGTYPE(pOptState->flags);

    /*
     *  Figure out what to do about option arguments.  An argument may be
     *  required, not associated with the option, or be optional.  We detect the
     *  latter by examining for an option marker on the next possible argument.
     *  Disabled mode option selection also disables option arguments.
     */
    if ((pOptState->flags & OPTST_DISABLED) != 0)
        arg_type = ARG_NONE;
    else if (at == OPARG_TYPE_NONE)
        arg_type = ARG_NONE;
    else if (pOptState->flags & OPTST_ARG_OPTIONAL)
        arg_type = ARG_MAY;
    else
        arg_type = ARG_MUST;

    switch (arg_type) {
    case ARG_MUST:
        /*
         *  An option argument is required.  Long options can either have
         *  a separate command line argument, or an argument attached by
         *  the '=' character.  Figure out which.
         */
        switch (pOptState->optType) {
        case TOPT_SHORT:
            /*
             *  See if an arg string follows the flag character
             */
            if (*++(pOpts->pzCurOpt) == NUL)
                pOpts->pzCurOpt = pOpts->origArgVect[ pOpts->curOptIdx++ ];
            pOptState->pzOptArg = pOpts->pzCurOpt;
            break;

        case TOPT_LONG:
            /*
             *  See if an arg string has already been assigned (glued on
             *  with an `=' character)
             */
            if (pOptState->pzOptArg == NULL)
                pOptState->pzOptArg = pOpts->origArgVect[ pOpts->curOptIdx++ ];
            break;

        default:
#ifdef DEBUG
            fputs( "AutoOpts lib error: option type not selected\n",
                   stderr );
            exit( EXIT_FAILURE );
#endif

        case TOPT_DEFAULT:
            /*
             *  The option was selected by default.  The current token is
             *  the option argument.
             */
            break;
        }

        /*
         *  Make sure we did not overflow the argument list.
         */
        if (pOpts->curOptIdx > pOpts->origArgCt) {
            fprintf( stderr, zMisArg, pOpts->pzProgPath,
                     pOptState->pOD->pz_Name );
            return FAILURE;
        }

        pOpts->pzCurOpt = NULL;  /* next time advance to next arg */
        break;

    case ARG_MAY:
        /*
         *  An option argument is optional.
         */
        switch (pOptState->optType) {
        case TOPT_SHORT:
            if (*++pOpts->pzCurOpt != NUL)
                pOptState->pzOptArg = pOpts->pzCurOpt;
            else {
                char* pzLA = pOpts->origArgVect[ pOpts->curOptIdx ];

                /*
                 *  BECAUSE it is optional, we must make sure
                 *  we did not find another flag and that there
                 *  is such an argument.
                 */
                if ((pzLA == NULL) || (*pzLA == '-'))
                    pOptState->pzOptArg = NULL;
                else {
                    pOpts->curOptIdx++; /* argument found */
                    pOptState->pzOptArg = pzLA;
                }
            }
            break;

        case TOPT_LONG:
            /*
             *  Look for an argument if we don't already have one (glued on
             *  with a `=' character) *AND* we are not in named argument mode
             */
            if (  (pOptState->pzOptArg == NULL)
               && (! NAMED_OPTS(pOpts))) {
                char* pzLA = pOpts->origArgVect[ pOpts->curOptIdx ];

                /*
                 *  BECAUSE it is optional, we must make sure
                 *  we did not find another flag and that there
                 *  is such an argument.
                 */
                if ((pzLA == NULL) || (*pzLA == '-'))
                    pOptState->pzOptArg = NULL;
                else {
                    pOpts->curOptIdx++; /* argument found */
                    pOptState->pzOptArg = pzLA;
                }
            }
            break;

        default:
        case TOPT_DEFAULT:
            fputs( "AutoOpts lib error: defaulted to option with optional arg\n",
                   stderr );
            exit( EX_SOFTWARE );
        }

        /*
         *  After an option with an optional argument, we will
         *  *always* start with the next option because if there
         *  were any characters following the option name/flag,
         *  they would be interpreted as the argument.
         */
        pOpts->pzCurOpt = NULL;
        break;

    default: /* CANNOT */
        /*
         *  No option argument.  Make sure next time around we find
         *  the correct option flag character for short options
         */
        if (pOptState->optType == TOPT_SHORT)
            (pOpts->pzCurOpt)++;

        /*
         *  It is a long option.  Make sure there was no ``=xxx'' argument
         */
        else if (pOptState->pzOptArg != NULL) {
            fprintf( stderr, zNoArg, pOpts->pzProgPath,
                     pOptState->pOD->pz_Name );
            return FAILURE;
        }

        /*
         *  It is a long option.  Advance to next command line argument.
         */
        else
            pOpts->pzCurOpt = NULL;
    }

    return SUCCESS;
}


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *
 *  DO PRESETS
 *
 *  The next several routines do the immediate action pass on the command
 *  line options, then the environment variables, then the config files in
 *  reverse order.  Once done with that, the order is reversed and all
 *  the config files and environment variables are processed again, this
 *  time only processing the non-immediate action options.  doPresets()
 *  will then return for optionProcess() to do the final pass on the command
 *  line arguments.
 */

/*
 *  doImmediateOpts - scan the command line for immediate action options
 */
LOCAL tSuccess
doImmediateOpts( tOptions* pOpts )
{
    pOpts->curOptIdx = 1;     /* start by skipping program name */
    pOpts->pzCurOpt  = NULL;

    /*
     *  Examine all the options from the start.  We process any options that
     *  are marked for immediate processing.
     */
    for (;;) {
        tOptState optState = OPTSTATE_INITIALIZER(PRESET);

        switch (nextOption( pOpts, &optState )) {
        case FAILURE: goto optionsDone;
        case PROBLEM: return SUCCESS; /* no more args */
        case SUCCESS: break;
        }

        /*
         *  IF this *is* an immediate-attribute option, then do it.
         */
        if (! DO_IMMEDIATELY(optState.flags))
            continue;

        if (! SUCCESSFUL( handleOption( pOpts, &optState )))
            break;
    } optionsDone:;

    if ((pOpts->fOptSet & OPTPROC_ERRSTOP) != 0)
        (*pOpts->pUsageProc)( pOpts, EXIT_FAILURE );
    return FAILURE;
}


LOCAL tSuccess
doRegularOpts( tOptions* pOpts )
{
    /*
     *  Now, process all the options from our current position onward.
     *  (This allows interspersed options and arguments for the few
     *  non-standard programs that require it.)
     */
    for (;;) {
        tOptState optState = OPTSTATE_INITIALIZER(DEFINED);

        switch (nextOption( pOpts, &optState )) {
        case FAILURE: goto optionsDone;
        case PROBLEM: return SUCCESS; /* no more args */
        case SUCCESS: break;
        }

        /*
         *  IF this is not being processed normally (i.e. is immediate action)
         *  THEN skip it (unless we are supposed to do it a second time).
         */
        if (! DO_NORMALLY(optState.flags)) {
            if (! DO_SECOND_TIME(optState.flags))
                continue;
            optState.pOD->optOccCt--; /* don't count last time */
        }

        if (! SUCCESSFUL( handleOption( pOpts, &optState )))
            break;
    } optionsDone:;
    if ((pOpts->fOptSet & OPTPROC_ERRSTOP) != 0)
        (*pOpts->pUsageProc)( pOpts, EXIT_FAILURE );
    return FAILURE;
}


/*
 *  doPresets - check for preset values from a config file or the envrionment
 */
static tSuccess
doPresets( tOptions* pOpts )
{
    tOptDesc * pOD = NULL;

    if (! SUCCESSFUL( doImmediateOpts( pOpts )))
        return FAILURE;

    /*
     *  IF this option set has a --save-opts option, then it also
     *  has a --load-opts option.  See if a command line option has disabled
     *  option presetting.
     */
    if (pOpts->specOptIdx.save_opts != 0) {
        pOD = pOpts->pOptDesc + pOpts->specOptIdx.save_opts + 1;
        if (DISABLED_OPT(pOD))
            return SUCCESS;
    }

    /*
     *  Until we return from this procedure, disable non-presettable opts
     */
    pOpts->fOptSet |= OPTPROC_PRESETTING;
    /*
     *  IF there are no config files,
     *  THEN do any environment presets and leave.
     */
    if (pOpts->papzHomeList == NULL) {
        doEnvPresets( pOpts, ENV_ALL );
    }
    else {
        doEnvPresets( pOpts, ENV_IMM );

        /*
         *  Check to see if environment variables have disabled presetting.
         */
        if ((pOD != NULL) && ! DISABLED_OPT(pOD))
            internalFileLoad( pOpts );

        /*
         *  ${PROGRAM_LOAD_OPTS} value of "no" cannot disable other environment
         *  variable options.  Only the loading of .rc files.
         */
        doEnvPresets( pOpts, ENV_NON_IMM );
    }
    pOpts->fOptSet &= ~OPTPROC_PRESETTING;

    return SUCCESS;
}


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *
 *  VERIFY OPTION CONSISTENCY
 *
 *  Make sure that the argument list passes our consistency tests.
 */
static int
checkConsistency( tOptions* pOpts )
{
    int        errCt = 0;
    tOptDesc*  pOD   = pOpts->pOptDesc;
    int        oCt   = pOpts->presetOptCt;

    /*
     *  FOR each of "oCt" options, ...
     */
    for (;;) {
        const int*  pMust = pOD->pOptMust;
        const int*  pCant = pOD->pOptCant;

        /*
         *  IF the current option was provided on the command line
         *  THEN ensure that any "MUST" requirements are not
         *       "DEFAULT" (unspecified) *AND* ensure that any
         *       "CANT" options have not been SET or DEFINED.
         */
        if (SELECTED_OPT(pOD)) {
            if (pMust != NULL) for (;;) {
                tOptDesc*  p = pOpts->pOptDesc + *(pMust++);
                if (UNUSED_OPT(p)) {
                    const tOptDesc* pN = pOpts->pOptDesc + pMust[-1];
                    errCt++;
                    fprintf( stderr, zReqFmt, pOD->pz_Name, pN->pz_Name );
                }

                if (*pMust == NO_EQUIVALENT)
                    break;
            }

            if (pCant != NULL) for (;;) {
                tOptDesc*  p = pOpts->pOptDesc + *(pCant++);
                if (SELECTED_OPT(p)) {
                    const tOptDesc* pN = pOpts->pOptDesc + pCant[-1];
                    errCt++;
                    fprintf( stderr, zCantFmt, pOD->pz_Name, pN->pz_Name );
                }

                if (*pCant == NO_EQUIVALENT)
                    break;
            }
        }

        /*
         *  IF       this option is not equivalenced to another,
         *        OR it is equivalenced to itself (is the equiv. root)
         *  THEN we need to make sure it occurs often enough.
         */
        if (  (pOD->optEquivIndex == NO_EQUIVALENT)
           || (pOD->optEquivIndex == pOD->optIndex) )   do {
            /*
             *  IF the occurrence counts have been satisfied,
             *  THEN there is no problem.
             */
            if (pOD->optOccCt >= pOD->optMinCt)
                break;

            /*
             *  IF MUST_SET means SET and PRESET are okay,
             *  so min occurrence count doesn't count
             */
            if (  (pOD->fOptState & OPTST_MUST_SET)
               && (pOD->fOptState & (OPTST_PRESET | OPTST_SET)) )
                break;

            errCt++;
            if (pOD->optMinCt > 1)
                 fprintf( stderr, zNotEnough, pOD->pz_Name, pOD->optMinCt );
            else fprintf( stderr, zNeedOne, pOD->pz_Name );
        } while (0);

        if (--oCt <= 0)
            break;
        pOD++;
    }

    /*
     *  IF we are stopping on errors, check to see if any remaining
     *  arguments are required to be there or prohibited from being there.
     */
    if ((pOpts->fOptSet & OPTPROC_ERRSTOP) != 0) {

        /*
         *  Check for prohibition
         */
        if ((pOpts->fOptSet & OPTPROC_NO_ARGS) != 0) {
            if (pOpts->origArgCt > pOpts->curOptIdx) {
                fprintf( stderr, zNoArgs, pOpts->pzProgName );
                ++errCt;
            }
        }

        /*
         *  ELSE not prohibited, check for being required
         */
        else if ((pOpts->fOptSet & OPTPROC_ARGS_REQ) != 0) {
            if (pOpts->origArgCt <= pOpts->curOptIdx) {
                fprintf( stderr, zArgsMust, pOpts->pzProgName );
                ++errCt;
            }
        }
    }

    return errCt;
}


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *
 *  THESE ROUTINES ARE CALLABLE FROM THE GENERATED OPTION PROCESSING CODE
 */
/*=--subblock=arg=arg_type,arg_name,arg_desc =*/
/*=*
 * library:  opts
 * header:   your-opts.h
 *
 * lib_description:
 *
 *  These are the routines that libopts users may call directly from their
 *  code.  There are several other routines that can be called by code
 *  generated by the libopts option templates, but they are not to be
 *  called from any other user code.  The @file{options.h} header is
 *  fairly clear about this, too.
=*/

/*=export_func optionProcess
 *
 * what: this is the main option processing routine
 *
 * arg:  + tOptions* + pOpts + program options descriptor +
 * arg:  + int       + argc  + program arg count  +
 * arg:  + char**    + argv  + program arg vector +
 *
 * ret_type:  int
 * ret_desc:  the count of the arguments processed
 *
 * doc:
 *
 * This is the main entry point for processing options.  It is intended
 * that this procedure be called once at the beginning of the execution of
 * a program.  Depending on options selected earlier, it is sometimes
 * necessary to stop and restart option processing, or to select completely
 * different sets of options.  This can be done easily, but you generally
 * do not want to do this.
 *
 * The number of arguments processed always includes the program name.
 * If one of the arguments is "--", then it is counted and the processing
 * stops.  If an error was encountered and errors are to be tolerated, then
 * the returned value is the index of the argument causing the error.
 * A hyphen by itself ("-") will also cause processing to stop and will
 * @emph{not} be counted among the processed arguments.  A hyphen by itself
 * is treated as an operand.  Encountering an operand stops option
 * processing.
 *
 * err:  Errors will cause diagnostics to be printed.  @code{exit(3)} may
 *       or may not be called.  It depends upon whether or not the options
 *       were generated with the "allow-errors" attribute, or if the
 *       ERRSKIP_OPTERR or ERRSTOP_OPTERR macros were invoked.
=*/
int
optionProcess(
    tOptions*  pOpts,
    int        argCt,
    char**     argVect )
{
    if (! SUCCESSFUL( validateOptionsStruct( pOpts, argVect[0] )))
        exit( EX_SOFTWARE );

    /*
     *  Establish the real program name, the program full path,
     *  and do all the presetting the first time thru only.
     */
    if ((pOpts->fOptSet & OPTPROC_INITDONE) == 0) {
        pOpts->origArgCt   = argCt;
        pOpts->origArgVect = argVect;
        pOpts->fOptSet    |= OPTPROC_INITDONE;

        if (! SUCCESSFUL( doPresets( pOpts )))
            return 0;

        if ((pOpts->fOptSet & OPTPROC_REORDER) != 0)
            optionSort( pOpts );

        pOpts->curOptIdx   = 1;
        pOpts->pzCurOpt    = NULL;
    }

    /*
     *  IF we are (re)starting,
     *  THEN reset option location
     */
    else if (pOpts->curOptIdx <= 0) {
        pOpts->curOptIdx = 1;
        pOpts->pzCurOpt  = NULL;
    }

    if (! SUCCESSFUL( doRegularOpts( pOpts )))
        return pOpts->origArgCt;

    /*
     *  IF    there were no errors
     *    AND we have RC/INI files
     *    AND there is a request to save the files
     *  THEN do that now before testing for conflicts.
     *       (conflicts are ignored in preset options)
     */
    if (pOpts->specOptIdx.save_opts != 0) {
        tOptDesc*  pOD = pOpts->pOptDesc + pOpts->specOptIdx.save_opts;

        if (SELECTED_OPT( pOD )) {
            optionSaveFile( pOpts );
            exit( EXIT_SUCCESS );
        }
    }

    /*
     *  IF we are checking for errors,
     *  THEN look for too few occurrences of required options
     */
    if ((pOpts->fOptSet & OPTPROC_ERRSTOP) != 0) {
        if (checkConsistency( pOpts ) != 0)
            (*pOpts->pUsageProc)( pOpts, EXIT_FAILURE );
    }

    return pOpts->curOptIdx;
}

/*
 * Local Variables:
 * mode: C
 * c-file-style: "stroustrup"
 * indent-tabs-mode: nil
 * End:
 * end of autoopts/autoopts.c */
