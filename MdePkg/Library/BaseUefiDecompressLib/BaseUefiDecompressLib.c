/** @file
  UEFI Decompress Library implementation refer to UEFI specification.

  Copyright (c) 2006 - 2019, Intel Corporation. All rights reserved.<BR>
  Portions copyright (c) 2008 - 2009, Apple Inc. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "BaseUefiDecompressLibInternals.h"

/**
  Read NumOfBit of bits from source into mBitBuf.

  Shift mBitBuf NumOfBits left. Read in NumOfBits of bits from source.

  @param  Sd        The global scratch data.
  @param  NumOfBits The number of bits to shift and read.

**/
VOID
FillBuf (
  IN  SCRATCH_DATA  *Sd,
  IN  UINT16        NumOfBits
  )
{
  //
  // Left shift NumOfBits of bits in advance
  //
  Sd->mBitBuf = (UINT32)LShiftU64 (((UINT64)Sd->mBitBuf), NumOfBits);

  //
  // Copy data needed in bytes into mSbuBitBuf
  //
  while (NumOfBits > Sd->mBitCount) {
    NumOfBits    = (UINT16)(NumOfBits - Sd->mBitCount);
    Sd->mBitBuf |= (UINT32)LShiftU64 (((UINT64)Sd->mSubBitBuf), NumOfBits);

    if (Sd->mCompSize > 0) {
      //
      // Get 1 byte into SubBitBuf
      //
      Sd->mCompSize--;
      Sd->mSubBitBuf = Sd->mSrcBase[Sd->mInBuf++];
      Sd->mBitCount  = 8;
    } else {
      //
      // No more bits from the source, just pad zero bit.
      //
      Sd->mSubBitBuf = 0;
      Sd->mBitCount  = 8;
    }
  }

  //
  // Calculate additional bit count read to update mBitCount
  //
  Sd->mBitCount = (UINT16)(Sd->mBitCount - NumOfBits);

  //
  // Copy NumOfBits of bits from mSubBitBuf into mBitBuf
  //
  Sd->mBitBuf |= Sd->mSubBitBuf >> Sd->mBitCount;
}

/**
  Get NumOfBits of bits out from mBitBuf.

  Get NumOfBits of bits out from mBitBuf. Fill mBitBuf with subsequent
  NumOfBits of bits from source. Returns NumOfBits of bits that are
  popped out.

  @param  Sd        The global scratch data.
  @param  NumOfBits The number of bits to pop and read.

  @return The bits that are popped out.

**/
UINT32
GetBits (
  IN  SCRATCH_DATA  *Sd,
  IN  UINT16        NumOfBits
  )
{
  UINT32  OutBits;

  //
  // Pop NumOfBits of Bits from Left
  //
  OutBits = (UINT32)(Sd->mBitBuf >> (BITBUFSIZ - NumOfBits));

  //
  // Fill up mBitBuf from source
  //
  FillBuf (Sd, NumOfBits);

  return OutBits;
}

