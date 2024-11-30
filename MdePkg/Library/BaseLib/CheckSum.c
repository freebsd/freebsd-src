/** @file
  Utility functions to generate checksum based on 2's complement
  algorithm.

  Copyright (c) 2007 - 2018, Intel Corporation. All rights reserved.<BR>
  Copyright (c) 2022, Pedro Falcato. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "BaseLibInternals.h"

/**
  Returns the sum of all elements in a buffer in unit of UINT8.
  During calculation, the carry bits are dropped.

  This function calculates the sum of all elements in a buffer
  in unit of UINT8. The carry bits in result of addition are dropped.
  The result is returned as UINT8. If Length is Zero, then Zero is
  returned.

  If Buffer is NULL, then ASSERT().
  If Length is greater than (MAX_ADDRESS - Buffer + 1), then ASSERT().

  @param  Buffer      The pointer to the buffer to carry out the sum operation.
  @param  Length      The size, in bytes, of Buffer.

  @return Sum         The sum of Buffer with carry bits dropped during additions.

**/
UINT8
EFIAPI
CalculateSum8 (
  IN      CONST UINT8  *Buffer,
  IN      UINTN        Length
  )
{
  UINT8  Sum;
  UINTN  Count;

  ASSERT (Buffer != NULL);
  ASSERT (Length <= (MAX_ADDRESS - ((UINTN)Buffer) + 1));

  for (Sum = 0, Count = 0; Count < Length; Count++) {
    Sum = (UINT8)(Sum + *(Buffer + Count));
  }

  return Sum;
}

/**
  Returns the two's complement checksum of all elements in a buffer
  of 8-bit values.

  This function first calculates the sum of the 8-bit values in the
  buffer specified by Buffer and Length.  The carry bits in the result
  of addition are dropped. Then, the two's complement of the sum is
  returned.  If Length is 0, then 0 is returned.

  If Buffer is NULL, then ASSERT().
  If Length is greater than (MAX_ADDRESS - Buffer + 1), then ASSERT().

  @param  Buffer      The pointer to the buffer to carry out the checksum operation.
  @param  Length      The size, in bytes, of Buffer.

  @return Checksum    The 2's complement checksum of Buffer.

**/
UINT8
EFIAPI
CalculateCheckSum8 (
  IN      CONST UINT8  *Buffer,
  IN      UINTN        Length
  )
{
  UINT8  CheckSum;

  CheckSum = CalculateSum8 (Buffer, Length);

  //
  // Return the checksum based on 2's complement.
  //
  return (UINT8)(0x100 - CheckSum);
}

/**
  Returns the sum of all elements in a buffer of 16-bit values.  During
  calculation, the carry bits are dropped.

  This function calculates the sum of the 16-bit values in the buffer
  specified by Buffer and Length. The carry bits in result of addition are dropped.
  The 16-bit result is returned.  If Length is 0, then 0 is returned.

  If Buffer is NULL, then ASSERT().
  If Buffer is not aligned on a 16-bit boundary, then ASSERT().
  If Length is not aligned on a 16-bit boundary, then ASSERT().
  If Length is greater than (MAX_ADDRESS - Buffer + 1), then ASSERT().

  @param  Buffer      The pointer to the buffer to carry out the sum operation.
  @param  Length      The size, in bytes, of Buffer.

  @return Sum         The sum of Buffer with carry bits dropped during additions.

**/
UINT16
EFIAPI
CalculateSum16 (
  IN      CONST UINT16  *Buffer,
  IN      UINTN         Length
  )
{
  UINT16  Sum;
  UINTN   Count;
  UINTN   Total;

  ASSERT (Buffer != NULL);
  ASSERT (((UINTN)Buffer & 0x1) == 0);
  ASSERT ((Length & 0x1) == 0);
  ASSERT (Length <= (MAX_ADDRESS - ((UINTN)Buffer) + 1));

  Total = Length / sizeof (*Buffer);
  for (Sum = 0, Count = 0; Count < Total; Count++) {
    Sum = (UINT16)(Sum + *(Buffer + Count));
  }

  return Sum;
}

