/** @file
  EFI SMM CPU Protocol as defined in the PI 1.2 specification.

  This protocol allows SMM drivers to access architecture-standard registers from any of the CPU 
  save state areas. In some cases, difference processors provide the same information in the save state, 
  but not in the same format. These so-called pseudo-registers provide this information in a standard 
  format.  

  Copyright (c) 2009 - 2012, Intel Corporation. All rights reserved.<BR>
  This program and the accompanying materials                          
  are licensed and made available under the terms and conditions of the BSD License         
  which accompanies this distribution.  The full text of the license may be found at        
  http://opensource.org/licenses/bsd-license.php                                            

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,                     
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.             

**/

#ifndef _SMM_CPU_H_
#define _SMM_CPU_H_

#define EFI_SMM_CPU_PROTOCOL_GUID \
  { \
    0xeb346b97, 0x975f, 0x4a9f, { 0x8b, 0x22, 0xf8, 0xe9, 0x2b, 0xb3, 0xd5, 0x69 } \
  }

///
/// Save State register index
///
typedef enum {
  ///
  /// x86/X64 standard registers
  ///
  EFI_SMM_SAVE_STATE_REGISTER_GDTBASE       = 4,
  EFI_SMM_SAVE_STATE_REGISTER_IDTBASE       = 5,
  EFI_SMM_SAVE_STATE_REGISTER_LDTBASE       = 6,
  EFI_SMM_SAVE_STATE_REGISTER_GDTLIMIT      = 7,
  EFI_SMM_SAVE_STATE_REGISTER_IDTLIMIT      = 8,
  EFI_SMM_SAVE_STATE_REGISTER_LDTLIMIT      = 9,
  EFI_SMM_SAVE_STATE_REGISTER_LDTINFO       = 10,
  EFI_SMM_SAVE_STATE_REGISTER_ES            = 20,
  EFI_SMM_SAVE_STATE_REGISTER_CS            = 21,
  EFI_SMM_SAVE_STATE_REGISTER_SS            = 22,
  EFI_SMM_SAVE_STATE_REGISTER_DS            = 23,
  EFI_SMM_SAVE_STATE_REGISTER_FS            = 24,
  EFI_SMM_SAVE_STATE_REGISTER_GS            = 25,
  EFI_SMM_SAVE_STATE_REGISTER_LDTR_SEL      = 26,
  EFI_SMM_SAVE_STATE_REGISTER_TR_SEL        = 27,
  EFI_SMM_SAVE_STATE_REGISTER_DR7           = 28,
  EFI_SMM_SAVE_STATE_REGISTER_DR6           = 29,
  EFI_SMM_SAVE_STATE_REGISTER_R8            = 30,
  EFI_SMM_SAVE_STATE_REGISTER_R9            = 31,
  EFI_SMM_SAVE_STATE_REGISTER_R10           = 32,
  EFI_SMM_SAVE_STATE_REGISTER_R11           = 33,
  EFI_SMM_SAVE_STATE_REGISTER_R12           = 34,
  EFI_SMM_SAVE_STATE_REGISTER_R13           = 35,
  EFI_SMM_SAVE_STATE_REGISTER_R14           = 36,
  EFI_SMM_SAVE_STATE_REGISTER_R15           = 37,  
  EFI_SMM_SAVE_STATE_REGISTER_RAX           = 38,
  EFI_SMM_SAVE_STATE_REGISTER_RBX           = 39,
  EFI_SMM_SAVE_STATE_REGISTER_RCX           = 40,
  EFI_SMM_SAVE_STATE_REGISTER_RDX           = 41,
  EFI_SMM_SAVE_STATE_REGISTER_RSP           = 42,
  EFI_SMM_SAVE_STATE_REGISTER_RBP           = 43,
  EFI_SMM_SAVE_STATE_REGISTER_RSI           = 44,
  EFI_SMM_SAVE_STATE_REGISTER_RDI           = 45,
  EFI_SMM_SAVE_STATE_REGISTER_RIP           = 46,
  EFI_SMM_SAVE_STATE_REGISTER_RFLAGS        = 51,
  EFI_SMM_SAVE_STATE_REGISTER_CR0           = 52,
  EFI_SMM_SAVE_STATE_REGISTER_CR3           = 53,
  EFI_SMM_SAVE_STATE_REGISTER_CR4           = 54,
  EFI_SMM_SAVE_STATE_REGISTER_FCW           = 256,
  EFI_SMM_SAVE_STATE_REGISTER_FSW           = 257,
  EFI_SMM_SAVE_STATE_REGISTER_FTW           = 258,  
  EFI_SMM_SAVE_STATE_REGISTER_OPCODE        = 259,
  EFI_SMM_SAVE_STATE_REGISTER_FP_EIP        = 260,
  EFI_SMM_SAVE_STATE_REGISTER_FP_CS         = 261,
  EFI_SMM_SAVE_STATE_REGISTER_DATAOFFSET    = 262,
  EFI_SMM_SAVE_STATE_REGISTER_FP_DS         = 263,
  EFI_SMM_SAVE_STATE_REGISTER_MM0           = 264,
  EFI_SMM_SAVE_STATE_REGISTER_MM1           = 265,
  EFI_SMM_SAVE_STATE_REGISTER_MM2           = 266,
  EFI_SMM_SAVE_STATE_REGISTER_MM3           = 267,
  EFI_SMM_SAVE_STATE_REGISTER_MM4           = 268,
  EFI_SMM_SAVE_STATE_REGISTER_MM5           = 269,
  EFI_SMM_SAVE_STATE_REGISTER_MM6           = 270,
  EFI_SMM_SAVE_STATE_REGISTER_MM7           = 271,
  EFI_SMM_SAVE_STATE_REGISTER_XMM0          = 272,
  EFI_SMM_SAVE_STATE_REGISTER_XMM1          = 273,
  EFI_SMM_SAVE_STATE_REGISTER_XMM2          = 274,
  EFI_SMM_SAVE_STATE_REGISTER_XMM3          = 275,
  EFI_SMM_SAVE_STATE_REGISTER_XMM4          = 276,
  EFI_SMM_SAVE_STATE_REGISTER_XMM5          = 277,
  EFI_SMM_SAVE_STATE_REGISTER_XMM6          = 278,
  EFI_SMM_SAVE_STATE_REGISTER_XMM7          = 279,
  EFI_SMM_SAVE_STATE_REGISTER_XMM8          = 280,
  EFI_SMM_SAVE_STATE_REGISTER_XMM9          = 281,
  EFI_SMM_SAVE_STATE_REGISTER_XMM10         = 282,
  EFI_SMM_SAVE_STATE_REGISTER_XMM11         = 283,
  EFI_SMM_SAVE_STATE_REGISTER_XMM12         = 284,
  EFI_SMM_SAVE_STATE_REGISTER_XMM13         = 285,
  EFI_SMM_SAVE_STATE_REGISTER_XMM14         = 286,
  EFI_SMM_SAVE_STATE_REGISTER_XMM15         = 287,  
  ///
  /// Pseudo-Registers
  ///
  EFI_SMM_SAVE_STATE_REGISTER_IO            = 512,
  EFI_SMM_SAVE_STATE_REGISTER_LMA           = 513,
  EFI_SMM_SAVE_STATE_REGISTER_PROCESSOR_ID  = 514
} EFI_SMM_SAVE_STATE_REGISTER;  

