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
	mux \
	nxprtc \
	pcf8591 \
	rtc8583 \
	s35390a \
	smb \
	smbus \

.if !empty(OPT_FDT)
SUBDIR += tca6416 \
	  rx8803
.endif

.if ${MACHINE_CPUARCH} == "aarch64" || ${MACHINE_CPUARCH} == "amd64" || \
    ${MACHINE_CPUARCH} == "i386"
SUBDIR += iichid
.endif

.include <bsd.subdir.mk>
