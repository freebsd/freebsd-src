/** @file
  MCA/PMI/INIT Protocol as defined in PI Specification VOLUME 4.

  This protocol provides services to handle Machine Checks (MCA),
  Initialization (INIT) events, and Platform Management Interrupt (PMI) events
  on an Intel Itanium Processor Family based system.

  Copyright (c) 2009 - 2010, Intel Corporation. All rights reserved.<BR>
  This program and the accompanying materials                          
  are licensed and made available under the terms and conditions of the BSD License         
  which accompanies this distribution.  The full text of the license may be found at        
  http://opensource.org/licenses/bsd-license.php                                            

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,                     
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#ifndef __MCA_INIT_PMI_PROTOCOL_H__
#define __MCA_INIT_PMI_PROTOCOL_H__

///
/// Global ID for the MCA/PMI/INIT Protocol.
///
#define EFI_SAL_MCA_INIT_PMI_PROTOCOL_GUID \
  { 0xb60dc6e8, 0x3b6f, 0x11d5, {0xaf, 0x9, 0x0, 0xa0, 0xc9, 0x44, 0xa0, 0x5b} }


///
/// Declare forward reference for the Timer Architectural Protocol
///
typedef struct _EFI_SAL_MCA_INIT_PMI_PROTOCOL  EFI_SAL_MCA_INIT_PMI_PROTOCOL;

#pragma pack(1)
///
/// MCA Records Structure
///
typedef struct {
  UINT64  First : 1;
  UINT64  Last : 1;
  UINT64  EntryCount : 16;
  UINT64  DispatchedCount : 16;
  UINT64  Reserved : 30;
} SAL_MCA_COUNT_STRUCTURE;

#pragma pack()

/**
  Prototype of MCA handler.

  @param  ModuleGlobal                The context of MCA Handler
  @param  ProcessorStateParameters    The processor state parameters (PSP)
  @param  MinstateBase                Base address of the min-state
  @param  RendezvouseStateInformation Rendezvous state information to be passed to
                                      the OS on OS MCA entry
  @param  CpuIndex                    Index of the logical processor
  @param  McaCountStructure           Pointer to the MCA records structure
  @param  CorrectedMachineCheck       This flag is set to TRUE is the MCA has been
                                      corrected by the handler or by a previous handler

  @retval EFI_SUCCESS                 Handler successfully returned

**/
typedef
EFI_STATUS
(EFIAPI *EFI_SAL_MCA_HANDLER)(
  IN  VOID                    *ModuleGlobal,
  IN  UINT64                  ProcessorStateParameters,
  IN  EFI_PHYSICAL_ADDRESS    MinstateBase,
  IN  UINT64                  RendezvouseStateInformation,
  IN  UINT64                  CpuIndex,
  IN  SAL_MCA_COUNT_STRUCTURE *McaCountStructure,
  OUT BOOLEAN                 *CorrectedMachineCheck
  );

/**
  Prototype of INIT handler.

  @param  ModuleGlobal                The context of INIT Handler
  @param  ProcessorStateParameters    The processor state parameters (PSP)
  @param  MinstateBase                Base address of the min-state
  @param  McaInProgress               This flag indicates if an MCA is in progress
  @param  CpuIndex                    Index of the logical processor
  @param  McaCountStructure           Pointer to the MCA records structure
  @param  DumpSwitchPressed           This flag indicates the crash dump switch has been pressed

  @retval EFI_SUCCESS                 Handler successfully returned

**/
typedef
EFI_STATUS
(EFIAPI *EFI_SAL_INIT_HANDLER)(
  IN  VOID                     *ModuleGlobal,
  IN  UINT64                   ProcessorStateParameters,
  IN  EFI_PHYSICAL_ADDRESS     MinstateBase,
  IN  BOOLEAN                  McaInProgress,
  IN  UINT64                   CpuIndex,
  IN  SAL_MCA_COUNT_STRUCTURE  *McaCountStructure,
  OUT BOOLEAN                  *DumpSwitchPressed
  );

/**
  Prototype of PMI handler

  @param  ModuleGlobal                The context of PMI Handler
  @param  CpuIndex                    Index of the logical processor
  @param  PmiVector                   The PMI vector number as received from the PALE_PMI exit state (GR24)

  @retval EFI_SUCCESS                 Handler successfully returned

**/
typedef
EFI_STATUS
(EFIAPI *EFI_SAL_PMI_HANDLER)(
  IN VOID          *ModuleGlobal,
  IN UINT64        CpuIndex,
  IN UINT64        PmiVector
  );

