
/**
 * \file stack.c
 *
 *  Time-stamp:      "2010-07-17 10:42:27 bkorb"
 *
 *  This is a special option processing routine that will save the
 *  argument to an option in a FIFO queue.
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

#ifdef WITH_LIBREGEX
#  include REGEX_HEADER
#endif

/*=export_func  optionUnstackArg
 * private:
 *
 * what:  Remove option args from a stack
 * arg:   + tOptions* + pOpts    + program options descriptor +
 * arg:   + tOptDesc* + pOptDesc + the descriptor for this arg +
 *
 * doc:
 *  Invoked for options that are equivalenced to stacked options.
=*/
void
optionUnstackArg(
    tOptions*  pOpts,
    tOptDesc*  pOptDesc )
{
    int       res;

    tArgList* pAL;

    if ((pOptDesc->fOptState & OPTST_RESET) != 0)
        return;
    pAL = (tArgList*)pOptDesc->optCookie;

    /*
     *  IF we don't have any stacked options,
     *  THEN indicate that we don't have any of these options
     */
    if (pAL == NULL) {
        pOptDesc->fOptState &= OPTST_PERSISTENT_MASK;
        if ((pOptDesc->fOptState & OPTST_INITENABLED) == 0)
            pOptDesc->fOptState |= OPTST_DISABLED;
        return;
    }

#ifdef WITH_LIBREGEX
    {
        regex_t   re;
        int       i, ct, dIdx;

        if (regcomp(&re, pOptDesc->optArg.argString, REG_NOSUB) != 0)
            return;

        /*
         *  search the list for the entry(s) to remove.  Entries that
         *  are removed are *not* copied into the result.  The source
         *  index is incremented every time.  The destination only when
         *  we are keeping a define.
         */
        for (i = 0, dIdx = 0, ct = pAL->useCt; --ct >= 0; i++) {
            tCC*      pzSrc = pAL->apzArgs[ i ];
            char*     pzEq  = strchr(pzSrc, '=');

            if (pzEq != NULL)
                *pzEq = NUL;

            res = regexec(&re, pzSrc, (size_t)0, NULL, 0);
            switch (res) {
            case 0:
                /*
                 *  Remove this entry by reducing the in-use count
                 *  and *not* putting the string pointer back into
                 *  the list.
                 */
                AGFREE(pzSrc);
                pAL->useCt--;
                break;

            default:
            case REG_NOMATCH:
                if (pzEq != NULL)
                    *pzEq = '=';

                /*
                 *  IF we have dropped an entry
                 *  THEN we have to move the current one.
                 */
                if (dIdx != i)
                    pAL->apzArgs[ dIdx ] = pzSrc;
                dIdx++;
            }
        }

        regfree(&re);
    }
#else  /* not WITH_LIBREGEX */
    {
        int i, ct, dIdx;

        /*
         *  search the list for the entry(s) to remove.  Entries that
         *  are removed are *not* copied into the result.  The source
         *  index is incremented every time.  The destination only when
         *  we are keeping a define.
         */
        for (i = 0, dIdx = 0, ct = pAL->useCt; --ct >= 0; i++) {
            tCC*      pzSrc = pAL->apzArgs[ i ];
            char*     pzEq  = strchr(pzSrc, '=');

            if (pzEq != NULL)
                *pzEq = NUL;

            if (strcmp(pzSrc, pOptDesc->optArg.argString) == 0) {
                /*
                 *  Remove this entry by reducing the in-use count
                 *  and *not* putting the string pointer back into
                 *  the list.
                 */
                AGFREE(pzSrc);
                pAL->useCt--;
            } else {
                if (pzEq != NULL)
                    *pzEq = '=';

                /*
                 *  IF we have dropped an entry
                 *  THEN we have to move the current one.
                 */
                if (dIdx != i)
                    pAL->apzArgs[ dIdx ] = pzSrc;
                dIdx++;
            }
        }
    }
#endif /* WITH_LIBREGEX */
    /*
     *  IF we have unstacked everything,
     *  THEN indicate that we don't have any of these options
     */
    if (pAL->useCt == 0) {
        pOptDesc->fOptState &= OPTST_PERSISTENT_MASK;
        if ((pOptDesc->fOptState & OPTST_INITENABLED) == 0)
            pOptDesc->fOptState |= OPTST_DISABLED;
        AGFREE((void*)pAL);
        pOptDesc->optCookie = NULL;
    }
}


/*
 *  Put an entry into an argument list.  The first argument points to
 *  a pointer to the argument list structure.  It gets passed around
 *  as an opaque address.
 */
LOCAL void
addArgListEntry(void** ppAL, void* entry)
{
    tArgList* pAL = *(void**)ppAL;

    /*
     *  IF we have never allocated one of these,
     *  THEN allocate one now
     */
    if (pAL == NULL) {
        pAL = (tArgList*)AGALOC(sizeof(*pAL), "new option arg stack");
        if (pAL == NULL)
            return;
        pAL->useCt   = 0;
        pAL->allocCt = MIN_ARG_ALLOC_CT;
        *ppAL = (void*)pAL;
    }

    /*
     *  ELSE if we are out of room
     *  THEN make it bigger
     */
    else if (pAL->useCt >= pAL->allocCt) {
        size_t sz = sizeof(*pAL);
        pAL->allocCt += INCR_ARG_ALLOC_CT;

        /*
         *  The base structure contains space for MIN_ARG_ALLOC_CT
         *  pointers.  We subtract it off to find our augment size.
         */
        sz += sizeof(char*) * (pAL->allocCt - MIN_ARG_ALLOC_CT);
        pAL = (tArgList*)AGREALOC((void*)pAL, sz, "expanded opt arg stack");
        if (pAL == NULL)
            return;
        *ppAL = (void*)pAL;
    }

    /*
     *  Insert the new argument into the list
     */
    pAL->apzArgs[ (pAL->useCt)++ ] = entry;
}


/*=export_func  optionStackArg
 * private:
 *
 * what:  put option args on a stack
 * arg:   + tOptions* + pOpts    + program options descriptor +
 * arg:   + tOptDesc* + pOptDesc + the descriptor for this arg +
 *
 * doc:
 *  Keep an entry-ordered list of option arguments.
=*/
void
optionStackArg(
    tOptions*  pOpts,
    tOptDesc*  pOD )
{
    char * pz;

    if ((pOD->fOptState & OPTST_RESET) != 0) {
        tArgList* pAL = (void*)pOD->optCookie;
        int ix;
        if (pAL == NULL)
            return;

        ix = pAL->useCt;
        while (--ix >= 0)
            AGFREE(pAL->apzArgs[ix]);
        AGFREE(pAL);

    } else {
        if (pOD->optArg.argString == NULL)
            return;

        AGDUPSTR(pz, pOD->optArg.argString, "stack arg");
        addArgListEntry(&(pOD->optCookie), (void*)pz);
    }
}
/*
 * Local Variables:
 * mode: C
 * c-file-style: "stroustrup"
 * indent-tabs-mode: nil
 * End:
 * end of autoopts/stack.c */
