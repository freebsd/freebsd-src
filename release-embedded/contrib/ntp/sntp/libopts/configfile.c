/*
 *  $Id: configfile.c,v 1.21 2007/04/15 19:01:18 bkorb Exp $
 *  Time-stamp:      "2007-04-15 11:22:46 bkorb"
 *
 *  configuration/rc/ini file handling.
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
filePreset(
    tOptions*     pOpts,
    char const*   pzFileName,
    int           direction );

static char*
handleComment( char* pzText );

static char*
handleConfig(
    tOptions*     pOpts,
    tOptState*    pOS,
    char*         pzText,
    int           direction );

static char*
handleDirective(
    tOptions*     pOpts,
    char*         pzText );

static char*
handleProgramSection(
    tOptions*     pOpts,
    char*         pzText );

static char*
handleStructure(
    tOptions*     pOpts,
    tOptState*    pOS,
    char*         pzText,
    int           direction );

static char*
parseKeyWordType(
    tOptions*     pOpts,
    char*         pzText,
    tOptionValue* pType );

static char*
parseLoadMode(
    char*               pzText,
    tOptionLoadMode*    pMode );

static char*
parseSetMemType(
    tOptions*     pOpts,
    char*         pzText,
    tOptionValue* pType );

static char*
parseValueType(
    char*         pzText,
    tOptionValue* pType );

static char*
skipUnknown( char* pzText );
/* = = = END-STATIC-FORWARD = = = */


/*=export_func  configFileLoad
 *
 * what:  parse a configuration file
 * arg:   + char const*     + pzFile + the file to load +
 *
 * ret_type:  const tOptionValue*
 * ret_desc:  An allocated, compound value structure
 *
 * doc:
 *  This routine will load a named configuration file and parse the
 *  text as a hierarchically valued option.  The option descriptor
 *  created from an option definition file is not used via this interface.
 *  The returned value is "named" with the input file name and is of
 *  type "@code{OPARG_TYPE_HIERARCHY}".  It may be used in calls to
 *  @code{optionGetValue()}, @code{optionNextValue()} and
 *  @code{optionUnloadNested()}.
 *
 * err:
 *  If the file cannot be loaded or processed, @code{NULL} is returned and
 *  @var{errno} is set.  It may be set by a call to either @code{open(2)}
 *  @code{mmap(2)} or other file system calls, or it may be:
 *  @itemize @bullet
 *  @item
 *  @code{ENOENT} - the file was empty.
 *  @item
 *  @code{EINVAL} - the file contents are invalid -- not properly formed.
 *  @item
 *  @code{ENOMEM} - not enough memory to allocate the needed structures.
 *  @end itemize
=*/
const tOptionValue*
configFileLoad( char const* pzFile )
{
    tmap_info_t   cfgfile;
    tOptionValue* pRes = NULL;
    tOptionLoadMode save_mode = option_load_mode;

    char* pzText =
        text_mmap( pzFile, PROT_READ, MAP_PRIVATE, &cfgfile );

    if (TEXT_MMAP_FAILED_ADDR(pzText))
        return NULL; /* errno is set */

    option_load_mode = OPTION_LOAD_COOKED;
    pRes = optionLoadNested(pzText, pzFile, strlen(pzFile));

    if (pRes == NULL) {
        int err = errno;
        text_munmap( &cfgfile );
        errno = err;
    } else
        text_munmap( &cfgfile );

    option_load_mode = save_mode;
    return pRes;
}


