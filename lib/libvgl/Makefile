# $FreeBSD: src/lib/libvgl/Makefile,v 1.6 2000/01/14 07:57:45 rgrimes Exp $
LIB=	vgl
SHLIB_MAJOR= 2
SHLIB_MINOR= 0
CFLAGS+=-Wall -I${.CURDIR}
SRCS=	main.c simple.c bitmap.c text.c mouse.c keyboard.c
INCS=	vgl.h
MAN3=	vgl.3
MLINKS+=	vgl.3 VGLBitmapCopy.3 \
		vgl.3 VGLBitmapPutChar.3 \
		vgl.3 VGLBitmapString.3 \
		vgl.3 VGLBlankDisplay.3 \
		vgl.3 VGLBox.3 \
		vgl.3 VGLCheckSwitch.3 \
		vgl.3 VGLClear.3 \
		vgl.3 VGLEllipse.3 \
		vgl.3 VGLEnd.3 \
		vgl.3 VGLFilledBox.3 \
		vgl.3 VGLFilledEllipse.3 \
		vgl.3 VGLInit.3 \
		vgl.3 VGLLine.3 \
		vgl.3 VGLMouseInit.3 \
		vgl.3 VGLMouseMode.3 \
		vgl.3 VGLMouseSetImage.3 \
		vgl.3 VGLMouseSetStdImage.3 \
		vgl.3 VGLMouseStatus.3 \
		vgl.3 VGLPanScreen.3 \
		vgl.3 VGLSetBorder.3 \
		vgl.3 VGLSetPalette.3 \
		vgl.3 VGLSetPaletteIndex.3 \
		vgl.3 VGLTextSetFontFile.3

.include <bsd.lib.mk>
