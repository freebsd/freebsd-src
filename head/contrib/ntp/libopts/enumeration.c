
/*
 *  $Id: enumeration.c,v 4.17 2007/02/04 17:44:12 bkorb Exp $
 * Time-stamp:      "2007-01-13 10:22:35 bkorb"
 *
 *   Automated Options Paged Usage module.
 *
 *  This routine will run run-on options through a pager so the
 *  user may examine, print or edit them at their leisure.
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

tSCC*  pz_enum_err_fmt;

/* = = = START-STATIC-FORWARD = = = */
/* static forward declarations maintained by :mkfwd */
static void
enumError(
    tOptions*     pOpts,
    tOptDesc*     pOD,
    tCC* const *  paz_names,
    int           name_ct );

static uintptr_t
findName(
    tCC*          pzName,
    tOptions*     pOpts,
    tOptDesc*     pOD,
    tCC* const *  paz_names,
    unsigned int  name_ct );
/* = = = END-STATIC-FORWARD = = = */

static void
enumError(
    tOptions*     pOpts,
    tOptDesc*     pOD,
    tCC* const *  paz_names,
    int           name_ct )
{
    size_t max_len = 0;
    size_t ttl_len = 0;

    if (pOpts != NULL)
        fprintf( option_usage_fp, pz_enum_err_fmt, pOpts->pzProgName,
                 pOD->optArg.argString, pOD->pz_Name );

    fprintf( option_usage_fp, zValidKeys, pOD->pz_Name );

    if (**paz_names == 0x7F) {
        paz_names++;
        name_ct--;
    }

    /*
     *  Figure out the maximum length of any name, plus the total length
     *  of all the names.
     */
    {
        tCC * const * paz = paz_names;
        int   ct  = name_ct;

        do  {
            size_t len = strlen( *(paz++) ) + 1;
            if (len > max_len)
                max_len = len;
            ttl_len += len;
        } while (--ct > 0);
    }

    /*
     *  IF any one entry is about 1/2 line or longer, print one per line
     */
    if (max_len > 35) {
        do  {
            fprintf( option_usage_fp, "  %s\n", *(paz_names++) );
        } while (--name_ct > 0);
    }

    /*
     *  ELSE IF they all fit on one line, then do so.
     */
    else if (ttl_len < 76) {
        fputc( ' ', option_usage_fp );
        do  {
            fputc( ' ', option_usage_fp );
            fputs( *(paz_names++), option_usage_fp );
        } while (--name_ct > 0);
        fputc( '\n', option_usage_fp );
    }

    /*
     *  Otherwise, columnize the output
     */
    else {
        int   ent_no = 0;
        char  zFmt[16];  /* format for all-but-last entries on a line */

        sprintf( zFmt, "%%-%ds", (int)max_len );
        max_len = 78 / max_len; /* max_len is now max entries on a line */
        fputs( "  ", option_usage_fp );

        /*
         *  Loop through all but the last entry
         */
        while (--name_ct > 0) {
            if (++ent_no == max_len) {
                /*
                 *  Last entry on a line.  Start next line, too.
                 */
                fprintf( option_usage_fp, "%s\n  ", *(paz_names++) );
                ent_no = 0;
            }

            else
                fprintf( option_usage_fp, zFmt, *(paz_names++) );
        }
        fprintf( option_usage_fp, "%s\n", *paz_names );
    }

    /*
     *  IF we do not have a pOpts pointer, then this output is being requested
     *  by the usage procedure.  Let's not re-invoke it recursively.
     */
    if (pOpts != NULL)
        (*(pOpts->pUsageProc))( pOpts, EXIT_FAILURE );
    if (OPTST_GET_ARGTYPE(pOD->fOptState) == OPARG_TYPE_MEMBERSHIP)
        fputs( zSetMemberSettings, option_usage_fp );
}