/*=export_func  optionFindValue
 *
 * what:  find a hierarcicaly valued option instance
 * arg:   + const tOptDesc* + pOptDesc + an option with a nested arg type +
 * arg:   + char const*     + name     + name of value to find +
 * arg:   + char const*     + value    + the matching value    +
 *
 * ret_type:  const tOptionValue*
 * ret_desc:  a compound value structure
 *
 * doc:
 *  This routine will find an entry in a nested value option or configurable.
 *  It will search through the list and return a matching entry.
 *
 * err:
 *  The returned result is NULL and errno is set:
 *  @itemize @bullet
 *  @item
 *  @code{EINVAL} - the @code{pOptValue} does not point to a valid
 *  hierarchical option value.
 *  @item
 *  @code{ENOENT} - no entry matched the given name.
 *  @end itemize
=*/
const tOptionValue*
optionFindValue( const tOptDesc* pOptDesc,
                 char const* pzName, char const* pzVal )
{
    const tOptionValue* pRes = NULL;

    if (  (pOptDesc == NULL)
       || (OPTST_GET_ARGTYPE(pOptDesc->fOptState) != OPARG_TYPE_HIERARCHY))  {
        errno = EINVAL;
    }

    else if (pOptDesc->optCookie == NULL) {
        errno = ENOENT;
    }

    else do {
        tArgList* pAL = pOptDesc->optCookie;
        int    ct   = pAL->useCt;
        void** ppOV = (void**)(pAL->apzArgs);

        if (ct == 0) {
            errno = ENOENT;
            break;
        }

        if (pzName == NULL) {
            pRes = (tOptionValue*)*ppOV;
            break;
        }

        while (--ct >= 0) {
            const tOptionValue* pOV = *(ppOV++);
            const tOptionValue* pRV = optionGetValue( pOV, pzName );

            if (pRV == NULL)
                continue;

            if (pzVal == NULL) {
                pRes = pOV;
                break;
            }
        }
        if (pRes == NULL)
            errno = ENOENT;
    } while (0);

    return pRes;
}


/*=export_func  optionFindNextValue
 *
 * what:  find a hierarcicaly valued option instance
 * arg:   + const tOptDesc* + pOptDesc + an option with a nested arg type +
 * arg:   + const tOptionValue* + pPrevVal + the last entry +
 * arg:   + char const*     + name     + name of value to find +
 * arg:   + char const*     + value    + the matching value    +
 *
 * ret_type:  const tOptionValue*
 * ret_desc:  a compound value structure
 *
 * doc:
 *  This routine will find the next entry in a nested value option or
 *  configurable.  It will search through the list and return the next entry
 *  that matches the criteria.
 *
 * err:
 *  The returned result is NULL and errno is set:
 *  @itemize @bullet
 *  @item
 *  @code{EINVAL} - the @code{pOptValue} does not point to a valid
 *  hierarchical option value.
 *  @item
 *  @code{ENOENT} - no entry matched the given name.
 *  @end itemize
=*/
const tOptionValue*
optionFindNextValue( const tOptDesc* pOptDesc, const tOptionValue* pPrevVal,
                 char const* pzName, char const* pzVal )
{
    int foundOldVal = 0;
    tOptionValue* pRes = NULL;

    if (  (pOptDesc == NULL)
       || (OPTST_GET_ARGTYPE(pOptDesc->fOptState) != OPARG_TYPE_HIERARCHY))  {
        errno = EINVAL;
    }

    else if (pOptDesc->optCookie == NULL) {
        errno = ENOENT;
    }

    else do {
        tArgList* pAL = pOptDesc->optCookie;
        int    ct   = pAL->useCt;
        void** ppOV = (void**)pAL->apzArgs;

        if (ct == 0) {
            errno = ENOENT;
            break;
        }

        while (--ct >= 0) {
            tOptionValue* pOV = *(ppOV++);
            if (foundOldVal) {
                pRes = pOV;
                break;
            }
            if (pOV == pPrevVal)
                foundOldVal = 1;
        }
        if (pRes == NULL)
            errno = ENOENT;
    } while (0);

    return pRes;
}


/*=export_func  optionGetValue
 *
 * what:  get a specific value from a hierarcical list
 * arg:   + const tOptionValue* + pOptValue + a hierarchcal value +
 * arg:   + char const*   + valueName + name of value to get +
 *
 * ret_type:  const tOptionValue*
 * ret_desc:  a compound value structure
 *
 * doc:
 *  This routine will find an entry in a nested value option or configurable.
 *  If "valueName" is NULL, then the first entry is returned.  Otherwise,
 *  the first entry with a name that exactly matches the argument will be
 *  returned.
 *
 * err:
 *  The returned result is NULL and errno is set:
 *  @itemize @bullet
 *  @item
 *  @code{EINVAL} - the @code{pOptValue} does not point to a valid
 *  hierarchical option value.
 *  @item
 *  @code{ENOENT} - no entry matched the given name.
 *  @end itemize
=*/
const tOptionValue*
optionGetValue( const tOptionValue* pOld, char const* pzValName )
{
    tArgList*     pAL;
    tOptionValue* pRes = NULL;

    if ((pOld == NULL) || (pOld->valType != OPARG_TYPE_HIERARCHY)) {
        errno = EINVAL;
        return NULL;
    }
    pAL = pOld->v.nestVal;

    if (pAL->useCt > 0) {
        int    ct    = pAL->useCt;
        void** papOV = (void**)(pAL->apzArgs);

        if (pzValName == NULL) {
            pRes = (tOptionValue*)*papOV;
        }

        else do {
            tOptionValue* pOV = *(papOV++);
            if (strcmp( pOV->pzName, pzValName ) == 0) {
                pRes = pOV;
                break;
            }
        } while (--ct > 0);
    }
    if (pRes == NULL)
        errno = ENOENT;
    return pRes;
}


