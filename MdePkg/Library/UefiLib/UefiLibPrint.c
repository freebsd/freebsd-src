/** @file
  Mde UEFI library API implementation.
  Print to StdErr or ConOut defined in EFI_SYSTEM_TABLE

  Copyright (c) 2007 - 2018, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "UefiLibInternal.h"

GLOBAL_REMOVE_IF_UNREFERENCED EFI_GRAPHICS_OUTPUT_BLT_PIXEL mEfiColors[16] = {
  { 0x00, 0x00, 0x00, 0x00 },
  { 0x98, 0x00, 0x00, 0x00 },
  { 0x00, 0x98, 0x00, 0x00 },
  { 0x98, 0x98, 0x00, 0x00 },
  { 0x00, 0x00, 0x98, 0x00 },
  { 0x98, 0x00, 0x98, 0x00 },
  { 0x00, 0x98, 0x98, 0x00 },
  { 0x98, 0x98, 0x98, 0x00 },
  { 0x10, 0x10, 0x10, 0x00 },
  { 0xff, 0x10, 0x10, 0x00 },
  { 0x10, 0xff, 0x10, 0x00 },
  { 0xff, 0xff, 0x10, 0x00 },
  { 0x10, 0x10, 0xff, 0x00 },
  { 0xf0, 0x10, 0xff, 0x00 },
  { 0x10, 0xff, 0xff, 0x00 },
  { 0xff, 0xff, 0xff, 0x00 }
};

/**
  Internal function which prints a formatted Unicode string to the console output device
  specified by Console

  This function prints a formatted Unicode string to the console output device
  specified by Console and returns the number of Unicode characters that printed
  to it.  If the length of the formatted Unicode string is greater than PcdUefiLibMaxPrintBufferSize,
  then only the first PcdUefiLibMaxPrintBufferSize characters are sent to Console.
  If Format is NULL, then ASSERT().
  If Format is not aligned on a 16-bit boundary, then ASSERT().

  @param Format   A Null-terminated Unicode format string.
  @param Console  The output console.
  @param Marker   A VA_LIST marker for the variable argument list.

  @return The number of Unicode characters in the produced
          output buffer, not including the Null-terminator.
**/
UINTN
InternalPrint (
  IN  CONST CHAR16                     *Format,
  IN  EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL  *Console,
  IN  VA_LIST                          Marker
  )
{
  EFI_STATUS  Status;
  UINTN       Return;
  CHAR16      *Buffer;
  UINTN       BufferSize;

  ASSERT (Format != NULL);
  ASSERT (((UINTN) Format & BIT0) == 0);
  ASSERT (Console != NULL);

  BufferSize = (PcdGet32 (PcdUefiLibMaxPrintBufferSize) + 1) * sizeof (CHAR16);

  Buffer = (CHAR16 *) AllocatePool(BufferSize);
  ASSERT (Buffer != NULL);

  Return = UnicodeVSPrint (Buffer, BufferSize, Format, Marker);

  if (Console != NULL && Return > 0) {
    //
    // To be extra safe make sure Console has been initialized
    //
    Status = Console->OutputString (Console, Buffer);
    if (EFI_ERROR (Status)) {
      Return = 0;
    }
  }

  FreePool (Buffer);

  return Return;
}

/**
  Prints a formatted Unicode string to the console output device specified by
  ConOut defined in the EFI_SYSTEM_TABLE.

  This function prints a formatted Unicode string to the console output device
  specified by ConOut in EFI_SYSTEM_TABLE and returns the number of Unicode
  characters that printed to ConOut.  If the length of the formatted Unicode
  string is greater than PcdUefiLibMaxPrintBufferSize, then only the first
  PcdUefiLibMaxPrintBufferSize characters are sent to ConOut.
  If Format is NULL, then ASSERT().
  If Format is not aligned on a 16-bit boundary, then ASSERT().
  If gST->ConOut is NULL, then ASSERT().

  @param Format   A Null-terminated Unicode format string.
  @param ...      A Variable argument list whose contents are accessed based
                  on the format string specified by Format.

  @return The number of Unicode characters printed to ConOut.

**/
UINTN
EFIAPI
Print (
  IN CONST CHAR16  *Format,
  ...
  )
{
  VA_LIST Marker;
  UINTN   Return;

  VA_START (Marker, Format);

  Return = InternalPrint (Format, gST->ConOut, Marker);

  VA_END (Marker);

  return Return;
}