/**
  Creates Huffman Code mapping table according to code length array.

  Creates Huffman Code mapping table for Extra Set, Char&Len Set
  and Position Set according to code length array.
  If TableBits > 16, then ASSERT ().

  @param  Sd        The global scratch data.
  @param  NumOfChar The number of symbols in the symbol set.
  @param  BitLen    Code length array.
  @param  TableBits The width of the mapping table.
  @param  Table     The table to be created.

  @retval  0 OK.
  @retval  BAD_TABLE The table is corrupted.

**/
UINT16
MakeTable (
  IN  SCRATCH_DATA  *Sd,
  IN  UINT16        NumOfChar,
  IN  UINT8         *BitLen,
  IN  UINT16        TableBits,
  OUT UINT16        *Table
  )
{
  UINT16  Count[17];
  UINT16  Weight[17];
  UINT16  Start[18];
  UINT16  *Pointer;
  UINT16  Index3;
  UINT16  Index;
  UINT16  Len;
  UINT16  Char;
  UINT16  JuBits;
  UINT16  Avail;
  UINT16  NextCode;
  UINT16  Mask;
  UINT16  WordOfStart;
  UINT16  WordOfCount;
  UINT16  MaxTableLength;

  //
  // The maximum mapping table width supported by this internal
  // working function is 16.
  //
  ASSERT (TableBits <= 16);

  for (Index = 0; Index <= 16; Index++) {
    Count[Index] = 0;
  }

  for (Index = 0; Index < NumOfChar; Index++) {
    if (BitLen[Index] > 16) {
      return (UINT16)BAD_TABLE;
    }

    Count[BitLen[Index]]++;
  }

  Start[0] = 0;
  Start[1] = 0;

  for (Index = 1; Index <= 16; Index++) {
    WordOfStart      = Start[Index];
    WordOfCount      = Count[Index];
    Start[Index + 1] = (UINT16)(WordOfStart + (WordOfCount << (16 - Index)));
  }

  if (Start[17] != 0) {
    /*(1U << 16)*/
    return (UINT16)BAD_TABLE;
  }

  JuBits = (UINT16)(16 - TableBits);

  Weight[0] = 0;
  for (Index = 1; Index <= TableBits; Index++) {
    Start[Index] >>= JuBits;
    Weight[Index]  = (UINT16)(1U << (TableBits - Index));
  }

  while (Index <= 16) {
    Weight[Index] = (UINT16)(1U << (16 - Index));
    Index++;
  }

  Index = (UINT16)(Start[TableBits + 1] >> JuBits);

  if (Index != 0) {
    Index3 = (UINT16)(1U << TableBits);
    if (Index < Index3) {
      SetMem16 (Table + Index, (Index3 - Index) * sizeof (*Table), 0);
    }
  }

  Avail          = NumOfChar;
  Mask           = (UINT16)(1U << (15 - TableBits));
  MaxTableLength = (UINT16)(1U << TableBits);

  for (Char = 0; Char < NumOfChar; Char++) {
    Len = BitLen[Char];
    if ((Len == 0) || (Len >= 17)) {
      continue;
    }

    NextCode = (UINT16)(Start[Len] + Weight[Len]);

    if (Len <= TableBits) {
      if ((Start[Len] >= NextCode) || (NextCode > MaxTableLength)) {
        return (UINT16)BAD_TABLE;
      }

      for (Index = Start[Len]; Index < NextCode; Index++) {
        Table[Index] = Char;
      }
    } else {
      Index3  = Start[Len];
      Pointer = &Table[Index3 >> JuBits];
      Index   = (UINT16)(Len - TableBits);

      while (Index != 0) {
        if ((*Pointer == 0) && (Avail < (2 * NC - 1))) {
          Sd->mRight[Avail] = Sd->mLeft[Avail] = 0;
          *Pointer          = Avail++;
        }

        if (*Pointer < (2 * NC - 1)) {
          if ((Index3 & Mask) != 0) {
            Pointer = &Sd->mRight[*Pointer];
          } else {
            Pointer = &Sd->mLeft[*Pointer];
          }
        }

        Index3 <<= 1;
        Index--;
      }

      *Pointer = Char;
    }

    Start[Len] = NextCode;
  }

  //
  // Succeeds
  //
  return 0;
}

/**
  Decodes a position value.

  Get a position value according to Position Huffman Table.

  @param  Sd The global scratch data.

  @return The position value decoded.

**/
UINT32
DecodeP (
  IN  SCRATCH_DATA  *Sd
  )
{
  UINT16  Val;
  UINT32  Mask;
  UINT32  Pos;

  Val = Sd->mPTTable[Sd->mBitBuf >> (BITBUFSIZ - 8)];

  if (Val >= MAXNP) {
    Mask = 1U << (BITBUFSIZ - 1 - 8);

    do {
      if ((Sd->mBitBuf & Mask) != 0) {
        Val = Sd->mRight[Val];
      } else {
        Val = Sd->mLeft[Val];
      }

      Mask >>= 1;
    } while (Val >= MAXNP);
  }

  //
  // Advance what we have read
  //
  FillBuf (Sd, Sd->mPTLen[Val]);

  Pos = Val;
  if (Val > 1) {
    Pos = (UINT32)((1U << (Val - 1)) + GetBits (Sd, (UINT16)(Val - 1)));
  }

  return Pos;
}

