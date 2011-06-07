
/*
 *  $Id: nested.c,v 4.14 2007/02/04 17:44:12 bkorb Exp $
 *  Time-stamp:      "2007-01-26 11:04:35 bkorb"
 *
 *   Automated Options Nested Values module.
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
removeBackslashes( char* pzSrc );

static char const*
scanQuotedString( char const* pzTxt );

static tOptionValue*
addStringValue( void** pp, char const* pzName, size_t nameLen,
                char const* pzValue, size_t dataLen );

static tOptionValue*
addBoolValue( void** pp, char const* pzName, size_t nameLen,
                char const* pzValue, size_t dataLen );

static tOptionValue*
addNumberValue( void** pp, char const* pzName, size_t nameLen,
                char const* pzValue, size_t dataLen );

static tOptionValue*
addNestedValue( void** pp, char const* pzName, size_t nameLen,
                char* pzValue, size_t dataLen );

static char const*
scanNameEntry(char const* pzName, tOptionValue* pRes);

static char const*
scanXmlEntry( char const* pzName, tOptionValue* pRes );

static void
unloadNestedArglist( tArgList* pAL );

static void
sortNestedList( tArgList* pAL );
/* = = = END-STATIC-FORWARD = = = */

/*  removeBackslashes
 *
 *  This function assumes that all newline characters were preceeded by
 *  backslashes that need removal.
 */
static void
removeBackslashes( char* pzSrc )
{
    char* pzD = strchr(pzSrc, '\n');

    if (pzD == NULL)
        return;
    *--pzD = '\n';

    for (;;) {
        char ch = ((*pzD++) = *(pzSrc++));
        switch (ch) {
        case '\n': *--pzD = ch; break;
        case NUL:  return;
        default:
            ;
        }
    }
}


/*  scanQuotedString
 *
 *  Find the end of a quoted string, skipping escaped quote characters.
 */
static char const*
scanQuotedString( char const* pzTxt )
{
    char q = *(pzTxt++); /* remember the type of quote */

    for (;;) {
        char ch = *(pzTxt++);
        if (ch == NUL)
            return pzTxt-1;

        if (ch == q)
            return pzTxt;

        if (ch == '\\') {
            ch = *(pzTxt++);
            /*
             *  IF the next character is NUL, drop the backslash, too.
             */
            if (ch == NUL)
                return pzTxt - 2;

            /*
             *  IF the quote character or the escape character were escaped,
             *  then skip both, as long as the string does not end.
             */
            if ((ch == q) || (ch == '\\')) {
                if (*(pzTxt++) == NUL)
                    return pzTxt-1;
            }
        }
    }
}


/*  addStringValue
 *
 *  Associate a name with either a string or no value.
 */
static tOptionValue*
addStringValue( void** pp, char const* pzName, size_t nameLen,
                char const* pzValue, size_t dataLen )
{
    tOptionValue* pNV;
    size_t sz = nameLen + dataLen + sizeof(*pNV);

    pNV = AGALOC( sz, "option name/str value pair" );
    if (pNV == NULL)
        return NULL;

    if (pzValue == NULL) {
        pNV->valType = OPARG_TYPE_NONE;
        pNV->pzName = pNV->v.strVal;

    } else {
        pNV->valType = OPARG_TYPE_STRING;
        if (dataLen > 0)
            memcpy( pNV->v.strVal, pzValue, dataLen );
        pNV->v.strVal[dataLen] = NUL;
        pNV->pzName = pNV->v.strVal + dataLen + 1;
    }

    memcpy( pNV->pzName, pzName, nameLen );
    pNV->pzName[ nameLen ] = NUL;
    addArgListEntry( pp, pNV );
    return pNV;
}


/*  addBoolValue
 *
 *  Associate a name with either a string or no value.
 */