/**
  Prints a formatted Unicode string to the console output device specified by
  StdErr defined in the EFI_SYSTEM_TABLE.

  This function prints a formatted Unicode string to the console output device
  specified by StdErr in EFI_SYSTEM_TABLE and returns the number of Unicode
  characters that printed to StdErr.  If the length of the formatted Unicode
  string is greater than PcdUefiLibMaxPrintBufferSize, then only the first
  PcdUefiLibMaxPrintBufferSize characters are sent to StdErr.
  If Format is NULL, then ASSERT().
  If Format is not aligned on a 16-bit boundary, then ASSERT().
  If gST->StdErr is NULL, then ASSERT().

  @param Format   A Null-terminated Unicode format string.
  @param ...      Variable argument list whose contents are accessed based
                  on the format string specified by Format.

  @return The number of Unicode characters printed to StdErr.

**/
UINTN
EFIAPI
ErrorPrint (
  IN CONST CHAR16  *Format,
  ...
  )
{
  VA_LIST Marker;
  UINTN   Return;

  VA_START (Marker, Format);

  Return = InternalPrint( Format, gST->StdErr, Marker);

  VA_END (Marker);

  return Return;
}


/**
  Internal function which prints a formatted ASCII string to the console output device
  specified by Console

  This function prints a formatted ASCII string to the console output device
  specified by Console and returns the number of ASCII characters that printed
  to it.  If the length of the formatted ASCII string is greater than PcdUefiLibMaxPrintBufferSize,
  then only the first PcdUefiLibMaxPrintBufferSize characters are sent to Console.

  If Format is NULL, then ASSERT().

  @param Format   A Null-terminated ASCII format string.
  @param Console  The output console.
  @param Marker   VA_LIST marker for the variable argument list.

  @return The number of Unicode characters in the produced
          output buffer not including the Null-terminator.

**/
UINTN
AsciiInternalPrint (
  IN  CONST CHAR8                      *Format,
  IN  EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL  *Console,
  IN  VA_LIST                          Marker
  )
{
  EFI_STATUS  Status;
  UINTN       Return;
  CHAR16      *Buffer;
  UINTN       BufferSize;

  ASSERT (Format != NULL);
  ASSERT (Console != NULL);

  BufferSize = (PcdGet32 (PcdUefiLibMaxPrintBufferSize) + 1) * sizeof (CHAR16);

  Buffer = (CHAR16 *) AllocatePool(BufferSize);
  ASSERT (Buffer != NULL);

  Return = UnicodeVSPrintAsciiFormat (Buffer, BufferSize, Format, Marker);

  if (Console != NULL) {
    //
    // To be extra safe make sure Console has been initialized
    //
    Status = Console->OutputString (Console, Buffer);
    if (EFI_ERROR (Status)) {
      Return = 0;
    }
  }

  FreePool (Buffer);

  return Return;
}

/**
  Prints a formatted ASCII string to the console output device specified by
  ConOut defined in the EFI_SYSTEM_TABLE.

  This function prints a formatted ASCII string to the console output device
  specified by ConOut in EFI_SYSTEM_TABLE and returns the number of ASCII
  characters that printed to ConOut.  If the length of the formatted ASCII
  string is greater than PcdUefiLibMaxPrintBufferSize, then only the first
  PcdUefiLibMaxPrintBufferSize characters are sent to ConOut.
  If Format is NULL, then ASSERT().
  If gST->ConOut is NULL, then ASSERT().

  @param Format   A Null-terminated ASCII format string.
  @param ...      Variable argument list whose contents are accessed based
                  on the format string specified by Format.

  @return The number of ASCII characters printed to ConOut.

**/
UINTN
EFIAPI
AsciiPrint (
  IN CONST CHAR8  *Format,
  ...
  )
{
  VA_LIST Marker;
  UINTN   Return;
  ASSERT (Format != NULL);

  VA_START (Marker, Format);

  Return = AsciiInternalPrint( Format, gST->ConOut, Marker);

  VA_END (Marker);

  return Return;
}

