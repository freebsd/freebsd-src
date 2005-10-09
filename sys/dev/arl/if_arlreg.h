/*-
 * Copyright (c) 1999-2001, Ivan Sharov, Vitaly Belekhov.
 * Copyright (c) 2004 Stanislav Svirid.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $RISS: if_arl/dev/arl/if_arlreg.h,v 1.4 2004/03/16 04:43:27 count Exp $
 * $FreeBSD$
 */

#ifndef _IF_ARLREG_H
#define _IF_ARLREG_H

#define ARL_BASE_START   0xC0000
#define ARL_BASE_END     0xDE000
#define ARL_BASE_STEP    0x2000

#define ARLAN_NAME_SIZE 16
#define ARLAN_NAME	"ArLan655-SCT"

/*
 * Statistics
 */
struct arl_stats {
	u_int32_t numDatagramsTransmitted;
	u_int32_t numReTransmissions;
	u_int32_t numFramesDiscarded;
	u_int32_t numDatagramsReceived;
	u_int32_t numDuplicateReceivedFrames;
	u_int32_t numDatagramsDiscarded;
	u_int16_t maxNumReTransmitDatagram;
	u_int16_t maxNumReTransmitFrames;
	u_int16_t maxNumConsecutiveDuplicateFrames;
	u_int32_t numBytesTransmitted;
	u_int32_t numBytesReceived;
	u_int32_t numCRCErrors;
	u_int32_t numLengthErrors;
	u_int32_t numAbortErrors;
	u_int32_t numTXUnderruns;
	u_int32_t numRXOverruns;
	u_int32_t numHoldOffs;
	u_int32_t numFramesTransmitted;
	u_int32_t numFramesReceived;
	u_int32_t numReceiveFramesLost;
	u_int32_t numRXBufferOverflows;
	u_int32_t numFramesDiscardedAddrMismatch;
	u_int32_t numFramesDiscardedSIDMismatch;
	u_int32_t numPollsTransmistted;
	u_int32_t numPollAcknowledges;
	u_int32_t numStatusVectorTimeouts;
	u_int32_t numNACKReceived;
} __attribute__((packed));

/*
 * Arlan private structure in memomory
 */
struct arl_private {
	/* Header Signature */
	char			textRegion[48];
	u_int8_t		resetFlag;
	u_int8_t		diagnosticInfo;
	u_int16_t		diagnosticOffset;
	u_int8_t		_1[12];
	u_int8_t		lanCardNodeId[6];
	u_int8_t		broadcastAddress[6];
	u_int8_t		hardwareType;
	u_int8_t		majorHardwareVersion;
	u_int8_t		minorHardwareVersion;
	u_int8_t		radioModule;
	u_int8_t		defaultChannelSet;
	u_int8_t		_2[47];

	/* Control/Status Block - 0x0080 */
	u_int8_t		interruptInProgress;
	u_int8_t		cntrlRegImage;
	u_int8_t		_3[14];
	u_int8_t		commandByte;
	u_int8_t		commandParameter[15];

	/* Receive Status - 0x00a0 */
	u_int8_t		rxStatusVector;
	u_int8_t		rxFrmType;
	u_int16_t		rxOffset;
	u_int16_t		rxLength;
	u_int8_t		rxSrc[6];
	u_int8_t		rxBroadcastFlag;
	u_int8_t		rxQuality;
	u_int8_t		scrambled;
	u_int8_t		_4[1];

	/* Transmit Status - 0x00b0 */
	u_int8_t		txStatusVector;
	u_int8_t		txAckQuality;
	u_int8_t		numRetries;
	u_int8_t		_5[14];
	u_int8_t		registeredRouter[6];
	u_int8_t		backboneRouter[6];
	u_int8_t		registrationStatus;
	u_int8_t		configuredStatusFlag;
	u_int8_t		_6[1];
	u_int8_t		ultimateDestAddress[6];
	u_int8_t		immedDestAddress[6];
	u_int8_t		immedSrcAddress[6];
	u_int16_t		rxSequenceNumber;
	u_int8_t		assignedLocaltalkAddress;
	u_int8_t		_7[27];

	/* System Parameter Block */

	/* - Driver Parameters (Novell Specific) */

	u_int16_t		txTimeout;
	u_int16_t		transportTime;
	u_int8_t		_8[4];

	/* - Configuration Parameters */
	u_int8_t		irqLevel;
	u_int8_t		spreadingCode;
	u_int8_t		channelSet;
	u_int8_t		channelNumber;
	u_int16_t		radioNodeId;
	u_int8_t		_9[2];
	u_int8_t		scramblingDisable;
	u_int8_t		radioType;
	u_int16_t		routerId;
	u_int8_t		_10[9];
	u_int8_t		txAttenuation;
	u_int8_t		systemId[4]; /* on an odd address for a long !!! */
	u_int16_t		globalChecksum;
	u_int8_t		_11[4];
	u_int16_t		maxDatagramSize;
	u_int16_t		maxFrameSize;
	u_int8_t		maxRetries;
	u_int8_t		receiveMode;
	u_int8_t		priority;
	u_int8_t		rootOrRepeater;
	u_int8_t		specifiedRouter[6];
	u_int16_t		fastPollPeriod;
	u_int8_t		pollDecay;
	u_int8_t		fastPollDelay[2];
	u_int8_t		arlThreshold;
	u_int8_t		arlDecay;
	u_int8_t		_12[1];
	u_int16_t		specRouterTimeout;
	u_int8_t		_13[5];

