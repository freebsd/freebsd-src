/*-
 * Copyright (c) 2007-2011 Broadcom Corporation. All rights reserved.
 *
 *    Gary Zambrano <zambrano@broadcom.com>
 *    David Christensen <davidch@broadcom.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of Broadcom Corporation nor the name of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written consent.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS'
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

 /*$FreeBSD$*/

#ifndef _BXE_DEBUG_H
#define	_BXE_DEBUG_H

extern uint32_t bxe_debug;

/*
 * Debugging macros and definitions.
 */

#define	BXE_CP_LOAD		0x00000001
#define	BXE_CP_SEND		0x00000002
#define	BXE_CP_RECV		0x00000004
#define	BXE_CP_INTR		0x00000008
#define	BXE_CP_UNLOAD		0x00000010
#define	BXE_CP_RESET		0x00000020
#define	BXE_CP_IOCTL		0x00000040
#define	BXE_CP_STATS		0x00000080
#define	BXE_CP_MISC		0x00000100
#define	BXE_CP_PHY		0x00000200
#define	BXE_CP_RAMROD		0x00000400
#define	BXE_CP_NVRAM		0x00000800
#define	BXE_CP_REGS		0x00001000
#define	BXE_CP_ALL		0x00FFFFFF
#define	BXE_CP_MASK		0x00FFFFFF

#define BXE_LEVEL_FATAL			0x00000000
#define BXE_LEVEL_WARN			0x01000000
#define BXE_LEVEL_INFO			0x02000000
#define BXE_LEVEL_VERBOSE		0x03000000
#define BXE_LEVEL_EXTREME		0x04000000
#define BXE_LEVEL_INSANE		0x05000000

#define BXE_LEVEL_MASK			0xFF000000

#define BXE_WARN_LOAD			(BXE_CP_LOAD | BXE_LEVEL_WARN)
#define BXE_INFO_LOAD			(BXE_CP_LOAD | BXE_LEVEL_INFO)
#define BXE_VERBOSE_LOAD		(BXE_CP_LOAD | BXE_LEVEL_VERBOSE)
#define BXE_EXTREME_LOAD		(BXE_CP_LOAD | BXE_LEVEL_EXTREME)
#define BXE_INSANE_LOAD			(BXE_CP_LOAD | BXE_LEVEL_INSANE)

#define BXE_WARN_SEND			(BXE_CP_SEND | BXE_LEVEL_WARN)
#define BXE_INFO_SEND			(BXE_CP_SEND | BXE_LEVEL_INFO)
#define BXE_VERBOSE_SEND		(BXE_CP_SEND | BXE_LEVEL_VERBOSE)
#define BXE_EXTREME_SEND		(BXE_CP_SEND | BXE_LEVEL_EXTREME)
#define BXE_INSANE_SEND			(BXE_CP_SEND | BXE_LEVEL_INSANE)

#define BXE_WARN_RECV			(BXE_CP_RECV | BXE_LEVEL_WARN)
#define BXE_INFO_RECV			(BXE_CP_RECV | BXE_LEVEL_INFO)
#define BXE_VERBOSE_RECV		(BXE_CP_RECV | BXE_LEVEL_VERBOSE)
#define BXE_EXTREME_RECV		(BXE_CP_RECV | BXE_LEVEL_EXTREME)
#define BXE_INSANE_RECV			(BXE_CP_RECV | BXE_LEVEL_INSANE)

#define BXE_WARN_INTR			(BXE_CP_INTR | BXE_LEVEL_WARN)
#define BXE_INFO_INTR			(BXE_CP_INTR | BXE_LEVEL_INFO)
#define BXE_VERBOSE_INTR		(BXE_CP_INTR | BXE_LEVEL_VERBOSE)
#define BXE_EXTREME_INTR		(BXE_CP_INTR | BXE_LEVEL_EXTREME)
#define BXE_INSANE_INTR			(BXE_CP_INTR | BXE_LEVEL_INSANE)