/**
  Prints a formatted ASCII string to the console output device specified by
  StdErr defined in the EFI_SYSTEM_TABLE.

  This function prints a formatted ASCII string to the console output device
  specified by StdErr in EFI_SYSTEM_TABLE and returns the number of ASCII
  characters that printed to StdErr.  If the length of the formatted ASCII
  string is greater than PcdUefiLibMaxPrintBufferSize, then only the first
  PcdUefiLibMaxPrintBufferSize characters are sent to StdErr.
  If Format is NULL, then ASSERT().
  If gST->StdErr is NULL, then ASSERT().

  @param Format   A Null-terminated ASCII format string.
  @param ...      Variable argument list whose contents are accessed based
                  on the format string specified by Format.

  @return The number of ASCII characters printed to ConErr.

**/
UINTN
EFIAPI
AsciiErrorPrint (
  IN CONST CHAR8  *Format,
  ...
  )
{
  VA_LIST Marker;
  UINTN   Return;

  ASSERT (Format != NULL);

  VA_START (Marker, Format);

  Return = AsciiInternalPrint( Format, gST->StdErr, Marker);

  VA_END (Marker);

  return Return;
}

/**
  Internal function to print a formatted Unicode string to a graphics console device specified by
  ConsoleOutputHandle defined in the EFI_SYSTEM_TABLE at the given (X,Y) coordinates.

  This function prints a formatted Unicode string to the graphics console device
  specified by ConsoleOutputHandle in EFI_SYSTEM_TABLE and returns the number of
  Unicode characters printed. The EFI_HII_FONT_PROTOCOL is used to convert the
  string to a bitmap using the glyphs registered with the
  HII database.  No wrapping is performed, so any portions of the string the fall
  outside the active display region will not be displayed.

  If a graphics console device is not associated with the ConsoleOutputHandle
  defined in the EFI_SYSTEM_TABLE then no string is printed, and 0 is returned.
  If the EFI_HII_FONT_PROTOCOL is not present in the handle database, then no
  string is printed, and 0 is returned.

  @param  PointX       An X coordinate to print the string.
  @param  PointY       A Y coordinate to print the string.
  @param  Foreground   The foreground color of the string being printed.  This is
                       an optional parameter that may be NULL.  If it is NULL,
                       then the foreground color of the current ConOut device
                       in the EFI_SYSTEM_TABLE is used.
  @param  Background   The background color of the string being printed.  This is
                       an optional parameter that may be NULL.  If it is NULL,
                       then the background color of the current ConOut device
                       in the EFI_SYSTEM_TABLE is used.
  @param  Buffer       A Null-terminated Unicode formatted string.
  @param  PrintNum     The number of Unicode formatted string to be printed.

  @return  The number of Unicode Characters printed. Zero means no any character
           displayed successfully.

**/
UINTN
InternalPrintGraphic (
  IN UINTN                            PointX,
  IN UINTN                            PointY,
  IN EFI_GRAPHICS_OUTPUT_BLT_PIXEL    *Foreground,
  IN EFI_GRAPHICS_OUTPUT_BLT_PIXEL    *Background,
  IN CHAR16                           *Buffer,
  IN UINTN                            PrintNum
  )
{
  EFI_STATUS                          Status;
  UINT32                              HorizontalResolution;
  UINT32                              VerticalResolution;
  UINT32                              ColorDepth;
  UINT32                              RefreshRate;
  EFI_HII_FONT_PROTOCOL               *HiiFont;
  EFI_IMAGE_OUTPUT                    *Blt;
  EFI_FONT_DISPLAY_INFO               FontInfo;
  EFI_HII_ROW_INFO                    *RowInfoArray;
  UINTN                               RowInfoArraySize;
  EFI_GRAPHICS_OUTPUT_PROTOCOL        *GraphicsOutput;
  EFI_UGA_DRAW_PROTOCOL               *UgaDraw;
  EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL     *Sto;
  EFI_HANDLE                          ConsoleHandle;
  UINTN                               Width;
  UINTN                               Height;
  UINTN                               Delta;

  HorizontalResolution  = 0;
  VerticalResolution    = 0;
  Blt                   = NULL;
  RowInfoArray          = NULL;

  ConsoleHandle = gST->ConsoleOutHandle;

  ASSERT( ConsoleHandle != NULL);

  Status = gBS->HandleProtocol (
                  ConsoleHandle,
                  &gEfiGraphicsOutputProtocolGuid,
                  (VOID **) &GraphicsOutput
                  );

  UgaDraw = NULL;
  if (EFI_ERROR (Status) && FeaturePcdGet (PcdUgaConsumeSupport)) {
    //
    // If no GOP available, try to open UGA Draw protocol if supported.
    //
    GraphicsOutput = NULL;

    Status = gBS->HandleProtocol (
                    ConsoleHandle,
                    &gEfiUgaDrawProtocolGuid,
                    (VOID **) &UgaDraw
                    );
  }
  if (EFI_ERROR (Status)) {
    goto Error;
  }

  Status = gBS->HandleProtocol (
                  ConsoleHandle,
                  &gEfiSimpleTextOutProtocolGuid,
                  (VOID **) &Sto
                  );

  if (EFI_ERROR (Status)) {
    goto Error;
  }

  if (GraphicsOutput != NULL) {
    HorizontalResolution = GraphicsOutput->Mode->Info->HorizontalResolution;
    VerticalResolution = GraphicsOutput->Mode->Info->VerticalResolution;
  } else if (UgaDraw != NULL && FeaturePcdGet (PcdUgaConsumeSupport)) {
    UgaDraw->GetMode (UgaDraw, &HorizontalResolution, &VerticalResolution, &ColorDepth, &RefreshRate);
  } else {
    goto Error;
  }

  ASSERT ((HorizontalResolution != 0) && (VerticalResolution !=0));

  Status = gBS->LocateProtocol (&gEfiHiiFontProtocolGuid, NULL, (VOID **) &HiiFont);
  if (EFI_ERROR (Status)) {
    goto Error;
  }

  Blt = (EFI_IMAGE_OUTPUT *) AllocateZeroPool (sizeof (EFI_IMAGE_OUTPUT));
  ASSERT (Blt != NULL);

  Blt->Width        = (UINT16) (HorizontalResolution);
  Blt->Height       = (UINT16) (VerticalResolution);

  ZeroMem (&FontInfo, sizeof (EFI_FONT_DISPLAY_INFO));

  if (Foreground != NULL) {
    CopyMem (&FontInfo.ForegroundColor, Foreground, sizeof (EFI_GRAPHICS_OUTPUT_BLT_PIXEL));
  } else {
    CopyMem (
      &FontInfo.ForegroundColor,
      &mEfiColors[Sto->Mode->Attribute & 0x0f],
      sizeof (EFI_GRAPHICS_OUTPUT_BLT_PIXEL)
      );
  }
  if (Background != NULL) {
    CopyMem (&FontInfo.BackgroundColor, Background, sizeof (EFI_GRAPHICS_OUTPUT_BLT_PIXEL));
  } else {
    CopyMem (
      &FontInfo.BackgroundColor,
      &mEfiColors[Sto->Mode->Attribute >> 4],
      sizeof (EFI_GRAPHICS_OUTPUT_BLT_PIXEL)
      );
  }

  if (GraphicsOutput != NULL) {
    Blt->Image.Screen = GraphicsOutput;

    Status = HiiFont->StringToImage (
                         HiiFont,
                         EFI_HII_IGNORE_IF_NO_GLYPH | EFI_HII_OUT_FLAG_CLIP |
                         EFI_HII_OUT_FLAG_CLIP_CLEAN_X | EFI_HII_OUT_FLAG_CLIP_CLEAN_Y |
                         EFI_HII_IGNORE_LINE_BREAK | EFI_HII_DIRECT_TO_SCREEN,
                         Buffer,
                         &FontInfo,
                         &Blt,
                         PointX,
                         PointY,
                         &RowInfoArray,
                         &RowInfoArraySize,
                         NULL
                         );
    if (EFI_ERROR (Status)) {
      goto Error;
    }

  } else if (FeaturePcdGet (PcdUgaConsumeSupport)) {
    ASSERT (UgaDraw!= NULL);

    //
    // Ensure Width * Height * sizeof (EFI_GRAPHICS_OUTPUT_BLT_PIXEL) doesn't overflow.
    //
    if (Blt->Width > DivU64x32 (MAX_UINTN, Blt->Height * sizeof (EFI_GRAPHICS_OUTPUT_BLT_PIXEL))) {
      goto Error;
    }

    Blt->Image.Bitmap = AllocateZeroPool ((UINT32) Blt->Width * Blt->Height * sizeof (EFI_GRAPHICS_OUTPUT_BLT_PIXEL));
    ASSERT (Blt->Image.Bitmap != NULL);

    //
    //  StringToImage only support blt'ing image to device using GOP protocol. If GOP is not supported in this platform,
    //  we ask StringToImage to print the string to blt buffer, then blt to device using UgaDraw.
    //
    Status = HiiFont->StringToImage (
                         HiiFont,
                         EFI_HII_IGNORE_IF_NO_GLYPH | EFI_HII_OUT_FLAG_CLIP |
                         EFI_HII_OUT_FLAG_CLIP_CLEAN_X | EFI_HII_OUT_FLAG_CLIP_CLEAN_Y |
                         EFI_HII_IGNORE_LINE_BREAK,
                         Buffer,
                         &FontInfo,
                         &Blt,
                         PointX,
                         PointY,
                         &RowInfoArray,
                         &RowInfoArraySize,
                         NULL
                         );

    if (!EFI_ERROR (Status)) {
      ASSERT (RowInfoArray != NULL);
      //
      // Explicit Line break characters are ignored, so the updated parameter RowInfoArraySize by StringToImage will
      // always be 1 or 0 (if there is no valid Unicode Char can be printed). ASSERT here to make sure.
      //
      ASSERT (RowInfoArraySize <= 1);

      if (RowInfoArraySize != 0) {
        Width  = RowInfoArray[0].LineWidth;
        Height = RowInfoArray[0].LineHeight;
        Delta  = Blt->Width * sizeof (EFI_GRAPHICS_OUTPUT_BLT_PIXEL);
      } else {
        Width  = 0;
        Height = 0;
        Delta  = 0;
      }
      Status = UgaDraw->Blt (
                          UgaDraw,
                          (EFI_UGA_PIXEL *) Blt->Image.Bitmap,
                          EfiUgaBltBufferToVideo,
                          PointX,
                          PointY,
                          PointX,
                          PointY,
                          Width,
                          Height,
                          Delta
                          );
    } else {
      goto Error;
    }
    FreePool (Blt->Image.Bitmap);
  } else {
    goto Error;
  }
  //
  // Calculate the number of actual printed characters
  //
  if (RowInfoArraySize != 0) {
    PrintNum = RowInfoArray[0].EndIndex - RowInfoArray[0].StartIndex + 1;
  } else {
    PrintNum = 0;
  }

  FreePool (RowInfoArray);
  FreePool (Blt);
  return PrintNum;

Error:
  if (Blt != NULL) {
    FreePool (Blt);
  }
  return 0;
}

