#
# Copyright (c) 2002, 2003 Sam Leffler, Errno Consulting, Atheros
# Communications, Inc.  All rights reserved.
#
# Redistribution and use in source and binary forms are permitted
# provided that the following conditions are met:
# 1. The materials contained herein are unmodified and are used
#    unmodified.
# 2. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following NO
#    ''WARRANTY'' disclaimer below (''Disclaimer''), without
#    modification.
# 3. Redistributions in binary form must reproduce at minimum a
#    disclaimer similar to the Disclaimer below and any redistribution
#    must be conditioned upon including a substantially similar
#    Disclaimer requirement for further binary redistribution.
# 4. Neither the names of the above-listed copyright holders nor the
#    names of any contributors may be used to endorse or promote
#    product derived from this software without specific prior written
#    permission.
#
# NO WARRANTY
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# ''AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF NONINFRINGEMENT,
# MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
# IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE
# FOR SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
# USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
# OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGES.
#
# $Id: ah_if.m,v 1.4 2003/06/25 04:55:02 sam Exp $
#

INTERFACE ath_hal;

METHOD	const char* ath_hal_probe {
	u_int16_t	vendorID;
	u_int16_	deviceID;
};

METHOD	struct ath_hal* ath_hal_attach {
	u_int16_t	deviceID;
	HAL_SOFTC	sc;
	HAL_BUS_TAG	st;
	HAL_BUS_HANDLE	sh;
	HAL_STATUS*	error;
};

METHOD u_int ath_hal_init_channels {
	struct ath_hal*	ah;
	HAL_CHANNEL*	chans;
	u_int		maxchans;
	u_int*		nchans;
	HAL_CTRY_CODE	cc;
	u_int16_t	modeSelect;
	int		enableOutdoor;
};

METHOD u_int ath_hal_getwirelessmodes {
	struct ath_hal*	ah;
	HAL_CTRY_CODE	cc;
};

METHOD const HAL_RATE_TABLE* ath_hal_getratetable {
	struct ath_hal*	ah;
	u_int		mode;
};

METHOD u_int16_t ath_hal_computetxtime {
	struct ath_hal*	ah;
	const HAL_RATE_TABLE* rates;
	u_int32_t	frameLength;
	u_int16_t	rateIndex;
	HAL_BOOL	shortPreamble;
};

METHOD u_int ath_hal_mhz2ieee {
	u_int		mhz;
	u_int		flags;
};

METHOD u_int ath_hal_ieee2mhz {
	u_int		ieee;
	u_int		flags;
};