/**
  Returns the two's complement checksum of all elements in a buffer of
  16-bit values.

  This function first calculates the sum of the 16-bit values in the buffer
  specified by Buffer and Length.  The carry bits in the result of addition
  are dropped. Then, the two's complement of the sum is returned.  If Length
  is 0, then 0 is returned.

  If Buffer is NULL, then ASSERT().
  If Buffer is not aligned on a 16-bit boundary, then ASSERT().
  If Length is not aligned on a 16-bit boundary, then ASSERT().
  If Length is greater than (MAX_ADDRESS - Buffer + 1), then ASSERT().

  @param  Buffer      The pointer to the buffer to carry out the checksum operation.
  @param  Length      The size, in bytes, of Buffer.

  @return Checksum    The 2's complement checksum of Buffer.

**/
UINT16
EFIAPI
CalculateCheckSum16 (
  IN      CONST UINT16  *Buffer,
  IN      UINTN         Length
  )
{
  UINT16  CheckSum;

  CheckSum = CalculateSum16 (Buffer, Length);

  //
  // Return the checksum based on 2's complement.
  //
  return (UINT16)(0x10000 - CheckSum);
}

/**
  Returns the sum of all elements in a buffer of 32-bit values. During
  calculation, the carry bits are dropped.

  This function calculates the sum of the 32-bit values in the buffer
  specified by Buffer and Length. The carry bits in result of addition are dropped.
  The 32-bit result is returned. If Length is 0, then 0 is returned.

  If Buffer is NULL, then ASSERT().
  If Buffer is not aligned on a 32-bit boundary, then ASSERT().
  If Length is not aligned on a 32-bit boundary, then ASSERT().
  If Length is greater than (MAX_ADDRESS - Buffer + 1), then ASSERT().

  @param  Buffer      The pointer to the buffer to carry out the sum operation.
  @param  Length      The size, in bytes, of Buffer.

  @return Sum         The sum of Buffer with carry bits dropped during additions.

**/
UINT32
EFIAPI
CalculateSum32 (
  IN      CONST UINT32  *Buffer,
  IN      UINTN         Length
  )
{
  UINT32  Sum;
  UINTN   Count;
  UINTN   Total;

  ASSERT (Buffer != NULL);
  ASSERT (((UINTN)Buffer & 0x3) == 0);
  ASSERT ((Length & 0x3) == 0);
  ASSERT (Length <= (MAX_ADDRESS - ((UINTN)Buffer) + 1));

  Total = Length / sizeof (*Buffer);
  for (Sum = 0, Count = 0; Count < Total; Count++) {
    Sum = Sum + *(Buffer + Count);
  }

  return Sum;
}

/**
  Returns the two's complement checksum of all elements in a buffer of
  32-bit values.

  This function first calculates the sum of the 32-bit values in the buffer
  specified by Buffer and Length.  The carry bits in the result of addition
  are dropped. Then, the two's complement of the sum is returned.  If Length
  is 0, then 0 is returned.

  If Buffer is NULL, then ASSERT().
  If Buffer is not aligned on a 32-bit boundary, then ASSERT().
  If Length is not aligned on a 32-bit boundary, then ASSERT().
  If Length is greater than (MAX_ADDRESS - Buffer + 1), then ASSERT().

  @param  Buffer      The pointer to the buffer to carry out the checksum operation.
  @param  Length      The size, in bytes, of Buffer.

  @return Checksum    The 2's complement checksum of Buffer.

**/
UINT32
EFIAPI
CalculateCheckSum32 (
  IN      CONST UINT32  *Buffer,
  IN      UINTN         Length
  )
{
  UINT32  CheckSum;

  CheckSum = CalculateSum32 (Buffer, Length);

  //
  // Return the checksum based on 2's complement.
  //
  return (UINT32)((UINT32)(-1) - CheckSum + 1);
}

/**
  Returns the sum of all elements in a buffer of 64-bit values.  During
  calculation, the carry bits are dropped.

  This function calculates the sum of the 64-bit values in the buffer
  specified by Buffer and Length. The carry bits in result of addition are dropped.
  The 64-bit result is returned.  If Length is 0, then 0 is returned.

  If Buffer is NULL, then ASSERT().
  If Buffer is not aligned on a 64-bit boundary, then ASSERT().
  If Length is not aligned on a 64-bit boundary, then ASSERT().
  If Length is greater than (MAX_ADDRESS - Buffer + 1), then ASSERT().

  @param  Buffer      The pointer to the buffer to carry out the sum operation.
  @param  Length      The size, in bytes, of Buffer.

  @return Sum         The sum of Buffer with carry bits dropped during additions.

**/
UINT64
EFIAPI
CalculateSum64 (
  IN      CONST UINT64  *Buffer,
  IN      UINTN         Length
  )
{
  UINT64  Sum;
  UINTN   Count;
  UINTN   Total;

  ASSERT (Buffer != NULL);
  ASSERT (((UINTN)Buffer & 0x7) == 0);
  ASSERT ((Length & 0x7) == 0);
  ASSERT (Length <= (MAX_ADDRESS - ((UINTN)Buffer) + 1));

  Total = Length / sizeof (*Buffer);
  for (Sum = 0, Count = 0; Count < Total; Count++) {
    Sum = Sum + *(Buffer + Count);
  }

  return Sum;
}

