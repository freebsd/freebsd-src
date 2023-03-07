# $FreeBSD$

SUBDIR = \
	ads111x \
	controllers \
	cyapa \
	ds1307 \
	ds13rtc \
	ds1672 \
	ds3231 \
	htu21 \
	icee \
	if_ic \
	iic \
	iicbb \
	iicbus \
	iicsmb \
	isl \
	isl12xx \
	jedec_dimm \
	max44009 \
	mux \
	nxprtc \
	pcf8574 \
	pcf8591 \
	rtc8583 \
	s35390a \
	smb \
	smbus \

.if !empty(OPT_FDT)
SUBDIR += rx8803 \
	  tca64xx \
	  tmp461
.endif

.if ${MACHINE_CPUARCH} == "aarch64" || ${MACHINE_CPUARCH} == "amd64" || \
    ${MACHINE_CPUARCH} == "i386"
SUBDIR += iichid
.endif

.include <bsd.subdir.mk>
