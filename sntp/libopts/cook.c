/**
 * \file cook.c
 *
 *  Time-stamp:      "2011-03-12 15:05:26 bkorb"
 *
 *  This file contains the routines that deal with processing quoted strings
 *  into an internal format.
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
static ag_bool
contiguous_quote(char ** pps, char * pq, int * lnct_p);
/* = = = END-STATIC-FORWARD = = = */

/*=export_func  ao_string_cook_escape_char
 * private:
 *
 * what:  escape-process a string fragment
 * arg:   + char const*  + pzScan  + points to character after the escape +
 * arg:   + char*        + pRes    + Where to put the result byte +
 * arg:   + unsigned int + nl_ch   + replacement char if scanned char is \n +
 *
 * ret-type: unsigned int
 * ret-desc: The number of bytes consumed processing the escaped character.
 *
 * doc:
 *
 *  This function converts "t" into "\t" and all your other favorite
 *  escapes, including numeric ones:  hex and ocatal, too.
 *  The returned result tells the caller how far to advance the
 *  scan pointer (passed in).  The default is to just pass through the
 *  escaped character and advance the scan by one.
 *
 *  Some applications need to keep an escaped newline, others need to
 *  suppress it.  This is accomplished by supplying a '\n' replacement
 *  character that is different from \n, if need be.  For example, use
 *  0x7F and never emit a 0x7F.
 *
 * err:  @code{NULL} is returned if the string is mal-formed.
=*/
unsigned int
ao_string_cook_escape_char( char const* pzIn, char* pRes, u_int nl )
{
    unsigned int  res = 1;

    switch (*pRes = *pzIn++) {
    case NUL:         /* NUL - end of input string */
        return 0;
    case '\r':
        if (*pzIn != '\n')
            return 1;
        res++;
        /* FALLTHROUGH */
    case '\n':        /* NL  - emit newline        */
        *pRes = (char)nl;
        return res;

    case 'a': *pRes = '\a'; break;
    case 'b': *pRes = '\b'; break;
    case 'f': *pRes = '\f'; break;
    case 'n': *pRes = '\n'; break;
    case 'r': *pRes = '\r'; break;
    case 't': *pRes = '\t'; break;
    case 'v': *pRes = '\v'; break;

    case 'x':
    case 'X':         /* HEX Escape       */
        if (IS_HEX_DIGIT_CHAR(*pzIn))  {
            char z[4], *pz = z;

            do *(pz++) = *(pzIn++);
            while (IS_HEX_DIGIT_CHAR(*pzIn) && (pz < z + 2));
            *pz = NUL;
            *pRes = (unsigned char)strtoul(z, NULL, 16);
            res += pz - z;
        }
        break;

    case '0': case '1': case '2': case '3':
    case '4': case '5': case '6': case '7':
    {
        /*
         *  IF the character copied was an octal digit,
         *  THEN set the output character to an octal value
         */
        char z[4], *pz = z + 1;
        unsigned long val;
        z[0] = *pRes;

        while (IS_OCT_DIGIT_CHAR(*pzIn) && (pz < z + 3))
            *(pz++) = *(pzIn++);
        *pz = NUL;
        val = strtoul(z, NULL, 8);
        if (val > 0xFF)
            val = 0xFF;
        *pRes = (unsigned char)val;
        res = pz - z;
        break;
    }

    default: ;
    }

    return res;
}


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *
 *  A quoted string has been found.
 *  Find the end of it and compress any escape sequences.
 */