/*=export_func  optionNextValue
 *
 * what:  get the next value from a hierarchical list
 * arg:   + const tOptionValue* + pOptValue + a hierarchcal list value +
 * arg:   + const tOptionValue* + pOldValue + a value from this list   +
 *
 * ret_type:  const tOptionValue*
 * ret_desc:  a compound value structure
 *
 * doc:
 *  This routine will return the next entry after the entry passed in.  At the
 *  end of the list, NULL will be returned.  If the entry is not found on the
 *  list, NULL will be returned and "@var{errno}" will be set to EINVAL.
 *  The "@var{pOldValue}" must have been gotten from a prior call to this
 *  routine or to "@code{opitonGetValue()}".
 *
 * err:
 *  The returned result is NULL and errno is set:
 *  @itemize @bullet
 *  @item
 *  @code{EINVAL} - the @code{pOptValue} does not point to a valid
 *  hierarchical option value or @code{pOldValue} does not point to a
 *  member of that option value.
 *  @item
 *  @code{ENOENT} - the supplied @code{pOldValue} pointed to the last entry.
 *  @end itemize
=*/
tOptionValue const *
optionNextValue(tOptionValue const * pOVList,tOptionValue const * pOldOV )
{
    tArgList*     pAL;
    tOptionValue* pRes = NULL;
    int           err  = EINVAL;

    if ((pOVList == NULL) || (pOVList->valType != OPARG_TYPE_HIERARCHY)) {
        errno = EINVAL;
        return NULL;
    }
    pAL = pOVList->v.nestVal;
    {
        int    ct    = pAL->useCt;
        void** papNV = (void**)(pAL->apzArgs);

        while (ct-- > 0) {
            tOptionValue* pNV = *(papNV++);
            if (pNV == pOldOV) {
                if (ct == 0) {
                    err = ENOENT;

                } else {
                    err  = 0;
                    pRes = (tOptionValue*)*papNV;
                }
                break;
            }
        }
    }
    if (err != 0)
        errno = err;
    return pRes;
}


/*  filePreset
 *
 *  Load a file containing presetting information (a configuration file).
 */
static void
filePreset(
    tOptions*     pOpts,
    char const*   pzFileName,
    int           direction )
{
    tmap_info_t   cfgfile;
    tOptState     st = OPTSTATE_INITIALIZER(PRESET);
    char*         pzFileText =
        text_mmap( pzFileName, PROT_READ|PROT_WRITE, MAP_PRIVATE, &cfgfile );

    if (TEXT_MMAP_FAILED_ADDR(pzFileText))
        return;

    if (direction == DIRECTION_CALLED) {
        st.flags  = OPTST_DEFINED;
        direction = DIRECTION_PROCESS;
    }

    /*
     *  IF this is called via "optionProcess", then we are presetting.
     *  This is the default and the PRESETTING bit will be set.
     *  If this is called via "optionFileLoad", then the bit is not set
     *  and we consider stuff set herein to be "set" by the client program.
     */
    if ((pOpts->fOptSet & OPTPROC_PRESETTING) == 0)
        st.flags = OPTST_SET;

    do  {
        while (isspace( (int)*pzFileText ))  pzFileText++;

        if (isalpha( (int)*pzFileText )) {
            pzFileText = handleConfig( pOpts, &st, pzFileText, direction );

        } else switch (*pzFileText) {
        case '<':
            if (isalpha( (int)pzFileText[1] ))
                pzFileText = handleStructure(pOpts, &st, pzFileText, direction);

            else switch (pzFileText[1]) {
            case '?':
                pzFileText = handleDirective( pOpts, pzFileText );
                break;

            case '!':
                pzFileText = handleComment( pzFileText );
                break;

            case '/':
                pzFileText = strchr( pzFileText+2, '>' );
                if (pzFileText++ != NULL)
                    break;

            default:
                goto all_done;
            }
            break;

        case '[':
            pzFileText = handleProgramSection( pOpts, pzFileText );
            break;

        case '#':
            pzFileText = strchr( pzFileText+1, '\n' );
            break;

        default:
            goto all_done; /* invalid format */
        }
    } while (pzFileText != NULL);

 all_done:
    text_munmap( &cfgfile );
}