///
/// The EFI_SMM_SAVE_STATE_REGISTER_LMA pseudo-register values
/// If the processor acts in 32-bit mode at the time the SMI occurred, the pseudo register value 
/// EFI_SMM_SAVE_STATE_REGISTER_LMA_32BIT is returned in Buffer. Otherwise, 
/// EFI_SMM_SAVE_STATE_REGISTER_LMA_64BIT is returned in Buffer.
///
#define EFI_SMM_SAVE_STATE_REGISTER_LMA_32BIT  32
#define EFI_SMM_SAVE_STATE_REGISTER_LMA_64BIT  64

///
/// Size width of I/O instruction
///
typedef enum {
  EFI_SMM_SAVE_STATE_IO_WIDTH_UINT8      = 0,
  EFI_SMM_SAVE_STATE_IO_WIDTH_UINT16     = 1,
  EFI_SMM_SAVE_STATE_IO_WIDTH_UINT32     = 2,
  EFI_SMM_SAVE_STATE_IO_WIDTH_UINT64     = 3
} EFI_SMM_SAVE_STATE_IO_WIDTH;

///
/// Types of I/O instruction
///
typedef enum {
  EFI_SMM_SAVE_STATE_IO_TYPE_INPUT       = 1,
  EFI_SMM_SAVE_STATE_IO_TYPE_OUTPUT      = 2,
  EFI_SMM_SAVE_STATE_IO_TYPE_STRING      = 4,
  EFI_SMM_SAVE_STATE_IO_TYPE_REP_PREFIX  = 8
} EFI_SMM_SAVE_STATE_IO_TYPE;

