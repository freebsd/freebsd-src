
/*
 *  save.c  $Id: save.c,v 4.18 2007/04/15 19:01:18 bkorb Exp $
 * Time-stamp:      "2007-04-15 11:11:10 bkorb"
 *
 *  This module's routines will take the currently set options and
 *  store them into an ".rc" file for re-interpretation the next
 *  time the invoking program is run.
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

tSCC  zWarn[] = "%s WARNING:  cannot save options - ";

/* = = = START-STATIC-FORWARD = = = */
/* static forward declarations maintained by :mkfwd */
static tCC*
findDirName( tOptions* pOpts, int* p_free );

static tCC*
findFileName( tOptions* pOpts, int* p_free_name );

static void
printEntry(
    FILE *     fp,
    tOptDesc * p,
    tCC*       pzLA );
/* = = = END-STATIC-FORWARD = = = */

static tCC*
findDirName( tOptions* pOpts, int* p_free )
{
    tCC*  pzDir;

    if (pOpts->specOptIdx.save_opts == 0)
        return NULL;

    pzDir = pOpts->pOptDesc[ pOpts->specOptIdx.save_opts ].optArg.argString;
    if ((pzDir != NULL) && (*pzDir != NUL))
        return pzDir;

    /*
     *  This function only works if there is a directory where
     *  we can stash the RC (INI) file.
     */
    {
        tCC* const* papz = pOpts->papzHomeList;
        if (papz == NULL)
            return NULL;

        while (papz[1] != NULL) papz++;
        pzDir = *papz;
    }

    /*
     *  IF it does not require deciphering an env value, then just copy it
     */
    if (*pzDir != '$')
        return pzDir;

    {
        tCC*  pzEndDir = strchr( ++pzDir, DIRCH );
        char* pzFileName;
        char* pzEnv;

        if (pzEndDir != NULL) {
            char z[ AO_NAME_SIZE ];
            if ((pzEndDir - pzDir) > AO_NAME_LIMIT )
                return NULL;
            strncpy( z, pzDir, (size_t)(pzEndDir - pzDir) );
            z[ (pzEndDir - pzDir) ] = NUL;
            pzEnv = getenv( z );
        } else {

            /*
             *  Make sure we can get the env value (after stripping off
             *  any trailing directory or file names)
             */
            pzEnv = getenv( pzDir );
        }

        if (pzEnv == NULL) {
            fprintf( stderr, zWarn, pOpts->pzProgName );
            fprintf( stderr, zNotDef, pzDir );
            return NULL;
        }

        if (pzEndDir == NULL)
            return pzEnv;

        {
            size_t sz = strlen( pzEnv ) + strlen( pzEndDir ) + 2;
            pzFileName = (char*)AGALOC( sz, "dir name" );
        }

        if (pzFileName == NULL)
            return NULL;

        *p_free = 1;
        /*
         *  Glue together the full name into the allocated memory.
         *  FIXME: We lose track of this memory.
         */
        sprintf( pzFileName, "%s/%s", pzEnv, pzEndDir );
        return pzFileName;
    }
}