/**
  Returns the two's complement checksum of all elements in a buffer of
  64-bit values.

  This function first calculates the sum of the 64-bit values in the buffer
  specified by Buffer and Length.  The carry bits in the result of addition
  are dropped. Then, the two's complement of the sum is returned.  If Length
  is 0, then 0 is returned.

  If Buffer is NULL, then ASSERT().
  If Buffer is not aligned on a 64-bit boundary, then ASSERT().
  If Length is not aligned on a 64-bit boundary, then ASSERT().
  If Length is greater than (MAX_ADDRESS - Buffer + 1), then ASSERT().

  @param  Buffer      The pointer to the buffer to carry out the checksum operation.
  @param  Length      The size, in bytes, of Buffer.

  @return Checksum    The 2's complement checksum of Buffer.

**/
UINT64
EFIAPI
CalculateCheckSum64 (
  IN      CONST UINT64  *Buffer,
  IN      UINTN         Length
  )
{
  UINT64  CheckSum;

  CheckSum = CalculateSum64 (Buffer, Length);

  //
  // Return the checksum based on 2's complement.
  //
  return (UINT64)((UINT64)(-1) - CheckSum + 1);
}

GLOBAL_REMOVE_IF_UNREFERENCED CONST UINT32  mCrcTable[256] = {
  0x00000000,
  0x77073096,
  0xEE0E612C,
  0x990951BA,
  0x076DC419,
  0x706AF48F,
  0xE963A535,
  0x9E6495A3,
  0x0EDB8832,
  0x79DCB8A4,
  0xE0D5E91E,
  0x97D2D988,
  0x09B64C2B,
  0x7EB17CBD,
  0xE7B82D07,
  0x90BF1D91,
  0x1DB71064,
  0x6AB020F2,
  0xF3B97148,
  0x84BE41DE,
  0x1ADAD47D,
  0x6DDDE4EB,
  0xF4D4B551,
  0x83D385C7,
  0x136C9856,
  0x646BA8C0,
  0xFD62F97A,
  0x8A65C9EC,
  0x14015C4F,
  0x63066CD9,
  0xFA0F3D63,
  0x8D080DF5,
  0x3B6E20C8,
  0x4C69105E,
  0xD56041E4,
  0xA2677172,
  0x3C03E4D1,
  0x4B04D447,
  0xD20D85FD,
  0xA50AB56B,
  0x35B5A8FA,
  0x42B2986C,
  0xDBBBC9D6,
  0xACBCF940,
  0x32D86CE3,
  0x45DF5C75,
  0xDCD60DCF,
  0xABD13D59,
  0x26D930AC,
  0x51DE003A,
  0xC8D75180,
  0xBFD06116,
  0x21B4F4B5,
  0x56B3C423,
  0xCFBA9599,
  0xB8BDA50F,
  0x2802B89E,
  0x5F058808,
  0xC60CD9B2,
  0xB10BE924,
  0x2F6F7C87,
  0x58684C11,
  0xC1611DAB,
  0xB6662D3D,
  0x76DC4190,
  0x01DB7106,
  0x98D220BC,
  0xEFD5102A,
  0x71B18589,
  0x06B6B51F,
  0x9FBFE4A5,
  0xE8B8D433,
  0x7807C9A2,
  0x0F00F934,
  0x9609A88E,
  0xE10E9818,
  0x7F6A0DBB,
  0x086D3D2D,
  0x91646C97,
  0xE6635C01,
  0x6B6B51F4,
  0x1C6C6162,
  0x856530D8,
  0xF262004E,
  0x6C0695ED,
  0x1B01A57B,
  0x8208F4C1,
  0xF50FC457,
  0x65B0D9C6,
  0x12B7E950,
  0x8BBEB8EA,
  0xFCB9887C,
  0x62DD1DDF,
  0x15DA2D49,
  0x8CD37CF3,
  0xFBD44C65,
  0x4DB26158,
  0x3AB551CE,
  0xA3BC0074,
  0xD4BB30E2,
  0x4ADFA541,
  0x3DD895D7,
  0xA4D1C46D,
  0xD3D6F4FB,
  0x4369E96A,
  0x346ED9FC,
  0xAD678846,
  0xDA60B8D0,
  0x44042D73,
  0x33031DE5,
  0xAA0A4C5F,
  0xDD0D7CC9,
  0x5005713C,
  0x270241AA,
  0xBE0B1010,
  0xC90C2086,
  0x5768B525,
  0x206F85B3,
  0xB966D409,
  0xCE61E49F,
  0x5EDEF90E,
  0x29D9C998,
  0xB0D09822,
  0xC7D7A8B4,
  0x59B33D17,
  0x2EB40D81,
  0xB7BD5C3B,
  0xC0BA6CAD,
  0xEDB88320,
  0x9ABFB3B6,
  0x03B6E20C,
  0x74B1D29A,
  0xEAD54739,
  0x9DD277AF,
  0x04DB2615,
  0x73DC1683,
  0xE3630B12,
  0x94643B84,
  0x0D6D6A3E,
  0x7A6A5AA8,
  0xE40ECF0B,
  0x9309FF9D,
  0x0A00AE27,
  0x7D079EB1,
  0xF00F9344,
  0x8708A3D2,
  0x1E01F268,
  0x6906C2FE,
  0xF762575D,
  0x806567CB,
  0x196C3671,
  0x6E6B06E7,
  0xFED41B76,
  0x89D32BE0,
  0x10DA7A5A,
  0x67DD4ACC,
  0xF9B9DF6F,
  0x8EBEEFF9,
  0x17B7BE43,
  0x60B08ED5,
  0xD6D6A3E8,
  0xA1D1937E,
  0x38D8C2C4,
  0x4FDFF252,
  0xD1BB67F1,
  0xA6BC5767,
  0x3FB506DD,
  0x48B2364B,
  0xD80D2BDA,
  0xAF0A1B4C,
  0x36034AF6,
  0x41047A60,
  0xDF60EFC3,
  0xA867DF55,
  0x316E8EEF,
  0x4669BE79,
  0xCB61B38C,
  0xBC66831A,
  0x256FD2A0,
  0x5268E236,
  0xCC0C7795,
  0xBB0B4703,
  0x220216B9,
  0x5505262F,
  0xC5BA3BBE,
  0xB2BD0B28,
  0x2BB45A92,
  0x5CB36A04,
  0xC2D7FFA7,
  0xB5D0CF31,
  0x2CD99E8B,
  0x5BDEAE1D,
  0x9B64C2B0,
  0xEC63F226,
  0x756AA39C,
  0x026D930A,
  0x9C0906A9,
  0xEB0E363F,
  0x72076785,
  0x05005713,
  0x95BF4A82,
  0xE2B87A14,
  0x7BB12BAE,
  0x0CB61B38,
  0x92D28E9B,
  0xE5D5BE0D,
  0x7CDCEFB7,
  0x0BDBDF21,
  0x86D3D2D4,
  0xF1D4E242,
  0x68DDB3F8,
  0x1FDA836E,
  0x81BE16CD,
  0xF6B9265B,
  0x6FB077E1,
  0x18B74777,
  0x88085AE6,
  0xFF0F6A70,
  0x66063BCA,
  0x11010B5C,
  0x8F659EFF,
  0xF862AE69,
  0x616BFFD3,
  0x166CCF45,
  0xA00AE278,
  0xD70DD2EE,
  0x4E048354,
  0x3903B3C2,
  0xA7672661,
  0xD06016F7,
  0x4969474D,
  0x3E6E77DB,
  0xAED16A4A,
  0xD9D65ADC,
  0x40DF0B66,
  0x37D83BF0,
  0xA9BCAE53,
  0xDEBB9EC5,
  0x47B2CF7F,
  0x30B5FFE9,
  0xBDBDF21C,
  0xCABAC28A,
  0x53B39330,
  0x24B4A3A6,
  0xBAD03605,
  0xCDD70693,
  0x54DE5729,
  0x23D967BF,
  0xB3667A2E,
  0xC4614AB8,
  0x5D681B02,
  0x2A6F2B94,
  0xB40BBE37,
  0xC30C8EA1,
  0x5A05DF1B,
  0x2D02EF8D
};

