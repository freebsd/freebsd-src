/** @file MockPciSegmentLib.h
  Google Test mocks for PciSegmentLib

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef MOCK_PCISEGMENTLIB_H_
#define MOCK_PCISEGMENTLIB_H_

#include <Library/GoogleTestLib.h>
#include <Library/FunctionMockLib.h>

extern "C" {
  #include <Uefi.h>
}

struct MockPciSegmentLib {
  MOCK_INTERFACE_DECLARATION (MockPciSegmentLib);

  MOCK_FUNCTION_DECLARATION (
    RETURN_STATUS,
    PciSegmentRegisterForRuntimeAccess,
    (
     IN UINTN  Address
    )
    );

  MOCK_FUNCTION_DECLARATION (
    UINT8,
    PciSegmentRead8,
    (
     IN UINT64  Address
    )
    );

  MOCK_FUNCTION_DECLARATION (
    UINT8,
    PciSegmentWrite8,
    (
     IN UINT64  Address,
     IN UINT8   Value
    )
    );

  MOCK_FUNCTION_DECLARATION (
    UINT8,
    PciSegmentOr8,
    (
     IN UINT64  Address,
     IN UINT8   OrData
    )
    );

  MOCK_FUNCTION_DECLARATION (
    UINT8,
    PciSegmentAnd8,
    (
     IN UINT64  Address,
     IN UINT8   AndData
    )
    );

  MOCK_FUNCTION_DECLARATION (
    UINT8,
    PciSegmentAndThenOr8,
    (
     IN UINT64  Address,
     IN UINT8   AndData,
     IN UINT8   OrData
    )
    );

  MOCK_FUNCTION_DECLARATION (
    UINT8,
    PciSegmentBitFieldRead8,
    (
     IN UINT64  Address,
     IN UINTN   StartBit,
     IN UINTN   EndBit
    )
    );

  MOCK_FUNCTION_DECLARATION (
    UINT8,
    PciSegmentBitFieldWrite8,
    (
     IN UINT64  Address,
     IN UINTN   StartBit,
     IN UINTN   EndBit,
     IN UINT8   Value
    )
    );

  MOCK_FUNCTION_DECLARATION (
    UINT8,
    PciSegmentBitFieldOr8,
    (
     IN UINT64  Address,
     IN UINTN   StartBit,
     IN UINTN   EndBit,
     IN UINT8   OrData
    )
    );

  MOCK_FUNCTION_DECLARATION (
    UINT8,
    PciSegmentBitFieldAnd8,
    (
     IN UINT64  Address,
     IN UINTN   StartBit,
     IN UINTN   EndBit,
     IN UINT8   AndData
    )
    );

  MOCK_FUNCTION_DECLARATION (
    UINT8,
    PciSegmentBitFieldAndThenOr8,
    (
     IN UINT64  Address,
     IN UINTN   StartBit,
     IN UINTN   EndBit,
     IN UINT8   AndData,
     IN UINT8   OrData
    )
    );

  MOCK_FUNCTION_DECLARATION (
    UINT16,
    PciSegmentRead16,
    (
     IN UINT64  Address
    )
    );

  MOCK_FUNCTION_DECLARATION (
    UINT16,
    PciSegmentWrite16,
    (
     IN UINT64  Address,
     IN UINT16  Value
    )
    );

  MOCK_FUNCTION_DECLARATION (
    UINT16,
    PciSegmentOr16,
    (
     IN UINT64  Address,
     IN UINT16  OrData
    )
    );

  MOCK_FUNCTION_DECLARATION (
    UINT16,
    PciSegmentAnd16,
    (
     IN UINT64  Address,
     IN UINT16  AndData
    )
    );

  MOCK_FUNCTION_DECLARATION (
    UINT16,
    PciSegmentAndThenOr16,
    (
     IN UINT64  Address,
     IN UINT16  AndData,
     IN UINT16  OrData
    )
    );

  MOCK_FUNCTION_DECLARATION (
    UINT16,
    PciSegmentBitFieldRead16,
    (
     IN UINT64  Address,
     IN UINTN   StartBit,
     IN UINTN   EndBit
    )
    );

  MOCK_FUNCTION_DECLARATION (
    UINT16,
    PciSegmentBitFieldWrite16,
    (
     IN UINT64  Address,
     IN UINTN   StartBit,
     IN UINTN   EndBit,
     IN UINT16  Value
    )
    );

  MOCK_FUNCTION_DECLARATION (
    UINT16,
    PciSegmentBitFieldOr16,
    (
     IN UINT64  Address,
     IN UINTN   StartBit,
     IN UINTN   EndBit,
     IN UINT16  OrData
    )
    );

  MOCK_FUNCTION_DECLARATION (
    UINT16,
    PciSegmentBitFieldAnd16,
    (
     IN UINT64  Address,
     IN UINTN   StartBit,
     IN UINTN   EndBit,
     IN UINT16  AndData
    )
    );

  MOCK_FUNCTION_DECLARATION (
    UINT16,
    PciSegmentBitFieldAndThenOr16,
    (
     IN UINT64  Address,
     IN UINTN   StartBit,
     IN UINTN   EndBit,
     IN UINT16  AndData,
     IN UINT16  OrData
    )
    );

  MOCK_FUNCTION_DECLARATION (
    UINT32,
    PciSegmentRead32,
    (
     IN UINT64  Address
    )
    );

  MOCK_FUNCTION_DECLARATION (
    UINT32,
    PciSegmentWrite32,
    (
     IN UINT64  Address,
     IN UINT32  Value
    )
    );

  MOCK_FUNCTION_DECLARATION (
    UINT32,
    PciSegmentOr32,
    (
     IN UINT64  Address,
     IN UINT32  OrData
    )
    );

  MOCK_FUNCTION_DECLARATION (
    UINT32,
    PciSegmentAnd32,
    (
     IN UINT64  Address,
     IN UINT32  AndData
    )
    );

  MOCK_FUNCTION_DECLARATION (
    UINT32,
    PciSegmentAndThenOr32,
    (
     IN UINT64  Address,
     IN UINT32  AndData,
     IN UINT32  OrData
    )
    );

  MOCK_FUNCTION_DECLARATION (
    UINT32,
    PciSegmentBitFieldRead32,
    (
     IN UINT64  Address,
     IN UINTN   StartBit,
     IN UINTN   EndBit
    )
    );

  MOCK_FUNCTION_DECLARATION (
    UINT32,
    PciSegmentBitFieldWrite32,
    (
     IN UINT64  Address,
     IN UINTN   StartBit,
     IN UINTN   EndBit,
     IN UINT32  Value
    )
    );

  MOCK_FUNCTION_DECLARATION (
    UINT32,
    PciSegmentBitFieldOr32,
    (
     IN UINT64  Address,
     IN UINTN   StartBit,
     IN UINTN   EndBit,
     IN UINT32  OrData
    )
    );

  MOCK_FUNCTION_DECLARATION (
    UINT32,
    PciSegmentBitFieldAnd32,
    (
     IN UINT64  Address,
     IN UINTN   StartBit,
     IN UINTN   EndBit,
     IN UINT32  AndData
    )
    );

  MOCK_FUNCTION_DECLARATION (
    UINT32,
    PciSegmentBitFieldAndThenOr32,
    (
     IN UINT64  Address,
     IN UINTN   StartBit,
     IN UINTN   EndBit,
     IN UINT32  AndData,
     IN UINT32  OrData
    )
    );

  MOCK_FUNCTION_DECLARATION (
    UINTN,
    PciSegmentReadBuffer,
    (
     IN  UINT64  StartAddress,
     IN  UINTN   Size,
     OUT VOID    *Buffer
    )
    );

  MOCK_FUNCTION_DECLARATION (
    UINTN,
    PciSegmentWriteBuffer,
    (
     IN UINT64  StartAddress,
     IN UINTN   Size,
     IN VOID    *Buffer
    )
    );
};

#endif