/**
  Prints a formatted Unicode string to a graphics console device specified by
  ConsoleOutputHandle defined in the EFI_SYSTEM_TABLE at the given (X,Y) coordinates.

  This function prints a formatted Unicode string to the graphics console device
  specified by ConsoleOutputHandle in EFI_SYSTEM_TABLE and returns the number of
  Unicode characters displayed, not including partial characters that may be clipped
  by the right edge of the display.  If the length of the formatted Unicode string is
  greater than PcdUefiLibMaxPrintBufferSize, then at most the first
  PcdUefiLibMaxPrintBufferSize characters are printed.The EFI_HII_FONT_PROTOCOL
  StringToImage() service is used to convert the string to a bitmap using the glyphs
  registered with the HII database. No wrapping is performed, so any portions of the
  string the fall outside the active display region will not be displayed. Please see
  Section 27.2.6 of the UEFI Specification for a description of the supported string
  format including the set of control codes supported by the StringToImage() service.

  If a graphics console device is not associated with the ConsoleOutputHandle
  defined in the EFI_SYSTEM_TABLE then no string is printed, and 0 is returned.
  If the EFI_HII_FONT_PROTOCOL is not present in the handle database, then no
  string is printed, and 0 is returned.
  If Format is NULL, then ASSERT().
  If Format is not aligned on a 16-bit boundary, then ASSERT().
  If gST->ConsoleOutputHandle is NULL, then ASSERT().

  @param  PointX       An X coordinate to print the string.
  @param  PointY       A Y coordinate to print the string.
  @param  ForeGround   The foreground color of the string being printed.  This is
                       an optional parameter that may be NULL.  If it is NULL,
                       then the foreground color of the current ConOut device
                       in the EFI_SYSTEM_TABLE is used.
  @param  BackGround   The background color of the string being printed.  This is
                       an optional parameter that may be NULL.  If it is NULL,
                       then the background color of the current ConOut device
                       in the EFI_SYSTEM_TABLE is used.
  @param  Format       A Null-terminated Unicode format string.  See Print Library
                       for the supported format string syntax.
  @param  ...          A Variable argument list whose contents are accessed based on
                       the format string specified by Format.

  @return  The number of Unicode characters printed.

**/
UINTN
EFIAPI
PrintXY (
  IN UINTN                            PointX,
  IN UINTN                            PointY,
  IN EFI_GRAPHICS_OUTPUT_BLT_PIXEL    *ForeGround, OPTIONAL
  IN EFI_GRAPHICS_OUTPUT_BLT_PIXEL    *BackGround, OPTIONAL
  IN CONST CHAR16                     *Format,
  ...
  )
{
  VA_LIST                             Marker;
  CHAR16                              *Buffer;
  UINTN                               BufferSize;
  UINTN                               PrintNum;
  UINTN                               ReturnNum;

  ASSERT (Format != NULL);
  ASSERT (((UINTN) Format & BIT0) == 0);

  VA_START (Marker, Format);

  BufferSize = (PcdGet32 (PcdUefiLibMaxPrintBufferSize) + 1) * sizeof (CHAR16);

  Buffer = (CHAR16 *) AllocatePool (BufferSize);
  ASSERT (Buffer != NULL);

  PrintNum = UnicodeVSPrint (Buffer, BufferSize, Format, Marker);

  VA_END (Marker);

  ReturnNum = InternalPrintGraphic (PointX, PointY, ForeGround, BackGround, Buffer, PrintNum);

  FreePool (Buffer);

  return ReturnNum;
}

