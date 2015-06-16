/******************************************************************************
 *
 * Module Name: osefixf - EFI OSL interfaces
 *
 *****************************************************************************/

/*
 * Copyright (C) 2000 - 2015, Intel Corp.
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

#include "acpi.h"
#include "accommon.h"
#include "acapps.h"

#define _COMPONENT          ACPI_OS_SERVICES
        ACPI_MODULE_NAME    ("osefixf")


/* Local definitions */

#define ACPI_EFI_PRINT_LENGTH   256


/* Local prototypes */

static ACPI_STATUS
AcpiEfiArgify (
    char                    *String,
    int                     *ArgcPtr,
    char                    ***ArgvPtr);

static BOOLEAN
AcpiEfiCompareGuid (
    EFI_GUID                *Guid1,
    EFI_GUID                *Guid2);

static ACPI_STATUS
AcpiEfiConvertArgcv (
    CHAR16                  *LoadOpt,
    UINT32                  LoadOptSize,
    int                     *ArgcPtr,
    char                    ***ArgvPtr,
    char                    **BufferPtr);

static ACPI_PHYSICAL_ADDRESS
AcpiEfiGetRsdpViaGuid (
    EFI_GUID                *Guid);

static CHAR16 *
AcpiEfiFlushFile (
    ACPI_FILE               File,
    CHAR16                  *Begin,
    CHAR16                  *End,
    CHAR16                  *Pos,
    BOOLEAN                 FlushAll);


/* Local variables */

static EFI_FILE_HANDLE      AcpiGbl_EfiCurrentVolume = NULL;
EFI_GUID                    AcpiGbl_LoadedImageProtocol = LOADED_IMAGE_PROTOCOL;
EFI_GUID                    AcpiGbl_TextInProtocol = SIMPLE_TEXT_INPUT_PROTOCOL;
EFI_GUID                    AcpiGbl_TextOutProtocol = SIMPLE_TEXT_OUTPUT_PROTOCOL;
EFI_GUID                    AcpiGbl_FileSystemProtocol = SIMPLE_FILE_SYSTEM_PROTOCOL;


/******************************************************************************
 *
 * FUNCTION:    AcpiEfiGetRsdpViaGuid
 *
 * PARAMETERS:  Guid1               - GUID to compare
 *              Guid2               - GUID to compare
 *
 * RETURN:      TRUE if Guid1 == Guid2
 *
 * DESCRIPTION: Compares two GUIDs
 *
 *****************************************************************************/

static BOOLEAN
AcpiEfiCompareGuid (
    EFI_GUID                *Guid1,
    EFI_GUID                *Guid2)
{
    INT32                   *g1;
    INT32                   *g2;
    INT32                   r;


    g1 = (INT32 *) Guid1;
    g2 = (INT32 *) Guid2;

    r  = g1[0] - g2[0];
    r |= g1[1] - g2[1];
    r |= g1[2] - g2[2];
    r |= g1[3] - g2[3];

    return (r ? FALSE : TRUE);
}


/******************************************************************************
 *
 * FUNCTION:    AcpiEfiGetRsdpViaGuid
 *
 * PARAMETERS:  None
 *
 * RETURN:      RSDP address if found
 *
 * DESCRIPTION: Find RSDP address via EFI using specified GUID.
 *
 *****************************************************************************/

static ACPI_PHYSICAL_ADDRESS
AcpiEfiGetRsdpViaGuid (
    EFI_GUID                *Guid)
{
    ACPI_PHYSICAL_ADDRESS   Address = 0;
    int                     i;


    for (i = 0; i < ST->NumberOfTableEntries; i++)
    {
        if (AcpiEfiCompareGuid (&ST->ConfigurationTable[i].VendorGuid, Guid))
        {
            Address = ACPI_PTR_TO_PHYSADDR (
                    ST->ConfigurationTable[i].VendorTable);
            break;
        }
    }

    return (Address);
}


/******************************************************************************
 *
 * FUNCTION:    AcpiOsGetRootPointer
 *
 * PARAMETERS:  None
 *
 * RETURN:      RSDP physical address
 *
 * DESCRIPTION: Gets the ACPI root pointer (RSDP)
 *
 *****************************************************************************/