/*  handleComment
 *
 *  "pzText" points to a "<!" sequence.
 *  Theoretically, we should ensure that it begins with "<!--",
 *  but actually I don't care that much.  It ends with "-->".
 */
static char*
handleComment( char* pzText )
{
    char* pz = strstr( pzText, "-->" );
    if (pz != NULL)
        pz += 3;
    return pz;
}


/*  handleConfig
 *
 *  "pzText" points to the start of some value name.
 *  The end of the entry is the end of the line that is not preceded by
 *  a backslash escape character.  The string value is always processed
 *  in "cooked" mode.
 */
static char*
handleConfig(
    tOptions*     pOpts,
    tOptState*    pOS,
    char*         pzText,
    int           direction )
{
    char* pzName = pzText++;
    char* pzEnd  = strchr( pzText, '\n' );

    if (pzEnd == NULL)
        return pzText + strlen(pzText);

    while (ISNAMECHAR( (int)*pzText ))  pzText++;
    while (isspace( (int)*pzText )) pzText++;
    if (pzText > pzEnd) {
    name_only:
        *pzEnd++ = NUL;
        loadOptionLine( pOpts, pOS, pzName, direction, OPTION_LOAD_UNCOOKED );
        return pzEnd;
    }

    /*
     *  Either the first character after the name is a ':' or '=',
     *  or else we must have skipped over white space.  Anything else
     *  is an invalid format and we give up parsing the text.
     */
    if ((*pzText == '=') || (*pzText == ':')) {
        while (isspace( (int)*++pzText ))   ;
        if (pzText > pzEnd)
            goto name_only;
    } else if (! isspace((int)pzText[-1]))
        return NULL;

    /*
     *  IF the value is continued, remove the backslash escape and push "pzEnd"
     *  on to a newline *not* preceded by a backslash.
     */
    if (pzEnd[-1] == '\\') {
        char* pcD = pzEnd-1;
        char* pcS = pzEnd;

        for (;;) {
            char ch = *(pcS++);
            switch (ch) {
            case NUL:
                pcS = NULL;

            case '\n':
                *pcD = NUL;
                pzEnd = pcS;
                goto copy_done;

            case '\\':
                if (*pcS == '\n') {
                    ch = *(pcS++);
                }
                /* FALLTHROUGH */
            default:
                *(pcD++) = ch;
            }
        } copy_done:;

    } else {
        /*
         *  The newline was not preceded by a backslash.  NUL it out
         */
        *(pzEnd++) = NUL;
    }

    /*
     *  "pzName" points to what looks like text for one option/configurable.
     *  It is NUL terminated.  Process it.
     */
    loadOptionLine( pOpts, pOS, pzName, direction, OPTION_LOAD_UNCOOKED );

    return pzEnd;
}


/*  handleDirective
 *
 *  "pzText" points to a "<?" sequence.
 *  For the moment, we only handle "<?program" directives.
 */
static char*
handleDirective(
    tOptions*     pOpts,
    char*         pzText )
{
    char   ztitle[32] = "<?";
    size_t title_len = strlen( zProg );
    size_t name_len;

    if (  (strncmp( pzText+2, zProg, title_len ) != 0)
       || (! isspace( (int)pzText[title_len+2] )) )  {
        pzText = strchr( pzText+2, '>' );
        if (pzText != NULL)
            pzText++;
        return pzText;
    }

    name_len = strlen( pOpts->pzProgName );
    strcpy( ztitle+2, zProg );
    title_len += 2;

    do  {
        pzText += title_len;

        if (isspace((int)*pzText)) {
            while (isspace((int)*pzText))  pzText++;
            if (  (strneqvcmp( pzText, pOpts->pzProgName, (int)name_len) == 0)
               && (pzText[name_len] == '>'))  {
                pzText += name_len + 1;
                break;
            }
        }

        pzText = strstr( pzText, ztitle );
    } while (pzText != NULL);

    return pzText;
}