/**
  Prints a formatted ASCII string to a graphics console device specified by
  ConsoleOutputHandle defined in the EFI_SYSTEM_TABLE at the given (X,Y) coordinates.

  This function prints a formatted ASCII string to the graphics console device
  specified by ConsoleOutputHandle in EFI_SYSTEM_TABLE and returns the number of
  ASCII characters displayed, not including partial characters that may be clipped
  by the right edge of the display.  If the length of the formatted ASCII string is
  greater than PcdUefiLibMaxPrintBufferSize, then at most the first
  PcdUefiLibMaxPrintBufferSize characters are printed.The EFI_HII_FONT_PROTOCOL
  StringToImage() service is used to convert the string to a bitmap using the glyphs
  registered with the HII database. No wrapping is performed, so any portions of the
  string the fall outside the active display region will not be displayed. Please see
  Section 27.2.6 of the UEFI Specification for a description of the supported string
  format including the set of control codes supported by the StringToImage() service.

  If a graphics console device is not associated with the ConsoleOutputHandle
  defined in the EFI_SYSTEM_TABLE then no string is printed, and 0 is returned.
  If the EFI_HII_FONT_PROTOCOL is not present in the handle database, then no
  string is printed, and 0 is returned.
  If Format is NULL, then ASSERT().
  If gST->ConsoleOutputHandle is NULL, then ASSERT().

  @param  PointX       An X coordinate to print the string.
  @param  PointY       A Y coordinate to print the string.
  @param  ForeGround   The foreground color of the string being printed.  This is
                       an optional parameter that may be NULL.  If it is NULL,
                       then the foreground color of the current ConOut device
                       in the EFI_SYSTEM_TABLE is used.
  @param  BackGround   The background color of the string being printed.  This is
                       an optional parameter that may be NULL.  If it is NULL,
                       then the background color of the current ConOut device
                       in the EFI_SYSTEM_TABLE is used.
  @param  Format       A Null-terminated ASCII format string.  See Print Library
                       for the supported format string syntax.
  @param  ...          Variable argument list whose contents are accessed based on
                       the format string specified by Format.

  @return  The number of ASCII characters printed.

**/
UINTN
EFIAPI
AsciiPrintXY (
  IN UINTN                            PointX,
  IN UINTN                            PointY,
  IN EFI_GRAPHICS_OUTPUT_BLT_PIXEL    *ForeGround, OPTIONAL
  IN EFI_GRAPHICS_OUTPUT_BLT_PIXEL    *BackGround, OPTIONAL
  IN CONST CHAR8                      *Format,
  ...
  )
{
  VA_LIST                             Marker;
  CHAR16                              *Buffer;
  UINTN                               BufferSize;
  UINTN                               PrintNum;
  UINTN                               ReturnNum;

  ASSERT (Format != NULL);

  VA_START (Marker, Format);

  BufferSize = (PcdGet32 (PcdUefiLibMaxPrintBufferSize) + 1) * sizeof (CHAR16);

  Buffer = (CHAR16 *) AllocatePool (BufferSize);
  ASSERT (Buffer != NULL);

  PrintNum = UnicodeSPrintAsciiFormat (Buffer, BufferSize, Format, Marker);

  VA_END (Marker);

  ReturnNum = InternalPrintGraphic (PointX, PointY, ForeGround, BackGround, Buffer, PrintNum);

  FreePool (Buffer);

  return ReturnNum;
}

