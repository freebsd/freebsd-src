/** @file
  The file provides services to access to images in the images database.

  Copyright (c) 2006 - 2018, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Revision Reference:
  This Protocol was introduced in UEFI Specification 2.1.

**/

#ifndef __HII_IMAGE_H__
#define __HII_IMAGE_H__

#include <Protocol/GraphicsOutput.h>

#define EFI_HII_IMAGE_PROTOCOL_GUID \
  { 0x31a6406a, 0x6bdf, 0x4e46, { 0xb2, 0xa2, 0xeb, 0xaa, 0x89, 0xc4, 0x9, 0x20 } }

typedef struct _EFI_HII_IMAGE_PROTOCOL EFI_HII_IMAGE_PROTOCOL;


///
/// Flags in EFI_IMAGE_INPUT
///
#define EFI_IMAGE_TRANSPARENT 0x00000001

/**

  Definition of EFI_IMAGE_INPUT.

  @param Flags  Describe image characteristics. If
                EFI_IMAGE_TRANSPARENT is set, then the image was
                designed for transparent display.

  @param Width  Image width, in pixels.

  @param Height Image height, in pixels.

  @param Bitmap A pointer to the actual bitmap, organized left-to-right,
                top-to-bottom. The size of the bitmap is
                Width*Height*sizeof(EFI_GRAPHICS_OUTPUT_BLT_PIXEL).


**/
typedef struct _EFI_IMAGE_INPUT {
  UINT32                          Flags;
  UINT16                          Width;
  UINT16                          Height;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL   *Bitmap;
} EFI_IMAGE_INPUT;


/**

  This function adds the image Image to the group of images
  owned by PackageList, and returns a new image identifier
  (ImageId).

  @param This        A pointer to the EFI_HII_IMAGE_PROTOCOL instance.

  @param PackageList Handle of the package list where this image will be added.

  @param ImageId     On return, contains the new image id, which is
                     unique within PackageList.

  @param Image       Points to the image.

  @retval EFI_SUCCESS             The new image was added
                                  successfully

  @retval EFI_OUT_OF_RESOURCES    Could not add the image.

  @retval EFI_INVALID_PARAMETER   Image is NULL or ImageId is
                                  NULL.


**/
typedef
EFI_STATUS
(EFIAPI *EFI_HII_NEW_IMAGE)(
  IN CONST  EFI_HII_IMAGE_PROTOCOL  *This,
  IN        EFI_HII_HANDLE          PackageList,
  OUT       EFI_IMAGE_ID            *ImageId,
  IN CONST  EFI_IMAGE_INPUT         *Image
);

/**

  This function retrieves the image specified by ImageId which
  is associated with the specified PackageList and copies it
  into the buffer specified by Image. If the image specified by
  ImageId is not present in the specified PackageList, then
  EFI_NOT_FOUND is returned. If the buffer specified by
  ImageSize is too small to hold the image, then
  EFI_BUFFER_TOO_SMALL will be returned. ImageSize will be
  updated to the size of buffer actually required to hold the
  image.

  @param This         A pointer to the EFI_HII_IMAGE_PROTOCOL instance.

  @param PackageList  The package list in the HII database to
                      search for the specified image.

  @param ImageId      The image's id, which is unique within
                      PackageList.

  @param Image        Points to the new image.

  @retval EFI_SUCCESS            The image was returned successfully.

  @retval EFI_NOT_FOUND          The image specified by ImageId is not
                                 available. Or The specified PackageList is not in the database.

  @retval EFI_INVALID_PARAMETER  The Image or Langugae was NULL.
  @retval EFI_OUT_OF_RESOURCES   The bitmap could not be retrieved because there was not
                                 enough memory.


**/
typedef
EFI_STATUS
(EFIAPI *EFI_HII_GET_IMAGE)(
  IN CONST  EFI_HII_IMAGE_PROTOCOL  *This,
  IN        EFI_HII_HANDLE          PackageList,
  IN        EFI_IMAGE_ID            ImageId,
  OUT       EFI_IMAGE_INPUT         *Image
);