/*  handleProgramSection
 *
 *  "pzText" points to a '[' character.
 *  The "traditional" [PROG_NAME] segmentation of the config file.
 *  Do not ever mix with the "<?program prog-name>" variation.
 */
static char*
handleProgramSection(
    tOptions*     pOpts,
    char*         pzText )
{
    size_t len = strlen( pOpts->pzPROGNAME );
    if (   (strncmp( pzText+1, pOpts->pzPROGNAME, len ) == 0)
        && (pzText[len+1] == ']'))
        return strchr( pzText + len + 2, '\n' );

    if (len > 16)
        return NULL;

    {
        char z[24];
        sprintf( z, "[%s]", pOpts->pzPROGNAME );
        pzText = strstr( pzText, z );
    }

    if (pzText != NULL)
        pzText = strchr( pzText, '\n' );
    return pzText;
}


/*  handleStructure
 *
 *  "pzText" points to a '<' character, followed by an alpha.
 *  The end of the entry is either the "/>" following the name, or else a
 *  "</name>" string.
 */
static char*
handleStructure(
    tOptions*     pOpts,
    tOptState*    pOS,
    char*         pzText,
    int           direction )
{
    tOptionLoadMode mode = option_load_mode;
    tOptionValue     valu;

    char* pzName = ++pzText;
    char* pzData;
    char* pcNulPoint;

    while (ISNAMECHAR( *pzText ))  pzText++;
    pcNulPoint = pzText;
    valu.valType = OPARG_TYPE_STRING;

    switch (*pzText) {
    case ' ':
    case '\t':
        pzText = parseAttributes( pOpts, pzText, &mode, &valu );
        if (*pzText == '>')
            break;
        if (*pzText != '/')
            return NULL;
        /* FALLTHROUGH */

    case '/':
        if (pzText[1] != '>')
            return NULL;
        *pzText = NUL;
        pzText += 2;
        loadOptionLine( pOpts, pOS, pzName, direction, mode );
        return pzText;

    case '>':
        break;

    default:
        pzText = strchr( pzText, '>');
        if (pzText != NULL)
            pzText++;
        return pzText;
    }

    /*
     *  If we are here, we have a value.  "pzText" points to a closing angle
     *  bracket.  Separate the name from the value for a moment.
     */
    *pcNulPoint = NUL;
    pzData = ++pzText;

    /*
     *  Find the end of the option text and NUL terminate it
     */
    {
        char   z[64], *pz = z;
        size_t len = strlen(pzName) + 4;
        if (len > sizeof(z))
            pz = AGALOC(len, "scan name");

        sprintf( pz, "</%s>", pzName );
        *pzText = ' ';
        pzText = strstr( pzText, pz );
        if (pz != z) AGFREE(pz);

        if (pzText == NULL)
            return pzText;

        *pzText = NUL;

        pzText += len-1;
    }

    /*
     *  Rejoin the name and value for parsing by "loadOptionLine()".
     *  Erase any attributes parsed by "parseAttributes()".
     */
    memset(pcNulPoint, ' ', pzData - pcNulPoint);

    /*
     *  "pzName" points to what looks like text for one option/configurable.
     *  It is NUL terminated.  Process it.
     */
    loadOptionLine( pOpts, pOS, pzName, direction, mode );

    return pzText;
}


/*  internalFileLoad
 *
 *  Load a configuration file.  This may be invoked either from
 *  scanning the "homerc" list, or from a specific file request.
 *  (see "optionFileLoad()", the implementation for --load-opts)
 */
