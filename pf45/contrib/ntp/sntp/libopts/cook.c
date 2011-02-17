
/*
 *  $Id: cook.c,v 4.10 2007/02/04 17:44:12 bkorb Exp $
 *  Time-stamp:      "2006-09-24 15:21:02 bkorb"
 *
 *  This file contains the routines that deal with processing quoted strings
 *  into an internal format.
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

    case 'x':         /* HEX Escape       */
        if (isxdigit( (int)*pzIn ))  {
            unsigned int  val;
            unsigned char ch = *pzIn++;

            if ((ch >= 'A') && (ch <= 'F'))
                val = 10 + (ch - 'A');
            else if ((ch >= 'a') && (ch <= 'f'))
                val = 10 + (ch - 'a');
            else val = ch - '0';

            ch = *pzIn;

            if (! isxdigit( ch )) {
                *pRes = val;
                res   = 2;
                break;
            }
            val <<= 4;
            if ((ch >= 'A') && (ch <= 'F'))
                val += 10 + (ch - 'A');
            else if ((ch >= 'a') && (ch <= 'f'))
                val += 10 + (ch - 'a');
            else val += ch - '0';

            res = 3;
            *pRes = val;
        }
        break;

    default:
        /*
         *  IF the character copied was an octal digit,
         *  THEN set the output character to an octal value
         */
        if (isdigit( (int)*pRes ) && (*pRes < '8'))  {
            unsigned int  val = *pRes - '0';
            unsigned char ch  = *pzIn++;

            /*
             *  IF the second character is *not* an octal digit,
             *  THEN save the value and bail
             */
            if ((ch < '0') || (ch > '7')) {
                *pRes = val;
                break;
            }

            val = (val<<3) + (ch - '0');
            ch  = *pzIn;
            res = 2;

            /*
             *  IF the THIRD character is *not* an octal digit,
             *  THEN save the value and bail
             */
            if ((ch < '0') || (ch > '7')) {
                *pRes = val;
                break;
            }

            /*
             *  IF the new value would not be too large,
             *  THEN add on the third and last character value
             */
            if ((val<<3) < 0xFF) {
                val = (val<<3) + (ch - '0');
                res = 3;
            }

            *pRes = val;
            break;
        }
    }

    return res;
}


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *
 *  A quoted string has been found.
 *  Find the end of it and compress any escape sequences.
 */
/*=export_func  ao_string_cook
 * private:
 *
 * what:  concatenate and escape-process strings
 * arg:   + char* + pzScan     + The *MODIFIABLE* input buffer +
 * arg:   + int*  + pLineCt    + The (possibly NULL) pointer to a line count +
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
char*
ao_string_cook( char* pzScan, int* pLineCt )
{
    int   l = 0;
    char  q = *pzScan;

    /*
     *  It is a quoted string.  Process the escape sequence characters
     *  (in the set "abfnrtv") and make sure we find a closing quote.
     */
    char* pzD = pzScan++;
    char* pzS = pzScan;

    if (pLineCt == NULL)
        pLineCt = &l;

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
            pzS++;

        scan_for_quote:
            while (isspace((int)*pzS))
                if (*(pzS++) == '\n')
                    (*pLineCt)++;

            /*
             *  IF the next character is a quote character,
             *  THEN we will concatenate the strings.
             */
            switch (*pzS) {
            case '"':
            case '\'':
                break;

            case '/':
                /*
                 *  Allow for a comment embedded in the concatenated string.
                 */
                switch (pzS[1]) {
                default:  return NULL;
                case '/':
                    /*
                     *  Skip to end of line
                     */
                    pzS = strchr( pzS, '\n' );
                    if (pzS == NULL)
                        return NULL;
                    (*pLineCt)++;
                    break;

                case '*':
                {
                    char* p = strstr( pzS+2, "*/" );
                    /*
                     *  Skip to terminating star slash
                     */
                    if (p == NULL)
                        return NULL;
                    while (pzS < p) {
                        if (*(pzS++) == '\n')
                            (*pLineCt)++;
                    }

                    pzS = p + 2;
                }
                }
                goto scan_for_quote;

            default:
                /*
                 *  The next non-whitespace character is not a quote.
                 *  The series of quoted strings has come to an end.
                 */
                return pzS;
            }

            q = *(pzS++);  /* assign new quote character and advance scan */
        }

        /*
         *  We are inside a quoted string.  Copy text.
         */
        switch (*(pzD++) = *(pzS++)) {
        case NUL:
            return NULL;

        case '\n':
            (*pLineCt)++;
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
                (*pLineCt)++;
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
