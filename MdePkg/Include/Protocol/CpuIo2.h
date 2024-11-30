/** @file
  This files describes the CPU I/O 2 Protocol.

  This protocol provides an I/O abstraction for a system processor. This protocol
  is used by a PCI root bridge I/O driver to perform memory-mapped I/O and I/O transactions.
  The I/O or memory primitives can be used by the consumer of the protocol to materialize
  bus-specific configuration cycles, such as the transitional configuration address and data
  ports for PCI. Only drivers that require direct access to the entire system should use this
  protocol.

  Note: This is a boot-services only protocol and it may not be used by runtime drivers after
  ExitBootServices(). It is different from the Framework CPU I/O Protocol, which is a runtime
  protocol and can be used by runtime drivers after ExitBootServices().

  Copyright (c) 2007 - 2018, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Revision Reference:
  This Protocol is defined in UEFI Platform Initialization Specification 1.2
  Volume 5: Standards

**/

#ifndef __CPU_IO2_H__
#define __CPU_IO2_H__

#define EFI_CPU_IO2_PROTOCOL_GUID \
  { \
    0xad61f191, 0xae5f, 0x4c0e, {0xb9, 0xfa, 0xe8, 0x69, 0xd2, 0x88, 0xc6, 0x4f} \
  }

typedef struct _EFI_CPU_IO2_PROTOCOL EFI_CPU_IO2_PROTOCOL;

///
/// Enumeration that defines the width of the I/O operation.
///
typedef enum {
  EfiCpuIoWidthUint8,
  EfiCpuIoWidthUint16,
  EfiCpuIoWidthUint32,
  EfiCpuIoWidthUint64,
  EfiCpuIoWidthFifoUint8,
  EfiCpuIoWidthFifoUint16,
  EfiCpuIoWidthFifoUint32,
  EfiCpuIoWidthFifoUint64,
  EfiCpuIoWidthFillUint8,
  EfiCpuIoWidthFillUint16,
  EfiCpuIoWidthFillUint32,
  EfiCpuIoWidthFillUint64,
  EfiCpuIoWidthMaximum
} EFI_CPU_IO_PROTOCOL_WIDTH;

/**
  Enables a driver to access registers in the PI CPU I/O space.

  The Io.Read() and Io.Write() functions enable a driver to access PCI controller
  registers in the PI CPU I/O space.

  The I/O operations are carried out exactly as requested. The caller is responsible
  for satisfying any alignment and I/O width restrictions that a PI System on a
  platform might require. For example on some platforms, width requests of
  EfiCpuIoWidthUint64 do not work. Misaligned buffers, on the other hand, will
  be handled by the driver.

  If Width is EfiCpuIoWidthUint8, EfiCpuIoWidthUint16, EfiCpuIoWidthUint32,
  or EfiCpuIoWidthUint64, then both Address and Buffer are incremented for
  each of the Count operations that is performed.

  If Width is EfiCpuIoWidthFifoUint8, EfiCpuIoWidthFifoUint16,
  EfiCpuIoWidthFifoUint32, or EfiCpuIoWidthFifoUint64, then only Buffer is
  incremented for each of the Count operations that is performed. The read or
  write operation is performed Count times on the same Address.

  If Width is EfiCpuIoWidthFillUint8, EfiCpuIoWidthFillUint16,
  EfiCpuIoWidthFillUint32, or EfiCpuIoWidthFillUint64, then only Address is
  incremented for each of the Count operations that is performed. The read or
  write operation is performed Count times from the first element of Buffer.

  @param[in]       This     A pointer to the EFI_CPU_IO2_PROTOCOL instance.
  @param[in]       Width    Signifies the width of the I/O or Memory operation.
  @param[in]       Address  The base address of the I/O operation.
  @param[in]       Count    The number of I/O operations to perform. The number
                            of bytes moved is Width size * Count, starting at Address.
  @param[in, out]  Buffer   For read operations, the destination buffer to store the results.
                            For write operations, the source buffer from which to write data.

  @retval EFI_SUCCESS            The data was read from or written to the PI system.
  @retval EFI_INVALID_PARAMETER  Width is invalid for this PI system.
  @retval EFI_INVALID_PARAMETER  Buffer is NULL.
  @retval EFI_UNSUPPORTED        The Buffer is not aligned for the given Width.
  @retval EFI_UNSUPPORTED        The address range specified by Address, Width,
                                 and Count is not valid for this PI system.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_CPU_IO_PROTOCOL_IO_MEM)(
  IN     EFI_CPU_IO2_PROTOCOL              *This,
  IN     EFI_CPU_IO_PROTOCOL_WIDTH         Width,
  IN     UINT64                            Address,
  IN     UINTN                             Count,
  IN OUT VOID                              *Buffer
  );

///
/// Service for read and write accesses.
///
typedef struct {
  ///
  /// This service provides the various modalities of memory and I/O read.
  ///
  EFI_CPU_IO_PROTOCOL_IO_MEM    Read;
  ///
  /// This service provides the various modalities of memory and I/O write.
  ///
  EFI_CPU_IO_PROTOCOL_IO_MEM    Write;
} EFI_CPU_IO_PROTOCOL_ACCESS;

///
/// Provides the basic memory and I/O interfaces that are used to abstract
/// accesses to devices in a system.
///
struct _EFI_CPU_IO2_PROTOCOL {
  ///
  /// Enables a driver to access memory-mapped registers in the EFI system memory space.
  ///
  EFI_CPU_IO_PROTOCOL_ACCESS    Mem;
  ///
  /// Enables a driver to access registers in the EFI CPU I/O space.
  ///
  EFI_CPU_IO_PROTOCOL_ACCESS    Io;
};

extern EFI_GUID  gEfiCpuIo2ProtocolGuid;

#endif
