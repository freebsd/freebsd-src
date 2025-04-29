/*++

Copyright (c)  1999 - 2003 Intel Corporation. All rights reserved
This software and associated documentation (if any) is furnished
under a license and may only be used or copied in accordance
with the terms of the license. Except as permitted by such
license, no part of this software or documentation may be
reproduced, stored in a retrieval system, or transmitted in any
form or by any means without the express written consent of
Intel Corporation.

Module Name:

    efefind.h

Abstract:

    EFI to compile bindings




Revision History

--*/

#pragma pack()

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// BugBug: Code to debug
//
#define BIT63   0x8000000000000000

#define PLATFORM_IOBASE_ADDRESS   (0xffffc000000 | BIT63)
#define PORT_TO_MEMD(_Port) (PLATFORM_IOBASE_ADDRESS | ( ( ( (_Port) & 0xfffc) << 10 ) | ( (_Port) & 0x0fff) ) )

//
// Macro's with casts make this much easier to use and read.
//
#define PORT_TO_MEM8D(_Port)  (*(UINT8  *)(PORT_TO_MEMD(_Port)))
#define POST_CODE(_Data)  (PORT_TO_MEM8D(0x80) = (_Data))
//
// BugBug: End Debug Code!!!
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

#define EFIERR(a)           (0x8000000000000000 | a)
#define EFI_ERROR_MASK      0x8000000000000000
#define EFIERR_OEM(a)       (0xc000000000000000 | a)

#define BAD_POINTER         0xFBFBFBFBFBFBFBFB
#define MAX_ADDRESS         0xFFFFFFFFFFFFFFFF

#define BREAKPOINT()  __break(0)

//
// Pointers must be aligned to these address to function
//  you will get an alignment fault if this value is less than 8
//
#define MIN_ALIGNMENT_SIZE  8

#define ALIGN_VARIABLE(Value , Adjustment) \
            (UINTN) Adjustment = 0; \
            if((UINTN)Value % MIN_ALIGNMENT_SIZE) \
                (UINTN)Adjustment = MIN_ALIGNMENT_SIZE - ((UINTN)Value % MIN_ALIGNMENT_SIZE); \
            Value = (UINTN)Value + (UINTN)Adjustment

//
// Define macros to create data structure signatures.
//

#define EFI_SIGNATURE_16(A,B)             ((A) | (B<<8))
#define EFI_SIGNATURE_32(A,B,C,D)         (EFI_SIGNATURE_16(A,B)     | (EFI_SIGNATURE_16(C,D)     << 16))
#define EFI_SIGNATURE_64(A,B,C,D,E,F,G,H) (EFI_SIGNATURE_32(A,B,C,D) | ((UINT64)(EFI_SIGNATURE_32(E,F,G,H)) << 32))

//
// EFIAPI - prototype calling convention for EFI function pointers
// BOOTSERVICE - prototype for implementation of a boot service interface
// RUNTIMESERVICE - prototype for implementation of a runtime service interface
// RUNTIMEFUNCTION - prototype for implementation of a runtime function that is not a service
// RUNTIME_CODE - pragma macro for declaring runtime code
//

#ifndef EFIAPI                  // Forces EFI calling conventions reguardless of compiler options
    #ifdef _MSC_EXTENSIONS
        #define EFIAPI __cdecl  // Force C calling convention for Microsoft C compiler
    #else
        #define EFIAPI          // Substitute expresion to force C calling convention
    #endif
#endif

#define BOOTSERVICE
#define RUNTIMESERVICE
#define RUNTIMEFUNCTION

#define RUNTIME_CODE(a)         alloc_text("rtcode", a)
#define BEGIN_RUNTIME_DATA()    data_seg("rtdata")
#define END_RUNTIME_DATA()      data_seg()

#define VOLATILE    volatile

//
// BugBug: Need to find out if this is portable across compilers.
//
void __mfa (void);
#define MEMORY_FENCE()    __mfa()

#ifdef EFI_NO_INTERFACE_DECL
  #define EFI_FORWARD_DECLARATION(x)
  #define EFI_INTERFACE_DECL(x)
#else
  #define EFI_FORWARD_DECLARATION(x) typedef struct _##x x
  #define EFI_INTERFACE_DECL(x) typedef struct x
#endif

//
// When build similar to FW, then link everything together as
// one big module.
//

#define EFI_DRIVER_ENTRY_POINT(InitFunction)

#define LOAD_INTERNAL_DRIVER(_if, type, name, entry)    \
            (_if)->LoadInternal(type, name, entry)
//        entry(NULL, ST)

#ifdef __FreeBSD__
#define INTERFACE_DECL(x) struct x
#else
//
// Some compilers don't support the forward reference construct:
//  typedef struct XXXXX
//
// The following macro provide a workaround for such cases.
//
#ifdef NO_INTERFACE_DECL
#define INTERFACE_DECL(x)
#else
#define INTERFACE_DECL(x) typedef struct x
#endif
#endif