ACPI_PHYSICAL_ADDRESS
AcpiOsGetRootPointer (
    void)
{
    ACPI_PHYSICAL_ADDRESS   Address;
    EFI_GUID                Guid10 = ACPI_TABLE_GUID;
    EFI_GUID                Guid20 = ACPI_20_TABLE_GUID;


    Address = AcpiEfiGetRsdpViaGuid (&Guid20);
    if (!Address)
    {
        Address = AcpiEfiGetRsdpViaGuid (&Guid10);
    }

    return (Address);
}


/******************************************************************************
 *
 * FUNCTION:    AcpiOsMapMemory
 *
 * PARAMETERS:  where               - Physical address of memory to be mapped
 *              length              - How much memory to map
 *
 * RETURN:      Pointer to mapped memory. Null on error.
 *
 * DESCRIPTION: Map physical memory into caller's address space
 *
 *****************************************************************************/

void *
AcpiOsMapMemory (
    ACPI_PHYSICAL_ADDRESS   where,
    ACPI_SIZE               length)
{

    return (ACPI_TO_POINTER ((ACPI_SIZE) where));
}


/******************************************************************************
 *
 * FUNCTION:    AcpiOsUnmapMemory
 *
 * PARAMETERS:  where               - Logical address of memory to be unmapped
 *              length              - How much memory to unmap
 *
 * RETURN:      None
 *
 * DESCRIPTION: Delete a previously created mapping. Where and Length must
 *              correspond to a previous mapping exactly.
 *
 *****************************************************************************/

void
AcpiOsUnmapMemory (
    void                    *where,
    ACPI_SIZE               length)
{

    return;
}


/******************************************************************************
 *
 * FUNCTION:    Spinlock interfaces
 *
 * DESCRIPTION: No-op on single threaded BIOS
 *
 *****************************************************************************/

ACPI_STATUS
AcpiOsCreateLock (
    ACPI_SPINLOCK           *OutHandle)
{
    return (AE_OK);
}

void
AcpiOsDeleteLock (
    ACPI_SPINLOCK           Handle)
{
}

ACPI_CPU_FLAGS
AcpiOsAcquireLock (
    ACPI_SPINLOCK           Handle)
{
    return (0);
}

void
AcpiOsReleaseLock (
    ACPI_SPINLOCK           Handle,
    ACPI_CPU_FLAGS          Flags)
{
}


/******************************************************************************
 *
 * FUNCTION:    AcpiOsAllocate
 *
 * PARAMETERS:  Size                - Amount to allocate, in bytes
 *
 * RETURN:      Pointer to the new allocation. Null on error.
 *
 * DESCRIPTION: Allocate memory. Algorithm is dependent on the OS.
 *
 *****************************************************************************/

void *
AcpiOsAllocate (
    ACPI_SIZE               Size)
{
    EFI_STATUS              EfiStatus;
    void                    *Mem;


    EfiStatus = uefi_call_wrapper (BS->AllocatePool, 3,
        EfiLoaderData, Size, &Mem);
    if (EFI_ERROR (EfiStatus))
    {
        AcpiLogError ("EFI_BOOT_SERVICES->AllocatePool(EfiLoaderData) failure.\n");
        return (NULL);
    }

    return (Mem);
}


#ifdef USE_NATIVE_ALLOCATE_ZEROED
/******************************************************************************
 *
 * FUNCTION:    AcpiOsAllocateZeroed
 *
 * PARAMETERS:  Size                - Amount to allocate, in bytes
 *
 * RETURN:      Pointer to the new allocation. Null on error.
 *
 * DESCRIPTION: Allocate and zero memory. Algorithm is dependent on the OS.
 *
 *****************************************************************************/

void *
AcpiOsAllocateZeroed (
    ACPI_SIZE               Size)
{
    void                    *Mem;


    Mem = AcpiOsAllocate (Size);
    if (Mem)
    {
        memset (Mem, 0, Size);
    }

    return (Mem);
}
#endif


/******************************************************************************
 *
 * FUNCTION:    AcpiOsFree
 *
 * PARAMETERS:  Mem                 - Pointer to previously allocated memory
 *
 * RETURN:      None
 *
 * DESCRIPTION: Free memory allocated via AcpiOsAllocate
 *
 *****************************************************************************/

