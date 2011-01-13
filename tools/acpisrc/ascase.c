
/******************************************************************************
 *
 * Module Name: ascase - Source conversion - lower/upper case utilities
 *
 *****************************************************************************/

/*
 * Copyright (C) 2000 - 2011, Intel Corp.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    substantially similar to the "NO WARRANTY" disclaimer below
 *    ("Disclaimer") and any redistribution must be conditioned upon
 *    including a substantially similar Disclaimer requirement for further
 *    binary redistribution.
 * 3. Neither the names of the above-listed copyright holders nor the names
 *    of any contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGES.
 */

#include "acpisrc.h"

/* Local prototypes */

void
AsUppercaseTokens (
    char                    *Buffer,
    char                    *PrefixString);


/******************************************************************************
 *
 * FUNCTION:    AsLowerCaseString
 *
 * DESCRIPTION: LowerCase all instances of a target string with a replacement
 *              string.  Returns count of the strings replaced.
 *
 ******************************************************************************/

int
AsLowerCaseString (
    char                    *Target,
    char                    *Buffer)
{
    char                    *SubString1;
    char                    *SubString2;
    char                    *SubBuffer;
    int                     TargetLength;
    int                     LowerCaseCount = 0;
    int                     i;


    TargetLength = strlen (Target);

    SubBuffer = Buffer;
    SubString1 = Buffer;

    while (SubString1)
    {
        /* Find the target string */

        SubString1 = strstr (SubBuffer, Target);
        if (!SubString1)
        {
            return LowerCaseCount;
        }

        /*
         * Check for translation escape string -- means to ignore
         * blocks of code while replacing
         */
        SubString2 = strstr (SubBuffer, AS_START_IGNORE);

        if ((SubString2) &&
            (SubString2 < SubString1))
        {
            /* Find end of the escape block starting at "Substring2" */

            SubString2 = strstr (SubString2, AS_STOP_IGNORE);
            if (!SubString2)
            {
                /* Didn't find terminator */

                return LowerCaseCount;
            }

            /* Move buffer to end of escape block and continue */

            SubBuffer = SubString2;
        }

        /* Do the actual replace if the target was found */

        else
        {
            if (!AsMatchExactWord (SubString1, TargetLength))
            {
                SubBuffer = SubString1 + 1;
                continue;
            }

            for (i = 0; i < TargetLength; i++)
            {
                SubString1[i] = (char) tolower ((int) SubString1[i]);
            }

            SubBuffer = SubString1 + TargetLength;

            if ((Gbl_WidenDeclarations) && (!Gbl_StructDefs))
            {
                if ((SubBuffer[0] == ' ') && (SubBuffer[1] == ' '))
                {
                    AsInsertData (SubBuffer, "        ", 8);
                }
            }

            LowerCaseCount++;
        }
    }

    return LowerCaseCount;
}


/******************************************************************************
 *
 * FUNCTION:    AsMixedCaseToUnderscores
 *
 * DESCRIPTION: Converts mixed case identifiers to underscored identifiers.
 *              for example,
 *
 *              ThisUsefullyNamedIdentifier   becomes:
 *
 *              this_usefully_named_identifier
 *
 ******************************************************************************/

