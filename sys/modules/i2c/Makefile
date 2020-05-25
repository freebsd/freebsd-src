# $FreeBSD$

SUBDIR = \
	ads111x \
	controllers \
	cyapa \
	ds1307 \
	ds13rtc \
	ds1672 \
	ds3231 \
	icee \
	if_ic \
	iic \
	iicbb \
	iicbus \
	iicsmb \
	isl \
	isl12xx \
	jedec_dimm \
	mux \
	nxprtc \
	rtc8583 \
	s35390a \
	smb \
	smbus \

.if !empty(OPT_FDT)
SUBDIR += tca6416
.endif

.include <bsd.subdir.mk>