static tOptionValue*
addBoolValue( void** pp, char const* pzName, size_t nameLen,
                char const* pzValue, size_t dataLen )
{
    tOptionValue* pNV;
    size_t sz = nameLen + sizeof(*pNV) + 1;

    pNV = AGALOC( sz, "option name/bool value pair" );
    if (pNV == NULL)
        return NULL;
    while (isspace( (int)*pzValue ) && (dataLen > 0)) {
        dataLen--; pzValue++;
    }
    if (dataLen == 0)
        pNV->v.boolVal = 0;
    else if (isdigit( (int)*pzValue ))
        pNV->v.boolVal = atoi( pzValue );
    else switch (*pzValue) {
    case 'f':
    case 'F':
    case 'n':
    case 'N':
        pNV->v.boolVal = 0; break;
    default:
        pNV->v.boolVal = 1;
    }

    pNV->valType = OPARG_TYPE_BOOLEAN;
    pNV->pzName = (char*)(pNV + 1);
    memcpy( pNV->pzName, pzName, nameLen );
    pNV->pzName[ nameLen ] = NUL;
    addArgListEntry( pp, pNV );
    return pNV;
}


/*  addNumberValue
 *
 *  Associate a name with either a string or no value.
 */
static tOptionValue*
addNumberValue( void** pp, char const* pzName, size_t nameLen,
                char const* pzValue, size_t dataLen )
{
    tOptionValue* pNV;
    size_t sz = nameLen + sizeof(*pNV) + 1;

    pNV = AGALOC( sz, "option name/bool value pair" );
    if (pNV == NULL)
        return NULL;
    while (isspace( (int)*pzValue ) && (dataLen > 0)) {
        dataLen--; pzValue++;
    }
    if (dataLen == 0)
        pNV->v.boolVal = 0;
    else
        pNV->v.boolVal = atoi( pzValue );

    pNV->valType = OPARG_TYPE_NUMERIC;
    pNV->pzName = (char*)(pNV + 1);
    memcpy( pNV->pzName, pzName, nameLen );
    pNV->pzName[ nameLen ] = NUL;
    addArgListEntry( pp, pNV );
    return pNV;
}


/*  addNestedValue
 *
 *  Associate a name with either a string or no value.
 */
static tOptionValue*
addNestedValue( void** pp, char const* pzName, size_t nameLen,
                char* pzValue, size_t dataLen )
{
    tOptionValue* pNV;

    if (dataLen == 0) {
        size_t sz = nameLen + sizeof(*pNV) + 1;
        pNV = AGALOC( sz, "empty nested value pair" );
        if (pNV == NULL)
            return NULL;
        pNV->v.nestVal = NULL;
        pNV->valType = OPARG_TYPE_HIERARCHY;
        pNV->pzName = (char*)(pNV + 1);
        memcpy( pNV->pzName, pzName, nameLen );
        pNV->pzName[ nameLen ] = NUL;

    } else {
        pNV = optionLoadNested( pzValue, pzName, nameLen );
    }

    if (pNV != NULL)
        addArgListEntry( pp, pNV );

    return pNV;
}


/*  scanNameEntry
 *
 *  We have an entry that starts with a name.  Find the end of it, cook it
 *  (if called for) and create the name/value association.
 */