/**
  Reads code lengths for the Extra Set or the Position Set.

  Read in the Extra Set or Position Set Length Array, then
  generate the Huffman code mapping for them.

  @param  Sd      The global scratch data.
  @param  nn      The number of symbols.
  @param  nbit    The number of bits needed to represent nn.
  @param  Special The special symbol that needs to be taken care of.

  @retval  0 OK.
  @retval  BAD_TABLE Table is corrupted.

**/
UINT16
ReadPTLen (
  IN  SCRATCH_DATA  *Sd,
  IN  UINT16        nn,
  IN  UINT16        nbit,
  IN  UINT16        Special
  )
{
  UINT16  Number;
  UINT16  CharC;
  UINT16  Index;
  UINT32  Mask;

  ASSERT (nn <= NPT);
  //
  // Read Extra Set Code Length Array size
  //
  Number = (UINT16)GetBits (Sd, nbit);

  if (Number == 0) {
    //
    // This represents only Huffman code used
    //
    CharC = (UINT16)GetBits (Sd, nbit);

    SetMem16 (&Sd->mPTTable[0], sizeof (Sd->mPTTable), CharC);

    SetMem (Sd->mPTLen, nn, 0);

    return 0;
  }

  Index = 0;

  while (Index < Number && Index < NPT) {
    CharC = (UINT16)(Sd->mBitBuf >> (BITBUFSIZ - 3));

    //
    // If a code length is less than 7, then it is encoded as a 3-bit
    // value. Or it is encoded as a series of "1"s followed by a
    // terminating "0". The number of "1"s = Code length - 4.
    //
    if (CharC == 7) {
      Mask = 1U << (BITBUFSIZ - 1 - 3);
      while (Mask & Sd->mBitBuf) {
        Mask >>= 1;
        CharC += 1;
      }
    }

    FillBuf (Sd, (UINT16)((CharC < 7) ? 3 : CharC - 3));

    Sd->mPTLen[Index++] = (UINT8)CharC;

    //
    // For Code&Len Set,
    // After the third length of the code length concatenation,
    // a 2-bit value is used to indicated the number of consecutive
    // zero lengths after the third length.
    //
    if (Index == Special) {
      CharC = (UINT16)GetBits (Sd, 2);
      while ((INT16)(--CharC) >= 0 && Index < NPT) {
        Sd->mPTLen[Index++] = 0;
      }
    }
  }

  while (Index < nn && Index < NPT) {
    Sd->mPTLen[Index++] = 0;
  }

  return MakeTable (Sd, nn, Sd->mPTLen, 8, Sd->mPTTable);
}

/**
  Reads code lengths for Char&Len Set.

  Read in and decode the Char&Len Set Code Length Array, then
  generate the Huffman Code mapping table for the Char&Len Set.

  @param  Sd The global scratch data.

**/
VOID
ReadCLen (
  SCRATCH_DATA  *Sd
  )
{
  UINT16  Number;
  UINT16  CharC;
  UINT16  Index;
  UINT32  Mask;

  Number = (UINT16)GetBits (Sd, CBIT);

  if (Number == 0) {
    //
    // This represents only Huffman code used
    //
    CharC = (UINT16)GetBits (Sd, CBIT);

    SetMem (Sd->mCLen, NC, 0);
    SetMem16 (&Sd->mCTable[0], sizeof (Sd->mCTable), CharC);

    return;
  }

  Index = 0;
  while (Index < Number && Index < NC) {
    CharC = Sd->mPTTable[Sd->mBitBuf >> (BITBUFSIZ - 8)];
    if (CharC >= NT) {
      Mask = 1U << (BITBUFSIZ - 1 - 8);

      do {
        if (Mask & Sd->mBitBuf) {
          CharC = Sd->mRight[CharC];
        } else {
          CharC = Sd->mLeft[CharC];
        }

        Mask >>= 1;
      } while (CharC >= NT);
    }

    //
    // Advance what we have read
    //
    FillBuf (Sd, Sd->mPTLen[CharC]);

    if (CharC <= 2) {
      if (CharC == 0) {
        CharC = 1;
      } else if (CharC == 1) {
        CharC = (UINT16)(GetBits (Sd, 4) + 3);
      } else if (CharC == 2) {
        CharC = (UINT16)(GetBits (Sd, CBIT) + 20);
      }

      while ((INT16)(--CharC) >= 0 && Index < NC) {
        Sd->mCLen[Index++] = 0;
      }
    } else {
      Sd->mCLen[Index++] = (UINT8)(CharC - 2);
    }
  }

  SetMem (Sd->mCLen + Index, NC - Index, 0);

  MakeTable (Sd, NC, Sd->mCLen, 12, Sd->mCTable);

  return;
}