LOCAL void
internalFileLoad( tOptions* pOpts )
{
    int     idx;
    int     inc = DIRECTION_PRESET;
    char    zFileName[ AG_PATH_MAX+1 ];

    if (pOpts->papzHomeList == NULL)
        return;

    /*
     *  Find the last RC entry (highest priority entry)
     */
    for (idx = 0; pOpts->papzHomeList[ idx+1 ] != NULL; ++idx)  ;

    /*
     *  For every path in the home list, ...  *TWICE* We start at the last
     *  (highest priority) entry, work our way down to the lowest priority,
     *  handling the immediate options.
     *  Then we go back up, doing the normal options.
     */
    for (;;) {
        struct stat StatBuf;
        cch_t*  pzPath;

        /*
         *  IF we've reached the bottom end, change direction
         */
        if (idx < 0) {
            inc = DIRECTION_PROCESS;
            idx = 0;
        }

        pzPath = pOpts->papzHomeList[ idx ];

        /*
         *  IF we've reached the top end, bail out
         */
        if (pzPath == NULL)
            break;

        idx += inc;

        if (! optionMakePath( zFileName, (int)sizeof(zFileName),
                              pzPath, pOpts->pzProgPath ))
            continue;

        /*
         *  IF the file name we constructed is a directory,
         *  THEN append the Resource Configuration file name
         *  ELSE we must have the complete file name
         */
        if (stat( zFileName, &StatBuf ) != 0)
            continue; /* bogus name - skip the home list entry */

        if (S_ISDIR( StatBuf.st_mode )) {
            size_t len = strlen( zFileName );
            char* pz;

            if (len + 1 + strlen( pOpts->pzRcName ) >= sizeof( zFileName ))
                continue;

            pz = zFileName + len;
            if (pz[-1] != DIRCH)
                *(pz++) = DIRCH;
            strcpy( pz, pOpts->pzRcName );
        }

        filePreset( pOpts, zFileName, inc );

        /*
         *  IF we are now to skip config files AND we are presetting,
         *  THEN change direction.  We must go the other way.
         */
        {
            tOptDesc * pOD = pOpts->pOptDesc + pOpts->specOptIdx.save_opts+1;
            if (DISABLED_OPT(pOD) && PRESETTING(inc)) {
                idx -= inc;  /* go back and reprocess current file */
                inc =  DIRECTION_PROCESS;
            }
        }
    } /* twice for every path in the home list, ... */
}


/*=export_func optionFileLoad
 *
 * what: Load the locatable config files, in order
 *
 * arg:  + tOptions*   + pOpts  + program options descriptor +
 * arg:  + char const* + pzProg + program name +
 *
 * ret_type:  int
 * ret_desc:  0 -> SUCCESS, -1 -> FAILURE
 *
 * doc:
 *
 * This function looks in all the specified directories for a configuration
 * file ("rc" file or "ini" file) and processes any found twice.  The first
 * time through, they are processed in reverse order (last file first).  At
 * that time, only "immediate action" configurables are processed.  For
 * example, if the last named file specifies not processing any more
 * configuration files, then no more configuration files will be processed.
 * Such an option in the @strong{first} named directory will have no effect.
 *
 * Once the immediate action configurables have been handled, then the
 * directories are handled in normal, forward order.  In that way, later
 * config files can override the settings of earlier config files.
 *
 * See the AutoOpts documentation for a thorough discussion of the
 * config file format.
 *
 * Configuration files not found or not decipherable are simply ignored.
 *
 * err:  Returns the value, "-1" if the program options descriptor
 *       is out of date or indecipherable.  Otherwise, the value "0" will
 *       always be returned.
=*/
int
optionFileLoad( tOptions* pOpts, char const* pzProgram )
{
    if (! SUCCESSFUL( validateOptionsStruct( pOpts, pzProgram )))
        return -1;

    pOpts->pzProgName = pzProgram;
    internalFileLoad( pOpts );
    return 0;
}


/*=export_func  optionLoadOpt
 * private:
 *
 * what:  Load an option rc/ini file
 * arg:   + tOptions* + pOpts    + program options descriptor +
 * arg:   + tOptDesc* + pOptDesc + the descriptor for this arg +
 *
 * doc:
 *  Processes the options found in the file named with
 *  pOptDesc->optArg.argString.
=*/
void
optionLoadOpt( tOptions* pOpts, tOptDesc* pOptDesc )
{
    /*
     *  IF the option is not being disabled, THEN load the file.  There must
     *  be a file.  (If it is being disabled, then the disablement processing
     *  already took place.  It must be done to suppress preloading of ini/rc
     *  files.)
     */
    if (! DISABLED_OPT( pOptDesc )) {
        struct stat sb;
        if (stat( pOptDesc->optArg.argString, &sb ) != 0) {
            if ((pOpts->fOptSet & OPTPROC_ERRSTOP) == 0)
                return;

            fprintf( stderr, zFSErrOptLoad, errno, strerror( errno ),
                     pOptDesc->optArg.argString );
            exit(EX_NOINPUT);
            /* NOT REACHED */
        }

        if (! S_ISREG( sb.st_mode )) {
            if ((pOpts->fOptSet & OPTPROC_ERRSTOP) == 0)
                return;

            fprintf( stderr, zNotFile, pOptDesc->optArg.argString );
            exit(EX_NOINPUT);
            /* NOT REACHED */
        }

        filePreset(pOpts, pOptDesc->optArg.argString, DIRECTION_CALLED);
    }
}