/**
  Appends a formatted Unicode string to a Null-terminated Unicode string

  This function appends a formatted Unicode string to the Null-terminated
  Unicode string specified by String.   String is optional and may be NULL.
  Storage for the formatted Unicode string returned is allocated using
  AllocatePool().  The pointer to the appended string is returned.  The caller
  is responsible for freeing the returned string.

  If String is not NULL and not aligned on a 16-bit boundary, then ASSERT().
  If FormatString is NULL, then ASSERT().
  If FormatString is not aligned on a 16-bit boundary, then ASSERT().

  @param[in] String         A Null-terminated Unicode string.
  @param[in] FormatString   A Null-terminated Unicode format string.
  @param[in]  Marker        VA_LIST marker for the variable argument list.

  @retval NULL    There was not enough available memory.
  @return         Null-terminated Unicode string is that is the formatted
                  string appended to String.
**/
CHAR16*
EFIAPI
CatVSPrint (
  IN  CHAR16  *String, OPTIONAL
  IN  CONST CHAR16  *FormatString,
  IN  VA_LIST       Marker
  )
{
  UINTN   CharactersRequired;
  UINTN   SizeRequired;
  CHAR16  *BufferToReturn;
  VA_LIST ExtraMarker;

  VA_COPY (ExtraMarker, Marker);
  CharactersRequired = SPrintLength(FormatString, ExtraMarker);
  VA_END (ExtraMarker);

  if (String != NULL) {
    SizeRequired = StrSize(String) + (CharactersRequired * sizeof(CHAR16));
  } else {
    SizeRequired = sizeof(CHAR16) + (CharactersRequired * sizeof(CHAR16));
  }

  BufferToReturn = AllocatePool(SizeRequired);

  if (BufferToReturn == NULL) {
    return NULL;
  } else {
    BufferToReturn[0] = L'\0';
  }

  if (String != NULL) {
    StrCpyS(BufferToReturn, SizeRequired / sizeof(CHAR16), String);
  }

  UnicodeVSPrint(BufferToReturn + StrLen(BufferToReturn), (CharactersRequired+1) * sizeof(CHAR16), FormatString, Marker);

  ASSERT(StrSize(BufferToReturn)==SizeRequired);

  return (BufferToReturn);
}