/**

  This function updates the image specified by ImageId in the
  specified PackageListHandle to the image specified by Image.


  @param This         A pointer to the EFI_HII_IMAGE_PROTOCOL instance.

  @param PackageList  The package list containing the images.

  @param ImageId      The image id, which is unique within PackageList.

  @param Image        Points to the image.

  @retval EFI_SUCCESS           The image was successfully updated.

  @retval EFI_NOT_FOUND         The image specified by ImageId is not in the database.
                                The specified PackageList is not in the database.

  @retval EFI_INVALID_PARAMETER The Image or Language was NULL.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_HII_SET_IMAGE)(
  IN CONST  EFI_HII_IMAGE_PROTOCOL  *This,
  IN        EFI_HII_HANDLE          PackageList,
  IN        EFI_IMAGE_ID            ImageId,
  IN CONST  EFI_IMAGE_INPUT         *Image
);


///
/// EFI_HII_DRAW_FLAGS describes how the image is to be drawn.
/// These flags are defined as EFI_HII_DRAW_FLAG_***
///
typedef UINT32  EFI_HII_DRAW_FLAGS;

#define EFI_HII_DRAW_FLAG_CLIP          0x00000001
#define EFI_HII_DRAW_FLAG_TRANSPARENT   0x00000030
#define EFI_HII_DRAW_FLAG_DEFAULT       0x00000000
#define EFI_HII_DRAW_FLAG_FORCE_TRANS   0x00000010
#define EFI_HII_DRAW_FLAG_FORCE_OPAQUE  0x00000020
#define EFI_HII_DIRECT_TO_SCREEN        0x00000080

/**

  Definition of EFI_IMAGE_OUTPUT.

  @param Width  Width of the output image.

  @param Height Height of the output image.

  @param Bitmap Points to the output bitmap.

  @param Screen Points to the EFI_GRAPHICS_OUTPUT_PROTOCOL which
                describes the screen on which to draw the
                specified image.

**/
typedef struct _EFI_IMAGE_OUTPUT {
  UINT16  Width;
  UINT16  Height;
  union {
    EFI_GRAPHICS_OUTPUT_BLT_PIXEL *Bitmap;
    EFI_GRAPHICS_OUTPUT_PROTOCOL  *Screen;
  } Image;
} EFI_IMAGE_OUTPUT;


/**

  This function renders an image to a bitmap or the screen using
  the specified color and options. It draws the image on an
  existing bitmap, allocates a new bitmap or uses the screen. The
  images can be clipped. If EFI_HII_DRAW_FLAG_CLIP is set, then
  all pixels drawn outside the bounding box specified by Width and
  Height are ignored. If EFI_HII_DRAW_FLAG_TRANSPARENT is set,
  then all 'off' pixels in the images drawn will use the
  pixel value from Blt. This flag cannot be used if Blt is NULL
  upon entry. If EFI_HII_DIRECT_TO_SCREEN is set, then the image
  will be written directly to the output device specified by
  Screen. Otherwise the image will be rendered to the bitmap
  specified by Bitmap.


  @param This       A pointer to the EFI_HII_IMAGE_PROTOCOL instance.

  @param Flags      Describes how the image is to be drawn.
                    EFI_HII_DRAW_FLAGS is defined in Related
                    Definitions, below.

  @param Image      Points to the image to be displayed.

  @param Blt        If this points to a non-NULL on entry, this points
                    to the image, which is Width pixels wide and
                    Height pixels high. The image will be drawn onto
                    this image and EFI_HII_DRAW_FLAG_CLIP is implied.
                    If this points to a NULL on entry, then a buffer
                    will be allocated to hold the generated image and
                    the pointer updated on exit. It is the caller's
                    responsibility to free this buffer.

  @param BltX, BltY Specifies the offset from the left and top
                    edge of the image of the first pixel in
                    the image.

  @retval EFI_SUCCESS           The image was successfully updated.

  @retval EFI_OUT_OF_RESOURCES  Unable to allocate an output
                                buffer for RowInfoArray or Blt.

  @retval EFI_INVALID_PARAMETER The Image or Blt or Height or
                                Width was NULL.


**/
typedef
EFI_STATUS
(EFIAPI *EFI_HII_DRAW_IMAGE)(
  IN CONST  EFI_HII_IMAGE_PROTOCOL  *This,
  IN        EFI_HII_DRAW_FLAGS      Flags,
  IN CONST  EFI_IMAGE_INPUT         *Image,
  IN OUT    EFI_IMAGE_OUTPUT        **Blt,
  IN        UINTN                   BltX,
  IN        UINTN                   BltY
);