/**
  Computes and returns a 32-bit CRC for a data buffer.
  CRC32 value bases on ITU-T V.42.

  If Buffer is NULL, then ASSERT().
  If Length is greater than (MAX_ADDRESS - Buffer + 1), then ASSERT().

  @param[in]  Buffer       A pointer to the buffer on which the 32-bit CRC is to be computed.
  @param[in]  Length       The number of bytes in the buffer Data.

  @retval Crc32            The 32-bit CRC was computed for the data buffer.

**/
UINT32
EFIAPI
CalculateCrc32 (
  IN  VOID   *Buffer,
  IN  UINTN  Length
  )
{
  UINTN   Index;
  UINT32  Crc;
  UINT8   *Ptr;

  ASSERT (Buffer != NULL);
  ASSERT (Length <= (MAX_ADDRESS - ((UINTN)Buffer) + 1));

  //
  // Compute CRC
  //
  Crc = 0xffffffff;
  for (Index = 0, Ptr = Buffer; Index < Length; Index++, Ptr++) {
    Crc = (Crc >> 8) ^ mCrcTable[(UINT8)Crc ^ *Ptr];
  }

  return Crc ^ 0xffffffff;
}

GLOBAL_REMOVE_IF_UNREFERENCED STATIC CONST UINT16  mCrc16LookupTable[256] =
{
  0x0000, 0xC0C1, 0xC181, 0x0140, 0xC301, 0x03C0, 0x0280, 0xC241,
  0xC601, 0x06C0, 0x0780, 0xC741, 0x0500, 0xC5C1, 0xC481, 0x0440,
  0xCC01, 0x0CC0, 0x0D80, 0xCD41, 0x0F00, 0xCFC1, 0xCE81, 0x0E40,
  0x0A00, 0xCAC1, 0xCB81, 0x0B40, 0xC901, 0x09C0, 0x0880, 0xC841,
  0xD801, 0x18C0, 0x1980, 0xD941, 0x1B00, 0xDBC1, 0xDA81, 0x1A40,
  0x1E00, 0xDEC1, 0xDF81, 0x1F40, 0xDD01, 0x1DC0, 0x1C80, 0xDC41,
  0x1400, 0xD4C1, 0xD581, 0x1540, 0xD701, 0x17C0, 0x1680, 0xD641,
  0xD201, 0x12C0, 0x1380, 0xD341, 0x1100, 0xD1C1, 0xD081, 0x1040,
  0xF001, 0x30C0, 0x3180, 0xF141, 0x3300, 0xF3C1, 0xF281, 0x3240,
  0x3600, 0xF6C1, 0xF781, 0x3740, 0xF501, 0x35C0, 0x3480, 0xF441,
  0x3C00, 0xFCC1, 0xFD81, 0x3D40, 0xFF01, 0x3FC0, 0x3E80, 0xFE41,
  0xFA01, 0x3AC0, 0x3B80, 0xFB41, 0x3900, 0xF9C1, 0xF881, 0x3840,
  0x2800, 0xE8C1, 0xE981, 0x2940, 0xEB01, 0x2BC0, 0x2A80, 0xEA41,
  0xEE01, 0x2EC0, 0x2F80, 0xEF41, 0x2D00, 0xEDC1, 0xEC81, 0x2C40,
  0xE401, 0x24C0, 0x2580, 0xE541, 0x2700, 0xE7C1, 0xE681, 0x2640,
  0x2200, 0xE2C1, 0xE381, 0x2340, 0xE101, 0x21C0, 0x2080, 0xE041,
  0xA001, 0x60C0, 0x6180, 0xA141, 0x6300, 0xA3C1, 0xA281, 0x6240,
  0x6600, 0xA6C1, 0xA781, 0x6740, 0xA501, 0x65C0, 0x6480, 0xA441,
  0x6C00, 0xACC1, 0xAD81, 0x6D40, 0xAF01, 0x6FC0, 0x6E80, 0xAE41,
  0xAA01, 0x6AC0, 0x6B80, 0xAB41, 0x6900, 0xA9C1, 0xA881, 0x6840,
  0x7800, 0xB8C1, 0xB981, 0x7940, 0xBB01, 0x7BC0, 0x7A80, 0xBA41,
  0xBE01, 0x7EC0, 0x7F80, 0xBF41, 0x7D00, 0xBDC1, 0xBC81, 0x7C40,
  0xB401, 0x74C0, 0x7580, 0xB541, 0x7700, 0xB7C1, 0xB681, 0x7640,
  0x7200, 0xB2C1, 0xB381, 0x7340, 0xB101, 0x71C0, 0x7080, 0xB041,
  0x5000, 0x90C1, 0x9181, 0x5140, 0x9301, 0x53C0, 0x5280, 0x9241,
  0x9601, 0x56C0, 0x5780, 0x9741, 0x5500, 0x95C1, 0x9481, 0x5440,
  0x9C01, 0x5CC0, 0x5D80, 0x9D41, 0x5F00, 0x9FC1, 0x9E81, 0x5E40,
  0x5A00, 0x9AC1, 0x9B81, 0x5B40, 0x9901, 0x59C0, 0x5880, 0x9841,
  0x8801, 0x48C0, 0x4980, 0x8941, 0x4B00, 0x8BC1, 0x8A81, 0x4A40,
  0x4E00, 0x8EC1, 0x8F81, 0x4F40, 0x8D01, 0x4DC0, 0x4C80, 0x8C41,
  0x4400, 0x84C1, 0x8581, 0x4540, 0x8701, 0x47C0, 0x4680, 0x8641,
  0x8201, 0x42C0, 0x4380, 0x8341, 0x4100, 0x81C1, 0x8081, 0x4040
};

