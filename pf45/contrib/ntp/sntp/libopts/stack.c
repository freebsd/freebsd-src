
/*
 *  stack.c
 *  $Id: stack.c,v 4.13 2007/02/04 17:44:12 bkorb Exp $
 *  Time-stamp:      "2007-01-13 10:43:21 bkorb"
 *
 *  This is a special option processing routine that will save the
 *  argument to an option in a FIFO queue.
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

    tArgList* pAL = (tArgList*)pOptDesc->optCookie;
    /*
     *  IF we don't have any stacked options,
     *  THEN indicate that we don't have any of these options
     */
    if (pAL == NULL) {
        pOptDesc->fOptState &= OPTST_PERSISTENT_MASK;
        if ( (pOptDesc->fOptState & OPTST_INITENABLED) == 0)
            pOptDesc->fOptState |= OPTST_DISABLED;
        return;
    }

#ifdef WITH_LIBREGEX
    {
        regex_t   re;
        int       i, ct, dIdx;

        if (regcomp( &re, pOptDesc->optArg.argString, REG_NOSUB ) != 0)
            return;

        /*
         *  search the list for the entry(s) to remove.  Entries that
         *  are removed are *not* copied into the result.  The source
         *  index is incremented every time.  The destination only when
         *  we are keeping a define.
         */
        for (i = 0, dIdx = 0, ct = pAL->useCt; --ct >= 0; i++) {
            tCC*      pzSrc = pAL->apzArgs[ i ];
            char*     pzEq  = strchr( pzSrc, '=' );

            if (pzEq != NULL)
                *pzEq = NUL;

            res = regexec( &re, pzSrc, (size_t)0, NULL, 0 );
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

        regfree( &re );
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
            char*     pzEq  = strchr( pzSrc, '=' );

            if (pzEq != NULL)
                *pzEq = NUL;

            if (strcmp( pzSrc, pOptDesc->optArg.argString ) == 0) {
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
        if ( (pOptDesc->fOptState & OPTST_INITENABLED) == 0)
            pOptDesc->fOptState |= OPTST_DISABLED;
        AGFREE( (void*)pAL );
        pOptDesc->optCookie = NULL;
    }
}


/*
 *  Put an entry into an argument list.  The first argument points to
 *  a pointer to the argument list structure.  It gets passed around
 *  as an opaque address.
 */
LOCAL void
addArgListEntry( void** ppAL, void* entry )
{
    tArgList* pAL = *(void**)ppAL;

    /*
     *  IF we have never allocated one of these,
     *  THEN allocate one now
     */
    if (pAL == NULL) {
        pAL = (tArgList*)AGALOC( sizeof( *pAL ), "new option arg stack" );
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
        size_t sz = sizeof( *pAL );
        pAL->allocCt += INCR_ARG_ALLOC_CT;

        /*
         *  The base structure contains space for MIN_ARG_ALLOC_CT
         *  pointers.  We subtract it off to find our augment size.
         */
        sz += sizeof(char*) * (pAL->allocCt - MIN_ARG_ALLOC_CT);
        pAL = (tArgList*)AGREALOC( (void*)pAL, sz, "expanded opt arg stack" );
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

    if (pOD->optArg.argString == NULL)
        return;

    AGDUPSTR(pz, pOD->optArg.argString, "stack arg");
    addArgListEntry( &(pOD->optCookie), (void*)pz );
}
/*
 * Local Variables:
 * mode: C
 * c-file-style: "stroustrup"
 * indent-tabs-mode: nil
 * End:
 * end of autoopts/stack.c */