static uintptr_t
findName(
    tCC*          pzName,
    tOptions*     pOpts,
    tOptDesc*     pOD,
    tCC* const *  paz_names,
    unsigned int  name_ct )
{
    uintptr_t     res = name_ct;
    size_t        len = strlen( (char*)pzName );
    uintptr_t     idx;
    /*
     *  Look for an exact match, but remember any partial matches.
     *  Multiple partial matches means we have an ambiguous match.
     */
    for (idx = 0; idx < name_ct; idx++) {
        if (strncmp( (char*)paz_names[idx], (char*)pzName, len) == 0) {
            if (paz_names[idx][len] == NUL)
                return idx;  /* full match */

            if (res != name_ct) {
                pz_enum_err_fmt = zAmbigKey;
                option_usage_fp = stderr;
                enumError( pOpts, pOD, paz_names, (int)name_ct );
            }
            res = idx; /* save partial match */
        }
    }

    /*
     *  no partial match -> error
     */
    if (res == name_ct) {
        pz_enum_err_fmt = zNoKey;
        option_usage_fp = stderr;
        enumError( pOpts, pOD, paz_names, (int)name_ct );
    }

    /*
     *  Return the matching index as a char* pointer.
     *  The result gets stashed in a char* pointer, so it will have to fit.
     */
    return res;
}


/*=export_func  optionKeywordName
 * what:  Convert between enumeration values and strings
 * private:
 *
 * arg:   tOptDesc*,     pOD,       enumeration option description
 * arg:   unsigned int,  enum_val,  the enumeration value to map
 *
 * ret_type:  char const*
 * ret_desc:  the enumeration name from const memory
 *
 * doc:   This converts an enumeration value into the matching string.
=*/
char const*
optionKeywordName(
    tOptDesc*     pOD,
    unsigned int  enum_val )
{
    tOptDesc od;

    od.optArg.argEnum = enum_val;
    (*(pOD->pOptProc))( (void*)(2UL), &od );
    return od.optArg.argString;
}


/*=export_func  optionEnumerationVal
 * what:  Convert from a string to an enumeration value
 * private:
 *
 * arg:   tOptions*,     pOpts,     the program options descriptor
 * arg:   tOptDesc*,     pOD,       enumeration option description
 * arg:   char const * const *,  paz_names, list of enumeration names
 * arg:   unsigned int,  name_ct,   number of names in list
 *
 * ret_type:  uintptr_t
 * ret_desc:  the enumeration value
 *
 * doc:   This converts the optArg.argString string from the option description
 *        into the index corresponding to an entry in the name list.
 *        This will match the generated enumeration value.
 *        Full matches are always accepted.  Partial matches are accepted
 *        if there is only one partial match.
=*/
uintptr_t
optionEnumerationVal(
    tOptions*     pOpts,
    tOptDesc*     pOD,
    tCC * const * paz_names,
    unsigned int  name_ct )
{
    uintptr_t res = 0UL;

    /*
     *  IF the program option descriptor pointer is invalid,
     *  then it is some sort of special request.
     */
    switch ((uintptr_t)pOpts) {
    case 0UL:
        /*
         *  print the list of enumeration names.
         */
        enumError( pOpts, pOD, paz_names, (int)name_ct );
        break;

    case 1UL:
    {
        unsigned int ix = pOD->optArg.argEnum;
        /*
         *  print the name string.
         */
        if (ix >= name_ct)
            printf( "INVALID-%d", ix );
        else
            fputs( paz_names[ ix ], stdout );

        break;
    }

    case 2UL:
    {
        tSCC zInval[] = "*INVALID*";
        unsigned int ix = pOD->optArg.argEnum;
        /*
         *  Replace the enumeration value with the name string.
         */
        if (ix >= name_ct)
            return (uintptr_t)zInval;

        res = (uintptr_t)paz_names[ ix ];
        break;
    }

    default:
        res = findName( pOD->optArg.argString, pOpts, pOD, paz_names, name_ct );

        if (pOD->fOptState & OPTST_ALLOC_ARG) {
            AGFREE(pOD->optArg.argString);
            pOD->fOptState &= ~OPTST_ALLOC_ARG;
            pOD->optArg.argString = NULL;
        }
    }

    return res;
}