/*  parseAttributes
 *
 *  Parse the various attributes of an XML-styled config file entry
 */
LOCAL char*
parseAttributes(
    tOptions*           pOpts,
    char*               pzText,
    tOptionLoadMode*    pMode,
    tOptionValue*       pType )
{
    size_t lenLoadType = strlen( zLoadType );
    size_t lenKeyWords = strlen( zKeyWords );
    size_t lenSetMem   = strlen( zSetMembers );

    do  {
        switch (*pzText) {
        case '/': pType->valType = OPARG_TYPE_NONE;
        case '>': return pzText;

        default:
        case NUL: return NULL;

        case ' ':
        case '\t':
        case '\n':
        case '\f':
        case '\r':
        case '\v':
            break;
        }

        while (isspace( (int)*++pzText ))   ;

        if (strncmp( pzText, zLoadType, lenLoadType ) == 0) {
            pzText = parseValueType( pzText+lenLoadType, pType );
            continue;
        }

        if (strncmp( pzText, zKeyWords, lenKeyWords ) == 0) {
            pzText = parseKeyWordType( pOpts, pzText+lenKeyWords, pType );
            continue;
        }

        if (strncmp( pzText, zSetMembers, lenSetMem ) == 0) {
            pzText = parseSetMemType( pOpts, pzText+lenSetMem, pType );
            continue;
        }

        pzText = parseLoadMode( pzText, pMode );
    } while (pzText != NULL);

    return pzText;
}


/*  parseKeyWordType
 *
 *  "pzText" points to the character after "words=".
 *  What should follow is a name of a keyword (enumeration) list.
 */
static char*
parseKeyWordType(
    tOptions*     pOpts,
    char*         pzText,
    tOptionValue* pType )
{
    return skipUnknown( pzText );
}


/*  parseLoadMode
 *
 *  "pzText" points to some name character.  We check for "cooked" or
 *  "uncooked" or "keep".  This function should handle any attribute
 *  that does not have an associated value.
 */
static char*
parseLoadMode(
    char*               pzText,
    tOptionLoadMode*    pMode )
{
    {
        size_t len = strlen(zLoadCooked);
        if (strncmp( pzText, zLoadCooked, len ) == 0) {
            if (  (pzText[len] == '>')
               || (pzText[len] == '/')
               || isspace((int)pzText[len])) {
                *pMode = OPTION_LOAD_COOKED;
                return pzText + len;
            }
            goto unknown;
        }
    }

    {
        size_t len = strlen(zLoadUncooked);
        if (strncmp( pzText, zLoadUncooked, len ) == 0) {
            if (  (pzText[len] == '>')
               || (pzText[len] == '/')
               || isspace((int)pzText[len])) {
                *pMode = OPTION_LOAD_UNCOOKED;
                return pzText + len;
            }
            goto unknown;
        }
    }

    {
        size_t len = strlen(zLoadKeep);
        if (strncmp( pzText, zLoadKeep, len ) == 0) {
            if (  (pzText[len] == '>')
               || (pzText[len] == '/')
               || isspace((int)pzText[len])) {
                *pMode = OPTION_LOAD_KEEP;
                return pzText + len;
            }
            goto unknown;
        }
    }

  unknown:
    return skipUnknown( pzText );
}


/*  parseSetMemType
 *
 *  "pzText" points to the character after "members="
 *  What should follow is a name of a "set membership".
 *  A collection of bit flags.
 */
static char*
parseSetMemType(
    tOptions*     pOpts,
    char*         pzText,
    tOptionValue* pType )
{
    return skipUnknown( pzText );
}


/*  parseValueType
 *
 *  "pzText" points to the character after "type="
 */
