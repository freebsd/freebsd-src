
/*
 *  $Id: environment.c,v 4.13 2007/04/15 19:01:18 bkorb Exp $
 * Time-stamp:      "2007-04-15 11:50:35 bkorb"
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

/* = = = START-STATIC-FORWARD = = = */
/* static forward declarations maintained by :mkfwd */
static void
checkEnvOpt(tOptState * os, char * env_name,
            tOptions* pOpts, teEnvPresetType type);
/* = = = END-STATIC-FORWARD = = = */

/*
 *  doPrognameEnv - check for preset values from the ${PROGNAME}
 *  environment variable.  This is accomplished by parsing the text into
 *  tokens, temporarily replacing the arg vector and calling
 *  doImmediateOpts and/or doRegularOpts.
 */
LOCAL void
doPrognameEnv( tOptions* pOpts, teEnvPresetType type )
{
    char const*   pczOptStr = getenv( pOpts->pzPROGNAME );
    token_list_t* pTL;
    int           sv_argc;
    tAoUI         sv_flag;
    char**        sv_argv;

    /*
     *  IF there is no such environment variable
     *   *or* there is, but we are doing immediate opts and there are
     *        no immediate opts to do (--help inside $PROGNAME is silly,
     *        but --no-load-defs is not, so that is marked)
     *  THEN bail out now.  (
     */
    if (  (pczOptStr == NULL)
       || (  (type == ENV_IMM)
          && ((pOpts->fOptSet & OPTPROC_HAS_IMMED) == 0)  )  )
        return;

    /*
     *  Tokenize the string.  If there's nothing of interest, we'll bail
     *  here immediately.
     */
    pTL = ao_string_tokenize( pczOptStr );
    if (pTL == NULL)
        return;

    /*
     *  Substitute our $PROGNAME argument list for the real one
     */
    sv_argc = pOpts->origArgCt;
    sv_argv = pOpts->origArgVect;
    sv_flag = pOpts->fOptSet;

    /*
     *  We add a bogus pointer to the start of the list.  The program name
     *  has already been pulled from "argv", so it won't get dereferenced.
     *  The option scanning code will skip the "program name" at the start
     *  of this list of tokens, so we accommodate this way ....
     */
    pOpts->origArgVect = (char**)(pTL->tkn_list - 1);
    pOpts->origArgCt   = pTL->tkn_ct   + 1;
    pOpts->fOptSet    &= ~OPTPROC_ERRSTOP;

    pOpts->curOptIdx   = 1;
    pOpts->pzCurOpt    = NULL;

    switch (type) {
    case ENV_IMM:
        /*
         *  We know the OPTPROC_HAS_IMMED bit is set.
         */
        (void)doImmediateOpts( pOpts );
        break;

    case ENV_NON_IMM:
        (void)doRegularOpts( pOpts );
        break;

    default:
        /*
         *  Only to immediate opts if the OPTPROC_HAS_IMMED bit is set.
         */
        if (pOpts->fOptSet & OPTPROC_HAS_IMMED) {
            (void)doImmediateOpts( pOpts );
            pOpts->curOptIdx = 1;
            pOpts->pzCurOpt  = NULL;
        }
        (void)doRegularOpts( pOpts );
        break;
    }

    /*
     *  Free up the temporary arg vector and restore the original program args.
     */
    free( pTL );
    pOpts->origArgVect = sv_argv;
    pOpts->origArgCt   = sv_argc;
    pOpts->fOptSet     = sv_flag;
}

static void
checkEnvOpt(tOptState * os, char * env_name,
            tOptions* pOpts, teEnvPresetType type)
{
    os->pzOptArg = getenv( env_name );
    if (os->pzOptArg == NULL)
        return;

    os->flags    = OPTST_PRESET | OPTST_ALLOC_ARG | os->pOD->fOptState;
    os->optType  = TOPT_UNDEFINED;

    if (  (os->pOD->pz_DisablePfx != NULL)
       && (streqvcmp( os->pzOptArg, os->pOD->pz_DisablePfx ) == 0)) {
        os->flags |= OPTST_DISABLED;
        os->pzOptArg = NULL;
    }

    switch (type) {
    case ENV_IMM:
        /*
         *  Process only immediate actions
         */
        if (DO_IMMEDIATELY(os->flags))
            break;
        return;

    case ENV_NON_IMM:
        /*
         *  Process only NON immediate actions
         */
        if (DO_NORMALLY(os->flags) || DO_SECOND_TIME(os->flags))
            break;
        return;

    default: /* process everything */
        break;
    }

    /*
     *  Make sure the option value string is persistent and consistent.
     *
     *  The interpretation of the option value depends
     *  on the type of value argument the option takes
     */
    if (os->pzOptArg != NULL) {
        if (OPTST_GET_ARGTYPE(os->pOD->fOptState) == OPARG_TYPE_NONE) {
            os->pzOptArg = NULL;
        } else if (  (os->pOD->fOptState & OPTST_ARG_OPTIONAL)
                     && (*os->pzOptArg == NUL)) {
            os->pzOptArg = NULL;
        } else if (*os->pzOptArg == NUL) {
            os->pzOptArg = zNil;
        } else {
            AGDUPSTR( os->pzOptArg, os->pzOptArg, "option argument" );
            os->flags |= OPTST_ALLOC_ARG;
        }
    }

    handleOption( pOpts, os );
}

/*
 *  doEnvPresets - check for preset values from the envrionment
 *  This routine should process in all, immediate or normal modes....
 */
LOCAL void
doEnvPresets( tOptions* pOpts, teEnvPresetType type )
{
    int        ct;
    tOptState  st;
    char*      pzFlagName;
    size_t     spaceLeft;
    char       zEnvName[ AO_NAME_SIZE ];

    /*
     *  Finally, see if we are to look at the environment
     *  variables for initial values.
     */
    if ((pOpts->fOptSet & OPTPROC_ENVIRON) == 0)
        return;

    doPrognameEnv( pOpts, type );

    ct  = pOpts->presetOptCt;
    st.pOD = pOpts->pOptDesc;

    pzFlagName = zEnvName
        + snprintf( zEnvName, sizeof( zEnvName ), "%s_", pOpts->pzPROGNAME );
    spaceLeft = AO_NAME_SIZE - (pzFlagName - zEnvName) - 1;

    for (;ct-- > 0; st.pOD++) {
        /*
         *  If presetting is disallowed, then skip this entry
         */
        if (  ((st.pOD->fOptState & OPTST_NO_INIT) != 0)
           || (st.pOD->optEquivIndex != NO_EQUIVALENT)  )
            continue;

        /*
         *  IF there is no such environment variable,
         *  THEN skip this entry, too.
         */
        if (strlen( st.pOD->pz_NAME ) >= spaceLeft)
            continue;

        /*
         *  Set up the option state
         */
        strcpy( pzFlagName, st.pOD->pz_NAME );
        checkEnvOpt(&st, zEnvName, pOpts, type);
    }

    /*
     *  Special handling for ${PROGNAME_LOAD_OPTS}
     */
    if (pOpts->specOptIdx.save_opts != 0) {
        st.pOD = pOpts->pOptDesc + pOpts->specOptIdx.save_opts + 1;
        strcpy( pzFlagName, st.pOD->pz_NAME );
        checkEnvOpt(&st, zEnvName, pOpts, type);
    }
}

/*
 * Local Variables:
 * mode: C
 * c-file-style: "stroustrup"
 * indent-tabs-mode: nil
 * End:
 * end of autoopts/environment.c */