/**
  Decode a character/length value.

  Read one value from mBitBuf, Get one code from mBitBuf. If it is at block boundary, generates
  Huffman code mapping table for Extra Set, Code&Len Set and
  Position Set.

  @param  Sd The global scratch data.

  @return The value decoded.

**/
UINT16
DecodeC (
  SCRATCH_DATA  *Sd
  )
{
  UINT16  Index2;
  UINT32  Mask;

  if (Sd->mBlockSize == 0) {
    //
    // Starting a new block
    // Read BlockSize from block header
    //
    Sd->mBlockSize = (UINT16)GetBits (Sd, 16);

    //
    // Read in the Extra Set Code Length Array,
    // Generate the Huffman code mapping table for Extra Set.
    //
    Sd->mBadTableFlag = ReadPTLen (Sd, NT, TBIT, 3);
    if (Sd->mBadTableFlag != 0) {
      return 0;
    }

    //
    // Read in and decode the Char&Len Set Code Length Array,
    // Generate the Huffman code mapping table for Char&Len Set.
    //
    ReadCLen (Sd);

    //
    // Read in the Position Set Code Length Array,
    // Generate the Huffman code mapping table for the Position Set.
    //
    Sd->mBadTableFlag = ReadPTLen (Sd, MAXNP, Sd->mPBit, (UINT16)(-1));
    if (Sd->mBadTableFlag != 0) {
      return 0;
    }
  }

  //
  // Get one code according to Code&Set Huffman Table
  //
  Sd->mBlockSize--;
  Index2 = Sd->mCTable[Sd->mBitBuf >> (BITBUFSIZ - 12)];

  if (Index2 >= NC) {
    Mask = 1U << (BITBUFSIZ - 1 - 12);

    do {
      if ((Sd->mBitBuf & Mask) != 0) {
        Index2 = Sd->mRight[Index2];
      } else {
        Index2 = Sd->mLeft[Index2];
      }

      Mask >>= 1;
    } while (Index2 >= NC);
  }

  //
  // Advance what we have read
  //
  FillBuf (Sd, Sd->mCLen[Index2]);

  return Index2;
}

/**
  Decode the source data and put the resulting data into the destination buffer.

  @param  Sd The global scratch data.

**/
VOID
Decode (
  SCRATCH_DATA  *Sd
  )
{
  UINT16  BytesRemain;
  UINT32  DataIdx;
  UINT16  CharC;

  BytesRemain = (UINT16)(-1);

  DataIdx = 0;

  for ( ; ;) {
    //
    // Get one code from mBitBuf
    //
    CharC = DecodeC (Sd);
    if (Sd->mBadTableFlag != 0) {
      goto Done;
    }

    if (CharC < 256) {
      //
      // Process an Original character
      //
      if (Sd->mOutBuf >= Sd->mOrigSize) {
        goto Done;
      } else {
        //
        // Write orignal character into mDstBase
        //
        Sd->mDstBase[Sd->mOutBuf++] = (UINT8)CharC;
      }
    } else {
      //
      // Process a Pointer
      //
      CharC = (UINT16)(CharC - (BIT8 - THRESHOLD));

      //
      // Get string length
      //
      BytesRemain = CharC;

      //
      // Locate string position
      //
      DataIdx = Sd->mOutBuf - DecodeP (Sd) - 1;

      //
      // Write BytesRemain of bytes into mDstBase
      //
      BytesRemain--;

      while ((INT16)(BytesRemain) >= 0) {
        if (Sd->mOutBuf >= Sd->mOrigSize) {
          goto Done;
        }

        if (DataIdx >= Sd->mOrigSize) {
          Sd->mBadTableFlag = (UINT16)BAD_TABLE;
          goto Done;
        }

        Sd->mDstBase[Sd->mOutBuf++] = Sd->mDstBase[DataIdx++];

        BytesRemain--;
      }

      //
      // Once mOutBuf is fully filled, directly return
      //
      if (Sd->mOutBuf >= Sd->mOrigSize) {
        goto Done;
      }
    }
  }

Done:
  return;
}

