/******************************************************************************
*                                                                             *
*   s t r s t r                                                               *
*                                                                             *
*   Find the first occurrence of a string in another string.                  *
*                                                                             *
* Format:                                                                     *
*             return = strstr(Source,What);                                   *
*                                                                             *
* Parameters:                                                                 *
*                                                                             *
* Returns:                                                                    *
*                                                                             *
* Scope:      PUBLIC                                                          *
*                                                                             *
******************************************************************************/

char *strstr(Source, What)
register const char *Source;
register const char *What;
{
register char WhatChar;
register char SourceChar;
register long Length;


    if ((WhatChar = *What++) != 0) {
        Length = strlen(What);
        do {
            do {
                if ((SourceChar = *Source++) == 0) {
                    return (0);
                }
            } while (SourceChar != WhatChar);
        } while (strncmp(Source, What, Length) != 0);
        Source--;
    }
    return ((char *)Source);

}/*strstr*/