///
/// Structure of the data which is returned when ReadSaveState() is called with 
/// EFI_SMM_SAVE_STATE_REGISTER_IO. If there was no I/O then ReadSaveState() will 
/// return EFI_NOT_FOUND.
///
/// This structure describes the I/O operation which was in process when the SMI was generated.
///
typedef struct _EFI_SMM_SAVE_STATE_IO_INFO {
  ///
  /// For input instruction (IN, INS), this is data read before the SMI occurred. For output 
  /// instructions (OUT, OUTS) this is data that was written before the SMI occurred. The 
  /// width of the data is specified by IoWidth.
  ///
  UINT64                        IoData;
  ///
  /// The I/O port that was being accessed when the SMI was triggered.
  ///
  UINT16                        IoPort;
  ///
  /// Defines the size width (UINT8, UINT16, UINT32, UINT64) for IoData.
  ///
  EFI_SMM_SAVE_STATE_IO_WIDTH   IoWidth;
  ///
  /// Defines type of I/O instruction.
  ///
  EFI_SMM_SAVE_STATE_IO_TYPE    IoType;
} EFI_SMM_SAVE_STATE_IO_INFO;
  
typedef struct _EFI_SMM_CPU_PROTOCOL  EFI_SMM_CPU_PROTOCOL;

/**
  Read data from the CPU save state.

  This function is used to read the specified number of bytes of the specified register from the CPU 
  save state of the specified CPU and place the value into the buffer. If the CPU does not support the
  specified register Register, then EFI_NOT_FOUND  should be returned. If the CPU does not 
  support the specified register width Width, then EFI_INVALID_PARAMETER is returned.

  @param[in]  This               The EFI_SMM_CPU_PROTOCOL instance.
  @param[in]  Width              The number of bytes to read from the CPU save state.
  @param[in]  Register           Specifies the CPU register to read form the save state.
  @param[in]  CpuIndex           Specifies the zero-based index of the CPU save state.
  @param[out] Buffer             Upon return, this holds the CPU register value read from the save state.
    
  @retval EFI_SUCCESS            The register was read from Save State.
  @retval EFI_NOT_FOUND          The register is not defined for the Save State of Processor.
  @retval EFI_INVALID_PARAMETER  Input parameters are not valid, for example, Processor No or register width 
                                 is not correct.This or Buffer is NULL.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_SMM_READ_SAVE_STATE)(
  IN CONST EFI_SMM_CPU_PROTOCOL   *This,
  IN UINTN                        Width,
  IN EFI_SMM_SAVE_STATE_REGISTER  Register,
  IN UINTN                        CpuIndex,
  OUT VOID                        *Buffer
  );


/**
  Write data to the CPU save state.

  This function is used to write the specified number of bytes of the specified register to the CPU save 
  state of the specified CPU and place the value into the buffer. If the CPU does not support the 
  specified register Register, then EFI_UNSUPPORTED should be returned. If the CPU does not 
  support the specified register width Width, then EFI_INVALID_PARAMETER is returned.

  @param[in]  This               The EFI_SMM_CPU_PROTOCOL instance.
  @param[in]  Width              The number of bytes to write to the CPU save state.
  @param[in]  Register           Specifies the CPU register to write to the save state.
  @param[in]  CpuIndex           Specifies the zero-based index of the CPU save state.
  @param[in]  Buffer             Upon entry, this holds the new CPU register value.
  
  @retval EFI_SUCCESS            The register was written to Save State.
  @retval EFI_NOT_FOUND          The register is not defined for the Save State of Processor.
  @retval EFI_INVALID_PARAMETER  Input parameters are not valid. For example: 
                                 ProcessorIndex or Width is not correct.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_SMM_WRITE_SAVE_STATE)(
  IN CONST EFI_SMM_CPU_PROTOCOL   *This,
  IN UINTN                        Width, 
  IN EFI_SMM_SAVE_STATE_REGISTER  Register,
  IN UINTN                        CpuIndex,
  IN CONST VOID                   *Buffer
  );

///
/// EFI SMM CPU Protocol provides access to CPU-related information while in SMM.
///
/// This protocol allows SMM drivers to access architecture-standard registers from any of the CPU 
/// save state areas. In some cases, difference processors provide the same information in the save state, 
/// but not in the same format. These so-called pseudo-registers provide this information in a standard 
/// format.  
///
struct _EFI_SMM_CPU_PROTOCOL {
  EFI_SMM_READ_SAVE_STATE   ReadSaveState;
  EFI_SMM_WRITE_SAVE_STATE  WriteSaveState;
};

extern EFI_GUID gEfiSmmCpuProtocolGuid;

#endif