	/* Scrambled Area */
	u_int8_t		SID[4];
	u_int8_t		encryptionKey[12];
	u_int8_t		_14[2];
	u_int8_t		waitTime[2];
	u_int8_t		lParameter[2];
	u_int8_t		_15[3];
	u_int16_t		headerSize;
	u_int16_t		sectionChecksum;

	u_int8_t		registrationMode;
	u_int8_t		registrationFill;
	u_int16_t		pollPeriod;
	u_int16_t		refreshPeriod;
	u_int8_t		name[ARLAN_NAME_SIZE];
	u_int8_t		NID[6];
	u_int8_t		localTalkAddress;
	u_int8_t		codeFormat;
	u_int8_t		SSCode[64];

	u_int8_t		_16[0x140];

	/* Statistics Block - 0x0300 */
	u_int8_t		hostcpuLock;
	u_int8_t		lancpuLock;
	u_int8_t		resetTime[18];

	struct arl_stats	stat;

	u_int8_t		_17[0x86];

	u_int8_t		txBuffer[0x800];
	u_int8_t		rxBuffer[0x800];

	u_int8_t		_18[0x0bfd];
	u_int8_t		resetFlag1;
	u_int8_t		_19;
	u_int8_t		controlRegister;
};

/*
 * Transmit parametrs
 */
struct arl_tx_param {
	u_int16_t	offset;
	u_int16_t	length;
	u_int8_t	dest[6];
	u_int8_t	clear;
	u_int8_t	retries;
	u_int8_t	routing;
	u_int8_t	scrambled;
};

#define ARL_HARDWARE_RESET		0x01
#define ARL_CHANNEL_ATTENTION		0x02
#define ARL_INTERRUPT_ENABLE		0x04
#define ARL_CLEAR_INTERRUPT		0x08

/* additions for sys/sockio.h ( socket ioctl parameters for arlan card ) */

#define SIOCGARLQLT	_IOWR('i', 70, struct ifreq)	/* get QUALITY */
#define SIOCGARLALL	_IOWR('i', 71, struct ifreq)	/* get ALL */
#define SIOCSARLALL	_IOWR('i', 72, struct ifreq)	/* set paramter (who_set) */
#define SIOCGARLSTB	_IOWR('i', 73, struct ifreq)	/* get statistic block */

/*
 * Arlan request struct via ioctl
 */
struct arl_cfg_param {
	u_char		name[ARLAN_NAME_SIZE];
	u_int8_t	sid[4];
	u_int8_t	channelSet;
	u_int8_t	channelNumber;
	u_int8_t	spreadingCode;
	u_int8_t	registrationMode;
	u_int8_t	lanCardNodeId[6];
	u_int8_t	specifiedRouter[6];
	u_int8_t	hardwareType;
	u_int8_t	majorHardwareVersion;
	u_int8_t	minorHardwareVersion;
	u_int8_t	radioModule;
	u_int8_t	priority;
	u_int8_t	receiveMode;
	u_int8_t	txRetry;
};

struct arl_req {
	u_int32_t		what_set;
	struct arl_cfg_param	cfg;
};

#ifdef ARLCACHE
#define MAXARLCACHE	16
#define ARLCACHE_RX 0
#define ARLCACHE_TX 1

struct arl_sigcache {
	u_int8_t	macsrc[6];	/* unique MAC address for entry */
	u_int8_t	level[2];
	u_int8_t	quality[2];
};
#endif

#define	ARLAN_SET_name			0x0001
#define	ARLAN_SET_sid			0x0002
#define	ARLAN_SET_channelSet		0x0004
#define	ARLAN_SET_channelNumber		0x0008
#define	ARLAN_SET_spreadingCode		0x0010
#define	ARLAN_SET_registrationMode	0x0020
#define	ARLAN_SET_lanCardNodeId		0x0040
#define ARLAN_SET_specifiedRouter	0x0080
#define ARLAN_SET_priority		0x0100
#define ARLAN_SET_receiveMode		0x0200
#define ARLAN_SET_txRetry		0x0400

#ifdef _KERNEL
struct arl_softc {
	struct ifnet		*arl_ifp;

	int			arl_unit;
	struct arl_private *	arl_mem;	/* arlan data */

	struct arl_cfg_param	arl_cfg;	/* arlan vars in our mem */
	u_char			arl_control;

	int	mem_rid;		/* resource id for mem */
	struct resource* mem_res;	/* resource for mem */
	int	irq_rid;		/* resource id for irq */
	struct resource* irq_res;	/* resource for irq */
	void*	irq_handle;		/* handle for irq handler */

	u_char	arl_tx[2048];
	int	tx_len;
	u_char	arl_rx[2048];
	int	rx_len;

#ifdef ARLCACHE
	struct arl_sigcache	arl_sigcache[MAXARLCACHE];
#endif
	struct ifmedia		arl_ifmedia;
};
#endif

#define	ARLAN_SIGN		"TELESYSTEM"
#define ARLAN_HEADER_SIZE	0x0C

#define ar	sc->arl_mem
#define arcfg	sc->arl_cfg

#define ARDELAY		10000
#define	ARDELAY1	50000

#define WAIT_RESET(cnt, delay) \
	do { \
		int i; \
		for (i = cnt; i && ar->resetFlag; i--) { \
			DELAY(delay); \
		} \
	} while (0);

#ifdef _KERNEL
void	arl_release_resources	(device_t);
int	arl_alloc_memory	(device_t, int, int);
int	arl_alloc_irq		(device_t, int, int);
int	arl_attach		(device_t);
int	arl_wait_reset		(struct arl_softc *, int, int);
void	arl_stop		(struct arl_softc *);

driver_intr_t	arl_intr;
#endif

#endif /* _IF_ARLREG_H */
