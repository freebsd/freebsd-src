
/**
 * \file environment.c
 *
 * Time-stamp:      "2011-04-06 09:35:55 bkorb"
 *
 *  This file contains all of the routines that must be linked into
 *  an executable to use the generated option processing.  The optional
 *  routines are in separately compiled modules so that they will not
 *  necessarily be linked in.
 *
 *  This file is part of AutoOpts, a companion to AutoGen.
 *  AutoOpts is free software.
 *  AutoOpts is Copyright (c) 1992-2011 by Bruce Korb - all rights reserved
 *
 *  AutoOpts is available under any one of two licenses.  The license
 *  in use must be one of these two and the choice is under the control
 *  of the user of the license.
 *
 *   The GNU Lesser General Public License, version 3 or later
 *      See the files "COPYING.lgplv3" and "COPYING.gplv3"
 *
 *   The Modified Berkeley Software Distribution License
 *      See the file "COPYING.mbsd"
 *
 *  These files have the following md5sums:
 *
 *  43b91e8ca915626ed3818ffb1b71248b pkg/libopts/COPYING.gplv3
 *  06a1a2e4760c90ea5e1dad8dfaac4d39 pkg/libopts/COPYING.lgplv3
 *  66a5cedaf62c4b2637025f049f9b826f pkg/libopts/COPYING.mbsd
 */

/* = = = START-STATIC-FORWARD = = = */
static void
do_env_opt(tOptState * os, char * env_name,
            tOptions* pOpts, teEnvPresetType type);
/* = = = END-STATIC-FORWARD = = = */

/*
 *  doPrognameEnv - check for preset values from the ${PROGNAME}
 *  environment variable.  This is accomplished by parsing the text into
 *  tokens, temporarily replacing the arg vector and calling
 *  doImmediateOpts and/or doRegularOpts.
 */
LOCAL void
doPrognameEnv(tOptions* pOpts, teEnvPresetType type)
{
    char const*   pczOptStr = getenv(pOpts->pzPROGNAME);
    token_list_t* pTL;
    int           sv_argc;
    tAoUI         sv_flag;
    char**        sv_argv;

    /*
     *  No such beast?  Then bail now.
     */
    if (pczOptStr == NULL)
        return;

    /*
     *  Tokenize the string.  If there's nothing of interest, we'll bail
     *  here immediately.
     */
    pTL = ao_string_tokenize(pczOptStr);
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
        (void)doImmediateOpts(pOpts);
        break;

    case ENV_ALL:
        (void)doImmediateOpts(pOpts);
        pOpts->curOptIdx = 1;
        pOpts->pzCurOpt  = NULL;
        /* FALLTHROUGH */

    case ENV_NON_IMM:
        (void)doRegularOpts(pOpts);
    }

    /*
     *  Free up the temporary arg vector and restore the original program args.
     */
    free(pTL);
    pOpts->origArgVect = sv_argv;
    pOpts->origArgCt   = sv_argc;
    pOpts->fOptSet     = sv_flag;
}

static void
do_env_opt(tOptState * os, char * env_name,
            tOptions* pOpts, teEnvPresetType type)
{
    os->pzOptArg = getenv(env_name);
    if (os->pzOptArg == NULL)
        return;

    os->flags   = OPTST_PRESET | OPTST_ALLOC_ARG | os->pOD->fOptState;
    os->optType = TOPT_UNDEFINED;

    if (  (os->pOD->pz_DisablePfx != NULL)
       && (streqvcmp(os->pzOptArg, os->pOD->pz_DisablePfx) == 0)) {
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
    if (OPTST_GET_ARGTYPE(os->pOD->fOptState) == OPARG_TYPE_NONE) {
        /*
         *  Ignore any value.
         */
        os->pzOptArg = NULL;

    } else if (os->pzOptArg[0] == NUL) {
        /*
         * If the argument is the empty string and the argument is
         * optional, then treat it as if the option was not specified.
         */
        if ((os->pOD->fOptState & OPTST_ARG_OPTIONAL) == 0)
            return;
        os->pzOptArg = NULL;

    } else {
        AGDUPSTR(os->pzOptArg, os->pzOptArg, "option argument");
        os->flags |= OPTST_ALLOC_ARG;
    }

    handle_opt(pOpts, os);
}

/*
 *  doEnvPresets - check for preset values from the envrionment
 *  This routine should process in all, immediate or normal modes....
 */
LOCAL void
doEnvPresets(tOptions* pOpts, teEnvPresetType type)
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

    doPrognameEnv(pOpts, type);

    ct  = pOpts->presetOptCt;
    st.pOD = pOpts->pOptDesc;

    pzFlagName = zEnvName
        + snprintf(zEnvName, sizeof(zEnvName), "%s_", pOpts->pzPROGNAME);
    spaceLeft = AO_NAME_SIZE - (pzFlagName - zEnvName) - 1;

    for (;ct-- > 0; st.pOD++) {
        size_t nln;

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
        nln = strlen(st.pOD->pz_NAME) + 1;
        if (nln <= spaceLeft) {
            /*
             *  Set up the option state
             */
            memcpy(pzFlagName, st.pOD->pz_NAME, nln);
            do_env_opt(&st, zEnvName, pOpts, type);
        }
    }

    /*
     *  Special handling for ${PROGNAME_LOAD_OPTS}
     */
    if (  (pOpts->specOptIdx.save_opts != NO_EQUIVALENT)
       && (pOpts->specOptIdx.save_opts != 0)) {
        size_t nln;
        st.pOD = pOpts->pOptDesc + pOpts->specOptIdx.save_opts + 1;

        if (st.pOD->pz_NAME == NULL)
            return;

        nln = strlen(st.pOD->pz_NAME) + 1;
            
        if (nln > spaceLeft)
            return;

        memcpy(pzFlagName, st.pOD->pz_NAME, nln);
        do_env_opt(&st, zEnvName, pOpts, type);
    }
}

/*
 * Local Variables:
 * mode: C
 * c-file-style: "stroustrup"
 * indent-tabs-mode: nil
 * End:
 * end of autoopts/environment.c */
