
/**
 * \file nested.c
 *
 *  Time-stamp:      "2010-08-22 11:17:56 bkorb"
 *
 *   Automated Options Nested Values module.
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

typedef struct {
    int     xml_ch;
    int     xml_len;
    char    xml_txt[8];
} xml_xlate_t;

static xml_xlate_t const xml_xlate[] = {
    { '&', 4, "amp;"  },
    { '<', 3, "lt;"   },
    { '>', 3, "gt;"   },
    { '"', 5, "quot;" },
    { '\'',5, "apos;" }
};

/* = = = START-STATIC-FORWARD = = = */
static void
remove_continuation(char* pzSrc);

static char const*
scan_q_str(char const* pzTxt);

static tOptionValue*
add_string(void** pp, char const* pzName, size_t nameLen,
               char const* pzValue, size_t dataLen);

static tOptionValue*
add_bool(void** pp, char const* pzName, size_t nameLen,
         char const* pzValue, size_t dataLen);

static tOptionValue*
add_number(void** pp, char const* pzName, size_t nameLen,
           char const* pzValue, size_t dataLen);

static tOptionValue*
add_nested(void** pp, char const* pzName, size_t nameLen,
           char* pzValue, size_t dataLen);

static char const *
scan_name(char const* pzName, tOptionValue* pRes);

static char const*
scan_xml(char const* pzName, tOptionValue* pRes);

static void
sort_list(tArgList* pAL);
/* = = = END-STATIC-FORWARD = = = */

/**
 *  Backslashes are used for line continuations.  We keep the newline
 *  characters, but trim out the backslash:
 */
static void
remove_continuation(char* pzSrc)
{
    char* pzD;

    do  {
        while (*pzSrc == '\n')  pzSrc++;
        pzD = strchr(pzSrc, '\n');
        if (pzD == NULL)
            return;

        /*
         *  pzD has skipped at least one non-newline character and now
         *  points to a newline character.  It now becomes the source and
         *  pzD goes to the previous character.
         */
        pzSrc = pzD--;
        if (*pzD != '\\')
            pzD++;
    } while (pzD == pzSrc);

    /*
     *  Start shifting text.
     */
    for (;;) {
        char ch = ((*pzD++) = *(pzSrc++));
        switch (ch) {
        case NUL:  return;
        case '\\':
            if (*pzSrc == '\n')
                --pzD; /* rewrite on next iteration */
        }
    }
}

/**
 *  Find the end of a quoted string, skipping escaped quote characters.
 */
static char const*
scan_q_str(char const* pzTxt)
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


/**
 *  Associate a name with either a string or no value.
 */
static tOptionValue*
add_string(void** pp, char const* pzName, size_t nameLen,
               char const* pzValue, size_t dataLen)
{
    tOptionValue* pNV;
    size_t sz = nameLen + dataLen + sizeof(*pNV);

    pNV = AGALOC(sz, "option name/str value pair");
    if (pNV == NULL)
        return NULL;

    if (pzValue == NULL) {
        pNV->valType = OPARG_TYPE_NONE;
        pNV->pzName = pNV->v.strVal;

    } else {
        pNV->valType = OPARG_TYPE_STRING;
        if (dataLen > 0) {
            char const * pzSrc = pzValue;
            char * pzDst = pNV->v.strVal;
            int    ct    = dataLen;
            do  {
                int ch = *(pzSrc++) & 0xFF;
                if (ch == NUL) goto data_copy_done;
                if (ch == '&')
                    ch = get_special_char(&pzSrc, &ct);
                *(pzDst++) = ch;
            } while (--ct > 0);
        data_copy_done:
            *pzDst = NUL;

        } else {
            pNV->v.strVal[0] = NUL;
        }

        pNV->pzName = pNV->v.strVal + dataLen + 1;
    }

    memcpy(pNV->pzName, pzName, nameLen);
    pNV->pzName[ nameLen ] = NUL;
    addArgListEntry(pp, pNV);
    return pNV;
}

/**
 *  Associate a name with either a string or no value.
 */