/*=export_func  optionSetMembers
 * what:  Convert between bit flag values and strings
 * private:
 *
 * arg:   tOptions*,     pOpts,     the program options descriptor
 * arg:   tOptDesc*,     pOD,       enumeration option description
 * arg:   char const * const *,
 *                       paz_names, list of enumeration names
 * arg:   unsigned int,  name_ct,   number of names in list
 *
 * doc:   This converts the optArg.argString string from the option description
 *        into the index corresponding to an entry in the name list.
 *        This will match the generated enumeration value.
 *        Full matches are always accepted.  Partial matches are accepted
 *        if there is only one partial match.
=*/
void
optionSetMembers(
    tOptions*     pOpts,
    tOptDesc*     pOD,
    tCC* const *  paz_names,
    unsigned int  name_ct )
{
    /*
     *  IF the program option descriptor pointer is invalid,
     *  then it is some sort of special request.
     */
    switch ((uintptr_t)pOpts) {
    case 0UL:
        /*
         *  print the list of enumeration names.
         */
        enumError( pOpts, pOD, paz_names, (int)name_ct );
        return;

    case 1UL:
    {
        /*
         *  print the name string.
         */
        uintptr_t bits = (uintptr_t)pOD->optCookie;
        uintptr_t res  = 0;
        size_t    len  = 0;

        while (bits != 0) {
            if (bits & 1) {
                if (len++ > 0) fputs( " | ", stdout );
                fputs( paz_names[ res ], stdout );
            }
            if (++res >= name_ct) break;
            bits >>= 1;
        }
        return;
    }

    case 2UL:
    {
        char*     pz;
        uintptr_t bits = (uintptr_t)pOD->optCookie;
        uintptr_t res  = 0;
        size_t    len  = 0;

        /*
         *  Replace the enumeration value with the name string.
         *  First, determine the needed length, then allocate and fill in.
         */
        while (bits != 0) {
            if (bits & 1)
                len += strlen( paz_names[ res ]) + 8;
            if (++res >= name_ct) break;
            bits >>= 1;
        }

        pOD->optArg.argString = pz = AGALOC( len, "enum name" );

        /*
         *  Start by clearing all the bits.  We want to turn off any defaults
         *  because we will be restoring to current state, not adding to
         *  the default set of bits.
         */
        strcpy( pz, "none" );
        pz += 4;
        bits = (uintptr_t)pOD->optCookie;
        res = 0;
        while (bits != 0) {
            if (bits & 1) {
                strcpy( pz, " + " );
                strcpy( pz+3, paz_names[ res ]);
                pz += strlen( paz_names[ res ]) + 3;
            }
            if (++res >= name_ct) break;
            bits >>= 1;
        }
        return;
    }

    default:
        break;
    }

    {
        tCC*      pzArg = pOD->optArg.argString;
        uintptr_t res;
        if ((pzArg == NULL) || (*pzArg == NUL)) {
            pOD->optCookie = (void*)0;
            return;
        }

        res = (uintptr_t)pOD->optCookie;
        for (;;) {
            tSCC zSpn[] = " ,|+\t\r\f\n";
            int  iv, len;

            pzArg += strspn( pzArg, zSpn );
            iv = (*pzArg == '!');
            if (iv)
                pzArg += strspn( pzArg+1, zSpn ) + 1;

            len = strcspn( pzArg, zSpn );
            if (len == 0)
                break;

            if ((len == 3) && (strncmp(pzArg, zAll, (size_t)3) == 0)) {
                if (iv)
                     res = 0;
                else res = ~0UL;
            }
            else if ((len == 4) && (strncmp(pzArg, zNone, (size_t)4) == 0)) {
                if (! iv)
                    res = 0;
            }
            else do {
                char* pz;
                uintptr_t bit = strtoul( pzArg, &pz, 0 );

                if (pz != pzArg + len) {
                    char z[ AO_NAME_SIZE ];
                    tCC* p;
                    if (*pz != NUL) {
                        if (len >= AO_NAME_LIMIT)
                            break;
                        strncpy( z, pzArg, (size_t)len );
                        z[len] = NUL;
                        p = z;
                    } else {
                        p = pzArg;
                    }

                    bit = 1UL << findName(p, pOpts, pOD, paz_names, name_ct);
                }
                if (iv)
                     res &= ~bit;
                else res |= bit;
            } while (0);

            if (pzArg[len] == NUL)
                break;
            pzArg += len + 1;
        }
        if (name_ct < (8 * sizeof( uintptr_t ))) {
            res &= (1UL << name_ct) - 1UL;
        }

        pOD->optCookie = (void*)res;
    }
}

/*
 * Local Variables:
 * mode: C
 * c-file-style: "stroustrup"
 * indent-tabs-mode: nil
 * End:
 * end of autoopts/enumeration.c */