/**

  This function renders an image as a bitmap or to the screen and
  can clip the image. The bitmap is either supplied by the caller
  or else is allocated by the function. The images can be drawn
  transparently or opaquely. If EFI_HII_DRAW_FLAG_CLIP is set,
  then all pixels drawn outside the bounding box specified by
  Width and Height are ignored. If EFI_HII_DRAW_FLAG_TRANSPARENT
  is set, then all "off" pixels in the character's glyph will
  use the pixel value from Blt. This flag cannot be used if Blt
  is NULL upon entry. If EFI_HII_DIRECT_TO_SCREEN is set, then
  the image will be written directly to the output device
  specified by Screen. Otherwise the image will be rendered to
  the bitmap specified by Bitmap.
  This function renders an image to a bitmap or the screen using
  the specified color and options. It draws the image on an
  existing bitmap, allocates a new bitmap or uses the screen. The
  images can be clipped. If EFI_HII_DRAW_FLAG_CLIP is set, then
  all pixels drawn outside the bounding box specified by Width and
  Height are ignored. The EFI_HII_DRAW_FLAG_TRANSPARENT flag
  determines whether the image will be drawn transparent or
  opaque. If EFI_HII_DRAW_FLAG_FORCE_TRANS is set, then the image
  will be drawn so that all 'off' pixels in the image will
  be drawn using the pixel value from Blt and all other pixels
  will be copied. If EFI_HII_DRAW_FLAG_FORCE_OPAQUE is set, then
  the image's pixels will be copied directly to the
  destination. If EFI_HII_DRAW_FLAG_DEFAULT is set, then the image
  will be drawn transparently or opaque, depending on the
  image's transparency setting (see EFI_IMAGE_TRANSPARENT).
  Images cannot be drawn transparently if Blt is NULL. If
  EFI_HII_DIRECT_TO_SCREEN is set, then the image will be written
  directly to the output device specified by Screen. Otherwise the
  image will be rendered to the bitmap specified by Bitmap.

  @param This         A pointer to the EFI_HII_IMAGE_PROTOCOL instance.

  @param Flags        Describes how the image is to be drawn.

  @param PackageList  The package list in the HII database to
                      search for the specified image.

  @param ImageId      The image's id, which is unique within PackageList.

  @param Blt          If this points to a non-NULL on entry, this points
                      to the image, which is Width pixels wide and
                      Height pixels high. The image will be drawn onto
                      this image and EFI_HII_DRAW_FLAG_CLIP is implied.
                      If this points to a NULL on entry, then a buffer
                      will be allocated to hold the generated image and
                      the pointer updated on exit. It is the caller's
                      responsibility to free this buffer.

  @param BltX, BltY   Specifies the offset from the left and top
                      edge of the output image of the first
                      pixel in the image.

  @retval EFI_SUCCESS           The image was successfully updated.

  @retval EFI_OUT_OF_RESOURCES  Unable to allocate an output
                                buffer for RowInfoArray or Blt.

  @retval EFI_NOT_FOUND         The image specified by ImageId is not in the database.
                                Or The specified PackageList is not in the database.

  @retval EFI_INVALID_PARAMETER The Blt was NULL.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_HII_DRAW_IMAGE_ID)(
IN CONST  EFI_HII_IMAGE_PROTOCOL  *This,
IN        EFI_HII_DRAW_FLAGS      Flags,
IN        EFI_HII_HANDLE          PackageList,
IN        EFI_IMAGE_ID            ImageId,
IN OUT    EFI_IMAGE_OUTPUT        **Blt,
IN        UINTN                   BltX,
IN        UINTN                   BltY
);


///
/// Services to access to images in the images database.
///
struct _EFI_HII_IMAGE_PROTOCOL {
  EFI_HII_NEW_IMAGE     NewImage;
  EFI_HII_GET_IMAGE     GetImage;
  EFI_HII_SET_IMAGE     SetImage;
  EFI_HII_DRAW_IMAGE    DrawImage;
  EFI_HII_DRAW_IMAGE_ID DrawImageId;
};

extern EFI_GUID gEfiHiiImageProtocolGuid;

#endif