void
AsMixedCaseToUnderscores (
    char                    *Buffer)
{
    UINT32                  Length;
    char                    *SubBuffer = Buffer;
    char                    *TokenEnd;
    char                    *TokenStart = NULL;
    char                    *SubString;
    BOOLEAN                 HasLowerCase = FALSE;


    while (*SubBuffer)
    {
        /* Ignore whitespace */

        if (*SubBuffer == ' ')
        {
            while (*SubBuffer == ' ')
            {
                SubBuffer++;
            }
            TokenStart = NULL;
            HasLowerCase = FALSE;
            continue;
        }

        /* Ignore commas */

        if ((*SubBuffer == ',') ||
            (*SubBuffer == '>') ||
            (*SubBuffer == ')'))
        {
            SubBuffer++;
            TokenStart = NULL;
            HasLowerCase = FALSE;
            continue;
        }

        /* Check for quoted string -- ignore */

        if (*SubBuffer == '"')
        {
            SubBuffer++;
            while (*SubBuffer != '"')
            {
                if (!*SubBuffer)
                {
                    return;
                }

                /* Handle embedded escape sequences */

                if (*SubBuffer == '\\')
                {
                    SubBuffer++;
                }
                SubBuffer++;
            }
            SubBuffer++;
            continue;
        }

        if (islower ((int) *SubBuffer))
        {
            HasLowerCase = TRUE;
        }

        /*
         * Check for translation escape string -- means to ignore
         * blocks of code while replacing
         */
        if ((SubBuffer[0] == '/') &&
            (SubBuffer[1] == '*') &&
            (SubBuffer[2] == '!'))
        {
            SubBuffer = strstr (SubBuffer, "!*/");
            if (!SubBuffer)
            {
                return;
            }
            continue;
        }

        /* Ignore hex constants */

        if (SubBuffer[0] == '0')
        {
            if ((SubBuffer[1] == 'x') ||
                (SubBuffer[1] == 'X'))
            {
                SubBuffer += 2;
                while (isxdigit ((int) *SubBuffer))
                {
                    SubBuffer++;
                }
                continue;
            }
        }

/* OBSOLETE CODE, all quoted strings now completely ignored. */
#if 0
        /* Ignore format specification fields */

        if (SubBuffer[0] == '%')
        {
            SubBuffer++;

            while ((isalnum (*SubBuffer)) || (*SubBuffer == '.'))
            {
                SubBuffer++;
            }

            continue;
        }
#endif

        /* Ignore standard escape sequences (\n, \r, etc.)  Not Hex or Octal escapes */

        if (SubBuffer[0] == '\\')
        {
            SubBuffer += 2;
            continue;
        }

        /*
         * Ignore identifiers that already contain embedded underscores
         * These are typically C macros or defines (all upper case)
         * Note: there are some cases where identifiers have underscores
         * AcpiGbl_* for example. HasLowerCase flag handles these.
         */
        if ((*SubBuffer == '_') && (!HasLowerCase) && (TokenStart))
        {
            /* Check the rest of the identifier for any lower case letters */

            SubString = SubBuffer;
            while ((isalnum ((int) *SubString)) || (*SubString == '_'))
            {
                if (islower ((int) *SubString))
                {
                    HasLowerCase = TRUE;
                }
                SubString++;
            }

            /* If no lower case letters, we can safely ignore the entire token */

            if (!HasLowerCase)
            {
                SubBuffer = SubString;
                continue;
            }
        }

        /* A capital letter may indicate the start of a token; save it */

        if (isupper ((int) SubBuffer[0]))
        {
            TokenStart = SubBuffer;
        }

        /*
         * Convert each pair of letters that matches the form:
         *
         *      <LowerCase><UpperCase>
         * to
         *      <LowerCase><Underscore><LowerCase>
         */
        else if ((islower ((int) SubBuffer[0]) || isdigit ((int) SubBuffer[0])) &&
                 (isupper ((int) SubBuffer[1])))
        {
            if (isdigit ((int) SubBuffer[0]))
            {
                /* Ignore <UpperCase><Digit><UpperCase> */
                /* Ignore <Underscore><Digit><UpperCase> */

                if (isupper ((int) *(SubBuffer-1)) ||
                    *(SubBuffer-1) == '_')
                {
                    SubBuffer++;
                    continue;
                }
            }

            /*
             * Matched the pattern.
             * Find the end of this identifier (token)
             */
            TokenEnd = SubBuffer;
            while ((isalnum ((int) *TokenEnd)) || (*TokenEnd == '_'))
            {
                TokenEnd++;
            }

            /* Force the UpperCase letter (#2) to lower case */

            Gbl_MadeChanges = TRUE;
            SubBuffer[1] = (char) tolower ((int) SubBuffer[1]);

            SubString = TokenEnd;
            Length = 0;

            while (*SubString != '\n')
            {
                /*
                 * If we have at least two trailing spaces, we can get rid of
                 * one to make up for the newly inserted underscore.  This will
                 * help preserve the alignment of the text
                 */
                if ((SubString[0] == ' ') &&
                    (SubString[1] == ' '))
                {
                    Length = SubString - SubBuffer - 2;
                    break;
                }

                SubString++;
            }

            if (!Length)
            {
                Length = strlen (&SubBuffer[1]);
            }

            memmove (&SubBuffer[2], &SubBuffer[1], Length + 1);
            SubBuffer[1] = '_';
            SubBuffer +=2;

            /* Lower case the leading character of the token */

            if (TokenStart)
            {
                *TokenStart = (char) tolower ((int) *TokenStart);
                TokenStart = NULL;
            }
        }

        SubBuffer++;
    }
}


