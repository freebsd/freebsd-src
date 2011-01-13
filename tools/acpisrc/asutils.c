
/******************************************************************************
 *
 * Module Name: asutils - common utilities
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


/******************************************************************************
 *
 * FUNCTION:    AsSkipUntilChar
 *
 * DESCRIPTION: Find the next instance of the input character
 *
 ******************************************************************************/

char *
AsSkipUntilChar (
    char                    *Buffer,
    char                    Target)
{

    while (*Buffer != Target)
    {
        if (!*Buffer)
        {
            return NULL;
        }

        Buffer++;
    }

    return (Buffer);
}


/******************************************************************************
 *
 * FUNCTION:    AsSkipPastChar
 *
 * DESCRIPTION: Find the next instance of the input character, return a buffer
 *              pointer to this character+1.
 *
 ******************************************************************************/

char *
AsSkipPastChar (
    char                    *Buffer,
    char                    Target)
{

    while (*Buffer != Target)
    {
        if (!*Buffer)
        {
            return NULL;
        }

        Buffer++;
    }

    Buffer++;

    return (Buffer);
}


/******************************************************************************
 *
 * FUNCTION:    AsReplaceData
 *
 * DESCRIPTION: This function inserts and removes data from the file buffer.
 *              if more data is inserted than is removed, the data in the buffer
 *              is moved to make room.  If less data is inserted than is removed,
 *              the remaining data is moved to close the hole.
 *
 ******************************************************************************/

char *
AsReplaceData (
    char                    *Buffer,
    UINT32                  LengthToRemove,
    char                    *BufferToAdd,
    UINT32                  LengthToAdd)
{
    UINT32                  BufferLength;


    /*
     * Buffer is a string, so the length must include the terminating zero
     */
    BufferLength = strlen (Buffer) + 1;

    if (LengthToRemove != LengthToAdd)
    {
        /*
         * Move some of the existing data
         * 1) If adding more bytes than removing, make room for the new data
         * 2) if removing more bytes than adding, delete the extra space
         */
        if (LengthToRemove > 0)
        {
            Gbl_MadeChanges = TRUE;
            memmove ((Buffer + LengthToAdd), (Buffer + LengthToRemove), (BufferLength - LengthToRemove));
        }
    }

    /*
     * Now we can move in the new data
     */
    if (LengthToAdd > 0)
    {
        Gbl_MadeChanges = TRUE;
        memmove (Buffer, BufferToAdd, LengthToAdd);
    }

    return (Buffer + LengthToAdd);
}


/******************************************************************************
 *
 * FUNCTION:    AsInsertData
 *
 * DESCRIPTION: This function inserts and removes data from the file buffer.
 *              if more data is inserted than is removed, the data in the buffer
 *              is moved to make room.  If less data is inserted than is removed,
 *              the remaining data is moved to close the hole.
 *
 ******************************************************************************/

char *
AsInsertData (
    char                    *Buffer,
    char                    *BufferToAdd,
    UINT32                  LengthToAdd)
{
    UINT32                  BufferLength;


    if (LengthToAdd > 0)
    {
        /*
         * Buffer is a string, so the length must include the terminating zero
         */
        BufferLength = strlen (Buffer) + 1;

        /*
         * Move some of the existing data
         * 1) If adding more bytes than removing, make room for the new data
         * 2) if removing more bytes than adding, delete the extra space
         */
        Gbl_MadeChanges = TRUE;
        memmove ((Buffer + LengthToAdd), Buffer, BufferLength);

        /*
         * Now we can move in the new data
         */
        memmove (Buffer, BufferToAdd, LengthToAdd);
    }

    return (Buffer + LengthToAdd);
}


/******************************************************************************
 *
 * FUNCTION:    AsRemoveData
 *
 * DESCRIPTION: This function inserts and removes data from the file buffer.
 *              if more data is inserted than is removed, the data in the buffer
 *              is moved to make room.  If less data is inserted than is removed,
 *              the remaining data is moved to close the hole.
 *
 ******************************************************************************/

char *
AsRemoveData (
    char                    *StartPointer,
    char                    *EndPointer)
{
    UINT32                  BufferLength;


    /*
     * Buffer is a string, so the length must include the terminating zero
     */
    BufferLength = strlen (EndPointer) + 1;

    Gbl_MadeChanges = TRUE;
    memmove (StartPointer, EndPointer, BufferLength);

    return (StartPointer);
}