/**
  Register a MCA handler with the MCA dispatcher.

  @param  This                        The EFI_SAL_MCA_INIT_PMI_PROTOCOL instance
  @param  McaHandler                  The MCA handler to register
  @param  ModuleGlobal                The context of MCA Handler
  @param  MakeFirst                   This flag specifies the handler should be made first in the list
  @param  MakeLast                    This flag specifies the handler should be made last in the list

  @retval EFI_SUCCESS                 MCA Handle was registered
  @retval EFI_OUT_OF_RESOURCES        No more resources to register an MCA handler
  @retval EFI_INVALID_PARAMETER       Invalid parameters were passed

**/
typedef
EFI_STATUS
(EFIAPI *EFI_SAL_REGISTER_MCA_HANDLER)(
  IN  EFI_SAL_MCA_INIT_PMI_PROTOCOL         *This,
  IN  EFI_SAL_MCA_HANDLER                   McaHandler,
  IN  VOID                                  *ModuleGlobal,
  IN  BOOLEAN                               MakeFirst,
  IN  BOOLEAN                               MakeLast
  );

/**
  Register an INIT handler with the INIT dispatcher.

  @param  This                        The EFI_SAL_MCA_INIT_PMI_PROTOCOL instance
  @param  InitHandler                 The INIT handler to register
  @param  ModuleGlobal                The context of INIT Handler
  @param  MakeFirst                   This flag specifies the handler should be made first in the list
  @param  MakeLast                    This flag specifies the handler should be made last in the list

  @retval EFI_SUCCESS                 INIT Handle was registered
  @retval EFI_OUT_OF_RESOURCES        No more resources to register an INIT handler
  @retval EFI_INVALID_PARAMETER       Invalid parameters were passed

**/
typedef
EFI_STATUS
(EFIAPI *EFI_SAL_REGISTER_INIT_HANDLER)(
  IN  EFI_SAL_MCA_INIT_PMI_PROTOCOL         *This,
  IN  EFI_SAL_INIT_HANDLER                  InitHandler,
  IN  VOID                                  *ModuleGlobal,
  IN  BOOLEAN                               MakeFirst,
  IN  BOOLEAN                               MakeLast
  );

/**
  Register a PMI handler with the PMI dispatcher.

  @param  This                        The EFI_SAL_MCA_INIT_PMI_PROTOCOL instance
  @param  PmiHandler                  The PMI handler to register
  @param  ModuleGlobal                The context of PMI Handler
  @param  MakeFirst                   This flag specifies the handler should be made first in the list
  @param  MakeLast                    This flag specifies the handler should be made last in the list

  @retval EFI_SUCCESS                 PMI Handle was registered
  @retval EFI_OUT_OF_RESOURCES        No more resources to register an PMI handler
  @retval EFI_INVALID_PARAMETER       Invalid parameters were passed

**/
typedef
EFI_STATUS
(EFIAPI *EFI_SAL_REGISTER_PMI_HANDLER)(
  IN  EFI_SAL_MCA_INIT_PMI_PROTOCOL         *This,
  IN  EFI_SAL_PMI_HANDLER                   PmiHandler,
  IN  VOID                                  *ModuleGlobal,
  IN  BOOLEAN                               MakeFirst,
  IN  BOOLEAN                               MakeLast
  );

///
/// This protocol is used to register MCA, INIT and PMI handlers with their respective dispatcher
///
struct _EFI_SAL_MCA_INIT_PMI_PROTOCOL {
  EFI_SAL_REGISTER_MCA_HANDLER  RegisterMcaHandler;
  EFI_SAL_REGISTER_INIT_HANDLER RegisterInitHandler;
  EFI_SAL_REGISTER_PMI_HANDLER  RegisterPmiHandler;
  BOOLEAN                       McaInProgress;       ///< Whether MCA handler is in progress
  BOOLEAN                       InitInProgress;      ///< Whether Init handler is in progress
  BOOLEAN                       PmiInProgress;       ///< Whether Pmi handler is in progress
};

extern EFI_GUID gEfiSalMcaInitPmiProtocolGuid;

#endif