static tOptionValue*
add_bool(void** pp, char const* pzName, size_t nameLen,
         char const* pzValue, size_t dataLen)
{
    tOptionValue* pNV;
    size_t sz = nameLen + sizeof(*pNV) + 1;

    pNV = AGALOC(sz, "option name/bool value pair");
    if (pNV == NULL)
        return NULL;
    while (IS_WHITESPACE_CHAR(*pzValue) && (dataLen > 0)) {
        dataLen--; pzValue++;
    }
    if (dataLen == 0)
        pNV->v.boolVal = 0;

    else if (IS_DEC_DIGIT_CHAR(*pzValue))
        pNV->v.boolVal = atoi(pzValue);

    else pNV->v.boolVal = ! IS_FALSE_TYPE_CHAR(*pzValue);

    pNV->valType = OPARG_TYPE_BOOLEAN;
    pNV->pzName = (char*)(pNV + 1);
    memcpy(pNV->pzName, pzName, nameLen);
    pNV->pzName[ nameLen ] = NUL;
    addArgListEntry(pp, pNV);
    return pNV;
}

/**
 *  Associate a name with either a string or no value.
 */
static tOptionValue*
add_number(void** pp, char const* pzName, size_t nameLen,
           char const* pzValue, size_t dataLen)
{
    tOptionValue* pNV;
    size_t sz = nameLen + sizeof(*pNV) + 1;

    pNV = AGALOC(sz, "option name/bool value pair");
    if (pNV == NULL)
        return NULL;
    while (IS_WHITESPACE_CHAR(*pzValue) && (dataLen > 0)) {
        dataLen--; pzValue++;
    }
    if (dataLen == 0)
        pNV->v.longVal = 0;
    else
        pNV->v.longVal = strtol(pzValue, 0, 0);

    pNV->valType = OPARG_TYPE_NUMERIC;
    pNV->pzName  = (char*)(pNV + 1);
    memcpy(pNV->pzName, pzName, nameLen);
    pNV->pzName[ nameLen ] = NUL;
    addArgListEntry(pp, pNV);
    return pNV;
}

/**
 *  Associate a name with either a string or no value.
 */
static tOptionValue*
add_nested(void** pp, char const* pzName, size_t nameLen,
           char* pzValue, size_t dataLen)
{
    tOptionValue* pNV;

    if (dataLen == 0) {
        size_t sz = nameLen + sizeof(*pNV) + 1;
        pNV = AGALOC(sz, "empty nested value pair");
        if (pNV == NULL)
            return NULL;
        pNV->v.nestVal = NULL;
        pNV->valType = OPARG_TYPE_HIERARCHY;
        pNV->pzName = (char*)(pNV + 1);
        memcpy(pNV->pzName, pzName, nameLen);
        pNV->pzName[ nameLen ] = NUL;

    } else {
        pNV = optionLoadNested(pzValue, pzName, nameLen);
    }

    if (pNV != NULL)
        addArgListEntry(pp, pNV);

    return pNV;
}

/**
 *  We have an entry that starts with a name.  Find the end of it, cook it
 *  (if called for) and create the name/value association.
 */
static char const *
scan_name(char const* pzName, tOptionValue* pRes)
{
    tOptionValue* pNV;
    char const * pzScan = pzName+1; /* we know first char is a name char */
    char const * pzVal;
    size_t       nameLen = 1;
    size_t       dataLen = 0;

    /*
     *  Scan over characters that name a value.  These names may not end
     *  with a colon, but they may contain colons.
     */
    while (IS_VALUE_NAME_CHAR(*pzScan))   { pzScan++; nameLen++; }
    if (pzScan[-1] == ':')                { pzScan--; nameLen--; }
    while (IS_HORIZ_WHITE_CHAR(*pzScan))    pzScan++;

 re_switch:

    switch (*pzScan) {
    case '=':
    case ':':
        while (IS_HORIZ_WHITE_CHAR((int)*++pzScan))  ;
        if ((*pzScan == '=') || (*pzScan == ':'))
            goto default_char;
        goto re_switch;

    case '\n':
    case ',':
        pzScan++;
        /* FALLTHROUGH */

    case NUL:
        add_string(&(pRes->v.nestVal), pzName, nameLen, NULL, (size_t)0);
        break;

    case '"':
    case '\'':
        pzVal = pzScan;
        pzScan = scan_q_str(pzScan);
        dataLen = pzScan - pzVal;
        pNV = add_string(&(pRes->v.nestVal), pzName, nameLen, pzVal,
                             dataLen);
        if ((pNV != NULL) && (option_load_mode == OPTION_LOAD_COOKED))
            ao_string_cook(pNV->v.strVal, NULL);
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
                pNV = add_string(&(pRes->v.nestVal), pzName, nameLen,
                                     pzVal, dataLen);
                if (pNV != NULL)
                    remove_continuation(pNV->v.strVal);
                goto leave_scan_name;
            }
        }
        break;
    } leave_scan_name:;

    return pzScan;
}