/**
  Given a compressed source buffer, this function retrieves the size of
  the uncompressed buffer and the size of the scratch buffer required
  to decompress the compressed source buffer.

  Retrieves the size of the uncompressed buffer and the temporary scratch buffer
  required to decompress the buffer specified by Source and SourceSize.
  If the size of the uncompressed buffer or the size of the scratch buffer cannot
  be determined from the compressed data specified by Source and SourceData,
  then RETURN_INVALID_PARAMETER is returned.  Otherwise, the size of the uncompressed
  buffer is returned in DestinationSize, the size of the scratch buffer is returned
  in ScratchSize, and RETURN_SUCCESS is returned.
  This function does not have scratch buffer available to perform a thorough
  checking of the validity of the source data.  It just retrieves the "Original Size"
  field from the beginning bytes of the source data and output it as DestinationSize.
  And ScratchSize is specific to the decompression implementation.

  If Source is NULL, then ASSERT().
  If DestinationSize is NULL, then ASSERT().
  If ScratchSize is NULL, then ASSERT().

  @param  Source          The source buffer containing the compressed data.
  @param  SourceSize      The size, in bytes, of the source buffer.
  @param  DestinationSize A pointer to the size, in bytes, of the uncompressed buffer
                          that will be generated when the compressed buffer specified
                          by Source and SourceSize is decompressed.
  @param  ScratchSize     A pointer to the size, in bytes, of the scratch buffer that
                          is required to decompress the compressed buffer specified
                          by Source and SourceSize.

  @retval  RETURN_SUCCESS The size of the uncompressed data was returned
                          in DestinationSize, and the size of the scratch
                          buffer was returned in ScratchSize.
  @retval  RETURN_INVALID_PARAMETER
                          The size of the uncompressed data or the size of
                          the scratch buffer cannot be determined from
                          the compressed data specified by Source
                          and SourceSize.
**/
RETURN_STATUS
EFIAPI
UefiDecompressGetInfo (
  IN  CONST VOID  *Source,
  IN  UINT32      SourceSize,
  OUT UINT32      *DestinationSize,
  OUT UINT32      *ScratchSize
  )
{
  UINT32  CompressedSize;

  ASSERT (Source != NULL);
  ASSERT (DestinationSize != NULL);
  ASSERT (ScratchSize != NULL);

  if (SourceSize < 8) {
    return RETURN_INVALID_PARAMETER;
  }

  CompressedSize = ReadUnaligned32 ((UINT32 *)Source);
  if ((SourceSize < (CompressedSize + 8)) || ((CompressedSize + 8) < 8)) {
    return RETURN_INVALID_PARAMETER;
  }

  *ScratchSize     = sizeof (SCRATCH_DATA);
  *DestinationSize = ReadUnaligned32 ((UINT32 *)Source + 1);

  return RETURN_SUCCESS;
}