static char const*
scanNameEntry(char const* pzName, tOptionValue* pRes)
{
    tOptionValue* pNV;
    char const * pzScan = pzName+1;
    char const * pzVal;
    size_t       nameLen = 1;
    size_t       dataLen = 0;

    while (ISNAMECHAR( (int)*pzScan ))  { pzScan++; nameLen++; }

    while (isspace( (int)*pzScan )) {
        char ch = *(pzScan++);
        if ((ch == '\n') || (ch == ',')) {
            addStringValue(&(pRes->v.nestVal), pzName, nameLen, NULL,(size_t)0);
            return pzScan - 1;
        }
    }

    switch (*pzScan) {
    case '=':
    case ':':
        while (isspace( (int)*++pzScan ))  ;
        switch (*pzScan) {
        case ',':  goto comma_char;
        case '"':
        case '\'': goto quote_char;
        case NUL:  goto nul_byte;
        default:   goto default_char;
        }

    case ',':
    comma_char:
        pzScan++;
        /* FALLTHROUGH */

    case NUL:
    nul_byte:
        addStringValue(&(pRes->v.nestVal), pzName, nameLen, NULL, (size_t)0);
        break;

    case '"':
    case '\'':
    quote_char:
        pzVal = pzScan;
        pzScan = scanQuotedString( pzScan );
        dataLen = pzScan - pzVal;
        pNV = addStringValue( &(pRes->v.nestVal), pzName, nameLen, pzVal,
                              dataLen );
        if ((pNV != NULL) && (option_load_mode == OPTION_LOAD_COOKED))
            ao_string_cook( pNV->v.strVal, NULL );
        break;

    default:
    default_char:
        /*
         *  We have found some strange text value.  It ends with a newline
         *  or a comma.
         */
        pzVal = pzScan;
        for (;;) {
            char ch = *(pzScan++);
            switch (ch) {
            case NUL:
                pzScan--;
                dataLen = pzScan - pzVal;
                goto string_done;
                /* FALLTHROUGH */

            case '\n':
                if (   (pzScan > pzVal + 2)
                    && (pzScan[-2] == '\\')
                    && (pzScan[ 0] != NUL))
                    continue;
                /* FALLTHROUGH */

            case ',':
                dataLen = (pzScan - pzVal) - 1;
            string_done:
                pNV = addStringValue( &(pRes->v.nestVal), pzName, nameLen,
                                      pzVal, dataLen );
                if (pNV != NULL)
                    removeBackslashes( pNV->v.strVal );
                goto leave_scan_name;
            }
        }
        break;
    } leave_scan_name:;

    return pzScan;
}


/*  scanXmlEntry
 *
 *  We've found a '<' character.  We ignore this if it is a comment or a
 *  directive.  If it is something else, then whatever it is we are looking
 *  at is bogus.  Returning NULL stops processing.
 */
static char const*
scanXmlEntry( char const* pzName, tOptionValue* pRes )
{
    size_t nameLen = 1, valLen = 0;
    char const*   pzScan = ++pzName;
    char const*   pzVal;
    tOptionValue  valu;
    tOptionValue* pNewVal;
    tOptionLoadMode save_mode = option_load_mode;

    if (! isalpha((int)*pzName)) {
        switch (*pzName) {
        default:
            pzName = NULL;
            break;

        case '!':
            pzName = strstr( pzName, "-->" );
            if (pzName != NULL)
                pzName += 3;
            break;

        case '?':
            pzName = strchr( pzName, '>' );
            if (pzName != NULL)
                pzName++;
            break;
        }
        return pzName;
    }

    while (isalpha( (int)*++pzScan ))  nameLen++;
    if (nameLen > 64)
        return NULL;
    valu.valType = OPARG_TYPE_STRING;

    switch (*pzScan) {
    case ' ':
    case '\t':
        pzScan = parseAttributes(
            NULL, (char*)pzScan, &option_load_mode, &valu );
        if (*pzScan == '>') {
            pzScan++;
            break;
        }

        if (*pzScan != '/') {
            option_load_mode = save_mode;
            return NULL;
        }
        /* FALLTHROUGH */

    case '/':
        if (*++pzScan != '>') {
            option_load_mode = save_mode;
            return NULL;
        }
        addStringValue(&(pRes->v.nestVal), pzName, nameLen, NULL, (size_t)0);
        option_load_mode = save_mode;
        return pzScan+2;

    default:
        option_load_mode = save_mode;
        return NULL;

    case '>':
        pzScan++;
        break;
    }

    pzVal = pzScan;

    {
        char z[68];
        char* pzD = z;
        int  ct = nameLen;
        char const* pzS = pzName;

        *(pzD++) = '<';
        *(pzD++) = '/';

        do  {
            *(pzD++) = *(pzS++);
        } while (--ct > 0);
        *(pzD++) = '>';
        *pzD = NUL;

        pzScan = strstr( pzScan, z );
        if (pzScan == NULL) {
            option_load_mode = save_mode;
            return NULL;
        }
        valLen = (pzScan - pzVal);
        pzScan += nameLen + 3;
        while (isspace(  (int)*pzScan ))  pzScan++;
    }

    switch (valu.valType) {
    case OPARG_TYPE_NONE:
        addStringValue( &(pRes->v.nestVal), pzName, nameLen, NULL, (size_t)0);
        break;

    case OPARG_TYPE_STRING:
        pNewVal = addStringValue(
            &(pRes->v.nestVal), pzName, nameLen, pzVal, valLen);

        if (option_load_mode == OPTION_LOAD_KEEP)
            break;
        mungeString( pNewVal->v.strVal, option_load_mode );
        break;

    case OPARG_TYPE_BOOLEAN:
        addBoolValue( &(pRes->v.nestVal), pzName, nameLen, pzVal, valLen );
        break;

    case OPARG_TYPE_NUMERIC:
        addNumberValue( &(pRes->v.nestVal), pzName, nameLen, pzVal, valLen );
        break;

    case OPARG_TYPE_HIERARCHY:
    {
        char* pz = AGALOC( valLen+1, "hierarchical scan" );
        if (pz == NULL)
            break;
        memcpy( pz, pzVal, valLen );
        pz[valLen] = NUL;
        addNestedValue( &(pRes->v.nestVal), pzName, nameLen, pz, valLen );
        AGFREE(pz);
        break;
    }

    case OPARG_TYPE_ENUMERATION:
    case OPARG_TYPE_MEMBERSHIP:
    default:
        break;
    }

    option_load_mode = save_mode;
    return pzScan;
}


