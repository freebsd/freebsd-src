/** @file
  Common I/O Library routines.

  Copyright (c) 2006 - 2008, Intel Corporation. All rights reserved.<BR>
  Copyright (c) 2017, AMD Incorporated. All rights reserved.<BR>

  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php.

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/


#include "BaseIoLibIntrinsicInternal.h"
#include <Library/PcdLib.h>

#define MAP_PORT_BASE_TO_MEM(_Port) \
    ((((_Port) & 0xfffc) << 10) | ((_Port) & 0x0fff))

/**
  Translates I/O port address to memory address.

  This function translates I/O port address to memory address by adding the 64MB
  aligned I/O Port space to the I/O address.
  If I/O Port space base is not 64MB aligned, then ASSERT ().  

  @param  Port  The I/O port to read.

  @return The memory address.

**/
UINTN
InternalGetMemoryMapAddress (
  IN UINTN                  Port
  )
{
  UINTN                     Address;
  UINTN                     IoBlockBaseAddress;

  Address            = MAP_PORT_BASE_TO_MEM (Port);
  IoBlockBaseAddress = PcdGet64(PcdIoBlockBaseAddressForIpf);

  //
  // Make sure that the I/O Port space base is 64MB aligned.
  // 
  ASSERT ((IoBlockBaseAddress & 0x3ffffff) == 0);
  Address += IoBlockBaseAddress;

  return Address;
}

/**
  Reads an 8-bit I/O port.

  Reads the 8-bit I/O port specified by Port. The 8-bit read value is returned.
  This function must guarantee that all I/O read and write operations are
  serialized.

  If 8-bit I/O port operations are not supported, then ASSERT().

  @param  Port  The I/O port to read.

  @return The value read.

**/
UINT8
EFIAPI
IoRead8 (
  IN      UINTN                     Port
  )
{
  return MmioRead8 (InternalGetMemoryMapAddress (Port));
}

/**
  Reads a 16-bit I/O port.

  Reads the 16-bit I/O port specified by Port. The 16-bit read value is returned.
  This function must guarantee that all I/O read and write operations are
  serialized.

  If 16-bit I/O port operations are not supported, then ASSERT().
  If Port is not aligned on a 16-bit boundary, then ASSERT().

  @param  Port  The I/O port to read.

  @return The value read.

**/
UINT16
EFIAPI
IoRead16 (
  IN      UINTN                     Port
  )
{
  return MmioRead16 (InternalGetMemoryMapAddress (Port));
}

/**
  Reads a 32-bit I/O port.

  Reads the 32-bit I/O port specified by Port. The 32-bit read value is returned.
  This function must guarantee that all I/O read and write operations are
  serialized.

  If 32-bit I/O port operations are not supported, then ASSERT().
  If Port is not aligned on a 32-bit boundary, then ASSERT().
  
  @param  Port  The I/O port to read.

  @return The value read.

**/
UINT32
EFIAPI
IoRead32 (
  IN      UINTN                     Port
  )
{
  return MmioRead32 (InternalGetMemoryMapAddress (Port));
}

/**
  Reads a 64-bit I/O port.

  Reads the 64-bit I/O port specified by Port. The 64-bit read value is returned.
  This function must guarantee that all I/O read and write operations are
  serialized.

  If 64-bit I/O port operations are not supported, then ASSERT().
  If Port is not aligned on a 64-bit boundary, then ASSERT().

  @param  Port  The I/O port to read.

  @return The value read.

**/
UINT64
EFIAPI
IoRead64 (
  IN      UINTN                     Port
  )
{
  ASSERT (FALSE);
  return 0;
}


/**
  Writes an 8-bit I/O port.

  Writes the 8-bit I/O port specified by Port with the value specified by Value
  and returns Value. This function must guarantee that all I/O read and write
  operations are serialized.

  If 8-bit I/O port operations are not supported, then ASSERT().

  @param  Port  The I/O port to write.
  @param  Value The value to write to the I/O port.

  @return The value written the I/O port.

**/
UINT8
EFIAPI
IoWrite8 (
  IN      UINTN                     Port,
  IN      UINT8                     Value
  )
{
  return MmioWrite8 (InternalGetMemoryMapAddress (Port), Value);
}

