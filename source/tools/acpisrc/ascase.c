/******************************************************************************
 *
 * Module Name: ascase - Source conversion - lower/upper case utilities
 *
 *****************************************************************************/

/******************************************************************************
 *
 * 1. Copyright Notice
 *
 * Some or all of this work - Copyright (c) 1999 - 2023, Intel Corp.
 * All rights reserved.
 *
 * 2. License
 *
 * 2.1. This is your license from Intel Corp. under its intellectual property
 * rights. You may have additional license terms from the party that provided
 * you this software, covering your right to use that party's intellectual
 * property rights.
 *
 * 2.2. Intel grants, free of charge, to any person ("Licensee") obtaining a
 * copy of the source code appearing in this file ("Covered Code") an
 * irrevocable, perpetual, worldwide license under Intel's copyrights in the
 * base code distributed originally by Intel ("Original Intel Code") to copy,
 * make derivatives, distribute, use and display any portion of the Covered
 * Code in any form, with the right to sublicense such rights; and
 *
 * 2.3. Intel grants Licensee a non-exclusive and non-transferable patent
 * license (with the right to sublicense), under only those claims of Intel
 * patents that are infringed by the Original Intel Code, to make, use, sell,
 * offer to sell, and import the Covered Code and derivative works thereof
 * solely to the minimum extent necessary to exercise the above copyright
 * license, and in no event shall the patent license extend to any additions
 * to or modifications of the Original Intel Code. No other license or right
 * is granted directly or by implication, estoppel or otherwise;
 *
 * The above copyright and patent license is granted only if the following
 * conditions are met:
 *
 * 3. Conditions
 *
 * 3.1. Redistribution of Source with Rights to Further Distribute Source.
 * Redistribution of source code of any substantial portion of the Covered
 * Code or modification with rights to further distribute source must include
 * the above Copyright Notice, the above License, this list of Conditions,
 * and the following Disclaimer and Export Compliance provision. In addition,
 * Licensee must cause all Covered Code to which Licensee contributes to
 * contain a file documenting the changes Licensee made to create that Covered
 * Code and the date of any change. Licensee must include in that file the
 * documentation of any changes made by any predecessor Licensee. Licensee
 * must include a prominent statement that the modification is derived,
 * directly or indirectly, from Original Intel Code.
 *
 * 3.2. Redistribution of Source with no Rights to Further Distribute Source.
 * Redistribution of source code of any substantial portion of the Covered
 * Code or modification without rights to further distribute source must
 * include the following Disclaimer and Export Compliance provision in the
 * documentation and/or other materials provided with distribution. In
 * addition, Licensee may not authorize further sublicense of source of any
 * portion of the Covered Code, and must include terms to the effect that the
 * license from Licensee to its licensee is limited to the intellectual
 * property embodied in the software Licensee provides to its licensee, and
 * not to intellectual property embodied in modifications its licensee may
 * make.
 *
 * 3.3. Redistribution of Executable. Redistribution in executable form of any
 * substantial portion of the Covered Code or modification must reproduce the
 * above Copyright Notice, and the following Disclaimer and Export Compliance
 * provision in the documentation and/or other materials provided with the
 * distribution.
 *
 * 3.4. Intel retains all right, title, and interest in and to the Original
 * Intel Code.
 *
 * 3.5. Neither the name Intel nor any other trademark owned or controlled by
 * Intel shall be used in advertising or otherwise to promote the sale, use or
 * other dealings in products derived from or relating to the Covered Code
 * without prior written authorization from Intel.
 *
 * 4. Disclaimer and Export Compliance
 *
 * 4.1. INTEL MAKES NO WARRANTY OF ANY KIND REGARDING ANY SOFTWARE PROVIDED
 * HERE. ANY SOFTWARE ORIGINATING FROM INTEL OR DERIVED FROM INTEL SOFTWARE
 * IS PROVIDED "AS IS," AND INTEL WILL NOT PROVIDE ANY SUPPORT, ASSISTANCE,
 * INSTALLATION, TRAINING OR OTHER SERVICES. INTEL WILL NOT PROVIDE ANY
 * UPDATES, ENHANCEMENTS OR EXTENSIONS. INTEL SPECIFICALLY DISCLAIMS ANY
 * IMPLIED WARRANTIES OF MERCHANTABILITY, NONINFRINGEMENT AND FITNESS FOR A
 * PARTICULAR PURPOSE.
 *
 * 4.2. IN NO EVENT SHALL INTEL HAVE ANY LIABILITY TO LICENSEE, ITS LICENSEES
 * OR ANY OTHER THIRD PARTY, FOR ANY LOST PROFITS, LOST DATA, LOSS OF USE OR
 * COSTS OF PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES, OR FOR ANY INDIRECT,
 * SPECIAL OR CONSEQUENTIAL DAMAGES ARISING OUT OF THIS AGREEMENT, UNDER ANY
 * CAUSE OF ACTION OR THEORY OF LIABILITY, AND IRRESPECTIVE OF WHETHER INTEL
 * HAS ADVANCE NOTICE OF THE POSSIBILITY OF SUCH DAMAGES. THESE LIMITATIONS
 * SHALL APPLY NOTWITHSTANDING THE FAILURE OF THE ESSENTIAL PURPOSE OF ANY
 * LIMITED REMEDY.
 *
 * 4.3. Licensee shall not export, either directly or indirectly, any of this
 * software or system incorporating such software without first obtaining any
 * required license or other approval from the U. S. Department of Commerce or
 * any other agency or department of the United States Government. In the
 * event Licensee exports any such software from the United States or
 * re-exports any such software from a foreign destination, Licensee shall
 * ensure that the distribution and export/re-export of the software is in
 * compliance with all laws, regulations, orders, or other restrictions of the
 * U.S. Export Administration Regulations. Licensee agrees that neither it nor
 * any of its subsidiaries will export/re-export any technical data, process,
 * software, or service, directly or indirectly, to any country for which the
 * United States government or any agency thereof requires an export license,
 * other governmental approval, or letter of assurance, without first obtaining
 * such license, approval or letter.
 *
 *****************************************************************************
 *
 * Alternatively, you may choose to be licensed under the terms of the
 * following license:
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Alternatively, you may choose to be licensed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 *
 *****************************************************************************/

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
 *              string. Returns count of the strings replaced.
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
            return (LowerCaseCount);
        }

        /*
         * Check for translation escape string -- means to ignore
         * blocks of code while replacing
         */
        if (Gbl_IgnoreTranslationEscapes)
        {
            SubString2 = NULL;
        }
        else
        {
            SubString2 = strstr (SubBuffer, AS_START_IGNORE);
        }

        if ((SubString2) &&
            (SubString2 < SubString1))
        {
            /* Find end of the escape block starting at "Substring2" */

            SubString2 = strstr (SubString2, AS_STOP_IGNORE);
            if (!SubString2)
            {
                /* Didn't find terminator */

                return (LowerCaseCount);
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

    return (LowerCaseCount);
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
    char                    *Buffer,
    char                    *Filename)
{
    UINT32                  Length;
    char                    *SubBuffer = Buffer;
    char                    *TokenEnd;
    char                    *TokenStart = NULL;
    char                    *SubString;
    UINT32                  LineNumber = 1;
    UINT32                  Count;


    /*
     * Examine the entire buffer (contains the entire file)
     * We are only interested in these tokens:
     *      Escape sequences - ignore entire sequence
     *      Single-quoted constants - ignore
     *      Quoted strings - ignore entire string
     *      Translation escape - starts with /,*,!
     *      Decimal and hex numeric constants - ignore entire token
     *      Entire uppercase token - ignore, it is a macro or define
     *      Starts with underscore, then a lowercase or digit: convert
     */
    while (*SubBuffer)
    {
        if (*SubBuffer == '\n')
        {
            LineNumber++;
            SubBuffer++;
            continue;
        }

        /* Ignore standard escape sequences (\n, \r, etc.)  Not Hex or Octal escapes */

        if (*SubBuffer == '\\')
        {
            SubBuffer += 2;
            continue;
        }

        /* Ignore single-quoted characters */

        if (*SubBuffer == '\'')
        {
            SubBuffer += 3;
            continue;
        }

        /* Ignore standard double-quoted strings */

        if (*SubBuffer == '"')
        {
            SubBuffer++;
            Count = 0;
            while (*SubBuffer != '"')
            {
                Count++;
                if ((!*SubBuffer) ||
                     (Count > 8192))
                {
                    printf ("Found an unterminated quoted string!, line %u: %s\n",
                        LineNumber, Filename);
                    return;
                }

                /* Handle escape sequences */

                if (*SubBuffer == '\\')
                {
                    SubBuffer++;
                }

                SubBuffer++;
            }

            SubBuffer++;
            continue;
        }

        /*
         * Check for translation escape string. It means to ignore
         * blocks of code during this code conversion.
         */
        if ((SubBuffer[0] == '/') &&
            (SubBuffer[1] == '*') &&
            (SubBuffer[2] == '!'))
        {
            SubBuffer = strstr (SubBuffer, "!*/");
            if (!SubBuffer)
            {
                printf ("Found an unterminated translation escape!, line %u: %s\n",
                    LineNumber, Filename);
                return;
            }

            continue;
        }

        /* Ignore anything that starts with a number (0-9) */

        if (isdigit ((int) *SubBuffer))
        {
            /* Ignore hex constants */

            if ((SubBuffer[0] == '0') &&
               ((SubBuffer[1] == 'x') || (SubBuffer[1] == 'X')))
            {
                SubBuffer += 2;
            }

            /* Skip over all digits, both decimal and hex */

            while (isxdigit ((int) *SubBuffer))
            {
                SubBuffer++;
            }
            TokenStart = NULL;
            continue;
        }

        /*
         * Check for fully upper case identifiers. These are usually macros
         * or defines. Allow decimal digits and embedded underscores.
         */
        if (isupper ((int) *SubBuffer))
        {
            SubString = SubBuffer + 1;
            while ((isupper ((int) *SubString)) ||
                   (isdigit ((int) *SubString)) ||
                   (*SubString == '_'))
            {
                SubString++;
            }

            /*
             * For the next character, anything other than a lower case
             * means that the identifier has terminated, and contains
             * exclusively Uppers/Digits/Underscores. Ignore the entire
             * identifier.
             */
            if (!islower ((int) *SubString))
            {
                SubBuffer = SubString + 1;
                continue;
            }
        }

        /*
         * These forms may indicate an identifier that can be converted:
         *      <UpperCase><LowerCase> (Ax)
         *      <UpperCase><Number> (An)
         */
        if (isupper ((int) SubBuffer[0]) &&
          ((islower ((int) SubBuffer[1])) || isdigit ((int) SubBuffer[1])))
        {
            TokenStart = SubBuffer;
            SubBuffer++;

            while (1)
            {
                /* Walk over the lower case letters and decimal digits */

                while (islower ((int) *SubBuffer) ||
                       isdigit ((int) *SubBuffer))
                {
                    SubBuffer++;
                }

                /* Check for end of line or end of token */

                if (*SubBuffer == '\n')
                {
                    LineNumber++;
                    break;
                }

                if (*SubBuffer == ' ')
                {
                    /* Check for form "Axx - " in a parameter header description */

                    while (*SubBuffer == ' ')
                    {
                        SubBuffer++;
                    }

                    SubBuffer--;
                    if ((SubBuffer[1] == '-') &&
                        (SubBuffer[2] == ' '))
                    {
                        if (TokenStart)
                        {
                            *TokenStart = (char) tolower ((int) *TokenStart);
                        }
                    }
                    break;
                }

                /*
                 * Ignore these combinations:
                 *      <Letter><Digit><UpperCase>
                 *      <Digit><Digit><UpperCase>
                 *      <Underscore><Digit><UpperCase>
                 */
                if (isdigit ((int) *SubBuffer))
                {
                    if (isalnum ((int) *(SubBuffer-1)) ||
                        *(SubBuffer-1) == '_')
                    {
                        break;
                    }
                }

                /* Ignore token if next character is not uppercase or digit */

                if (!isupper ((int) *SubBuffer) &&
                    !isdigit ((int) *SubBuffer))
                {
                    break;
                }

                /*
                 * Form <UpperCase><LowerCaseLetters><UpperCase> (AxxB):
                 * Convert leading character of the token to lower case
                 */
                if (TokenStart)
                {
                    *TokenStart = (char) tolower ((int) *TokenStart);
                    TokenStart = NULL;
                }

                /* Find the end of this identifier (token) */

                TokenEnd = SubBuffer - 1;
                while ((isalnum ((int) *TokenEnd)) ||
                       (*TokenEnd == '_'))
                {
                    TokenEnd++;
                }

                SubString = TokenEnd;
                Length = 0;

                while (*SubString != '\n')
                {
                    /*
                     * If we have at least two trailing spaces, we can get rid of
                     * one to make up for the newly inserted underscore. This will
                     * help preserve the alignment of the text
                     */
                    if ((SubString[0] == ' ') &&
                        (SubString[1] == ' '))
                    {
                        Length = SubString - SubBuffer - 1;
                        break;
                    }

                    SubString++;
                }

                if (!Length)
                {
                    Length = strlen (&SubBuffer[0]);
                }

                /*
                 * Within this identifier, convert this pair of letters that
                 * matches the form:
                 *
                 *      <LowerCase><UpperCase>
                 * to
                 *      <LowerCase><Underscore><LowerCase>
                 */
                Gbl_MadeChanges = TRUE;

                /* Insert the underscore */

                memmove (&SubBuffer[1], &SubBuffer[0], Length + 1);
                SubBuffer[0] = '_';

                /*
                 * If we have <UpperCase><UpperCase>, leave them as-is
                 * Enables transforms like:
                 *      LocalFADT -> local_FADT
                 */
                if (isupper ((int) SubBuffer[2]))
                {
                    SubBuffer += 1;
                    break;
                }

                /* Lower case the original upper case letter */

                SubBuffer[1] = (char) tolower ((int) SubBuffer[1]);
                SubBuffer += 2;
            }
        }

        SubBuffer++;
    }
}


/******************************************************************************
 *
 * FUNCTION:    AsLowerCaseIdentifiers
 *
 * DESCRIPTION: Converts mixed case identifiers to lower case. Leaves comments,
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