/*  unloadNestedArglist
 *
 *  Deallocate a list of option arguments.  This must have been gotten from
 *  a hierarchical option argument, not a stacked list of strings.  It is
 *  an internal call, so it is not validated.  The caller is responsible for
 *  knowing what they are doing.
 */
static void
unloadNestedArglist( tArgList* pAL )
{
    int ct = pAL->useCt;
    tCC** ppNV = pAL->apzArgs;

    while (ct-- > 0) {
        tOptionValue* pNV = (tOptionValue*)(void*)*(ppNV++);
        if (pNV->valType == OPARG_TYPE_HIERARCHY)
            unloadNestedArglist( pNV->v.nestVal );
        AGFREE( pNV );
    }

    AGFREE( (void*)pAL );
}


/*=export_func  optionUnloadNested
 *
 * what:  Deallocate the memory for a nested value
 * arg:   + tOptionValue const * + pOptVal + the hierarchical value +
 *
 * doc:
 *  A nested value needs to be deallocated.  The pointer passed in should
 *  have been gotten from a call to @code{configFileLoad()} (See
 *  @pxref{libopts-configFileLoad}).
=*/
void
optionUnloadNested( tOptionValue const * pOV )
{
    if (pOV == NULL) return;
    if (pOV->valType != OPARG_TYPE_HIERARCHY) {
        errno = EINVAL;
        return;
    }

    unloadNestedArglist( pOV->v.nestVal );

    AGFREE( (void*)pOV );
}


/*  sortNestedList
 *
 *  This is a _stable_ sort.  The entries are sorted alphabetically,
 *  but within entries of the same name the ordering is unchanged.
 *  Typically, we also hope the input is sorted.
 */
static void
sortNestedList( tArgList* pAL )
{
    int ix;
    int lm = pAL->useCt;

    /*
     *  This loop iterates "useCt" - 1 times.
     */
    for (ix = 0; ++ix < lm;) {
        int iy = ix-1;
        tOptionValue* pNewNV = (tOptionValue*)(void*)(pAL->apzArgs[ix]);
        tOptionValue* pOldNV = (tOptionValue*)(void*)(pAL->apzArgs[iy]);

        /*
         *  For as long as the new entry precedes the "old" entry,
         *  move the old pointer.  Stop before trying to extract the
         *  "-1" entry.
         */
        while (strcmp( pOldNV->pzName, pNewNV->pzName ) > 0) {
            pAL->apzArgs[iy+1] = (void*)pOldNV;
            pOldNV = (tOptionValue*)(void*)(pAL->apzArgs[--iy]);
            if (iy < 0)
                break;
        }

        /*
         *  Always store the pointer.  Sometimes it is redundant,
         *  but the redundancy is cheaper than a test and branch sequence.
         */
        pAL->apzArgs[iy+1] = (void*)pNewNV;
    }
}