/**
  Writes a 16-bit I/O port.

  Writes the 16-bit I/O port specified by Port with the value specified by Value
  and returns Value. This function must guarantee that all I/O read and write
  operations are serialized.

  If 16-bit I/O port operations are not supported, then ASSERT().
  If Port is not aligned on a 16-bit boundary, then ASSERT().
  
  @param  Port  The I/O port to write.
  @param  Value The value to write to the I/O port.

  @return The value written the I/O port.

**/
UINT16
EFIAPI
IoWrite16 (
  IN      UINTN                     Port,
  IN      UINT16                    Value
  )
{
  return MmioWrite16 (InternalGetMemoryMapAddress (Port), Value);
}

/**
  Writes a 32-bit I/O port.

  Writes the 32-bit I/O port specified by Port with the value specified by Value
  and returns Value. This function must guarantee that all I/O read and write
  operations are serialized.

  If 32-bit I/O port operations are not supported, then ASSERT().
  If Port is not aligned on a 32-bit boundary, then ASSERT().
  
  @param  Port  The I/O port to write.
  @param  Value The value to write to the I/O port.

  @return The value written the I/O port.

**/
UINT32
EFIAPI
IoWrite32 (
  IN      UINTN                     Port,
  IN      UINT32                    Value
  )
{
  return MmioWrite32 (InternalGetMemoryMapAddress (Port), Value);
}

/**
  Writes a 64-bit I/O port.

  Writes the 64-bit I/O port specified by Port with the value specified by Value
  and returns Value. This function must guarantee that all I/O read and write
  operations are serialized.

  If 64-bit I/O port operations are not supported, then ASSERT().
  If Port is not aligned on a 64-bit boundary, then ASSERT().

  @param  Port  The I/O port to write.
  @param  Value The value to write to the I/O port.

  @return The value written the I/O port.

**/
UINT64
EFIAPI
IoWrite64 (
  IN      UINTN                     Port,
  IN      UINT64                    Value
  )
{
  ASSERT (FALSE);
  return 0;
}

/**
  Reads an 8-bit I/O port fifo into a block of memory.

  Reads the 8-bit I/O fifo port specified by Port.
  The port is read Count times, and the read data is
  stored in the provided Buffer.

  This function must guarantee that all I/O read and write operations are
  serialized.

  If 8-bit I/O port operations are not supported, then ASSERT().

  @param  Port    The I/O port to read.
  @param  Count   The number of times to read I/O port.
  @param  Buffer  The buffer to store the read data into.

**/
VOID
EFIAPI
IoReadFifo8 (
  IN      UINTN                     Port,
  IN      UINTN                     Count,
  OUT     VOID                      *Buffer
  )
{
  UINT8 *Buffer8;

  Buffer8 = (UINT8 *)Buffer;
  while (Count-- > 0) {
    *Buffer8++ = IoRead8 (Port);
  }
}

/**
  Reads a 16-bit I/O port fifo into a block of memory.

  Reads the 16-bit I/O fifo port specified by Port.
  The port is read Count times, and the read data is
  stored in the provided Buffer.

  This function must guarantee that all I/O read and write operations are
  serialized.

  If 16-bit I/O port operations are not supported, then ASSERT().

  @param  Port    The I/O port to read.
  @param  Count   The number of times to read I/O port.
  @param  Buffer  The buffer to store the read data into.

**/
VOID
EFIAPI
IoReadFifo16 (
  IN      UINTN                     Port,
  IN      UINTN                     Count,
  OUT     VOID                      *Buffer
  )
{
  UINT16 *Buffer16;

  Buffer16 = (UINT16 *)Buffer;
  while (Count-- > 0) {
    *Buffer16++ = IoRead16 (Port);
  }
}

/**
  Reads a 32-bit I/O port fifo into a block of memory.

  Reads the 32-bit I/O fifo port specified by Port.
  The port is read Count times, and the read data is
  stored in the provided Buffer.

  This function must guarantee that all I/O read and write operations are
  serialized.

  If 32-bit I/O port operations are not supported, then ASSERT().

  @param  Port    The I/O port to read.
  @param  Count   The number of times to read I/O port.
  @param  Buffer  The buffer to store the read data into.

**/
VOID
EFIAPI
IoReadFifo32 (
  IN      UINTN                     Port,
  IN      UINTN                     Count,
  OUT     VOID                      *Buffer
  )
{
  UINT32 *Buffer32;

  Buffer32 = (UINT32 *)Buffer;
  while (Count-- > 0) {
    *Buffer32++ = IoRead32 (Port);
  }
}