/**
  Decompresses a compressed source buffer.

  Extracts decompressed data to its original form.
  This function is designed so that the decompression algorithm can be implemented
  without using any memory services.  As a result, this function is not allowed to
  call any memory allocation services in its implementation.  It is the caller's
  responsibility to allocate and free the Destination and Scratch buffers.
  If the compressed source data specified by Source is successfully decompressed
  into Destination, then RETURN_SUCCESS is returned.  If the compressed source data
  specified by Source is not in a valid compressed data format,
  then RETURN_INVALID_PARAMETER is returned.

  If Source is NULL, then ASSERT().
  If Destination is NULL, then ASSERT().
  If the required scratch buffer size > 0 and Scratch is NULL, then ASSERT().
  If the Version is not 1 or 2, then ASSERT().

  @param  Source      The source buffer containing the compressed data.
  @param  Destination The destination buffer to store the decompressed data.
  @param  Scratch     A temporary scratch buffer that is used to perform the decompression.
                      This is an optional parameter that may be NULL if the
                      required scratch buffer size is 0.
  @param  Version     1 for UEFI Decompress algoruthm, 2 for Tiano Decompess algorithm.

  @retval  RETURN_SUCCESS Decompression completed successfully, and
                          the uncompressed buffer is returned in Destination.
  @retval  RETURN_INVALID_PARAMETER
                          The source buffer specified by Source is corrupted
                          (not in a valid compressed format).
**/
RETURN_STATUS
UefiTianoDecompress (
  IN CONST VOID  *Source,
  IN OUT VOID    *Destination,
  IN OUT VOID    *Scratch,
  IN UINT32      Version
  )
{
  UINT32        CompSize;
  UINT32        OrigSize;
  SCRATCH_DATA  *Sd;
  CONST UINT8   *Src;
  UINT8         *Dst;

  ASSERT (Source != NULL);
  ASSERT (Destination != NULL);
  ASSERT (Scratch != NULL);
  ASSERT (Version == 1 || Version == 2);

  Src = Source;
  Dst = Destination;

  Sd = (SCRATCH_DATA *)Scratch;

  CompSize = Src[0] + (Src[1] << 8) + (Src[2] << 16) + (Src[3] << 24);
  OrigSize = Src[4] + (Src[5] << 8) + (Src[6] << 16) + (Src[7] << 24);

  //
  // If compressed file size is 0, return
  //
  if (OrigSize == 0) {
    return RETURN_SUCCESS;
  }

  Src = Src + 8;
  SetMem (Sd, sizeof (SCRATCH_DATA), 0);

  //
  // The length of the field 'Position Set Code Length Array Size' in Block Header.
  // For UEFI 2.0 de/compression algorithm(Version 1), mPBit = 4
  // For Tiano de/compression algorithm(Version 2), mPBit = 5
  //
  switch (Version) {
    case 1:
      Sd->mPBit = 4;
      break;
    case 2:
      Sd->mPBit = 5;
      break;
    default:
      ASSERT (FALSE);
  }

  Sd->mSrcBase = (UINT8 *)Src;
  Sd->mDstBase = Dst;
  //
  // CompSize and OrigSize are calculated in bytes
  //
  Sd->mCompSize = CompSize;
  Sd->mOrigSize = OrigSize;

  //
  // Fill the first BITBUFSIZ bits
  //
  FillBuf (Sd, BITBUFSIZ);

  //
  // Decompress it
  //
  Decode (Sd);

  if (Sd->mBadTableFlag != 0) {
    //
    // Something wrong with the source
    //
    return RETURN_INVALID_PARAMETER;
  }

  return RETURN_SUCCESS;
}

/**
  Decompresses a UEFI compressed source buffer.

  Extracts decompressed data to its original form.
  This function is designed so that the decompression algorithm can be implemented
  without using any memory services.  As a result, this function is not allowed to
  call any memory allocation services in its implementation.  It is the caller's
  responsibility to allocate and free the Destination and Scratch buffers.
  If the compressed source data specified by Source is successfully decompressed
  into Destination, then RETURN_SUCCESS is returned.  If the compressed source data
  specified by Source is not in a valid compressed data format,
  then RETURN_INVALID_PARAMETER is returned.

  If Source is NULL, then ASSERT().
  If Destination is NULL, then ASSERT().
  If the required scratch buffer size > 0 and Scratch is NULL, then ASSERT().

  @param  Source      The source buffer containing the compressed data.
  @param  Destination The destination buffer to store the decompressed data
  @param  Scratch     A temporary scratch buffer that is used to perform the decompression.
                      This is an optional parameter that may be NULL if the
                      required scratch buffer size is 0.

  @retval  RETURN_SUCCESS Decompression completed successfully, and
                          the uncompressed buffer is returned in Destination.
  @retval  RETURN_INVALID_PARAMETER
                          The source buffer specified by Source is corrupted
                          (not in a valid compressed format).
**/
RETURN_STATUS
EFIAPI
UefiDecompress (
  IN CONST VOID  *Source,
  IN OUT VOID    *Destination,
  IN OUT VOID    *Scratch  OPTIONAL
  )
{
  return UefiTianoDecompress (Source, Destination, Scratch, 1);
}