/******************************************************************************
 *
 * FUNCTION:    AsLowerCaseIdentifiers
 *
 * DESCRIPTION: Converts mixed case identifiers to lower case.  Leaves comments,
 *              quoted strings, and all-upper-case macros alone.
 *
 ******************************************************************************/

void
AsLowerCaseIdentifiers (
    char                    *Buffer)
{
    char                    *SubBuffer = Buffer;


    while (*SubBuffer)
    {
        /*
         * Check for translation escape string -- means to ignore
         * blocks of code while replacing
         */
        if ((SubBuffer[0] == '/') &&
            (SubBuffer[1] == '*') &&
            (SubBuffer[2] == '!'))
        {
            SubBuffer = strstr (SubBuffer, "!*/");
            if (!SubBuffer)
            {
                return;
            }
        }

        /* Ignore comments */

        if ((SubBuffer[0] == '/') &&
            (SubBuffer[1] == '*'))
        {
            SubBuffer = strstr (SubBuffer, "*/");
            if (!SubBuffer)
            {
                return;
            }

            SubBuffer += 2;
        }

        /* Ignore quoted strings */

        if ((SubBuffer[0] == '\"') && (SubBuffer[1] != '\''))
        {
            SubBuffer++;

            /* Find the closing quote */

            while (SubBuffer[0])
            {
                /* Ignore escaped quote characters */

                if (SubBuffer[0] == '\\')
                {
                    SubBuffer++;
                }
                else if (SubBuffer[0] == '\"')
                {
                    SubBuffer++;
                    break;
                }
                SubBuffer++;
            }
        }

        if (!SubBuffer[0])
        {
            return;
        }

        /*
         * Only lower case if we have an upper followed by a lower
         * This leaves the all-uppercase things (macros, etc.) intact
         */
        if ((isupper ((int) SubBuffer[0])) &&
            (islower ((int) SubBuffer[1])))
        {
            Gbl_MadeChanges = TRUE;
            *SubBuffer = (char) tolower ((int) *SubBuffer);
        }

        SubBuffer++;
    }
}


/******************************************************************************
 *
 * FUNCTION:    AsUppercaseTokens
 *
 * DESCRIPTION: Force to uppercase all tokens that begin with the prefix string.
 *              used to convert mixed-case macros and constants to uppercase.
 *
 ******************************************************************************/

void
AsUppercaseTokens (
    char                    *Buffer,
    char                    *PrefixString)
{
    char                    *SubBuffer;
    char                    *TokenEnd;
    char                    *SubString;
    int                     i;
    UINT32                  Length;


    SubBuffer = Buffer;

    while (SubBuffer)
    {
        SubBuffer = strstr (SubBuffer, PrefixString);
        if (SubBuffer)
        {
            TokenEnd = SubBuffer;
            while ((isalnum ((int) *TokenEnd)) || (*TokenEnd == '_'))
            {
                TokenEnd++;
            }

            for (i = 0; i < (TokenEnd - SubBuffer); i++)
            {
                if ((islower ((int) SubBuffer[i])) &&
                    (isupper ((int) SubBuffer[i+1])))
                {

                    SubString = TokenEnd;
                    Length = 0;

                    while (*SubString != '\n')
                    {
                        if ((SubString[0] == ' ') &&
                            (SubString[1] == ' '))
                        {
                            Length = SubString - &SubBuffer[i] - 2;
                            break;
                        }

                        SubString++;
                    }

                    if (!Length)
                    {
                        Length = strlen (&SubBuffer[i+1]);
                    }

                    memmove (&SubBuffer[i+2], &SubBuffer[i+1], (Length+1));
                    SubBuffer[i+1] = '_';
                    i +=2;
                    TokenEnd++;
                }
            }

            for (i = 0; i < (TokenEnd - SubBuffer); i++)
            {
                SubBuffer[i] = (char) toupper ((int) SubBuffer[i]);
            }

            SubBuffer = TokenEnd;
        }
    }
}