/**
  Writes a block of memory into an 8-bit I/O port fifo.

  Writes the 8-bit I/O fifo port specified by Port.
  The port is written Count times, and the write data is
  retrieved from the provided Buffer.

  This function must guarantee that all I/O write and write operations are
  serialized.

  If 8-bit I/O port operations are not supported, then ASSERT().

  @param  Port    The I/O port to write.
  @param  Count   The number of times to write I/O port.
  @param  Buffer  The buffer to retrieve the write data from.

**/
VOID
EFIAPI
IoWriteFifo8 (
  IN      UINTN                     Port,
  IN      UINTN                     Count,
  IN      VOID                      *Buffer
  )
{
  UINT8 *Buffer8;

  Buffer8 = (UINT8 *)Buffer;
  while (Count-- > 0) {
    IoWrite8 (Port, *Buffer8++);
  }
}

/**
  Writes a block of memory into a 16-bit I/O port fifo.

  Writes the 16-bit I/O fifo port specified by Port.
  The port is written Count times, and the write data is
  retrieved from the provided Buffer.

  This function must guarantee that all I/O write and write operations are
  serialized.

  If 16-bit I/O port operations are not supported, then ASSERT().

  @param  Port    The I/O port to write.
  @param  Count   The number of times to write I/O port.
  @param  Buffer  The buffer to retrieve the write data from.

**/
VOID
EFIAPI
IoWriteFifo16 (
  IN      UINTN                     Port,
  IN      UINTN                     Count,
  IN      VOID                      *Buffer
  )
{
  UINT16 *Buffer16;

  Buffer16 = (UINT16 *)Buffer;
  while (Count-- > 0) {
    IoWrite16 (Port, *Buffer16++);
  }
}

/**
  Writes a block of memory into a 32-bit I/O port fifo.

  Writes the 32-bit I/O fifo port specified by Port.
  The port is written Count times, and the write data is
  retrieved from the provided Buffer.

  This function must guarantee that all I/O write and write operations are
  serialized.

  If 32-bit I/O port operations are not supported, then ASSERT().

  @param  Port    The I/O port to write.
  @param  Count   The number of times to write I/O port.
  @param  Buffer  The buffer to retrieve the write data from.

**/
VOID
EFIAPI
IoWriteFifo32 (
  IN      UINTN                     Port,
  IN      UINTN                     Count,
  IN      VOID                      *Buffer
  )
{
  UINT32 *Buffer32;

  Buffer32 = (UINT32 *)Buffer;
  while (Count-- > 0) {
    IoWrite32 (Port, *Buffer32++);
  }
}

/**
  Reads an 8-bit MMIO register.

  Reads the 8-bit MMIO register specified by Address. The 8-bit read value is
  returned. This function must guarantee that all MMIO read and write
  operations are serialized.

  If 8-bit MMIO register operations are not supported, then ASSERT().

  @param  Address The MMIO register to read.

  @return The value read.

**/
UINT8
EFIAPI
MmioRead8 (
  IN      UINTN                     Address
  )
{
  UINT8            Data;

  Address |= BIT63;

  MemoryFence ();
  Data = *((volatile UINT8 *) Address);
  MemoryFence ();

  return Data;
}

/**
  Reads a 16-bit MMIO register.

  Reads the 16-bit MMIO register specified by Address. The 16-bit read value is
  returned. This function must guarantee that all MMIO read and write
  operations are serialized.

  If 16-bit MMIO register operations are not supported, then ASSERT().
  If Address is not aligned on a 16-bit boundary, then ASSERT().

  @param  Address The MMIO register to read.

  @return The value read.

**/
UINT16
EFIAPI
MmioRead16 (
  IN      UINTN                     Address
  )
{
  UINT16           Data;

  //
  // Make sure that Address is 16-bit aligned.
  // 
  ASSERT ((Address & 1) == 0);

  Address |= BIT63;

  MemoryFence ();
  Data = *((volatile UINT16 *) Address);
  MemoryFence ();

  return Data;
}

/**
  Reads a 32-bit MMIO register.

  Reads the 32-bit MMIO register specified by Address. The 32-bit read value is
  returned. This function must guarantee that all MMIO read and write
  operations are serialized.

  If 32-bit MMIO register operations are not supported, then ASSERT().
  If Address is not aligned on a 32-bit boundary, then ASSERT().

  @param  Address The MMIO register to read.

  @return The value read.

**/
UINT32
EFIAPI
MmioRead32 (
  IN      UINTN                     Address
  )
{
  UINT32           Data;

  //
  // Make sure that Address is 32-bit aligned.
  // 
  ASSERT ((Address & 3) == 0);

  Address |= BIT63;

  MemoryFence ();
  Data = *((volatile UINT32 *) Address);
  MemoryFence ();

  return Data;
}

