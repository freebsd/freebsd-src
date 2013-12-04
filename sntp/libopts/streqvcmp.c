
/**
 * \file streqvcmp.c
 *
 * Time-stamp:      "2010-07-17 10:16:24 bkorb"
 *
 *  String Equivalence Comparison
 *
 *  These routines allow any character to be mapped to any other
 *  character before comparison.  In processing long option names,
 *  the characters "-", "_" and "^" all need to be equivalent
 *  (because they are treated so by different development environments).
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
 *
 * This array is designed for mapping upper and lower case letter
 * together for a case independent comparison.  The mappings are
 * based upon ascii character sequences.
 */
static unsigned char charmap[] = {
    0x00, 0x01, 0x02, 0x03,  0x04, 0x05, 0x06, '\a',
    '\b', '\t', '\n', '\v',  '\f', '\r', 0x0E, 0x0F,
    0x10, 0x11, 0x12, 0x13,  0x14, 0x15, 0x16, 0x17,
    0x18, 0x19, 0x1A, 0x1B,  0x1C, 0x1D, 0x1E, 0x1F,

    ' ',  '!',  '"',  '#',   '$',  '%',  '&',  '\'',
    '(',  ')',  '*',  '+',   ',',  '-',  '.',  '/',
    '0',  '1',  '2',  '3',   '4',  '5',  '6',  '7',
    '8',  '9',  ':',  ';',   '<',  '=',  '>',  '?',

    '@',  'a',  'b',  'c',   'd',  'e',  'f',  'g',
    'h',  'i',  'j',  'k',   'l',  'm',  'n',  'o',
    'p',  'q',  'r',  's',   't',  'u',  'v',  'w',
    'x',  'y',  'z',  '[',   '\\', ']',  '^',  '_',
    '`',  'a',  'b',  'c',   'd',  'e',  'f',  'g',
    'h',  'i',  'j',  'k',   'l',  'm',  'n',  'o',
    'p',  'q',  'r',  's',   't',  'u',  'v',  'w',
    'x',  'y',  'z',  '{',   '|',  '}',  '~',  0x7f,

    0x80, 0x81, 0x82, 0x83,  0x84, 0x85, 0x86, 0x87,
    0x88, 0x89, 0x8A, 0x8B,  0x8C, 0x8D, 0x8E, 0x8F,
    0x90, 0x91, 0x92, 0x93,  0x94, 0x95, 0x96, 0x97,
    0x98, 0x99, 0x9A, 0x9B,  0x9C, 0x9D, 0x9E, 0x9F,
    0xA0, 0xA1, 0xA2, 0xA3,  0xA4, 0xA5, 0xA6, 0xA7,
    0xA8, 0xA9, 0xAA, 0xAB,  0xAC, 0xAD, 0xAE, 0xAF,
    0xB0, 0xB1, 0xB2, 0xB3,  0xB4, 0xB5, 0xB6, 0xB7,
    0xB8, 0xB9, 0xBA, 0xBB,  0xBC, 0xBD, 0xBE, 0xBF,

    0xC0, 0xC1, 0xC2, 0xC3,  0xC4, 0xC5, 0xC6, 0xC7,
    0xC8, 0xC9, 0xCA, 0xCB,  0xCC, 0xCD, 0xCE, 0xCF,
    0xD0, 0xD1, 0xD2, 0xD3,  0xD4, 0xD5, 0xD6, 0xD7,
    0xD8, 0xD9, 0xDA, 0xDB,  0xDC, 0xDD, 0xDE, 0xDF,
    0xE0, 0xE1, 0xE2, 0xE3,  0xE4, 0xE5, 0xE6, 0xE7,
    0xE8, 0xE9, 0xEA, 0xEB,  0xEC, 0xED, 0xEE, 0xEF,
    0xF0, 0xF1, 0xF2, 0xF3,  0xF4, 0xF5, 0xF6, 0xF7,
    0xF8, 0xF9, 0xFA, 0xFB,  0xFC, 0xFD, 0xFE, 0xFF,
};


