/*
        This program is, essentially, a cross product generator.  It takes
        an input file which is said to consist of a number of lines, and
        expands each line.  A line like
                (a,b)(c,d)
        will be expanded to lines like
                ac
                ad
                bc
                bd
        (without regard for the ORDER of the output; ie: the lines can appear
        in any order).

        Parenthesis can be nested, so
                (a,b)(c(d,e),f)
        will produce
                acd
                ace
                af
                bcd
                bce
                bf
 */


#include <stdio.h>

char leftParen,                         /* The left parenthesis character */
        rightParen;                     /* The right parenthesis character */


/* Finds next occurrence of 'character' at this level of nesting.
        Returns 0 if no such character found.
 */

char *
ThisLevel(string, character)
char *string, character;
{
        int level;                      /* Level 0 is OUR level */

        level = 0;

        while (*string != '\0') {
                if (*string == leftParen)
                        level++;
                else if (*string == rightParen) {
                        level--;
                        if (level < 0)
                                return(0);
                }
                if ((level == 0) && (*string == character))
                                return(string);
                string++;
        }