/**
  Reads a 64-bit MMIO register.

  Reads the 64-bit MMIO register specified by Address. The 64-bit read value is
  returned. This function must guarantee that all MMIO read and write
  operations are serialized.

  If 64-bit MMIO register operations are not supported, then ASSERT().
  If Address is not aligned on a 64-bit boundary, then ASSERT().

  @param  Address The MMIO register to read.

  @return The value read.

**/
UINT64
EFIAPI
MmioRead64 (
  IN      UINTN                     Address
  )
{
  UINT64           Data;

  //
  // Make sure that Address is 64-bit aligned.
  // 
  ASSERT ((Address & 7) == 0);

  Address |= BIT63;

  MemoryFence ();
  Data = *((volatile UINT64 *) Address);
  MemoryFence ();

  return Data;

}

/**
  Writes an 8-bit MMIO register.

  Writes the 8-bit MMIO register specified by Address with the value specified
  by Value and returns Value. This function must guarantee that all MMIO read
  and write operations are serialized.

  If 8-bit MMIO register operations are not supported, then ASSERT().

  @param  Address The MMIO register to write.
  @param  Value   The value to write to the MMIO register.
  
  @return Value.

**/
UINT8
EFIAPI
MmioWrite8 (
  IN      UINTN                     Address,
  IN      UINT8                     Value
  )
{
  Address |= BIT63;

  MemoryFence ();
  *((volatile UINT8 *) Address) = Value;
  MemoryFence ();

  return Value;
}

/**
  Writes a 16-bit MMIO register.

  Writes the 16-bit MMIO register specified by Address with the value specified
  by Value and returns Value. This function must guarantee that all MMIO read
  and write operations are serialized.

  If 16-bit MMIO register operations are not supported, then ASSERT().
  If Address is not aligned on a 16-bit boundary, then ASSERT().

  @param  Address The MMIO register to write.
  @param  Value   The value to write to the MMIO register.
  
  @return Value.

**/
UINT16
EFIAPI
MmioWrite16 (
  IN      UINTN                     Address,
  IN      UINT16                    Value
  )
{
  //
  // Make sure that Address is 16-bit aligned.
  // 
  ASSERT ((Address & 1) == 0);

  Address |= BIT63;

  MemoryFence ();
  *((volatile UINT16 *) Address) = Value;
  MemoryFence ();

  return Value;
}

/**
  Writes a 32-bit MMIO register.

  Writes the 32-bit MMIO register specified by Address with the value specified
  by Value and returns Value. This function must guarantee that all MMIO read
  and write operations are serialized.

  If 32-bit MMIO register operations are not supported, then ASSERT().
  If Address is not aligned on a 32-bit boundary, then ASSERT().

  @param  Address The MMIO register to write.
  @param  Value   The value to write to the MMIO register.
  
  @return Value.

**/
UINT32
EFIAPI
MmioWrite32 (
  IN      UINTN                     Address,
  IN      UINT32                    Value
  )
{
  //
  // Make sure that Address is 32-bit aligned.
  // 
  ASSERT ((Address & 3) == 0);

  Address |= BIT63;

  MemoryFence ();
  *((volatile UINT32 *) Address) = Value;
  MemoryFence ();

  return Value;
}

/**
  Writes a 64-bit MMIO register.

  Writes the 64-bit MMIO register specified by Address with the value specified
  by Value and returns Value. This function must guarantee that all MMIO read
  and write operations are serialized.

  If 64-bit MMIO register operations are not supported, then ASSERT().
  If Address is not aligned on a 64-bit boundary, then ASSERT().

  @param  Address The MMIO register to write.
  @param  Value   The value to write to the MMIO register.

**/
UINT64
EFIAPI
MmioWrite64 (
  IN      UINTN                     Address,
  IN      UINT64                    Value
  )
{
  //
  // Make sure that Address is 64-bit aligned.
  // 
  ASSERT ((Address & 7) == 0);

  Address |= BIT63;

  MemoryFence ();
  *((volatile UINT64 *) Address) = Value;
  MemoryFence ();

  return Value;
}