/**
 *  We've found a '<' character.  We ignore this if it is a comment or a
 *  directive.  If it is something else, then whatever it is we are looking
 *  at is bogus.  Returning NULL stops processing.
 */
static char const*
scan_xml(char const* pzName, tOptionValue* pRes)
{
    size_t nameLen = 1, valLen = 0;
    char const*   pzScan = ++pzName;
    char const*   pzVal;
    tOptionValue  valu;
    tOptionValue* pNewVal;
    tOptionLoadMode save_mode = option_load_mode;

    if (! IS_VAR_FIRST_CHAR(*pzName)) {
        switch (*pzName) {
        default:
            pzName = NULL;
            break;

        case '!':
            pzName = strstr(pzName, "-->");
            if (pzName != NULL)
                pzName += 3;
            break;

        case '?':
            pzName = strchr(pzName, '>');
            if (pzName != NULL)
                pzName++;
            break;
        }
        return pzName;
    }

    pzScan++;
    while (IS_VALUE_NAME_CHAR((int)*pzScan))  { pzScan++; nameLen++; }
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
        add_string(&(pRes->v.nestVal), pzName, nameLen, NULL, (size_t)0);
        option_load_mode = save_mode;
        return pzScan+1;

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

        pzScan = strstr(pzScan, z);
        if (pzScan == NULL) {
            option_load_mode = save_mode;
            return NULL;
        }
        valLen = (pzScan - pzVal);
        pzScan += nameLen + 3;
        while (IS_WHITESPACE_CHAR(*pzScan))  pzScan++;
    }

    switch (valu.valType) {
    case OPARG_TYPE_NONE:
        add_string(&(pRes->v.nestVal), pzName, nameLen, NULL, (size_t)0);
        break;

    case OPARG_TYPE_STRING:
        pNewVal = add_string(
            &(pRes->v.nestVal), pzName, nameLen, pzVal, valLen);

        if (option_load_mode == OPTION_LOAD_KEEP)
            break;
        mungeString(pNewVal->v.strVal, option_load_mode);
        break;

    case OPARG_TYPE_BOOLEAN:
        add_bool(&(pRes->v.nestVal), pzName, nameLen, pzVal, valLen);
        break;

    case OPARG_TYPE_NUMERIC:
        add_number(&(pRes->v.nestVal), pzName, nameLen, pzVal, valLen);
        break;

    case OPARG_TYPE_HIERARCHY:
    {
        char* pz = AGALOC(valLen+1, "hierarchical scan");
        if (pz == NULL)
            break;
        memcpy(pz, pzVal, valLen);
        pz[valLen] = NUL;
        add_nested(&(pRes->v.nestVal), pzName, nameLen, pz, valLen);
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


/**
 *  Deallocate a list of option arguments.  This must have been gotten from
 *  a hierarchical option argument, not a stacked list of strings.  It is
 *  an internal call, so it is not validated.  The caller is responsible for
 *  knowing what they are doing.
 */
LOCAL void
unload_arg_list(tArgList* pAL)
{
    int ct = pAL->useCt;
    tCC** ppNV = pAL->apzArgs;

    while (ct-- > 0) {
        tOptionValue* pNV = (tOptionValue*)(void*)*(ppNV++);
        if (pNV->valType == OPARG_TYPE_HIERARCHY)
            unload_arg_list(pNV->v.nestVal);
        AGFREE(pNV);
    }

    AGFREE((void*)pAL);
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
optionUnloadNested(tOptionValue const * pOV)
{
    if (pOV == NULL) return;
    if (pOV->valType != OPARG_TYPE_HIERARCHY) {
        errno = EINVAL;
        return;
    }

    unload_arg_list(pOV->v.nestVal);

    AGFREE((void*)pOV);
}

/**
 *  This is a _stable_ sort.  The entries are sorted alphabetically,
 *  but within entries of the same name the ordering is unchanged.
 *  Typically, we also hope the input is sorted.
 */
static void
sort_list(tArgList* pAL)
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
        while (strcmp(pOldNV->pzName, pNewNV->pzName) > 0) {
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

    /*
     *  Make sure we have some data and we have space to put what we find.
     */
    if (pzTxt == NULL) {
        errno = EINVAL;
        return NULL;
    }
    while (IS_WHITESPACE_CHAR(*pzTxt))  pzTxt++;
    if (*pzTxt == NUL) {
        errno = ENOENT;
        return NULL;
    }
    pRes = AGALOC(sizeof(*pRes) + nameLen + 1, "nested args");
    if (pRes == NULL) {
        errno = ENOMEM;
        return NULL;
    }
    pRes->valType   = OPARG_TYPE_HIERARCHY;
    pRes->pzName    = (char*)(pRes + 1);
    memcpy(pRes->pzName, pzName, nameLen);
    pRes->pzName[nameLen] = NUL;

    {
        tArgList * pAL = AGALOC(sizeof(*pAL), "nested arg list");
        if (pAL == NULL) {
            AGFREE(pRes);
            return NULL;
        }

        pRes->v.nestVal = pAL;
        pAL->useCt   = 0;
        pAL->allocCt = MIN_ARG_ALLOC_CT;
    }

    /*
     *  Scan until we hit a NUL.
     */
    do  {
        while (IS_WHITESPACE_CHAR((int)*pzTxt))  pzTxt++;
        if (IS_VAR_FIRST_CHAR((int)*pzTxt)) {
            pzTxt = scan_name(pzTxt, pRes);
        }
        else switch (*pzTxt) {
        case NUL: goto scan_done;
        case '<': pzTxt = scan_xml(pzTxt, pRes);
                  if (pzTxt == NULL) goto woops;
                  if (*pzTxt == ',') pzTxt++;     break;
        case '#': pzTxt = strchr(pzTxt, '\n');  break;
        default:  goto woops;
        }
    } while (pzTxt != NULL); scan_done:;

    {
        tArgList * al = pRes->v.nestVal;
        if (al->useCt != 0)
            sort_list(al);
    }

    return pRes;

 woops:
    AGFREE(pRes->v.nestVal);
    AGFREE(pRes);
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
optionNestedVal(tOptions* pOpts, tOptDesc* pOD)
{
    if (pOpts < OPTPROC_EMIT_LIMIT)
        return;

    if (pOD->fOptState & OPTST_RESET) {
        tArgList* pAL = pOD->optCookie;
        int       ct;
        tCC **    av;

        if (pAL == NULL)
            return;
        ct = pAL->useCt;
        av = pAL->apzArgs;

        while (--ct >= 0) {
            void * p = (void *)*(av++);
            optionUnloadNested((tOptionValue const *)p);
        }

        AGFREE(pOD->optCookie);

    } else {
        tOptionValue* pOV = optionLoadNested(
            pOD->optArg.argString, pOD->pz_Name, strlen(pOD->pz_Name));

        if (pOV != NULL)
            addArgListEntry(&(pOD->optCookie), (void*)pOV);
    }
}

/*
 * get_special_char
 */
LOCAL int
get_special_char(char const ** ppz, int * ct)
{
    char const * pz = *ppz;

    if (*ct < 3)
        return '&';

    if (*pz == '#') {
        int base = 10;
        int retch;

        pz++;
        if (*pz == 'x') {
            base = 16;
            pz++;
        }
        retch = (int)strtoul(pz, (char **)&pz, base);
        if (*pz != ';')
            return '&';
        base = ++pz - *ppz;
        if (base > *ct)
            return '&';

        *ct -= base;
        *ppz = pz;
        return retch;
    }

    {
        int ctr = sizeof(xml_xlate) / sizeof(xml_xlate[0]);
        xml_xlate_t const * xlatp = xml_xlate;

        for (;;) {
            if (  (*ct >= xlatp->xml_len)
               && (strncmp(pz, xlatp->xml_txt, xlatp->xml_len) == 0)) {
                *ppz += xlatp->xml_len;
                *ct  -= xlatp->xml_len;
                return xlatp->xml_ch;
            }

            if (--ctr <= 0)
                break;
            xlatp++;
        }
    }
    return '&';
}

/*
 * emit_special_char
 */
LOCAL void
emit_special_char(FILE * fp, int ch)
{
    int ctr = sizeof(xml_xlate) / sizeof(xml_xlate[0]);
    xml_xlate_t const * xlatp = xml_xlate;

    putc('&', fp);
    for (;;) {
        if (ch == xlatp->xml_ch) {
            fputs(xlatp->xml_txt, fp);
            return;
        }
        if (--ctr <= 0)
            break;
        xlatp++;
    }
    fprintf(fp, "#x%02X;", (ch & 0xFF));
}

/*
 * Local Variables:
 * mode: C
 * c-file-style: "stroustrup"
 * indent-tabs-mode: nil
 * End:
 * end of autoopts/nested.c */