static ag_bool
contiguous_quote(char ** pps, char * pq, int * lnct_p)
{
    char * ps = *pps + 1;

    for (;;) {
        while (IS_WHITESPACE_CHAR(*ps))
            if (*(ps++) == '\n')
                (*lnct_p)++;

        /*
         *  IF the next character is a quote character,
         *  THEN we will concatenate the strings.
         */
        switch (*ps) {
        case '"':
        case '\'':
            *pq  = *(ps++);  /* assign new quote character and return */
            *pps = ps;
            return AG_TRUE;

        case '/':
            /*
             *  Allow for a comment embedded in the concatenated string.
             */
            switch (ps[1]) {
            default:
                *pps = NULL;
                return AG_FALSE;

            case '/':
                /*
                 *  Skip to end of line
                 */
                ps = strchr(ps, '\n');
                if (ps == NULL) {
                    *pps = NULL;
                    return AG_FALSE;
                }
                break;

            case '*':
            {
                char* p = strstr( ps+2, "*/" );
                /*
                 *  Skip to terminating star slash
                 */
                if (p == NULL) {
                    *pps = NULL;
                    return AG_FALSE;
                }

                while (ps < p) {
                    if (*(ps++) == '\n')
                        (*lnct_p)++;
                }

                ps = p + 2;
            }
            }
            continue;

        default:
            /*
             *  The next non-whitespace character is not a quote.
             *  The series of quoted strings has come to an end.
             */
            *pps = ps;
            return AG_FALSE;
        }
    }
}

/*=export_func  ao_string_cook
 * private:
 *
 * what:  concatenate and escape-process strings
 * arg:   + char* + pzScan  + The *MODIFIABLE* input buffer +
 * arg:   + int*  + lnct_p  + The (possibly NULL) pointer to a line count +
 *
 * ret-type: char*
 * ret-desc: The address of the text following the processed strings.
 *           The return value is NULL if the strings are ill-formed.
 *
 * doc:
 *
 *  A series of one or more quoted strings are concatenated together.
 *  If they are quoted with double quotes (@code{"}), then backslash
 *  escapes are processed per the C programming language.  If they are
 *  single quote strings, then the backslashes are honored only when they
 *  precede another backslash or a single quote character.
 *
 * err:  @code{NULL} is returned if the string(s) is/are mal-formed.
=*/
char *
ao_string_cook(char * pzScan, int * lnct_p)
{
    int   l = 0;
    char  q = *pzScan;

    /*
     *  It is a quoted string.  Process the escape sequence characters
     *  (in the set "abfnrtv") and make sure we find a closing quote.
     */
    char* pzD = pzScan++;
    char* pzS = pzScan;

    if (lnct_p == NULL)
        lnct_p = &l;

    for (;;) {
        /*
         *  IF the next character is the quote character, THEN we may end the
         *  string.  We end it unless the next non-blank character *after* the
         *  string happens to also be a quote.  If it is, then we will change
         *  our quote character to the new quote character and continue
         *  condensing text.
         */
        while (*pzS == q) {
            *pzD = NUL; /* This is probably the end of the line */
            if (! contiguous_quote(&pzS, &q, lnct_p))
                return pzS;
        }

        /*
         *  We are inside a quoted string.  Copy text.
         */
        switch (*(pzD++) = *(pzS++)) {
        case NUL:
            return NULL;

        case '\n':
            (*lnct_p)++;
            break;

        case '\\':
            /*
             *  IF we are escaping a new line,
             *  THEN drop both the escape and the newline from
             *       the result string.
             */
            if (*pzS == '\n') {
                pzS++;
                pzD--;
                (*lnct_p)++;
            }

            /*
             *  ELSE IF the quote character is '"' or '`',
             *  THEN we do the full escape character processing
             */
            else if (q != '\'') {
                int ct = ao_string_cook_escape_char( pzS, pzD-1, (u_int)'\n' );
                if (ct == 0)
                    return NULL;

                pzS += ct;
            }     /* if (q != '\'')                  */

            /*
             *  OTHERWISE, we only process "\\", "\'" and "\#" sequences.
             *  The latter only to easily hide preprocessing directives.
             */
            else switch (*pzS) {
            case '\\':
            case '\'':
            case '#':
                pzD[-1] = *pzS++;
            }
        }     /* switch (*(pzD++) = *(pzS++))    */
    }         /* for (;;)                        */
}
/*
 * Local Variables:
 * mode: C
 * c-file-style: "stroustrup"
 * indent-tabs-mode: nil
 * End:
 * end of autoopts/cook.c */