static tCC*
findFileName( tOptions* pOpts, int* p_free_name )
{
    tCC*   pzDir;
    struct stat stBuf;
    int    free_dir_name = 0;

    pzDir = findDirName( pOpts, &free_dir_name );
    if (pzDir == NULL)
        return NULL;

    /*
     *  See if we can find the specified directory.  We use a once-only loop
     *  structure so we can bail out early.
     */
    if (stat( pzDir, &stBuf ) != 0) do {

        /*
         *  IF we could not, check to see if we got a full
         *  path to a file name that has not been created yet.
         */
        if (errno == ENOENT) {
            char z[AG_PATH_MAX];

            /*
             *  Strip off the last component, stat the remaining string and
             *  that string must name a directory
             */
            char* pzDirCh = strrchr( pzDir, DIRCH );
            if (pzDirCh == NULL) {
                stBuf.st_mode = S_IFREG;
                continue;  /* bail out of error condition */
            }

            strncpy( z, pzDir, (size_t)(pzDirCh - pzDir));
            z[ pzDirCh - pzDir ] = NUL;

            if (  (stat( z, &stBuf ) == 0)
               && S_ISDIR( stBuf.st_mode )) {

                /*
                 *  We found the directory.  Restore the file name and
                 *  mark the full name as a regular file
                 */
                stBuf.st_mode = S_IFREG;
                continue;  /* bail out of error condition */
            }
        }

        /*
         *  We got a bogus name.
         */
        fprintf( stderr, zWarn, pOpts->pzProgName );
        fprintf( stderr, zNoStat, errno, strerror( errno ), pzDir );
        if (free_dir_name)
            AGFREE( (void*)pzDir );
        return NULL;
    } while (0);

    /*
     *  IF what we found was a directory,
     *  THEN tack on the config file name
     */
    if (S_ISDIR( stBuf.st_mode )) {
        size_t sz = strlen( pzDir ) + strlen( pOpts->pzRcName ) + 2;

        {
            char*  pzPath = (char*)AGALOC( sz, "file name" );
#ifdef HAVE_SNPRINTF
            snprintf( pzPath, sz, "%s/%s", pzDir, pOpts->pzRcName );
#else
            sprintf( pzPath, "%s/%s", pzDir, pOpts->pzRcName );
#endif
            if (free_dir_name)
                AGFREE( (void*)pzDir );
            pzDir = pzPath;
            free_dir_name = 1;
        }

        /*
         *  IF we cannot stat the object for any reason other than
         *     it does not exist, then we bail out
         */
        if (stat( pzDir, &stBuf ) != 0) {
            if (errno != ENOENT) {
                fprintf( stderr, zWarn, pOpts->pzProgName );
                fprintf( stderr, zNoStat, errno, strerror( errno ),
                         pzDir );
                AGFREE( (void*)pzDir );
                return NULL;
            }

            /*
             *  It does not exist yet, but it will be a regular file
             */
            stBuf.st_mode = S_IFREG;
        }
    }

    /*
     *  Make sure that whatever we ultimately found, that it either is
     *  or will soon be a file.
     */
    if (! S_ISREG( stBuf.st_mode )) {
        fprintf( stderr, zWarn, pOpts->pzProgName );
        fprintf( stderr, zNotFile, pzDir );
        if (free_dir_name)
            AGFREE( (void*)pzDir );
        return NULL;
    }

    /*
     *  Get rid of the old file
     */
    unlink( pzDir );
    *p_free_name = free_dir_name;
    return pzDir;
}


static void
printEntry(
    FILE *     fp,
    tOptDesc * p,
    tCC*       pzLA )
{
    /*
     *  There is an argument.  Pad the name so values line up.
     *  Not disabled *OR* this got equivalenced to another opt,
     *  then use current option name.
     *  Otherwise, there must be a disablement name.
     */
    {
        char const * pz;
        if (! DISABLED_OPT(p) || (p->optEquivIndex != NO_EQUIVALENT))
            pz = p->pz_Name;
        else
            pz = p->pz_DisableName;

        fprintf(fp, "%-18s", pz);
    }
    /*
     *  IF the option is numeric only,
     *  THEN the char pointer is really the number
     */
    if (OPTST_GET_ARGTYPE(p->fOptState) == OPARG_TYPE_NUMERIC)
        fprintf( fp, "  %d\n", (int)(t_word)pzLA );

    /*
     *  OTHERWISE, FOR each line of the value text, ...
     */
    else if (pzLA == NULL)
        fputc( '\n', fp );

    else {
        fputc( ' ', fp ); fputc( ' ', fp );
        for (;;) {
            tCC* pzNl = strchr( pzLA, '\n' );

            /*
             *  IF this is the last line
             *  THEN bail and print it
             */
            if (pzNl == NULL)
                break;

            /*
             *  Print the continuation and the text from the current line
             */
            (void)fwrite( pzLA, (size_t)(pzNl - pzLA), (size_t)1, fp );
            pzLA = pzNl+1; /* advance the Last Arg pointer */
            fputs( "\\\n", fp );
        }

        /*
         *  Terminate the entry
         */
        fputs( pzLA, fp );
        fputc( '\n', fp );
    }
}