#define BXE_WARN_UNLOAD			(BXE_CP_UNLOAD | BXE_LEVEL_WARN)
#define BXE_INFO_UNLOAD			(BXE_CP_UNLOAD | BXE_LEVEL_INFO)
#define BXE_VERBOSE_UNLOAD		(BXE_CP_UNLOAD | BXE_LEVEL_VERBOSE)
#define BXE_EXTREME_UNLOAD		(BXE_CP_UNLOAD | BXE_LEVEL_EXTREME)
#define BXE_INSANE_UNLOAD		(BXE_CP_UNLOAD | BXE_LEVEL_INSANE)

#define BXE_WARN_RESET			(BXE_CP_RESET | BXE_LEVEL_WARN)
#define BXE_INFO_RESET			(BXE_CP_RESET | BXE_LEVEL_INFO)
#define BXE_VERBOSE_RESET		(BXE_CP_RESET | BXE_LEVEL_VERBOSE)
#define BXE_EXTREME_RESET		(BXE_CP_RESET | BXE_LEVEL_EXTREME)
#define BXE_INSANE_RESET		(BXE_CP_RESET | BXE_LEVEL_INSANE)

#define BXE_WARN_IOCTL			(BXE_CP_IOCTL | BXE_LEVEL_WARN)
#define BXE_INFO_IOCTL			(BXE_CP_IOCTL | BXE_LEVEL_INFO)
#define BXE_VERBOSE_IOCTL		(BXE_CP_IOCTL | BXE_LEVEL_VERBOSE)
#define BXE_EXTREME_IOCTL		(BXE_CP_IOCTL | BXE_LEVEL_EXTREME)
#define BXE_INSANE_IOCTL		(BXE_CP_IOCTL | BXE_LEVEL_INSANE)

#define BXE_WARN_STATS			(BXE_CP_STATS | BXE_LEVEL_WARN)
#define BXE_INFO_STATS			(BXE_CP_STATS | BXE_LEVEL_INFO)
#define BXE_VERBOSE_STATS		(BXE_CP_STATS | BXE_LEVEL_VERBOSE)
#define BXE_EXTREME_STATS		(BXE_CP_STATS | BXE_LEVEL_EXTREME)
#define BXE_INSANE_STATS		(BXE_CP_STATS | BXE_LEVEL_INSANE)

#define BXE_WARN_MISC			(BXE_CP_MISC | BXE_LEVEL_WARN)
#define BXE_INFO_MISC			(BXE_CP_MISC | BXE_LEVEL_INFO)
#define BXE_VERBOSE_MISC		(BXE_CP_MISC | BXE_LEVEL_VERBOSE)
#define BXE_EXTREME_MISC		(BXE_CP_MISC | BXE_LEVEL_EXTREME)
#define BXE_INSANE_MISC			(BXE_CP_MISC | BXE_LEVEL_INSANE)

#define BXE_WARN_PHY			(BXE_CP_PHY | BXE_LEVEL_WARN)
#define BXE_INFO_PHY			(BXE_CP_PHY | BXE_LEVEL_INFO)
#define BXE_VERBOSE_PHY			(BXE_CP_PHY | BXE_LEVEL_VERBOSE)
#define BXE_EXTREME_PHY			(BXE_CP_PHY | BXE_LEVEL_EXTREME)
#define BXE_INSANE_PHY			(BXE_CP_PHY | BXE_LEVEL_INSANE)

#define BXE_WARN_RAMROD			(BXE_CP_RAMROD | BXE_LEVEL_WARN)
#define BXE_INFO_RAMROD			(BXE_CP_RAMROD | BXE_LEVEL_INFO)
#define BXE_VERBOSE_RAMROD		(BXE_CP_RAMROD | BXE_LEVEL_VERBOSE)
#define BXE_EXTREME_RAMROD		(BXE_CP_RAMROD | BXE_LEVEL_EXTREME)
#define BXE_INSANE_RAMROD		(BXE_CP_RAMROD | BXE_LEVEL_INSANE)

#define BXE_WARN_NVRAM			(BXE_CP_NVRAM | BXE_LEVEL_WARN)
#define BXE_INFO_NVRAM			(BXE_CP_NVRAM | BXE_LEVEL_INFO)
#define BXE_VERBOSE_NVRAM		(BXE_CP_NVRAM | BXE_LEVEL_VERBOSE)
#define BXE_EXTREME_NVRAM		(BXE_CP_NVRAM | BXE_LEVEL_EXTREME)
#define BXE_INSANE_NVRAM		(BXE_CP_NVRAM | BXE_LEVEL_INSANE)