/**
   Calculates the CRC16-ANSI checksum of the given buffer.

   @param[in]      Buffer        Pointer to the buffer.
   @param[in]      Length        Length of the buffer, in bytes.
   @param[in]      InitialValue  Initial value of the CRC.

   @return The CRC16-ANSI checksum.
**/
UINT16
EFIAPI
CalculateCrc16Ansi (
  IN CONST VOID  *Buffer,
  IN UINTN       Length,
  IN UINT16      InitialValue
  )
{
  CONST UINT8  *Buf;
  UINT16       Crc;

  Buf = Buffer;

  Crc = InitialValue;

  while (Length-- != 0) {
    Crc = mCrc16LookupTable[(Crc & 0xFF) ^ *(Buf++)] ^ (Crc >> 8);
  }

  return Crc;
}

GLOBAL_REMOVE_IF_UNREFERENCED STATIC CONST UINT32  mCrc32cLookupTable[256] = {
  0x00000000, 0xf26b8303, 0xe13b70f7, 0x1350f3f4, 0xc79a971f, 0x35f1141c,
  0x26a1e7e8, 0xd4ca64eb, 0x8ad958cf, 0x78b2dbcc, 0x6be22838, 0x9989ab3b,
  0x4d43cfd0, 0xbf284cd3, 0xac78bf27, 0x5e133c24, 0x105ec76f, 0xe235446c,
  0xf165b798, 0x030e349b, 0xd7c45070, 0x25afd373, 0x36ff2087, 0xc494a384,
  0x9a879fa0, 0x68ec1ca3, 0x7bbcef57, 0x89d76c54, 0x5d1d08bf, 0xaf768bbc,
  0xbc267848, 0x4e4dfb4b, 0x20bd8ede, 0xd2d60ddd, 0xc186fe29, 0x33ed7d2a,
  0xe72719c1, 0x154c9ac2, 0x061c6936, 0xf477ea35, 0xaa64d611, 0x580f5512,
  0x4b5fa6e6, 0xb93425e5, 0x6dfe410e, 0x9f95c20d, 0x8cc531f9, 0x7eaeb2fa,
  0x30e349b1, 0xc288cab2, 0xd1d83946, 0x23b3ba45, 0xf779deae, 0x05125dad,
  0x1642ae59, 0xe4292d5a, 0xba3a117e, 0x4851927d, 0x5b016189, 0xa96ae28a,
  0x7da08661, 0x8fcb0562, 0x9c9bf696, 0x6ef07595, 0x417b1dbc, 0xb3109ebf,
  0xa0406d4b, 0x522bee48, 0x86e18aa3, 0x748a09a0, 0x67dafa54, 0x95b17957,
  0xcba24573, 0x39c9c670, 0x2a993584, 0xd8f2b687, 0x0c38d26c, 0xfe53516f,
  0xed03a29b, 0x1f682198, 0x5125dad3, 0xa34e59d0, 0xb01eaa24, 0x42752927,
  0x96bf4dcc, 0x64d4cecf, 0x77843d3b, 0x85efbe38, 0xdbfc821c, 0x2997011f,
  0x3ac7f2eb, 0xc8ac71e8, 0x1c661503, 0xee0d9600, 0xfd5d65f4, 0x0f36e6f7,
  0x61c69362, 0x93ad1061, 0x80fde395, 0x72966096, 0xa65c047d, 0x5437877e,
  0x4767748a, 0xb50cf789, 0xeb1fcbad, 0x197448ae, 0x0a24bb5a, 0xf84f3859,
  0x2c855cb2, 0xdeeedfb1, 0xcdbe2c45, 0x3fd5af46, 0x7198540d, 0x83f3d70e,
  0x90a324fa, 0x62c8a7f9, 0xb602c312, 0x44694011, 0x5739b3e5, 0xa55230e6,
  0xfb410cc2, 0x092a8fc1, 0x1a7a7c35, 0xe811ff36, 0x3cdb9bdd, 0xceb018de,
  0xdde0eb2a, 0x2f8b6829, 0x82f63b78, 0x709db87b, 0x63cd4b8f, 0x91a6c88c,
  0x456cac67, 0xb7072f64, 0xa457dc90, 0x563c5f93, 0x082f63b7, 0xfa44e0b4,
  0xe9141340, 0x1b7f9043, 0xcfb5f4a8, 0x3dde77ab, 0x2e8e845f, 0xdce5075c,
  0x92a8fc17, 0x60c37f14, 0x73938ce0, 0x81f80fe3, 0x55326b08, 0xa759e80b,
  0xb4091bff, 0x466298fc, 0x1871a4d8, 0xea1a27db, 0xf94ad42f, 0x0b21572c,
  0xdfeb33c7, 0x2d80b0c4, 0x3ed04330, 0xccbbc033, 0xa24bb5a6, 0x502036a5,
  0x4370c551, 0xb11b4652, 0x65d122b9, 0x97baa1ba, 0x84ea524e, 0x7681d14d,
  0x2892ed69, 0xdaf96e6a, 0xc9a99d9e, 0x3bc21e9d, 0xef087a76, 0x1d63f975,
  0x0e330a81, 0xfc588982, 0xb21572c9, 0x407ef1ca, 0x532e023e, 0xa145813d,
  0x758fe5d6, 0x87e466d5, 0x94b49521, 0x66df1622, 0x38cc2a06, 0xcaa7a905,
  0xd9f75af1, 0x2b9cd9f2, 0xff56bd19, 0x0d3d3e1a, 0x1e6dcdee, 0xec064eed,
  0xc38d26c4, 0x31e6a5c7, 0x22b65633, 0xd0ddd530, 0x0417b1db, 0xf67c32d8,
  0xe52cc12c, 0x1747422f, 0x49547e0b, 0xbb3ffd08, 0xa86f0efc, 0x5a048dff,
  0x8ecee914, 0x7ca56a17, 0x6ff599e3, 0x9d9e1ae0, 0xd3d3e1ab, 0x21b862a8,
  0x32e8915c, 0xc083125f, 0x144976b4, 0xe622f5b7, 0xf5720643, 0x07198540,
  0x590ab964, 0xab613a67, 0xb831c993, 0x4a5a4a90, 0x9e902e7b, 0x6cfbad78,
  0x7fab5e8c, 0x8dc0dd8f, 0xe330a81a, 0x115b2b19, 0x020bd8ed, 0xf0605bee,
  0x24aa3f05, 0xd6c1bc06, 0xc5914ff2, 0x37faccf1, 0x69e9f0d5, 0x9b8273d6,
  0x88d28022, 0x7ab90321, 0xae7367ca, 0x5c18e4c9, 0x4f48173d, 0xbd23943e,
  0xf36e6f75, 0x0105ec76, 0x12551f82, 0xe03e9c81, 0x34f4f86a, 0xc69f7b69,
  0xd5cf889d, 0x27a40b9e, 0x79b737ba, 0x8bdcb4b9, 0x988c474d, 0x6ae7c44e,
  0xbe2da0a5, 0x4c4623a6, 0x5f16d052, 0xad7d5351
};

