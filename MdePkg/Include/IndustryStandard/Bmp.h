/** @file
  This file defines BMP file header data structures.

Copyright (c) 2006 - 2011, Intel Corporation. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef _BMP_H_
#define _BMP_H_

#pragma pack(1)

typedef struct {
  UINT8    Blue;
  UINT8    Green;
  UINT8    Red;
  UINT8    Reserved;
} BMP_COLOR_MAP;

typedef struct {
  CHAR8     CharB;
  CHAR8     CharM;
  UINT32    Size;
  UINT16    Reserved[2];
  UINT32    ImageOffset;
  UINT32    HeaderSize;
  UINT32    PixelWidth;
  UINT32    PixelHeight;
  UINT16    Planes;              ///< Must be 1
  UINT16    BitPerPixel;         ///< 1, 4, 8, or 24
  UINT32    CompressionType;
  UINT32    ImageSize;           ///< Compressed image size in bytes
  UINT32    XPixelsPerMeter;
  UINT32    YPixelsPerMeter;
  UINT32    NumberOfColors;
  UINT32    ImportantColors;
} BMP_IMAGE_HEADER;

#pragma pack()

#endif