#define BXE_WARN_REGS			(BXE_CP_REGS | BXE_LEVEL_WARN)
#define BXE_INFO_REGS			(BXE_CP_REGS | BXE_LEVEL_INFO)
#define BXE_VERBOSE_REGS		(BXE_CP_REGS | BXE_LEVEL_VERBOSE)
#define BXE_EXTREME_REGS		(BXE_CP_REGS | BXE_LEVEL_EXTREME)
#define BXE_INSANE_REGS			(BXE_CP_REGS | BXE_LEVEL_INSANE)

#define BXE_FATAL				(BXE_CP_ALL | BXE_LEVEL_FATAL)
#define BXE_WARN				(BXE_CP_ALL | BXE_LEVEL_WARN)
#define BXE_INFO				(BXE_CP_ALL | BXE_LEVEL_INFO)
#define BXE_VERBOSE				(BXE_CP_ALL | BXE_LEVEL_VERBOSE)
#define BXE_EXTREME				(BXE_CP_ALL | BXE_LEVEL_EXTREME)
#define BXE_INSANE				(BXE_CP_ALL | BXE_LEVEL_INSANE)

#define BXE_CODE_PATH(cp)		((cp & BXE_CP_MASK) & bxe_debug)
#define BXE_MSG_LEVEL(lv)		((lv & BXE_LEVEL_MASK) <= (bxe_debug & BXE_LEVEL_MASK))
#define BXE_LOG_MSG(m)			(BXE_CODE_PATH(m) && BXE_MSG_LEVEL(m))


#ifdef BXE_DEBUG