/*=export_func  optionSaveFile
 *
 * what:  saves the option state to a file
 *
 * arg:   tOptions*,   pOpts,  program options descriptor
 *
 * doc:
 *
 * This routine will save the state of option processing to a file.  The name
 * of that file can be specified with the argument to the @code{--save-opts}
 * option, or by appending the @code{rcfile} attribute to the last
 * @code{homerc} attribute.  If no @code{rcfile} attribute was specified, it
 * will default to @code{.@i{programname}rc}.  If you wish to specify another
 * file, you should invoke the @code{SET_OPT_SAVE_OPTS( @i{filename} )} macro.
 *
 * err:
 *
 * If no @code{homerc} file was specified, this routine will silently return
 * and do nothing.  If the output file cannot be created or updated, a message
 * will be printed to @code{stderr} and the routine will return.
=*/
void
optionSaveFile( tOptions* pOpts )
{
    tOptDesc* pOD;
    int       ct;
    FILE*     fp;

    {
        int   free_name = 0;
        tCC*  pzFName = findFileName( pOpts, &free_name );
        if (pzFName == NULL)
            return;

        fp = fopen( pzFName, "w" FOPEN_BINARY_FLAG );
        if (fp == NULL) {
            fprintf( stderr, zWarn, pOpts->pzProgName );
            fprintf( stderr, zNoCreat, errno, strerror( errno ), pzFName );
            if (free_name)
                AGFREE((void*) pzFName );
            return;
        }

        if (free_name)
            AGFREE( (void*)pzFName );
    }

    {
        char const*  pz = pOpts->pzUsageTitle;
        fputs( "#  ", fp );
        do { fputc( *pz, fp ); } while (*(pz++) != '\n');
    }

    {
        time_t  timeVal = time( NULL );
        char*   pzTime  = ctime( &timeVal );

        fprintf( fp, zPresetFile, pzTime );
#ifdef HAVE_ALLOCATED_CTIME
        /*
         *  The return values for ctime(), localtime(), and gmtime()
         *  normally point to static data that is overwritten by each call.
         *  The test to detect allocated ctime, so we leak the memory.
         */
        AGFREE( (void*)pzTime );
#endif
    }

    /*
     *  FOR each of the defined options, ...
     */
    ct  = pOpts->presetOptCt;
    pOD = pOpts->pOptDesc;
    do  {
        int arg_state;
        tOptDesc*  p;

        /*
         *  IF    the option has not been defined
         *     OR it does not take an initialization value
         *     OR it is equivalenced to another option
         *  THEN continue (ignore it)
         */
        if (UNUSED_OPT( pOD ))
            continue;

        if ((pOD->fOptState & (OPTST_NO_INIT|OPTST_DOCUMENT|OPTST_OMITTED))
            != 0)
            continue;

        if (  (pOD->optEquivIndex != NO_EQUIVALENT)
              && (pOD->optEquivIndex != pOD->optIndex))
            continue;

        /*
         *  Set a temporary pointer to the real option description
         *  (i.e. account for equivalencing)
         */
        p = ((pOD->fOptState & OPTST_EQUIVALENCE) != 0)
            ? (pOpts->pOptDesc + pOD->optActualIndex) : pOD;

        /*
         *  IF    no arguments are allowed
         *  THEN just print the name and continue
         */
        if (OPTST_GET_ARGTYPE(pOD->fOptState) == OPARG_TYPE_NONE) {
            char const * pznm =
                (DISABLED_OPT( p )) ? p->pz_DisableName : p->pz_Name;
            /*
             *  If the option was disabled and the disablement name is NULL,
             *  then the disablement was caused by aliasing.
             *  Use the name as the string to emit.
             */
            if (pznm == NULL)
                pznm = p->pz_Name;

            fprintf(fp, "%s\n", pznm);
            continue;
        }

        arg_state = OPTST_GET_ARGTYPE(p->fOptState);
        switch (arg_state) {
        case 0:
        case OPARG_TYPE_NUMERIC:
            printEntry( fp, p, (void*)(p->optArg.argInt));
            break;

        case OPARG_TYPE_STRING:
            if (p->fOptState & OPTST_STACKED) {
                tArgList*  pAL = (tArgList*)p->optCookie;
                int        uct = pAL->useCt;
                tCC**      ppz = pAL->apzArgs;

                /*
                 *  Disallow multiple copies of disabled options.
                 */
                if (uct > 1)
                    p->fOptState &= ~OPTST_DISABLED;

                while (uct-- > 0)
                    printEntry( fp, p, *(ppz++) );
            } else {
                printEntry( fp, p, p->optArg.argString );
            }
            break;

        case OPARG_TYPE_ENUMERATION:
        case OPARG_TYPE_MEMBERSHIP:
        {
            uintptr_t val = p->optArg.argEnum;
            /*
             *  This is a magic incantation that will convert the
             *  bit flag values back into a string suitable for printing.
             */
            (*(p->pOptProc))( (tOptions*)2UL, p );
            printEntry( fp, p, (void*)(p->optArg.argString));

            if (  (p->optArg.argString != NULL)
               && (arg_state != OPARG_TYPE_ENUMERATION)) {
                /*
                 *  set membership strings get allocated
                 */
                AGFREE( (void*)p->optArg.argString );
                p->fOptState &= ~OPTST_ALLOC_ARG;
            }

            p->optArg.argEnum = val;
            break;
        }

        case OPARG_TYPE_BOOLEAN:
            printEntry( fp, p, p->optArg.argBool ? "true" : "false" );
            break;

        default:
            break; /* cannot handle - skip it */
        }
    } while ( (pOD++), (--ct > 0));

    fclose( fp );
}
/*
 * Local Variables:
 * mode: C
 * c-file-style: "stroustrup"
 * indent-tabs-mode: nil
 * End:
 * end of autoopts/save.c */