static char*
parseValueType(
    char*         pzText,
    tOptionValue* pType )
{
    {
        size_t len = strlen(zLtypeString);
        if (strncmp( pzText, zLtypeString, len ) == 0) {
            if ((pzText[len] == '>') || isspace((int)pzText[len])) {
                pType->valType = OPARG_TYPE_STRING;
                return pzText + len;
            }
            goto unknown;
        }
    }

    {
        size_t len = strlen(zLtypeInteger);
        if (strncmp( pzText, zLtypeInteger, len ) == 0) {
            if ((pzText[len] == '>') || isspace((int)pzText[len])) {
                pType->valType = OPARG_TYPE_NUMERIC;
                return pzText + len;
            }
            goto unknown;
        }
    }

    {
        size_t len = strlen(zLtypeBool);
        if (strncmp( pzText, zLtypeBool, len ) == 0) {
            if ((pzText[len] == '>') || isspace(pzText[len])) {
                pType->valType = OPARG_TYPE_BOOLEAN;
                return pzText + len;
            }
            goto unknown;
        }
    }

    {
        size_t len = strlen(zLtypeKeyword);
        if (strncmp( pzText, zLtypeKeyword, len ) == 0) {
            if ((pzText[len] == '>') || isspace((int)pzText[len])) {
                pType->valType = OPARG_TYPE_ENUMERATION;
                return pzText + len;
            }
            goto unknown;
        }
    }

    {
        size_t len = strlen(zLtypeSetMembership);
        if (strncmp( pzText, zLtypeSetMembership, len ) == 0) {
            if ((pzText[len] == '>') || isspace((int)pzText[len])) {
                pType->valType = OPARG_TYPE_MEMBERSHIP;
                return pzText + len;
            }
            goto unknown;
        }
    }

    {
        size_t len = strlen(zLtypeNest);
        if (strncmp( pzText, zLtypeNest, len ) == 0) {
            if ((pzText[len] == '>') || isspace((int)pzText[len])) {
                pType->valType = OPARG_TYPE_HIERARCHY;
                return pzText + len;
            }
            goto unknown;
        }
    }

  unknown:
    pType->valType = OPARG_TYPE_NONE;
    return skipUnknown( pzText );
}


/*  skipUnknown
 *
 *  Skip over some unknown attribute
 */
static char*
skipUnknown( char* pzText )
{
    for (;; pzText++) {
        if (isspace( (int)*pzText ))  return pzText;
        switch (*pzText) {
        case NUL: return NULL;
        case '/':
        case '>': return pzText;
        }
    }
}


/*  validateOptionsStruct
 *
 *  Make sure the option descriptor is there and that we understand it.
 *  This should be called from any user entry point where one needs to
 *  worry about validity.  (Some entry points are free to assume that
 *  the call is not the first to the library and, thus, that this has
 *  already been called.)
 */
LOCAL tSuccess
validateOptionsStruct( tOptions* pOpts, char const* pzProgram )
{
    if (pOpts == NULL) {
        fputs( zAO_Bad, stderr );
        exit( EX_CONFIG );
    }

    /*
     *  IF the client has enabled translation and the translation procedure
     *  is available, then go do it.
     */
    if (  ((pOpts->fOptSet & OPTPROC_TRANSLATE) != 0)
       && (pOpts->pTransProc != 0) ) {
        (*pOpts->pTransProc)();
        pOpts->fOptSet &= ~OPTPROC_TRANSLATE;
    }

    /*
     *  IF the struct version is not the current, and also
     *     either too large (?!) or too small,
     *  THEN emit error message and fail-exit
     */
    if (  ( pOpts->structVersion  != OPTIONS_STRUCT_VERSION  )
       && (  (pOpts->structVersion > OPTIONS_STRUCT_VERSION  )
          || (pOpts->structVersion < OPTIONS_MINIMUM_VERSION )
       )  )  {

        fprintf( stderr, zAO_Err, pOpts->origArgVect[0],
                 NUM_TO_VER( pOpts->structVersion ));
        if (pOpts->structVersion > OPTIONS_STRUCT_VERSION )
            fputs( zAO_Big, stderr );
        else
            fputs( zAO_Sml, stderr );

        return FAILURE;
    }

    /*
     *  If the program name hasn't been set, then set the name and the path
     *  and the set of equivalent characters.
     */
    if (pOpts->pzProgName == NULL) {
        char const* pz = strrchr( pzProgram, DIRCH );

        if (pz == NULL)
             pOpts->pzProgName = pzProgram;
        else pOpts->pzProgName = pz+1;

        pOpts->pzProgPath = pzProgram;

        /*
         *  when comparing long names, these are equivalent
         */
        strequate( zSepChars );
    }

    return SUCCESS;
}


/**
 * Local Variables:
 * mode: C
 * c-file-style: "stroustrup"
 * indent-tabs-mode: nil
 * End:
 * end of autoopts/configfile.c */