/* Print a message based on the logging level and code path. */
#define DBPRINT(sc, level, format, args...)				\
	do {								\
		if (BXE_LOG_MSG(level)) {				\
			device_printf(sc->dev, format, ## args);	\
		}							\
	} while (0)

/* Runs a particular command when debugging is enabled. */
#define DBRUN(args...)							\
	do {								\
		args;							\
	} while (0)

/* Runs a particular command based on the logging level. */
#define DBRUNLV(level, args...) 					\
	if (BXE_MSG_LEVEL(level)) { 					\
		args; 							\
	}

/* Runs a particular command based on the code path. */
#define DBRUNCP(cp, args...) 						\
	if (BXE_CODE_PATH(cp)) { 					\
		args; 							\
	}

/* Runs a particular command based on a condition. */
#define DBRUNIF(cond, args...)						\
	if (cond) {							\
		args;							\
	}

/* Runs a particular command based on the logging level and code path. */
#define DBRUNMSG(msg, args...)						\
	if (BXE_LOG_MSG(msg)) {						\
		args;							\
	}

/* Announces function entry. */
#define DBENTER(cond)							\
	DBPRINT(sc, (cond), "%s(enter:%d)\n", __FUNCTION__, curcpu)

/* Announces function exit. */
#define DBEXIT(cond)							\
	DBPRINT(sc, (cond), "%s(exit:%d)\n", __FUNCTION__, curcpu)

/* Needed for random() function which is only used in debugging. */
#include <sys/random.h>

/* Returns FALSE in "defects" per 2^31 - 1 calls, otherwise returns TRUE. */
#define DB_RANDOMFALSE(defects)        (random() > defects)
#define DB_OR_RANDOMFALSE(defects)  || (random() > defects)
#define DB_AND_RANDOMFALSE(defects) && (random() > ddfects)

/* Returns TRUE in "defects" per 2^31 - 1 calls, otherwise returns FALSE. */
#define DB_RANDOMTRUE(defects)         (random() < defects)
#define DB_OR_RANDOMTRUE(defects)   || (random() < defects)
#define DB_AND_RANDOMTRUE(defects)  && (random() < defects)

#else

#define DBPRINT(...)
#define DBRUN(...)
#define DBRUNLV(...)
#define DBRUNCP(...)
#define DBRUNIF(...)
#define DBRUNMSG(...)
#define DBENTER(...)
#define DBENTER_UNLOCKED(...)
#define DBEXIT(...)
#define DBEXIT_UNLOCKED(...)
#define DB_RANDOMFALSE(...)
#define DB_OR_RANDOMFALSE(...)
#define DB_AND_RANDOMFALSE(...)
#define DB_RANDOMTRUE(...)
#define DB_OR_RANDOMTRUE(...)
#define DB_AND_RANDOMTRUE(...)

#endif /* BXE_DEBUG */

/* Generic bit decoding for printf("%b"). */
#define BXE_DWORD_PRINTFB	\
	"\020"					\
	"\40b31"				\
	"\37b30"				\
	"\36b29"				\
	"\35b28"				\
	"\34b27"				\
	"\33b26"				\
	"\32b25"				\
	"\31b24"				\
	"\30b23"				\
	"\27b22"				\
	"\26b21"				\
	"\25b20"				\
	"\24b19"				\
	"\23b18"				\
	"\22b17"				\
	"\21b16"				\
	"\20b15"				\
	"\17b14"				\
	"\16b13"				\
	"\15b12"				\
	"\14b11"				\
	"\13b10"				\
	"\12b9"					\
	"\11b8"					\
	"\10b7"					\
	"\07b6"					\
	"\06b5"					\
	"\05b4"					\
	"\04b3"					\
	"\03b2"					\
	"\02b1"					\
	"\01b0"

/* Supported link settings bit decoding for printf("%b"). */
#define BXE_SUPPORTED_PRINTFB	\
	"\020"						\
	"\040b31"					\
	"\037b30"					\
	"\036b29"					\
	"\035b28"					\
	"\034b27"					\
	"\033b26"					\
	"\032b25"					\
	"\031b24"					\
	"\030b23"					\
	"\027b22"					\
	"\026b21"					\
	"\025b20"					\
	"\024b19"					\
	"\023b18"					\
	"\022b17"					\
	"\02110000BaseT-Full"		\
	"\0202500BaseX-Full"  		\
	"\017b14"					\
	"\016b13"					\
	"\015b12"					\
	"\014Pause"					\
	"\013Asym-Pause" 			\
	"\012Autoneg"				\
	"\011Fiber"					\
	"\010TP"			  		\
	"\0071000BaseT-Full"  		\
	"\0061000BaseT-Half"  		\
	"\005100BaseTX-Full"  		\
	"\004100BaseTX-Half"  		\
	"\00310BaseT-Full"			\
	"\00210BaseT-Half"			\
	"\001b0"

/* Transmit BD TCP flags bit decoding for printf("%b"). */
#define BXE_ETH_TX_PARSE_BD_TCP_FLAGS_PRINTFB	\
	"\020"										\
	"\10CWR"									\
	"\07ECE"									\
	"\06URG"									\
	"\05ACK"									\
	"\04PSH"									\
	"\03RST"									\
	"\02SYN"									\
	"\01FIN"

/* Parsing BD global data bit decoding for printf("%b"). */
#define BXE_ETH_TX_PARSE_BD_GLOBAL_DATA_PRINTFB	\
	"\020"										\
	"\10NS"										\
	"\07LLC_SNAP"								\
	"\06PSEUDO_CS_WO_LEN"						\
	"\05CS_ANY"

/* Transmit BD flags bit decoding for printf("%b"). */
#define BXE_ETH_TX_BD_FLAGS_PRINTFB				\
	"\020"										\
	"\10IPv6"									\
	"\07LSO"									\
	"\06HDR_POOL" 								\
	"\05START"									\
	"\04END"									\
	"\03TCP_CSUM" 								\
	"\02IP_CSUM" 								\
	"\01VLAN"

/* Receive CQE error flags bit decoding for printf("%b"). */
#define BXE_ETH_FAST_PATH_RX_CQE_ERROR_FLAGS_PRINTFB	\
	"\020"												\
	"\10RSRVD"											\
	"\07RSRVD"											\
	"\06END_FLAG" 										\
	"\05START_FLAG"										\
	"\04L4_BAD_XSUM"									\
	"\03IP_BAD_XSUM"									\
	"\02PHY_DECODE_ERR"									\
	"\01SP"

#endif /* _BXE_DEBUG_H */
