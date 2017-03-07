/// @file
///  Assembly procedures to get and set ESAL entry point.
///
/// Copyright (c) 2006 - 2011, Intel Corporation. All rights reserved.<BR>
/// This program and the accompanying materials
/// are licensed and made available under the terms and conditions of the BSD License
/// which accompanies this distribution.  The full text of the license may be found at
/// http://opensource.org/licenses/bsd-license.php.
///
/// THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
/// WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
///

.auto
.text

#include  "IpfMacro.i"

//
// Exports
//
ASM_GLOBAL GetEsalEntryPoint

//-----------------------------------------------------------------------------
//++
// GetEsalEntryPoint
//
// Return Esal global and PSR register.
//
// On Entry :
//
//
// Return Value:
//        r8  = EFI_SAL_SUCCESS
//        r9  = Physical Plabel
//        r10 = Virtual Plabel
//        r11 = psr
// 
// As per static calling conventions. 
// 
//--
//---------------------------------------------------------------------------
PROCEDURE_ENTRY (GetEsalEntryPoint)

      NESTED_SETUP (0,8,0,0)

EsalCalcStart:
      mov   r8  = ip;;
      add   r8  = (EsalEntryPoint - EsalCalcStart), r8;;
      mov   r9  = r8;;
      add   r10 = 0x10, r8;;
      mov   r11 = psr;;
      mov   r8  = r0;;

      NESTED_RETURN

PROCEDURE_EXIT (GetEsalEntryPoint)

//-----------------------------------------------------------------------------
//++
// SetEsalPhysicalEntryPoint
//
// Set the dispatcher entry point
//
// On Entry:
//  in0 = Physical address of Esal Dispatcher
//  in1 = Physical GP
//
// Return Value: 
//   r8 = EFI_SAL_SUCCESS
// 
// As per static calling conventions. 
// 
//--
//---------------------------------------------------------------------------
PROCEDURE_ENTRY (SetEsalPhysicalEntryPoint)

      NESTED_SETUP (2,8,0,0)

EsalCalcStart1:
      mov   r8   = ip;;
      add   r8   = (EsalEntryPoint - EsalCalcStart1), r8;;
      st8   [r8] = in0;;
      add   r8   = 0x08, r8;;
      st8   [r8] = in1;;
      mov   r8   = r0;;

      NESTED_RETURN

PROCEDURE_EXIT (SetEsalPhysicalEntryPoint)

.align 32
EsalEntryPoint: 
    data8 0   // Physical Entry
    data8 0   //         GP
    data8 0   // Virtual Entry
    data8 0   //         GP