void
AcpiOsFree (
    void                    *Mem)
{

    uefi_call_wrapper (BS->FreePool, 1, Mem);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiOsOpenFile
 *
 * PARAMETERS:  Path                - File path
 *              Modes               - File operation type
 *
 * RETURN:      File descriptor
 *
 * DESCRIPTION: Open a file for reading (ACPI_FILE_READING) or/and writing
 *              (ACPI_FILE_WRITING).
 *
 ******************************************************************************/

ACPI_FILE
AcpiOsOpenFile (
    const char              *Path,
    UINT8                   Modes)
{
    EFI_STATUS              EfiStatus = EFI_SUCCESS;
    UINT64                  OpenModes;
    EFI_FILE_HANDLE         EfiFile = NULL;
    CHAR16                  *Path16 = NULL;
    CHAR16                  *Pos16;
    const char              *Pos;
    INTN                    Count, i;


    if (!Path)
    {
        return (NULL);
    }

    /* Convert modes */

    OpenModes = EFI_FILE_MODE_READ;
    if (Modes & ACPI_FILE_WRITING)
    {
        OpenModes |= (EFI_FILE_MODE_WRITE | EFI_FILE_MODE_CREATE);
    }

    /* Allocate path buffer */

    Count = strlen (Path);
    Path16 = ACPI_ALLOCATE_ZEROED ((Count + 1) * sizeof (CHAR16));
    if (!Path16)
    {
        EfiStatus = EFI_BAD_BUFFER_SIZE;
        goto ErrorExit;
    }
    Pos = Path;
    Pos16 = Path16;
    while (*Pos == '/' || *Pos == '\\')
    {
        Pos++;
        Count--;
    }
    for (i = 0; i < Count; i++)
    {
        if (*Pos == '/')
        {
            *Pos16++ = '\\';
            Pos++;
        }
        else
        {
            *Pos16++ = *Pos++;
        }
    }
    *Pos16 = '\0';

    EfiStatus = uefi_call_wrapper (AcpiGbl_EfiCurrentVolume->Open, 5,
        AcpiGbl_EfiCurrentVolume, &EfiFile, Path16, OpenModes, 0);
    if (EFI_ERROR (EfiStatus))
    {
        AcpiLogError ("EFI_FILE_HANDLE->Open() failure.\n");
        goto ErrorExit;
    }

ErrorExit:

    if (Path16)
    {
        ACPI_FREE (Path16);
    }

    return ((ACPI_FILE) EfiFile);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiOsCloseFile
 *
 * PARAMETERS:  File                - File descriptor
 *
 * RETURN:      None.
 *
 * DESCRIPTION: Close a file.
 *
 ******************************************************************************/

void
AcpiOsCloseFile (
    ACPI_FILE               File)
{
    EFI_FILE_HANDLE         EfiFile;


    if (File == ACPI_FILE_OUT ||
        File == ACPI_FILE_ERR)
    {
        return;
    }
    EfiFile = (EFI_FILE_HANDLE) File;
    (void) uefi_call_wrapper (AcpiGbl_EfiCurrentVolume->Close, 1, EfiFile);

    return;
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiOsReadFile
 *
 * PARAMETERS:  File                - File descriptor
 *              Buffer              - Data buffer
 *              Size                - Data block size
 *              Count               - Number of data blocks
 *
 * RETURN:      Size of successfully read buffer
 *
 * DESCRIPTION: Read from a file.
 *
 ******************************************************************************/

int
AcpiOsReadFile (
    ACPI_FILE               File,
    void                    *Buffer,
    ACPI_SIZE               Size,
    ACPI_SIZE               Count)
{
    int                     Length = -1;
    EFI_FILE_HANDLE         EfiFile;
    UINTN                   ReadSize;
    EFI_STATUS              EfiStatus;


    if (File == ACPI_FILE_OUT ||
        File == ACPI_FILE_ERR)
    {
    }
    else
    {
        EfiFile = (EFI_FILE_HANDLE) File;
        if (!EfiFile)
        {
            goto ErrorExit;
        }
        ReadSize = Size * Count;

        EfiStatus = uefi_call_wrapper (AcpiGbl_EfiCurrentVolume->Read, 3,
            EfiFile, &ReadSize, Buffer);
        if (EFI_ERROR (EfiStatus))
        {
            AcpiLogError ("EFI_FILE_HANDLE->Read() failure.\n");
            goto ErrorExit;
        }
        Length = ReadSize;
    }

ErrorExit:

    return (Length);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiEfiFlushFile
 *
 * PARAMETERS:  File                - File descriptor
 *              Begin               - String with boundary
 *              End                 - Boundary of the string
 *              Pos                 - Current position
 *              FlushAll            - Whether checking boundary before flushing
 *
 * RETURN:      Updated position
 *
 * DESCRIPTION: Flush cached buffer to the file.
 *
 ******************************************************************************/

static CHAR16 *
AcpiEfiFlushFile (
    ACPI_FILE               File,
    CHAR16                  *Begin,
    CHAR16                  *End,
    CHAR16                  *Pos,
    BOOLEAN                 FlushAll)
{

    if (FlushAll || Pos >= (End - 1))
    {
        *Pos = 0;
        uefi_call_wrapper (File->OutputString, 2, File, Begin);
        Pos = Begin;
    }

    return (Pos);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiOsWriteFile
 *
 * PARAMETERS:  File                - File descriptor
 *              Buffer              - Data buffer
 *              Size                - Data block size
 *              Count               - Number of data blocks
 *
 * RETURN:      Size of successfully written buffer
 *
 * DESCRIPTION: Write to a file.
 *
 ******************************************************************************/

int
AcpiOsWriteFile (
    ACPI_FILE               File,
    void                    *Buffer,
    ACPI_SIZE               Size,
    ACPI_SIZE               Count)
{
    int                     Length = -1;
    CHAR16                  String[ACPI_EFI_PRINT_LENGTH];
    const char              *Ascii;
    CHAR16                  *End;
    CHAR16                  *Pos;
    int                     i, j;
    EFI_FILE_HANDLE         EfiFile;
    UINTN                   WriteSize;
    EFI_STATUS              EfiStatus;


    if (File == ACPI_FILE_OUT ||
        File == ACPI_FILE_ERR)
    {
        Pos = String;
        End = String + ACPI_EFI_PRINT_LENGTH - 1;
        Ascii = ACPI_CAST_PTR (const char, Buffer);
        Length = 0;

        for (j = 0; j < Count; j++)
        {
            for (i = 0; i < Size; i++)
            {
                if (*Ascii == '\n')
                {
                    *Pos++ = '\r';
                    Pos = AcpiEfiFlushFile (File, String,
                            End, Pos, FALSE);
                }
                *Pos++ = *Ascii++;
                Length++;
                Pos = AcpiEfiFlushFile (File, String,
                        End, Pos, FALSE);
            }
        }
        Pos = AcpiEfiFlushFile (File, String, End, Pos, TRUE);
    }
    else
    {
        EfiFile = (EFI_FILE_HANDLE) File;
        if (!EfiFile)
        {
            goto ErrorExit;
        }
        WriteSize = Size * Count;

        EfiStatus = uefi_call_wrapper (AcpiGbl_EfiCurrentVolume->Write, 3,
            EfiFile, &WriteSize, Buffer);
        if (EFI_ERROR (EfiStatus))
        {
            AcpiLogError ("EFI_FILE_HANDLE->Write() failure.\n");
            goto ErrorExit;
        }
        Length = WriteSize;
    }

ErrorExit:

    return (Length);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiOsGetFileOffset
 *
 * PARAMETERS:  File                - File descriptor
 *
 * RETURN:      Size of current position
 *
 * DESCRIPTION: Get current file offset.
 *
 ******************************************************************************/

long
AcpiOsGetFileOffset (
    ACPI_FILE               File)
{
    long                    Offset = -1;


    return (Offset);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiOsSetFileOffset
 *
 * PARAMETERS:  File                - File descriptor
 *              Offset              - File offset
 *              From                - From begin/end of file
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Set current file offset.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiOsSetFileOffset (
    ACPI_FILE               File,
    long                    Offset,
    UINT8                   From)
{

    return (AE_SUPPORT);
}


/******************************************************************************
 *
 * FUNCTION:    AcpiOsPrintf
 *
 * PARAMETERS:  Format, ...         - Standard printf format
 *
 * RETURN:      None
 *
 * DESCRIPTION: Formatted output.
 *
 *****************************************************************************/

void ACPI_INTERNAL_VAR_XFACE
AcpiOsPrintf (
    const char              *Format,
    ...)
{
    va_list                 Args;


    va_start (Args, Format);
    AcpiOsVprintf (Format, Args);
    va_end (Args);
}


/******************************************************************************
 *
 * FUNCTION:    AcpiOsVprintf
 *
 * PARAMETERS:  Format              - Standard printf format
 *              Args                - Argument list
 *
 * RETURN:      None
 *
 * DESCRIPTION: Formatted output with arguments list pointer.
 *
 *****************************************************************************/

void
AcpiOsVprintf (
    const char              *Format,
    va_list                 Args)
{

    (void) AcpiUtFileVprintf (ACPI_FILE_OUT, Format, Args);
}


/******************************************************************************
 *
 * FUNCTION:    AcpiOsInitialize
 *
 * PARAMETERS:  None
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Initialize this module.
 *
 *****************************************************************************/

ACPI_STATUS
AcpiOsInitialize (
    void)
{

    return (AE_OK);
}


/******************************************************************************
 *
 * FUNCTION:    AcpiEfiArgify
 *
 * PARAMETERS:  String              - Pointer to command line argument strings
 *                                    which are seperated with spaces
 *              ArgcPtr             - Return number of the arguments
 *              ArgvPtr             - Return vector of the arguments
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Convert EFI arguments into C arguments.
 *
 *****************************************************************************/

static ACPI_STATUS
AcpiEfiArgify (
    char                    *String,
    int                     *ArgcPtr,
    char                    ***ArgvPtr)
{
    char                    *CopyBuffer;
    int                     MaxArgc = *ArgcPtr;
    int                     Argc = 0;
    char                    **Argv = *ArgvPtr;
    char                    *Arg;
    BOOLEAN                 IsSingleQuote = FALSE;
    BOOLEAN                 IsDoubleQuote = FALSE;
    BOOLEAN                 IsEscape = FALSE;


    if (String == NULL)
    {
        return (AE_BAD_PARAMETER);
    }

    CopyBuffer = String;

    while (*String != '\0')
    {
        while (isspace (*String))
        {
            *String++ = '\0';
        }
        Arg = CopyBuffer;
        while (*String != '\0')
        {
            if (isspace (*String) &&
                !IsSingleQuote && !IsDoubleQuote && !IsEscape)
            {
                *Arg++ = '\0';
                String++;
                break;
            }
            if (IsEscape)
            {
                IsEscape = FALSE;
                *Arg++ = *String;
            }
            else if (*String == '\\')
            {
                IsEscape = TRUE;
            }
            else if (IsSingleQuote)
            {
                if (*String == '\'')
                {
                    IsSingleQuote = FALSE;
                    *Arg++ = '\0';
                }
                else
                {
                    *Arg++ = *String;
                }
            }
            else if (IsDoubleQuote)
            {
                if (*String == '"')
                {
                    IsDoubleQuote = FALSE;
                    *Arg = '\0';
                }
                else
                {
                    *Arg++ = *String;
                }
            }
            else
            {
                if (*String == '\'')
                {
                    IsSingleQuote = TRUE;
                }
                else if (*String == '"')
                {
                    IsDoubleQuote = TRUE;
                }
                else
                {
                    *Arg++ = *String;
                }
            }
            String++;
        }
        if (Argv && Argc < MaxArgc)
        {
            Argv[Argc] = CopyBuffer;
        }
        Argc++;
        CopyBuffer = Arg;
    }
    if (Argv && Argc < MaxArgc)
    {
        Argv[Argc] = NULL;
    }

    *ArgcPtr = Argc;
    *ArgvPtr = Argv;

    return ((MaxArgc < Argc) ? AE_NO_MEMORY : AE_OK);
}


/******************************************************************************
 *
 * FUNCTION:    AcpiEfiConvertArgcv
 *
 * PARAMETERS:  LoadOptions         - Pointer to the EFI options buffer, which
 *                                    is NULL terminated
 *              LoadOptionsSize     - Size of the EFI options buffer
 *              ArgcPtr             - Return number of the arguments
 *              ArgvPtr             - Return vector of the arguments
 *              BufferPtr           - Buffer to contain the argument strings
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Convert EFI arguments into C arguments.
 *
 *****************************************************************************/

static ACPI_STATUS
AcpiEfiConvertArgcv (
    CHAR16                  *LoadOptions,
    UINT32                  LoadOptionsSize,
    int                     *ArgcPtr,
    char                    ***ArgvPtr,
    char                    **BufferPtr)
{
    ACPI_STATUS             Status = AE_OK;
    UINT32                  Count = LoadOptionsSize / sizeof (CHAR16);
    UINT32                  i;
    CHAR16                  *From;
    char                    *To;
    int                     Argc = 0;
    char                    **Argv = NULL;
    char                    *Buffer;


    /* Prepare a buffer to contain the argument strings */

    Buffer = ACPI_ALLOCATE_ZEROED (Count);
    if (!Buffer)
    {
        Status = AE_NO_MEMORY;
        goto ErrorExit;
    }

TryAgain:

    /* Extend the argument vector */

    if (Argv)
    {
        ACPI_FREE (Argv);
        Argv = NULL;
    }
    if (Argc > 0)
    {
        Argv = ACPI_ALLOCATE_ZEROED (sizeof (char *) * (Argc + 1));
        if (!Argv)
        {
            Status = AE_NO_MEMORY;
            goto ErrorExit;
        }
    }

    /*
     * Note: As AcpiEfiArgify() will modify the content of the buffer, so
     *       we need to restore it each time before invoking
     *       AcpiEfiArgify().
     */
    From = LoadOptions;
    To = ACPI_CAST_PTR (char, Buffer);
    for (i = 0; i < Count; i++)
    {
        *To++ = (char) *From++;
    }

    /*
     * The "Buffer" will contain NULL terminated strings after invoking
     * AcpiEfiArgify(). The number of the strings are saved in Argc and the
     * pointers of the strings are saved in Argv.
     */
    Status = AcpiEfiArgify (Buffer, &Argc, &Argv);
    if (ACPI_FAILURE (Status))
    {
        if (Status == AE_NO_MEMORY)
        {
            goto TryAgain;
        }
    }

ErrorExit:

    if (ACPI_FAILURE (Status))
    {
        ACPI_FREE (Buffer);
        ACPI_FREE (Argv);
    }
    else
    {
        *ArgcPtr = Argc;
        *ArgvPtr = Argv;
        *BufferPtr = Buffer;
    }
    return (Status);
}


/******************************************************************************
 *
 * FUNCTION:    efi_main
 *
 * PARAMETERS:  Image               - EFI image handle
 *              SystemTab           - EFI system table
 *
 * RETURN:      EFI Status
 *
 * DESCRIPTION: Entry point of EFI executable
 *
 *****************************************************************************/

EFI_STATUS
efi_main (
    EFI_HANDLE              Image,
    EFI_SYSTEM_TABLE        *SystemTab)
{
    EFI_LOADED_IMAGE        *Info;
    EFI_STATUS              EfiStatus = EFI_SUCCESS;
    ACPI_STATUS             Status;
    int                     argc;
    char                    **argv = NULL;
    char                    *OptBuffer = NULL;
    EFI_FILE_IO_INTERFACE   *Volume = NULL;


    /* Initialize global variables */

    ST = SystemTab;
    BS = SystemTab->BootServices;

    /* Retrieve image information */

    EfiStatus = uefi_call_wrapper (BS->HandleProtocol, 3,
        Image, &AcpiGbl_LoadedImageProtocol, ACPI_CAST_PTR (VOID, &Info));
    if (EFI_ERROR (EfiStatus))
    {
        AcpiLogError ("EFI_BOOT_SERVICES->HandleProtocol(LoadedImageProtocol) failure.\n");
        return (EfiStatus);
    }

    EfiStatus = uefi_call_wrapper (BS->HandleProtocol, 3,
        Info->DeviceHandle, &AcpiGbl_FileSystemProtocol, (void **) &Volume);
    if (EFI_ERROR (EfiStatus))
    {
        AcpiLogError ("EFI_BOOT_SERVICES->HandleProtocol(FileSystemProtocol) failure.\n");
        return (EfiStatus);
    }
    EfiStatus = uefi_call_wrapper (Volume->OpenVolume, 2,
        Volume, &AcpiGbl_EfiCurrentVolume);
    if (EFI_ERROR (EfiStatus))
    {
        AcpiLogError ("EFI_FILE_IO_INTERFACE->OpenVolume() failure.\n");
        return (EfiStatus);
    }

    Status = AcpiEfiConvertArgcv (Info->LoadOptions,
        Info->LoadOptionsSize, &argc, &argv, &OptBuffer);
    if (ACPI_FAILURE (Status))
    {
        EfiStatus = EFI_DEVICE_ERROR;
        goto ErrorAlloc;
    }

    acpi_main (argc, argv);

ErrorAlloc:

    if (argv)
    {
        ACPI_FREE (argv);
    }
    if (OptBuffer)
    {
        ACPI_FREE (OptBuffer);
    }

    return (EfiStatus);
}