/**
   Calculates the CRC32c checksum of the given buffer.

   @param[in]      Buffer        Pointer to the buffer.
   @param[in]      Length        Length of the buffer, in bytes.
   @param[in]      InitialValue  Initial value of the CRC.

   @return The CRC32c checksum.
**/
UINT32
EFIAPI
CalculateCrc32c (
  IN CONST VOID  *Buffer,
  IN UINTN       Length,
  IN UINT32      InitialValue
  )
{
  CONST UINT8  *Buf;
  UINT32       Crc;

  Buf = Buffer;
  Crc = ~InitialValue;

  while (Length-- != 0) {
    Crc = mCrc32cLookupTable[(Crc & 0xFF) ^ *(Buf++)] ^ (Crc >> 8);
  }

  return ~Crc;
}

// The lookup table is inherited from [https://crccalc.com/](https://crccalc.com/%60)
GLOBAL_REMOVE_IF_UNREFERENCED STATIC CONST UINT16  mCrc16CcittFLookupTable[256] = {
  0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50a5, 0x60c6, 0x70e7,
  0x8108, 0x9129, 0xa14a, 0xb16b, 0xc18c, 0xd1ad, 0xe1ce, 0xf1ef,
  0x1231, 0x0210, 0x3273, 0x2252, 0x52b5, 0x4294, 0x72f7, 0x62d6,
  0x9339, 0x8318, 0xb37b, 0xa35a, 0xd3bd, 0xc39c, 0xf3ff, 0xe3de,
  0x2462, 0x3443, 0x0420, 0x1401, 0x64e6, 0x74c7, 0x44a4, 0x5485,
  0xa56a, 0xb54b, 0x8528, 0x9509, 0xe5ee, 0xf5cf, 0xc5ac, 0xd58d,
  0x3653, 0x2672, 0x1611, 0x0630, 0x76d7, 0x66f6, 0x5695, 0x46b4,
  0xb75b, 0xa77a, 0x9719, 0x8738, 0xf7df, 0xe7fe, 0xd79d, 0xc7bc,
  0x48c4, 0x58e5, 0x6886, 0x78a7, 0x0840, 0x1861, 0x2802, 0x3823,
  0xc9cc, 0xd9ed, 0xe98e, 0xf9af, 0x8948, 0x9969, 0xa90a, 0xb92b,
  0x5af5, 0x4ad4, 0x7ab7, 0x6a96, 0x1a71, 0x0a50, 0x3a33, 0x2a12,
  0xdbfd, 0xcbdc, 0xfbbf, 0xeb9e, 0x9b79, 0x8b58, 0xbb3b, 0xab1a,
  0x6ca6, 0x7c87, 0x4ce4, 0x5cc5, 0x2c22, 0x3c03, 0x0c60, 0x1c41,
  0xedae, 0xfd8f, 0xcdec, 0xddcd, 0xad2a, 0xbd0b, 0x8d68, 0x9d49,
  0x7e97, 0x6eb6, 0x5ed5, 0x4ef4, 0x3e13, 0x2e32, 0x1e51, 0x0e70,
  0xff9f, 0xefbe, 0xdfdd, 0xcffc, 0xbf1b, 0xaf3a, 0x9f59, 0x8f78,
  0x9188, 0x81a9, 0xb1ca, 0xa1eb, 0xd10c, 0xc12d, 0xf14e, 0xe16f,
  0x1080, 0x00a1, 0x30c2, 0x20e3, 0x5004, 0x4025, 0x7046, 0x6067,
  0x83b9, 0x9398, 0xa3fb, 0xb3da, 0xc33d, 0xd31c, 0xe37f, 0xf35e,
  0x02b1, 0x1290, 0x22f3, 0x32d2, 0x4235, 0x5214, 0x6277, 0x7256,
  0xb5ea, 0xa5cb, 0x95a8, 0x8589, 0xf56e, 0xe54f, 0xd52c, 0xc50d,
  0x34e2, 0x24c3, 0x14a0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405,
  0xa7db, 0xb7fa, 0x8799, 0x97b8, 0xe75f, 0xf77e, 0xc71d, 0xd73c,
  0x26d3, 0x36f2, 0x0691, 0x16b0, 0x6657, 0x7676, 0x4615, 0x5634,
  0xd94c, 0xc96d, 0xf90e, 0xe92f, 0x99c8, 0x89e9, 0xb98a, 0xa9ab,
  0x5844, 0x4865, 0x7806, 0x6827, 0x18c0, 0x08e1, 0x3882, 0x28a3,
  0xcb7d, 0xdb5c, 0xeb3f, 0xfb1e, 0x8bf9, 0x9bd8, 0xabbb, 0xbb9a,
  0x4a75, 0x5a54, 0x6a37, 0x7a16, 0x0af1, 0x1ad0, 0x2ab3, 0x3a92,
  0xfd2e, 0xed0f, 0xdd6c, 0xcd4d, 0xbdaa, 0xad8b, 0x9de8, 0x8dc9,
  0x7c26, 0x6c07, 0x5c64, 0x4c45, 0x3ca2, 0x2c83, 0x1ce0, 0x0cc1,
  0xef1f, 0xff3e, 0xcf5d, 0xdf7c, 0xaf9b, 0xbfba, 0x8fd9, 0x9ff8,
  0x6e17, 0x7e36, 0x4e55, 0x5e74, 0x2e93, 0x3eb2, 0x0ed1, 0x1ef0,
};

/**
  Calculates the CRC16-CCITT-FALSE checksum of the given buffer.

  @param[in]      Buffer        Pointer to the buffer.
  @param[in]      Length        Length of the buffer, in bytes.
  @param[in]      InitialValue  Initial value of the CRC.

  @return The CRC16-CCITT-FALSE checksum.
**/
UINT16
EFIAPI
CalculateCrc16CcittF (
  IN CONST VOID  *Buffer,
  IN UINTN       Length,
  IN UINT16      InitialValue
  )
{
  CONST UINT8  *Buf;
  UINT16       Crc;

  ASSERT (Buffer != NULL);
  ASSERT (Length <= (MAX_ADDRESS - ((UINTN)Buffer) + 1));

  Buf = Buffer;
  Crc = InitialValue;

  while (Length-- != 0) {
    Crc = mCrc16CcittFLookupTable[((Crc >> 8) ^ *(Buf++)) & 0xFF] ^ (Crc << 8);
  }

  return Crc;
}
