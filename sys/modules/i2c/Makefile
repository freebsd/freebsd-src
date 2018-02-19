# $FreeBSD$

SUBDIR = \
	controllers \
	cyapa \
	ds1307 \
	ds13rtc \
	ds3231 \
	icee \
	if_ic \
	iic \
	iicbb \
	iicbus \
	iicsmb \
	isl \
	isl12xx \
	jedec_ts \
	nxprtc \
	s35390a \
	smb \
	smbus \

.if !empty(OPT_FDT)
SUBDIR += ofw_iicbus
.endif

.include <bsd.subdir.mk>