/* optionLoadNested
 * private:
 *
 * what:  parse a hierarchical option argument
 * arg:   + char const*     + pzTxt   + the text to scan +
 * arg:   + char const*     + pzName  + the name for the text +
 * arg:   + size_t          + nameLen + the length of "name"  +
 *
 * ret_type:  tOptionValue*
 * ret_desc:  An allocated, compound value structure
 *
 * doc:
 *  A block of text represents a series of values.  It may be an
 *  entire configuration file, or it may be an argument to an
 *  option that takes a hierarchical value.
 */
LOCAL tOptionValue*
optionLoadNested(char const* pzTxt, char const* pzName, size_t nameLen)
{
    tOptionValue* pRes;
    tArgList*     pAL;

    /*
     *  Make sure we have some data and we have space to put what we find.
     */
    if (pzTxt == NULL) {
        errno = EINVAL;
        return NULL;
    }
    while (isspace( (int)*pzTxt ))  pzTxt++;
    if (*pzTxt == NUL) {
        errno = ENOENT;
        return NULL;
    }
    pRes = AGALOC( sizeof(*pRes) + nameLen + 1, "nested args" );
    if (pRes == NULL) {
        errno = ENOMEM;
        return NULL;
    }
    pRes->valType   = OPARG_TYPE_HIERARCHY;
    pRes->pzName    = (char*)(pRes + 1);
    memcpy( pRes->pzName, pzName, nameLen );
    pRes->pzName[ nameLen ] = NUL;

    pAL = AGALOC( sizeof(*pAL), "nested arg list" );
    if (pAL == NULL) {
        AGFREE( pRes );
        return NULL;
    }
    pRes->v.nestVal = pAL;
    pAL->useCt   = 0;
    pAL->allocCt = MIN_ARG_ALLOC_CT;

    /*
     *  Scan until we hit a NUL.
     */
    do  {
        while (isspace( (int)*pzTxt ))  pzTxt++;
        if (isalpha( (int)*pzTxt )) {
            pzTxt = scanNameEntry( pzTxt, pRes );
        }
        else switch (*pzTxt) {
        case NUL: goto scan_done;
        case '<': pzTxt = scanXmlEntry( pzTxt, pRes );
                  if (*pzTxt == ',') pzTxt++;     break;
        case '#': pzTxt = strchr( pzTxt, '\n' );  break;
        default:  goto woops;
        }
    } while (pzTxt != NULL); scan_done:;

    pAL = pRes->v.nestVal;
    if (pAL->useCt != 0) {
        sortNestedList( pAL );
        return pRes;
    }

 woops:
    AGFREE( pRes->v.nestVal );
    AGFREE( pRes );
    return NULL;
}


/*=export_func  optionNestedVal
 * private:
 *
 * what:  parse a hierarchical option argument
 * arg:   + tOptions* + pOpts    + program options descriptor +
 * arg:   + tOptDesc* + pOptDesc + the descriptor for this arg +
 *
 * doc:
 *  Nested value was found on the command line
=*/
void
optionNestedVal( tOptions* pOpts, tOptDesc* pOD )
{
    tOptionValue* pOV = optionLoadNested(
        pOD->optArg.argString, pOD->pz_Name, strlen(pOD->pz_Name));

    if (pOV != NULL)
        addArgListEntry( &(pOD->optCookie), (void*)pOV );
}
/*
 * Local Variables:
 * mode: C
 * c-file-style: "stroustrup"
 * indent-tabs-mode: nil
 * End:
 * end of autoopts/nested.c */
