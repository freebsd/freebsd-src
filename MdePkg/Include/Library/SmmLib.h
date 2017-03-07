/** @file
  Library class name: SmmLib
  
  SMM Library Services that abstracts both S/W SMI generation and detection. 

  Copyright (c) 2007 - 2010, Intel Corporation. All rights reserved.<BR>
  This program and the accompanying materials                          
  are licensed and made available under the terms and conditions of the BSD License         
  which accompanies this distribution.  The full text of the license may be found at        
  http://opensource.org/licenses/bsd-license.php                                            

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,                     
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.        

**/

#ifndef __SMM_LIB_H__
#define __SMM_LIB_H__


/**
  Triggers an SMI at boot time.  

  This function triggers a software SMM interrupt at boot time.

**/
VOID
EFIAPI
TriggerBootServiceSoftwareSmi (
  VOID
  );


/**
  Triggers an SMI at run time.  

  This function triggers a software SMM interrupt at run time.

**/
VOID
EFIAPI
TriggerRuntimeSoftwareSmi (
  VOID
  );


/**
  Test if a boot time software SMI happened.  

  This function tests if a software SMM interrupt happened. If a software SMM interrupt happened and
  it was triggered at boot time, it returns TRUE. Otherwise, it returns FALSE.

  @retval TRUE   A software SMI triggered at boot time happened.
  @retval FLASE  No software SMI happened, or the software SMI was triggered at run time.

**/
BOOLEAN
EFIAPI
IsBootServiceSoftwareSmi (
  VOID
  );


/**
  Test if a run time software SMI happened.  

  This function tests if a software SMM interrupt happened. If a software SMM interrupt happened and
  it was triggered at run time, it returns TRUE. Otherwise, it returns FALSE.

  @retval TRUE   A software SMI triggered at run time happened.
  @retval FLASE  No software SMI happened or the software SMI was triggered at boot time.

**/
BOOLEAN
EFIAPI
IsRuntimeSoftwareSmi (
  VOID
  );

/**
  Clear APM SMI Status Bit; Set the EOS bit. 
  
**/
VOID
EFIAPI
ClearSmi (
  VOID
  );
#endif