/**
  Appends a formatted Unicode string to a Null-terminated Unicode string

  This function appends a formatted Unicode string to the Null-terminated
  Unicode string specified by String.   String is optional and may be NULL.
  Storage for the formatted Unicode string returned is allocated using
  AllocatePool().  The pointer to the appended string is returned.  The caller
  is responsible for freeing the returned string.

  If String is not NULL and not aligned on a 16-bit boundary, then ASSERT().
  If FormatString is NULL, then ASSERT().
  If FormatString is not aligned on a 16-bit boundary, then ASSERT().

  @param[in] String         A Null-terminated Unicode string.
  @param[in] FormatString   A Null-terminated Unicode format string.
  @param[in] ...            The variable argument list whose contents are
                            accessed based on the format string specified by
                            FormatString.

  @retval NULL    There was not enough available memory.
  @return         Null-terminated Unicode string is that is the formatted
                  string appended to String.
**/
CHAR16 *
EFIAPI
CatSPrint (
  IN  CHAR16  *String, OPTIONAL
  IN  CONST CHAR16  *FormatString,
  ...
  )
{
  VA_LIST   Marker;
  CHAR16    *NewString;

  VA_START (Marker, FormatString);
  NewString = CatVSPrint(String, FormatString, Marker);
  VA_END (Marker);
  return NewString;
}