/*=export_func strneqvcmp
 *
 * what: compare two strings with an equivalence mapping
 *
 * arg:  + char const* + str1 + first string +
 * arg:  + char const* + str2 + second string +
 * arg:  + int         + ct   + compare length +
 *
 * ret_type:  int
 * ret_desc:  the difference between two differing characters
 *
 * doc:
 *
 * Using a character mapping, two strings are compared for "equivalence".
 * Each input character is mapped to a comparison character and the
 * mapped-to characters are compared for the two NUL terminated input strings.
 * The comparison is limited to @code{ct} bytes.
 * This function name is mapped to option_strneqvcmp so as to not conflict
 * with the POSIX name space.
 *
 * err:  none checked.  Caller responsible for seg faults.
=*/
int
strneqvcmp(tCC* s1, tCC* s2, int ct)
{
    for (; ct > 0; --ct) {
        unsigned char u1 = (unsigned char) *s1++;
        unsigned char u2 = (unsigned char) *s2++;
        int dif = charmap[ u1 ] - charmap[ u2 ];

        if (dif != 0)
            return dif;

        if (u1 == NUL)
            return 0;
    }

    return 0;
}


/*=export_func streqvcmp
 *
 * what: compare two strings with an equivalence mapping
 *
 * arg:  + char const* + str1 + first string +
 * arg:  + char const* + str2 + second string +
 *
 * ret_type:  int
 * ret_desc:  the difference between two differing characters
 *
 * doc:
 *
 * Using a character mapping, two strings are compared for "equivalence".
 * Each input character is mapped to a comparison character and the
 * mapped-to characters are compared for the two NUL terminated input strings.
 * This function name is mapped to option_streqvcmp so as to not conflict
 * with the POSIX name space.
 *
 * err:  none checked.  Caller responsible for seg faults.
=*/
int
streqvcmp(tCC* s1, tCC* s2)
{
    for (;;) {
        unsigned char u1 = (unsigned char) *s1++;
        unsigned char u2 = (unsigned char) *s2++;
        int dif = charmap[ u1 ] - charmap[ u2 ];

        if (dif != 0)
            return dif;

        if (u1 == NUL)
            return 0;
    }
}


/*=export_func streqvmap
 *
 * what: Set the character mappings for the streqv functions
 *
 * arg:  + char + From + Input character +
 * arg:  + char + To   + Mapped-to character +
 * arg:  + int  + ct   + compare length +
 *
 * doc:
 *
 * Set the character mapping.  If the count (@code{ct}) is set to zero, then
 * the map is cleared by setting all entries in the map to their index
 * value.  Otherwise, the "@code{From}" character is mapped to the "@code{To}"
 * character.  If @code{ct} is greater than 1, then @code{From} and @code{To}
 * are incremented and the process repeated until @code{ct} entries have been
 * set. For example,
 * @example
 *    streqvmap('a', 'A', 26);
 * @end example
 * @noindent
 * will alter the mapping so that all English lower case letters
 * will map to upper case.
 *
 * This function name is mapped to option_streqvmap so as to not conflict
 * with the POSIX name space.
 *
 * err:  none.
=*/
void
streqvmap(char From, char To, int ct)
{
    if (ct == 0) {
        ct = sizeof(charmap) - 1;
        do  {
            charmap[ ct ] = ct;
        } while (--ct >= 0);
    }

    else {
        int  chTo   = (int)To   & 0xFF;
        int  chFrom = (int)From & 0xFF;

        do  {
            charmap[ chFrom ] = (unsigned)chTo;
            chFrom++;
            chTo++;
            if ((chFrom >= sizeof(charmap)) || (chTo >= sizeof(charmap)))
                break;
        } while (--ct > 0);
    }
}


/*=export_func strequate
 *
 * what: map a list of characters to the same value
 *
 * arg:  + char const* + ch_list + characters to equivalence +
 *
 * doc:
 *
 * Each character in the input string get mapped to the first character
 * in the string.
 * This function name is mapped to option_strequate so as to not conflict
 * with the POSIX name space.
 *
 * err:  none.
=*/
void
strequate(char const* s)
{
    if ((s != NULL) && (*s != NUL)) {
        unsigned char equiv = (unsigned)*s;
        while (*s != NUL)
            charmap[ (unsigned)*(s++) ] = equiv;
    }
}


/*=export_func strtransform
 *
 * what: convert a string into its mapped-to value
 *
 * arg:  + char*       + dest + output string +
 * arg:  + char const* + src  + input string +
 *
 * doc:
 *
 * Each character in the input string is mapped and the mapped-to
 * character is put into the output.
 * This function name is mapped to option_strtransform so as to not conflict
 * with the POSIX name space.
 *
 * The source and destination may be the same.
 *
 * err:  none.
=*/
void
strtransform(char* d, char const* s)
{
    do  {
        *(d++) = (char)charmap[ (unsigned)*s ];
    } while (*(s++) != NUL);
}

/*
 * Local Variables:
 * mode: C
 * c-file-style: "stroustrup"
 * indent-tabs-mode: nil
 * End:
 * end of autoopts/streqvcmp.c */
