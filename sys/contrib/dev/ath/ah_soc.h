/*-
 * Copyright (c) 2006-2008 Sam Leffler, Errno Consulting, Atheros
 * Communications, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the following conditions are met:
 * 1. The materials contained herein are unmodified and are used
 *    unmodified.
 * 2. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following NO
 *    ''WARRANTY'' disclaimer below (''Disclaimer''), without
 *    modification.
 * 3. Redistributions in binary form must reproduce at minimum a
 *    disclaimer similar to the Disclaimer below and any redistribution
 *    must be conditioned upon including a substantially similar
 *    Disclaimer requirement for further binary redistribution.
 * 4. Neither the names of the above-listed copyright holders nor the
 *    names of any contributors may be used to endorse or promote
 *    product derived from this software without specific prior written
 *    permission.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ''AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF NONINFRINGEMENT,
 * MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE
 * FOR SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGES.
 *
 * $Id: //depot/sw/branches/sam_hal/ah_soc.h#4 $
 */
#ifndef _ATH_AH_SOC_H_
#define _ATH_AH_SOC_H_
/*
 * Atheros System on Chip (SoC) public definitions.
 */

/*
 * This is board-specific data that is stored in a "known"
 * location in flash.  To find the start of this data search
 * back from the (aliased) end of flash by 0x1000 bytes at a
 * time until you find the string "5311", which marks the
 * start of Board Configuration.  Typically one gives up if
 * more than 500KB is searched.
 */
struct ar531x_boarddata {
	u_int32_t magic;             /* board data is valid */
#define AR531X_BD_MAGIC 0x35333131   /* "5311", for all 531x platforms */
	u_int16_t cksum;             /* checksum (starting with BD_REV 2) */
	u_int16_t rev;               /* revision of this struct */
#define BD_REV  4
	char   boardName[64];        /* Name of board */
	u_int16_t major;             /* Board major number */
	u_int16_t minor;             /* Board minor number */
	u_int32_t config;            /* Board configuration */
#define BD_ENET0        0x00000001   /* ENET0 is stuffed */
#define BD_ENET1        0x00000002   /* ENET1 is stuffed */
#define BD_UART1        0x00000004   /* UART1 is stuffed */
#define BD_UART0        0x00000008   /* UART0 is stuffed (dma) */
#define BD_RSTFACTORY   0x00000010   /* Reset factory defaults stuffed */
#define BD_SYSLED       0x00000020   /* System LED stuffed */
#define BD_EXTUARTCLK   0x00000040   /* External UART clock */
#define BD_CPUFREQ      0x00000080   /* cpu freq is valid in nvram */
#define BD_SYSFREQ      0x00000100   /* sys freq is set in nvram */
#define BD_WLAN0        0x00000200   /* Enable WLAN0 */
#define BD_MEMCAP       0x00000400   /* CAP SDRAM @ memCap for testing */
#define BD_DISWATCHDOG  0x00000800   /* disable system watchdog */
#define BD_WLAN1        0x00001000   /* Enable WLAN1 (ar5212) */
#define BD_ISCASPER     0x00002000   /* FLAG for AR2312 */
#define BD_WLAN0_2G_EN  0x00004000   /* FLAG for radio0_2G */
#define BD_WLAN0_5G_EN  0x00008000   /* FLAG for radio0_2G */
#define BD_WLAN1_2G_EN  0x00020000   /* FLAG for radio0_2G */
#define BD_WLAN1_5G_EN  0x00040000   /* FLAG for radio0_2G */
	u_int16_t resetConfigGpio;   /* Reset factory GPIO pin */
	u_int16_t sysLedGpio;        /* System LED GPIO pin */
	
	u_int32_t cpuFreq;           /* CPU core frequency in Hz */
	u_int32_t sysFreq;           /* System frequency in Hz */
	u_int32_t cntFreq;           /* Calculated C0_COUNT frequency */
	
	u_int8_t  wlan0Mac[6];
	u_int8_t  enet0Mac[6];
	u_int8_t  enet1Mac[6];
	
	u_int16_t pciId;             /* Pseudo PCIID for common code */
	u_int16_t memCap;            /* cap bank1 in MB */
	
	/* version 3 */
	u_int8_t  wlan1Mac[6];       /* (ar5212) */
};

/*
 * Board support data.  The driver is required to locate
 * and fill-in this information before passing a reference to
 * this structure as the HAL_BUS_TAG parameter supplied to
 * ath_hal_attach.
 */
struct ar531x_config {
	const struct ar531x_boarddata *board;	/* board config data */
	const char	*radio;			/* radio config data */
	int		unit;			/* unit number [0, 1] */
	void		*tag;			/* bus space tag */
};
#endif	/* _ATH_AH_SOC_H_ */
