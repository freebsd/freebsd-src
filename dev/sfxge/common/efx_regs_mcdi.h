/*-
 * Copyright 2008-2011 Solarflare Communications Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef _SIENA_MC_DRIVER_PCOL_H
#define	_SIENA_MC_DRIVER_PCOL_H


/* Values to be written into FMCR_CZ_RESET_STATE_REG to control boot. */
/* Power-on reset state */
#define MC_FW_STATE_POR (1)
/* If this is set in MC_RESET_STATE_REG then it should be
 * possible to jump into IMEM without loading code from flash. */
#define MC_FW_WARM_BOOT_OK (2)
/* The MC main image has started to boot. */
#define MC_FW_STATE_BOOTING (4)
/* The Scheduler has started. */
#define MC_FW_STATE_SCHED (8)

/* Siena MC shared memmory offsets */
/* The 'doorbell' addresses are hard-wired to alert the MC when written */
#define	MC_SMEM_P0_DOORBELL_OFST	0x000
#define	MC_SMEM_P1_DOORBELL_OFST	0x004
/* The rest of these are firmware-defined */
#define	MC_SMEM_P0_PDU_OFST		0x008
#define	MC_SMEM_P1_PDU_OFST		0x108
#define	MC_SMEM_PDU_LEN			0x100
#define	MC_SMEM_P0_PTP_TIME_OFST	0x7f0
#define	MC_SMEM_P0_STATUS_OFST		0x7f8
#define	MC_SMEM_P1_STATUS_OFST		0x7fc

/* Values to be written to the per-port status dword in shared
 * memory on reboot and assert */
#define MC_STATUS_DWORD_REBOOT (0xb007b007)
#define MC_STATUS_DWORD_ASSERT (0xdeaddead)

/* The current version of the MCDI protocol.
 *
 * Note that the ROM burnt into the card only talks V0, so at the very
 * least every driver must support version 0 and MCDI_PCOL_VERSION
 */
#ifdef WITH_MCDI_V2
#define MCDI_PCOL_VERSION 2
#else
#define MCDI_PCOL_VERSION 1
#endif

/* Unused commands: 0x23, 0x27, 0x30, 0x31 */

/* MCDI version 1
 *
 * Each MCDI request starts with an MCDI_HEADER, which is a 32byte
 * structure, filled in by the client.
 *
 *       0       7  8     16    20     22  23  24    31
 *      | CODE | R | LEN | SEQ | Rsvd | E | R | XFLAGS |
 *               |                      |   |
 *               |                      |   \--- Response
 *               |                      \------- Error
 *               \------------------------------ Resync (always set)
 *
 * The client writes it's request into MC shared memory, and rings the
 * doorbell. Each request is completed by either by the MC writting
 * back into shared memory, or by writting out an event.
 *
 * All MCDI commands support completion by shared memory response. Each
 * request may also contain additional data (accounted for by HEADER.LEN),
 * and some response's may also contain additional data (again, accounted
 * for by HEADER.LEN).
 *
 * Some MCDI commands support completion by event, in which any associated
 * response data is included in the event.
 *
 * The protocol requires one response to be delivered for every request, a
 * request should not be sent unless the response for the previous request
 * has been received (either by polling shared memory, or by receiving
 * an event).
 */

/** Request/Response structure */
#define MCDI_HEADER_OFST 0
#define MCDI_HEADER_CODE_LBN 0
#define MCDI_HEADER_CODE_WIDTH 7
#define MCDI_HEADER_RESYNC_LBN 7
#define MCDI_HEADER_RESYNC_WIDTH 1
#define MCDI_HEADER_DATALEN_LBN 8
#define MCDI_HEADER_DATALEN_WIDTH 8
#define MCDI_HEADER_SEQ_LBN 16
#define MCDI_HEADER_RSVD_LBN 20
#define MCDI_HEADER_RSVD_WIDTH 2
#define MCDI_HEADER_SEQ_WIDTH 4
#define MCDI_HEADER_ERROR_LBN 22
#define MCDI_HEADER_ERROR_WIDTH 1
#define MCDI_HEADER_RESPONSE_LBN 23
#define MCDI_HEADER_RESPONSE_WIDTH 1
#define MCDI_HEADER_XFLAGS_LBN 24
#define MCDI_HEADER_XFLAGS_WIDTH 8
/* Request response using event */
#define MCDI_HEADER_XFLAGS_EVREQ 0x01

/* Maximum number of payload bytes */
#ifdef WITH_MCDI_V2
#define MCDI_CTL_SDU_LEN_MAX 0x400
#else
#define MCDI_CTL_SDU_LEN_MAX 0xfc
#endif

/* The MC can generate events for two reasons:
 *   - To complete a shared memory request if XFLAGS_EVREQ was set
 *   - As a notification (link state, i2c event), controlled
 *     via MC_CMD_LOG_CTRL
 *
 * Both events share a common structure:
 *
 *  0      32     33      36    44     52     60
 * | Data | Cont | Level | Src | Code | Rsvd |
 *           |
 *           \ There is another event pending in this notification
 *
 * If Code==CMDDONE, then the fields are further interpreted as:
 *
 *   - LEVEL==INFO    Command succeeded
 *   - LEVEL==ERR     Command failed
 *
 *    0     8         16      24     32
 *   | Seq | Datalen | Errno | Rsvd |
 *
 *   These fields are taken directly out of the standard MCDI header, i.e.,
 *   LEVEL==ERR, Datalen == 0 => Reboot
 *
 * Events can be squirted out of the UART (using LOG_CTRL) without a
 * MCDI header.  An event can be distinguished from a MCDI response by
 * examining the first byte which is 0xc0.  This corresponds to the
 * non-existent MCDI command MC_CMD_DEBUG_LOG.
 *
 *      0         7        8
 *     | command | Resync |     = 0xc0
 *
 * Since the event is written in big-endian byte order, this works
 * providing bits 56-63 of the event are 0xc0.
 *
 *      56     60  63
 *     | Rsvd | Code |    = 0xc0
 *
 * Which means for convenience the event code is 0xc for all MC
 * generated events.
 */
#define FSE_AZ_EV_CODE_MCDI_EVRESPONSE 0xc


/* Non-existent command target */
#define MC_CMD_ERR_ENOENT 2
/* assert() has killed the MC */
#define MC_CMD_ERR_EINTR 4
/* Caller does not hold required locks */
#define MC_CMD_ERR_EACCES 13
/* Resource is currently unavailable (e.g. lock contention) */
#define MC_CMD_ERR_EBUSY 16
/* Invalid argument to target */
#define MC_CMD_ERR_EINVAL 22
/* Non-recursive resource is already acquired */
#define MC_CMD_ERR_EDEADLK 35
/* Operation not implemented */
#define MC_CMD_ERR_ENOSYS 38
/* Operation timed out */
#define MC_CMD_ERR_ETIME 62

#define MC_CMD_ERR_CODE_OFST 0

/* We define 8 "escape" commands to allow
   for command number space extension */

#define MC_CMD_CMD_SPACE_ESCAPE_0	      0x78
#define MC_CMD_CMD_SPACE_ESCAPE_1	      0x79
#define MC_CMD_CMD_SPACE_ESCAPE_2	      0x7A
#define MC_CMD_CMD_SPACE_ESCAPE_3	      0x7B
#define MC_CMD_CMD_SPACE_ESCAPE_4	      0x7C
#define MC_CMD_CMD_SPACE_ESCAPE_5	      0x7D
#define MC_CMD_CMD_SPACE_ESCAPE_6	      0x7E
#define MC_CMD_CMD_SPACE_ESCAPE_7	      0x7F

/* Vectors in the boot ROM */
/* Point to the copycode entry point. */
#define MC_BOOTROM_COPYCODE_VEC (0x7f4)
/* Points to the recovery mode entry point. */
#define MC_BOOTROM_NOFLASH_VEC (0x7f8)

/* The command set exported by the boot ROM (MCDI v0) */
#define MC_CMD_GET_VERSION_V0_SUPPORTED_FUNCS {		\
	(1 << MC_CMD_READ32)	|			\
	(1 << MC_CMD_WRITE32)	|			\
	(1 << MC_CMD_COPYCODE)	|			\
	(1 << MC_CMD_GET_VERSION),			\
	0, 0, 0 }

#define MC_CMD_SENSOR_INFO_OUT_OFFSET_OFST(_x) \
	(MC_CMD_SENSOR_ENTRY_OFST + (_x))

#define MC_CMD_DBI_WRITE_IN_ADDRESS_OFST(n) (  \
        (MC_CMD_DBI_WRITE_IN_DBIWROP_OFST+     \
         MC_CMD_DBIWROP_TYPEDEF_ADDRESS_OFST)+ \
         ((n)*MC_CMD_DBIWROP_TYPEDEF_LEN))

#define MC_CMD_DBI_WRITE_IN_BYTE_MASK_OFST(n) (  \
        (MC_CMD_DBI_WRITE_IN_DBIWROP_OFST+     \
         MC_CMD_DBIWROP_TYPEDEF_BYTE_MASK_OFST)+ \
         ((n)*MC_CMD_DBIWROP_TYPEDEF_LEN))

#define MC_CMD_DBI_WRITE_IN_VALUE_OFST(n) (  \
        (MC_CMD_DBI_WRITE_IN_DBIWROP_OFST+     \
         MC_CMD_DBIWROP_TYPEDEF_VALUE_OFST)+ \
         ((n)*MC_CMD_DBIWROP_TYPEDEF_LEN))


#ifdef WITH_MCDI_V2

/* Version 2 adds an optional argument to error returns: the errno value
 * may be followed by the (0-based) number of the first argument that
 * could not be processed.
 */
#define MC_CMD_ERR_ARG_OFST 4

/* Try again */
#define MC_CMD_ERR_EAGAIN 11
/* No space */
#define MC_CMD_ERR_ENOSPC 28

#endif

/* MCDI_EVENT structuredef */
#define	MCDI_EVENT_LEN 8
#define	MCDI_EVENT_CONT_LBN 32
#define	MCDI_EVENT_CONT_WIDTH 1
#define	MCDI_EVENT_LEVEL_LBN 33
#define	MCDI_EVENT_LEVEL_WIDTH 3
#define	MCDI_EVENT_LEVEL_INFO  0x0 /* enum */
#define	MCDI_EVENT_LEVEL_WARN 0x1 /* enum */
#define	MCDI_EVENT_LEVEL_ERR 0x2 /* enum */
#define	MCDI_EVENT_LEVEL_FATAL 0x3 /* enum */
#define	MCDI_EVENT_DATA_OFST 0
#define	MCDI_EVENT_CMDDONE_SEQ_LBN 0
#define	MCDI_EVENT_CMDDONE_SEQ_WIDTH 8
#define	MCDI_EVENT_CMDDONE_DATALEN_LBN 8
#define	MCDI_EVENT_CMDDONE_DATALEN_WIDTH 8
#define	MCDI_EVENT_CMDDONE_ERRNO_LBN 16
#define	MCDI_EVENT_CMDDONE_ERRNO_WIDTH 8
#define	MCDI_EVENT_LINKCHANGE_LP_CAP_LBN 0
#define	MCDI_EVENT_LINKCHANGE_LP_CAP_WIDTH 16
#define	MCDI_EVENT_LINKCHANGE_SPEED_LBN 16
#define	MCDI_EVENT_LINKCHANGE_SPEED_WIDTH 4
#define	MCDI_EVENT_LINKCHANGE_SPEED_100M  0x1 /* enum */
#define	MCDI_EVENT_LINKCHANGE_SPEED_1G  0x2 /* enum */
#define	MCDI_EVENT_LINKCHANGE_SPEED_10G  0x3 /* enum */
#define	MCDI_EVENT_LINKCHANGE_FCNTL_LBN 20
#define	MCDI_EVENT_LINKCHANGE_FCNTL_WIDTH 4
#define	MCDI_EVENT_LINKCHANGE_LINK_FLAGS_LBN 24
#define	MCDI_EVENT_LINKCHANGE_LINK_FLAGS_WIDTH 8
#define	MCDI_EVENT_SENSOREVT_MONITOR_LBN 0
#define	MCDI_EVENT_SENSOREVT_MONITOR_WIDTH 8
#define	MCDI_EVENT_SENSOREVT_STATE_LBN 8
#define	MCDI_EVENT_SENSOREVT_STATE_WIDTH 8
#define	MCDI_EVENT_SENSOREVT_VALUE_LBN 16
#define	MCDI_EVENT_SENSOREVT_VALUE_WIDTH 16
#define	MCDI_EVENT_FWALERT_DATA_LBN 8
#define	MCDI_EVENT_FWALERT_DATA_WIDTH 24
#define	MCDI_EVENT_FWALERT_REASON_LBN 0
#define	MCDI_EVENT_FWALERT_REASON_WIDTH 8
#define	MCDI_EVENT_FWALERT_REASON_SRAM_ACCESS 0x1 /* enum */
#define	MCDI_EVENT_FLR_VF_LBN 0
#define	MCDI_EVENT_FLR_VF_WIDTH 8
#define	MCDI_EVENT_TX_ERR_TXQ_LBN 0
#define	MCDI_EVENT_TX_ERR_TXQ_WIDTH 12
#define	MCDI_EVENT_TX_ERR_TYPE_LBN 12
#define	MCDI_EVENT_TX_ERR_TYPE_WIDTH 4
#define	MCDI_EVENT_TX_ERR_DL_FAIL 0x1 /* enum */
#define	MCDI_EVENT_TX_ERR_NO_EOP 0x2 /* enum */
#define	MCDI_EVENT_TX_ERR_2BIG 0x3 /* enum */
#define	MCDI_EVENT_TX_ERR_INFO_LBN 16
#define	MCDI_EVENT_TX_ERR_INFO_WIDTH 16
#define	MCDI_EVENT_TX_FLUSH_TXQ_LBN 0
#define	MCDI_EVENT_TX_FLUSH_TXQ_WIDTH 12
#define	MCDI_EVENT_PTP_ERR_TYPE_LBN 0
#define	MCDI_EVENT_PTP_ERR_TYPE_WIDTH 8
#define	MCDI_EVENT_PTP_ERR_PLL_LOST 0x1 /* enum */
#define	MCDI_EVENT_PTP_ERR_FILTER 0x2 /* enum */
#define	MCDI_EVENT_PTP_ERR_FIFO 0x3 /* enum */
#define	MCDI_EVENT_PTP_ERR_QUEUE 0x4 /* enum */
#define	MCDI_EVENT_AOE_ERR_TYPE_LBN 0
#define	MCDI_EVENT_AOE_ERR_TYPE_WIDTH 8
#define	MCDI_EVENT_AOE_NO_LOAD 0x1 /* enum */
#define	MCDI_EVENT_AOE_FC_ASSERT 0x2 /* enum */
#define	MCDI_EVENT_AOE_FC_WATCHDOG 0x3 /* enum */
#define	MCDI_EVENT_AOE_FC_NO_START 0x4 /* enum */
#define	MCDI_EVENT_AOE_FAULT 0x5 /* enum */
#define	MCDI_EVENT_AOE_CPLD_REPROGRAMMED 0x6 /* enum */
#define	MCDI_EVENT_AOE_LOAD 0x7 /* enum */
#define	MCDI_EVENT_AOE_DMA 0x8 /* enum */
#define	MCDI_EVENT_AOE_BYTEBLASTER 0x9 /* enum */
#define	MCDI_EVENT_AOE_DDR_ECC_STATUS 0xa /* enum */
#define	MCDI_EVENT_AOE_PTP_STATUS 0xb /* enum */
#define	MCDI_EVENT_AOE_ERR_DATA_LBN 8
#define	MCDI_EVENT_AOE_ERR_DATA_WIDTH 8
#define	MCDI_EVENT_DATA_LBN 0
#define	MCDI_EVENT_DATA_WIDTH 32
#define	MCDI_EVENT_SRC_LBN 36
#define	MCDI_EVENT_SRC_WIDTH 8
#define	MCDI_EVENT_EV_CODE_LBN 60
#define	MCDI_EVENT_EV_CODE_WIDTH 4
#define	MCDI_EVENT_CODE_LBN 44
#define	MCDI_EVENT_CODE_WIDTH 8
#define	MCDI_EVENT_CODE_BADSSERT 0x1 /* enum */
#define	MCDI_EVENT_CODE_PMNOTICE 0x2 /* enum */
#define	MCDI_EVENT_CODE_CMDDONE 0x3 /* enum */
#define	MCDI_EVENT_CODE_LINKCHANGE 0x4 /* enum */
#define	MCDI_EVENT_CODE_SENSOREVT 0x5 /* enum */
#define	MCDI_EVENT_CODE_SCHEDERR 0x6 /* enum */
#define	MCDI_EVENT_CODE_REBOOT 0x7 /* enum */
#define	MCDI_EVENT_CODE_MAC_STATS_DMA 0x8 /* enum */
#define	MCDI_EVENT_CODE_FWALERT 0x9 /* enum */
#define	MCDI_EVENT_CODE_FLR 0xa /* enum */
#define	MCDI_EVENT_CODE_TX_ERR 0xb /* enum */
#define	MCDI_EVENT_CODE_TX_FLUSH  0xc /* enum */
#define	MCDI_EVENT_CODE_PTP_RX  0xd /* enum */
#define	MCDI_EVENT_CODE_PTP_FAULT  0xe /* enum */
#define	MCDI_EVENT_CODE_PTP_PPS  0xf /* enum */
#define	MCDI_EVENT_CODE_AOE  0x12 /* enum */
#define	MCDI_EVENT_CODE_VCAL_FAIL  0x13 /* enum */
#define	MCDI_EVENT_CODE_HW_PPS  0x14 /* enum */
#define	MCDI_EVENT_CMDDONE_DATA_OFST 0
#define	MCDI_EVENT_CMDDONE_DATA_LBN 0
#define	MCDI_EVENT_CMDDONE_DATA_WIDTH 32
#define	MCDI_EVENT_LINKCHANGE_DATA_OFST 0
#define	MCDI_EVENT_LINKCHANGE_DATA_LBN 0
#define	MCDI_EVENT_LINKCHANGE_DATA_WIDTH 32
#define	MCDI_EVENT_SENSOREVT_DATA_OFST 0
#define	MCDI_EVENT_SENSOREVT_DATA_LBN 0
#define	MCDI_EVENT_SENSOREVT_DATA_WIDTH 32
#define	MCDI_EVENT_MAC_STATS_DMA_GENERATION_OFST 0
#define	MCDI_EVENT_MAC_STATS_DMA_GENERATION_LBN 0
#define	MCDI_EVENT_MAC_STATS_DMA_GENERATION_WIDTH 32
#define	MCDI_EVENT_TX_ERR_DATA_OFST 0
#define	MCDI_EVENT_TX_ERR_DATA_LBN 0
#define	MCDI_EVENT_TX_ERR_DATA_WIDTH 32
#define	MCDI_EVENT_PTP_SECONDS_OFST 0
#define	MCDI_EVENT_PTP_SECONDS_LBN 0
#define	MCDI_EVENT_PTP_SECONDS_WIDTH 32
#define	MCDI_EVENT_PTP_NANOSECONDS_OFST 0
#define	MCDI_EVENT_PTP_NANOSECONDS_LBN 0
#define	MCDI_EVENT_PTP_NANOSECONDS_WIDTH 32
#define	MCDI_EVENT_PTP_UUID_OFST 0
#define	MCDI_EVENT_PTP_UUID_LBN 0
#define	MCDI_EVENT_PTP_UUID_WIDTH 32

/* FCDI_EVENT structuredef */
#define	FCDI_EVENT_LEN 8
#define	FCDI_EVENT_CONT_LBN 32
#define	FCDI_EVENT_CONT_WIDTH 1
#define	FCDI_EVENT_LEVEL_LBN 33
#define	FCDI_EVENT_LEVEL_WIDTH 3
#define	FCDI_EVENT_LEVEL_INFO  0x0 /* enum */
#define	FCDI_EVENT_LEVEL_WARN 0x1 /* enum */
#define	FCDI_EVENT_LEVEL_ERR 0x2 /* enum */
#define	FCDI_EVENT_LEVEL_FATAL 0x3 /* enum */
#define	FCDI_EVENT_DATA_OFST 0
#define	FCDI_EVENT_LINK_STATE_STATUS_LBN 0
#define	FCDI_EVENT_LINK_STATE_STATUS_WIDTH 1
#define	FCDI_EVENT_LINK_DOWN 0x0 /* enum */
#define	FCDI_EVENT_LINK_UP 0x1 /* enum */
#define	FCDI_EVENT_DATA_LBN 0
#define	FCDI_EVENT_DATA_WIDTH 32
#define	FCDI_EVENT_SRC_LBN 36
#define	FCDI_EVENT_SRC_WIDTH 8
#define	FCDI_EVENT_EV_CODE_LBN 60
#define	FCDI_EVENT_EV_CODE_WIDTH 4
#define	FCDI_EVENT_CODE_LBN 44
#define	FCDI_EVENT_CODE_WIDTH 8
#define	FCDI_EVENT_CODE_REBOOT 0x1 /* enum */
#define	FCDI_EVENT_CODE_ASSERT 0x2 /* enum */
#define	FCDI_EVENT_CODE_DDR_TEST_RESULT 0x3 /* enum */
#define	FCDI_EVENT_CODE_LINK_STATE 0x4 /* enum */
#define	FCDI_EVENT_CODE_TIMED_READ 0x5 /* enum */
#define	FCDI_EVENT_CODE_PPS_IN 0x6 /* enum */
#define	FCDI_EVENT_CODE_PTP_TICK 0x7 /* enum */
#define	FCDI_EVENT_CODE_DDR_ECC_STATUS 0x8 /* enum */
#define	FCDI_EVENT_CODE_PTP_STATUS 0x9 /* enum */
#define	FCDI_EVENT_ASSERT_INSTR_ADDRESS_OFST 0
#define	FCDI_EVENT_ASSERT_INSTR_ADDRESS_LBN 0
#define	FCDI_EVENT_ASSERT_INSTR_ADDRESS_WIDTH 32
#define	FCDI_EVENT_ASSERT_TYPE_LBN 36
#define	FCDI_EVENT_ASSERT_TYPE_WIDTH 8
#define	FCDI_EVENT_DDR_TEST_RESULT_STATUS_CODE_LBN 36
#define	FCDI_EVENT_DDR_TEST_RESULT_STATUS_CODE_WIDTH 8
#define	FCDI_EVENT_DDR_TEST_RESULT_RESULT_OFST 0
#define	FCDI_EVENT_DDR_TEST_RESULT_RESULT_LBN 0
#define	FCDI_EVENT_DDR_TEST_RESULT_RESULT_WIDTH 32
#define	FCDI_EVENT_LINK_STATE_DATA_OFST 0
#define	FCDI_EVENT_LINK_STATE_DATA_LBN 0
#define	FCDI_EVENT_LINK_STATE_DATA_WIDTH 32
#define	FCDI_EVENT_PTP_STATE_OFST 0
#define	FCDI_EVENT_PTP_UNDEFINED 0x0 /* enum */
#define	FCDI_EVENT_PTP_SETUP_FAILED 0x1 /* enum */
#define	FCDI_EVENT_PTP_OPERATIONAL 0x2 /* enum */
#define	FCDI_EVENT_PTP_STATE_LBN 0
#define	FCDI_EVENT_PTP_STATE_WIDTH 32
#define	FCDI_EVENT_DDR_ECC_STATUS_BANK_ID_LBN 36
#define	FCDI_EVENT_DDR_ECC_STATUS_BANK_ID_WIDTH 8
#define	FCDI_EVENT_DDR_ECC_STATUS_STATUS_OFST 0
#define	FCDI_EVENT_DDR_ECC_STATUS_STATUS_LBN 0
#define	FCDI_EVENT_DDR_ECC_STATUS_STATUS_WIDTH 32

/* FCDI_EXTENDED_EVENT_PPS structuredef */
#define	FCDI_EXTENDED_EVENT_PPS_LENMIN 16
#define	FCDI_EXTENDED_EVENT_PPS_LENMAX 248
#define	FCDI_EXTENDED_EVENT_PPS_LEN(num) (8+8*(num))
#define	FCDI_EXTENDED_EVENT_PPS_COUNT_OFST 0
#define	FCDI_EXTENDED_EVENT_PPS_COUNT_LBN 0
#define	FCDI_EXTENDED_EVENT_PPS_COUNT_WIDTH 32
#define	FCDI_EXTENDED_EVENT_PPS_SECONDS_OFST 8
#define	FCDI_EXTENDED_EVENT_PPS_SECONDS_LBN 64
#define	FCDI_EXTENDED_EVENT_PPS_SECONDS_WIDTH 32
#define	FCDI_EXTENDED_EVENT_PPS_NANOSECONDS_OFST 12
#define	FCDI_EXTENDED_EVENT_PPS_NANOSECONDS_LBN 96
#define	FCDI_EXTENDED_EVENT_PPS_NANOSECONDS_WIDTH 32
#define	FCDI_EXTENDED_EVENT_PPS_TIMESTAMPS_OFST 8
#define	FCDI_EXTENDED_EVENT_PPS_TIMESTAMPS_LEN 8
#define	FCDI_EXTENDED_EVENT_PPS_TIMESTAMPS_LO_OFST 8
#define	FCDI_EXTENDED_EVENT_PPS_TIMESTAMPS_HI_OFST 12
#define	FCDI_EXTENDED_EVENT_PPS_TIMESTAMPS_MINNUM 1
#define	FCDI_EXTENDED_EVENT_PPS_TIMESTAMPS_MAXNUM 30
#define	FCDI_EXTENDED_EVENT_PPS_TIMESTAMPS_LBN 64
#define	FCDI_EXTENDED_EVENT_PPS_TIMESTAMPS_WIDTH 64


/***********************************/
/* MC_CMD_READ32
 * Read multiple 32byte words from MC memory.
 */
#define	MC_CMD_READ32 0x1

/* MC_CMD_READ32_IN msgrequest */
#define	MC_CMD_READ32_IN_LEN 8
#define	MC_CMD_READ32_IN_ADDR_OFST 0
#define	MC_CMD_READ32_IN_NUMWORDS_OFST 4

/* MC_CMD_READ32_OUT msgresponse */
#define	MC_CMD_READ32_OUT_LENMIN 4
#define	MC_CMD_READ32_OUT_LENMAX 252
#define	MC_CMD_READ32_OUT_LEN(num) (0+4*(num))
#define	MC_CMD_READ32_OUT_BUFFER_OFST 0
#define	MC_CMD_READ32_OUT_BUFFER_LEN 4
#define	MC_CMD_READ32_OUT_BUFFER_MINNUM 1
#define	MC_CMD_READ32_OUT_BUFFER_MAXNUM 63


/***********************************/
/* MC_CMD_WRITE32
 * Write multiple 32byte words to MC memory.
 */
#define	MC_CMD_WRITE32 0x2

/* MC_CMD_WRITE32_IN msgrequest */
#define	MC_CMD_WRITE32_IN_LENMIN 8
#define	MC_CMD_WRITE32_IN_LENMAX 252
#define	MC_CMD_WRITE32_IN_LEN(num) (4+4*(num))
#define	MC_CMD_WRITE32_IN_ADDR_OFST 0
#define	MC_CMD_WRITE32_IN_BUFFER_OFST 4
#define	MC_CMD_WRITE32_IN_BUFFER_LEN 4
#define	MC_CMD_WRITE32_IN_BUFFER_MINNUM 1
#define	MC_CMD_WRITE32_IN_BUFFER_MAXNUM 62

/* MC_CMD_WRITE32_OUT msgresponse */
#define	MC_CMD_WRITE32_OUT_LEN 0


/***********************************/
/* MC_CMD_COPYCODE
 * Copy MC code between two locations and jump.
 */
#define	MC_CMD_COPYCODE 0x3

/* MC_CMD_COPYCODE_IN msgrequest */
#define	MC_CMD_COPYCODE_IN_LEN 16
#define	MC_CMD_COPYCODE_IN_SRC_ADDR_OFST 0
#define	MC_CMD_COPYCODE_IN_DEST_ADDR_OFST 4
#define	MC_CMD_COPYCODE_IN_NUMWORDS_OFST 8
#define	MC_CMD_COPYCODE_IN_JUMP_OFST 12
#define	MC_CMD_COPYCODE_JUMP_NONE 0x1 /* enum */

/* MC_CMD_COPYCODE_OUT msgresponse */
#define	MC_CMD_COPYCODE_OUT_LEN 0


/***********************************/
/* MC_CMD_SET_FUNC 
 */
#define	MC_CMD_SET_FUNC  0x4

/* MC_CMD_SET_FUNC_IN msgrequest */
#define	MC_CMD_SET_FUNC_IN_LEN 4
#define	MC_CMD_SET_FUNC_IN_FUNC_OFST 0

/* MC_CMD_SET_FUNC_OUT msgresponse */
#define	MC_CMD_SET_FUNC_OUT_LEN 0


/***********************************/
/* MC_CMD_GET_BOOT_STATUS
 */
#define	MC_CMD_GET_BOOT_STATUS 0x5

/* MC_CMD_GET_BOOT_STATUS_IN msgrequest */
#define	MC_CMD_GET_BOOT_STATUS_IN_LEN 0

/* MC_CMD_GET_BOOT_STATUS_OUT msgresponse */
#define	MC_CMD_GET_BOOT_STATUS_OUT_LEN 8
#define	MC_CMD_GET_BOOT_STATUS_OUT_BOOT_OFFSET_OFST 0
#define	MC_CMD_GET_BOOT_STATUS_OUT_FLAGS_OFST 4
#define	MC_CMD_GET_BOOT_STATUS_OUT_FLAGS_WATCHDOG_LBN 0
#define	MC_CMD_GET_BOOT_STATUS_OUT_FLAGS_WATCHDOG_WIDTH 1
#define	MC_CMD_GET_BOOT_STATUS_OUT_FLAGS_PRIMARY_LBN 1
#define	MC_CMD_GET_BOOT_STATUS_OUT_FLAGS_PRIMARY_WIDTH 1
#define	MC_CMD_GET_BOOT_STATUS_OUT_FLAGS_BACKUP_LBN 2
#define	MC_CMD_GET_BOOT_STATUS_OUT_FLAGS_BACKUP_WIDTH 1


/***********************************/
/* MC_CMD_GET_ASSERTS 
 * Get and clear any assertion status.
 */
#define	MC_CMD_GET_ASSERTS  0x6

/* MC_CMD_GET_ASSERTS_IN msgrequest */
#define	MC_CMD_GET_ASSERTS_IN_LEN 4
#define	MC_CMD_GET_ASSERTS_IN_CLEAR_OFST 0

/* MC_CMD_GET_ASSERTS_OUT msgresponse */
#define	MC_CMD_GET_ASSERTS_OUT_LEN 140
#define	MC_CMD_GET_ASSERTS_OUT_GLOBAL_FLAGS_OFST 0
#define	MC_CMD_GET_ASSERTS_FLAGS_NO_FAILS 0x1 /* enum */
#define	MC_CMD_GET_ASSERTS_FLAGS_SYS_FAIL 0x2 /* enum */
#define	MC_CMD_GET_ASSERTS_FLAGS_THR_FAIL 0x3 /* enum */
#define	MC_CMD_GET_ASSERTS_FLAGS_WDOG_FIRED 0x4 /* enum */
#define	MC_CMD_GET_ASSERTS_OUT_SAVED_PC_OFFS_OFST 4
#define	MC_CMD_GET_ASSERTS_OUT_GP_REGS_OFFS_OFST 8
#define	MC_CMD_GET_ASSERTS_OUT_GP_REGS_OFFS_LEN 4
#define	MC_CMD_GET_ASSERTS_OUT_GP_REGS_OFFS_NUM 31
#define	MC_CMD_GET_ASSERTS_OUT_THREAD_OFFS_OFST 132
#define	MC_CMD_GET_ASSERTS_OUT_RESERVED_OFST 136


/***********************************/
/* MC_CMD_LOG_CTRL 
 * Configure the output stream for various events and messages.
 */
#define	MC_CMD_LOG_CTRL  0x7

/* MC_CMD_LOG_CTRL_IN msgrequest */
#define	MC_CMD_LOG_CTRL_IN_LEN 8
#define	MC_CMD_LOG_CTRL_IN_LOG_DEST_OFST 0
#define	MC_CMD_LOG_CTRL_IN_LOG_DEST_UART 0x1 /* enum */
#define	MC_CMD_LOG_CTRL_IN_LOG_DEST_EVQ 0x2 /* enum */
#define	MC_CMD_LOG_CTRL_IN_LOG_DEST_EVQ_OFST 4

/* MC_CMD_LOG_CTRL_OUT msgresponse */
#define	MC_CMD_LOG_CTRL_OUT_LEN 0


/***********************************/
/* MC_CMD_GET_VERSION 
 * Get version information about the MC firmware.
 */
#define	MC_CMD_GET_VERSION  0x8

/* MC_CMD_GET_VERSION_IN msgrequest */
#define	MC_CMD_GET_VERSION_IN_LEN 0

/* MC_CMD_GET_VERSION_V0_OUT msgresponse */
#define	MC_CMD_GET_VERSION_V0_OUT_LEN 4
#define	MC_CMD_GET_VERSION_OUT_FIRMWARE_OFST 0
#define	MC_CMD_GET_VERSION_OUT_FIRMWARE_ANY 0xffffffff /* enum */
#define	MC_CMD_GET_VERSION_OUT_FIRMWARE_BOOTROM 0xb0070000 /* enum */
#define	MC_CMD_GET_VERSION_OUT_FIRMWARE_SIENA_BOOTROM 0xb0070000 /* enum */
#define	MC_CMD_GET_VERSION_OUT_FIRMWARE_HUNT_BOOTROM 0xb0070001 /* enum */

/* MC_CMD_GET_VERSION_OUT msgresponse */
#define	MC_CMD_GET_VERSION_OUT_LEN 32
/*            MC_CMD_GET_VERSION_OUT_FIRMWARE_OFST 0 */
/*            Enum values, see field(s): */
/*               MC_CMD_GET_VERSION_V0_OUT/MC_CMD_GET_VERSION_OUT_FIRMWARE */
#define	MC_CMD_GET_VERSION_OUT_PCOL_OFST 4
#define	MC_CMD_GET_VERSION_OUT_SUPPORTED_FUNCS_OFST 8
#define	MC_CMD_GET_VERSION_OUT_SUPPORTED_FUNCS_LEN 16
#define	MC_CMD_GET_VERSION_OUT_VERSION_OFST 24
#define	MC_CMD_GET_VERSION_OUT_VERSION_LEN 8
#define	MC_CMD_GET_VERSION_OUT_VERSION_LO_OFST 24
#define	MC_CMD_GET_VERSION_OUT_VERSION_HI_OFST 28


/***********************************/
/* MC_CMD_FC 
 * Perform an FC operation
 */
#define	MC_CMD_FC  0x9

/* MC_CMD_FC_IN msgrequest */
#define	MC_CMD_FC_IN_LEN 4
#define	MC_CMD_FC_IN_OP_HDR_OFST 0
#define	MC_CMD_FC_IN_OP_LBN 0
#define	MC_CMD_FC_IN_OP_WIDTH 8
#define	MC_CMD_FC_OP_NULL 0x1 /* enum */
#define	MC_CMD_FC_OP_UNUSED 0x2 /* enum */
#define	MC_CMD_FC_OP_MAC 0x3 /* enum */
#define	MC_CMD_FC_OP_READ32 0x4 /* enum */
#define	MC_CMD_FC_OP_WRITE32 0x5 /* enum */
#define	MC_CMD_FC_OP_TRC_READ 0x6 /* enum */
#define	MC_CMD_FC_OP_TRC_WRITE 0x7 /* enum */
#define	MC_CMD_FC_OP_GET_VERSION 0x8 /* enum */
#define	MC_CMD_FC_OP_TRC_RX_READ 0x9 /* enum */
#define	MC_CMD_FC_OP_TRC_RX_WRITE 0xa /* enum */
#define	MC_CMD_FC_OP_SFP 0xb /* enum */
#define	MC_CMD_FC_OP_DDR_TEST 0xc /* enum */
#define	MC_CMD_FC_OP_GET_ASSERT 0xd /* enum */
#define	MC_CMD_FC_OP_FPGA_BUILD 0xe /* enum */
#define	MC_CMD_FC_OP_READ_MAP 0xf /* enum */
#define	MC_CMD_FC_OP_CAPABILITIES 0x10 /* enum */
#define	MC_CMD_FC_OP_GLOBAL_FLAGS 0x11 /* enum */
#define	MC_CMD_FC_OP_IO_REL 0x12 /* enum */
#define	MC_CMD_FC_OP_UHLINK 0x13 /* enum */
#define	MC_CMD_FC_OP_SET_LINK 0x14 /* enum */
#define	MC_CMD_FC_OP_LICENSE 0x15 /* enum */
#define	MC_CMD_FC_OP_STARTUP 0x16 /* enum */
#define	MC_CMD_FC_OP_DMA 0x17 /* enum */
#define	MC_CMD_FC_OP_TIMED_READ 0x18 /* enum */
#define	MC_CMD_FC_OP_LOG 0x19 /* enum */
#define	MC_CMD_FC_OP_CLOCK 0x1a /* enum */
#define	MC_CMD_FC_OP_DDR 0x1b /* enum */
#define	MC_CMD_FC_OP_TIMESTAMP 0x1c /* enum */
#define	MC_CMD_FC_OP_SPI 0x1d /* enum */
#define	MC_CMD_FC_OP_DIAG 0x1e /* enum */
#define	MC_CMD_FC_IN_PORT_EXT_OFST 0x0 /* enum */
#define	MC_CMD_FC_IN_PORT_INT_OFST 0x40 /* enum */

/* MC_CMD_FC_IN_NULL msgrequest */
#define	MC_CMD_FC_IN_NULL_LEN 4
#define	MC_CMD_FC_IN_CMD_OFST 0

/* MC_CMD_FC_IN_MAC msgrequest */
#define	MC_CMD_FC_IN_MAC_LEN 8
/*            MC_CMD_FC_IN_CMD_OFST 0 */
#define	MC_CMD_FC_IN_MAC_HEADER_OFST 4
#define	MC_CMD_FC_IN_MAC_OP_LBN 0
#define	MC_CMD_FC_IN_MAC_OP_WIDTH 8
#define	MC_CMD_FC_OP_MAC_OP_RECONFIGURE 0x1 /* enum */
#define	MC_CMD_FC_OP_MAC_OP_SET_LINK 0x2 /* enum */
#define	MC_CMD_FC_OP_MAC_OP_GET_STATS 0x3 /* enum */
#define	MC_CMD_FC_OP_MAC_OP_GET_RX_STATS 0x6 /* enum */
#define	MC_CMD_FC_OP_MAC_OP_GET_TX_STATS 0x7 /* enum */
#define	MC_CMD_FC_OP_MAC_OP_READ_STATUS 0x8 /* enum */
#define	MC_CMD_FC_IN_MAC_PORT_TYPE_LBN 8
#define	MC_CMD_FC_IN_MAC_PORT_TYPE_WIDTH 8
#define	MC_CMD_FC_PORT_EXT 0x0 /* enum */
#define	MC_CMD_FC_PORT_INT 0x1 /* enum */
#define	MC_CMD_FC_IN_MAC_PORT_IDX_LBN 16
#define	MC_CMD_FC_IN_MAC_PORT_IDX_WIDTH 8
#define	MC_CMD_FC_IN_MAC_CMD_FORMAT_LBN 24
#define	MC_CMD_FC_IN_MAC_CMD_FORMAT_WIDTH 8
#define	MC_CMD_FC_OP_MAC_CMD_FORMAT_DEFAULT 0x0 /* enum */
#define	MC_CMD_FC_OP_MAC_CMD_FORMAT_PORT_OVERRIDE 0x1 /* enum */

/* MC_CMD_FC_IN_MAC_RECONFIGURE msgrequest */
#define	MC_CMD_FC_IN_MAC_RECONFIGURE_LEN 8
/*            MC_CMD_FC_IN_CMD_OFST 0 */
/*            MC_CMD_FC_IN_MAC_HEADER_OFST 4 */

/* MC_CMD_FC_IN_MAC_SET_LINK msgrequest */
#define	MC_CMD_FC_IN_MAC_SET_LINK_LEN 32
/*            MC_CMD_FC_IN_CMD_OFST 0 */
/*            MC_CMD_FC_IN_MAC_HEADER_OFST 4 */
#define	MC_CMD_FC_IN_MAC_SET_LINK_MTU_OFST 8
#define	MC_CMD_FC_IN_MAC_SET_LINK_DRAIN_OFST 12
#define	MC_CMD_FC_IN_MAC_SET_LINK_ADDR_OFST 16
#define	MC_CMD_FC_IN_MAC_SET_LINK_ADDR_LEN 8
#define	MC_CMD_FC_IN_MAC_SET_LINK_ADDR_LO_OFST 16
#define	MC_CMD_FC_IN_MAC_SET_LINK_ADDR_HI_OFST 20
#define	MC_CMD_FC_IN_MAC_SET_LINK_REJECT_OFST 24
#define	MC_CMD_FC_IN_MAC_SET_LINK_REJECT_UNICAST_LBN 0
#define	MC_CMD_FC_IN_MAC_SET_LINK_REJECT_UNICAST_WIDTH 1
#define	MC_CMD_FC_IN_MAC_SET_LINK_REJECT_BRDCAST_LBN 1
#define	MC_CMD_FC_IN_MAC_SET_LINK_REJECT_BRDCAST_WIDTH 1
#define	MC_CMD_FC_IN_MAC_SET_LINK_FCNTL_OFST 28

/* MC_CMD_FC_IN_MAC_READ_STATUS msgrequest */
#define	MC_CMD_FC_IN_MAC_READ_STATUS_LEN 8
/*            MC_CMD_FC_IN_CMD_OFST 0 */
/*            MC_CMD_FC_IN_MAC_HEADER_OFST 4 */

/* MC_CMD_FC_IN_MAC_GET_RX_STATS msgrequest */
#define	MC_CMD_FC_IN_MAC_GET_RX_STATS_LEN 8
/*            MC_CMD_FC_IN_CMD_OFST 0 */
/*            MC_CMD_FC_IN_MAC_HEADER_OFST 4 */

/* MC_CMD_FC_IN_MAC_GET_TX_STATS msgrequest */
#define	MC_CMD_FC_IN_MAC_GET_TX_STATS_LEN 8
/*            MC_CMD_FC_IN_CMD_OFST 0 */
/*            MC_CMD_FC_IN_MAC_HEADER_OFST 4 */

/* MC_CMD_FC_IN_MAC_GET_STATS msgrequest */
#define	MC_CMD_FC_IN_MAC_GET_STATS_LEN 20
/*            MC_CMD_FC_IN_CMD_OFST 0 */
/*            MC_CMD_FC_IN_MAC_HEADER_OFST 4 */
#define	MC_CMD_FC_IN_MAC_GET_STATS_STATS_INDEX_OFST 8
#define	MC_CMD_FC_IN_MAC_GET_STATS_FLAGS_OFST 12
#define	MC_CMD_FC_IN_MAC_GET_STATS_CLEAR_ALL_LBN 0
#define	MC_CMD_FC_IN_MAC_GET_STATS_CLEAR_ALL_WIDTH 1
#define	MC_CMD_FC_IN_MAC_GET_STATS_CLEAR_LBN 1
#define	MC_CMD_FC_IN_MAC_GET_STATS_CLEAR_WIDTH 1
#define	MC_CMD_FC_IN_MAC_GET_STATS_UPDATE_LBN 2
#define	MC_CMD_FC_IN_MAC_GET_STATS_UPDATE_WIDTH 1
#define	MC_CMD_FC_IN_MAC_GET_STATS_NUM_OFST 16
#define	MC_CMD_FC_MAC_NSTATS_PER_BLOCK 0x1e /* enum */
#define	MC_CMD_FC_MAC_NBYTES_PER_STAT 0x8 /* enum */

/* MC_CMD_FC_IN_READ32 msgrequest */
#define	MC_CMD_FC_IN_READ32_LEN 16
/*            MC_CMD_FC_IN_CMD_OFST 0 */
#define	MC_CMD_FC_IN_READ32_ADDR_HI_OFST 4
#define	MC_CMD_FC_IN_READ32_ADDR_LO_OFST 8
#define	MC_CMD_FC_IN_READ32_NUMWORDS_OFST 12

/* MC_CMD_FC_IN_WRITE32 msgrequest */
#define	MC_CMD_FC_IN_WRITE32_LENMIN 16
#define	MC_CMD_FC_IN_WRITE32_LENMAX 252
#define	MC_CMD_FC_IN_WRITE32_LEN(num) (12+4*(num))
/*            MC_CMD_FC_IN_CMD_OFST 0 */
#define	MC_CMD_FC_IN_WRITE32_ADDR_HI_OFST 4
#define	MC_CMD_FC_IN_WRITE32_ADDR_LO_OFST 8
#define	MC_CMD_FC_IN_WRITE32_BUFFER_OFST 12
#define	MC_CMD_FC_IN_WRITE32_BUFFER_LEN 4
#define	MC_CMD_FC_IN_WRITE32_BUFFER_MINNUM 1
#define	MC_CMD_FC_IN_WRITE32_BUFFER_MAXNUM 60

/* MC_CMD_FC_IN_TRC_READ msgrequest */
#define	MC_CMD_FC_IN_TRC_READ_LEN 12
/*            MC_CMD_FC_IN_CMD_OFST 0 */
#define	MC_CMD_FC_IN_TRC_READ_TRC_OFST 4
#define	MC_CMD_FC_IN_TRC_READ_CHANNEL_OFST 8

/* MC_CMD_FC_IN_TRC_WRITE msgrequest */
#define	MC_CMD_FC_IN_TRC_WRITE_LEN 28
/*            MC_CMD_FC_IN_CMD_OFST 0 */
#define	MC_CMD_FC_IN_TRC_WRITE_TRC_OFST 4
#define	MC_CMD_FC_IN_TRC_WRITE_CHANNEL_OFST 8
#define	MC_CMD_FC_IN_TRC_WRITE_DATA_OFST 12
#define	MC_CMD_FC_IN_TRC_WRITE_DATA_LEN 4
#define	MC_CMD_FC_IN_TRC_WRITE_DATA_NUM 4

/* MC_CMD_FC_IN_GET_VERSION msgrequest */
#define	MC_CMD_FC_IN_GET_VERSION_LEN 4
/*            MC_CMD_FC_IN_CMD_OFST 0 */

/* MC_CMD_FC_IN_TRC_RX_READ msgrequest */
#define	MC_CMD_FC_IN_TRC_RX_READ_LEN 12
/*            MC_CMD_FC_IN_CMD_OFST 0 */
#define	MC_CMD_FC_IN_TRC_RX_READ_TRC_OFST 4
#define	MC_CMD_FC_IN_TRC_RX_READ_CHANNEL_OFST 8

/* MC_CMD_FC_IN_TRC_RX_WRITE msgrequest */
#define	MC_CMD_FC_IN_TRC_RX_WRITE_LEN 20
/*            MC_CMD_FC_IN_CMD_OFST 0 */
#define	MC_CMD_FC_IN_TRC_RX_WRITE_TRC_OFST 4
#define	MC_CMD_FC_IN_TRC_RX_WRITE_CHANNEL_OFST 8
#define	MC_CMD_FC_IN_TRC_RX_WRITE_DATA_OFST 12
#define	MC_CMD_FC_IN_TRC_RX_WRITE_DATA_LEN 4
#define	MC_CMD_FC_IN_TRC_RX_WRITE_DATA_NUM 2

/* MC_CMD_FC_IN_SFP msgrequest */
#define	MC_CMD_FC_IN_SFP_LEN 24
/*            MC_CMD_FC_IN_CMD_OFST 0 */
#define	MC_CMD_FC_IN_SFP_SPEED_OFST 4
#define	MC_CMD_FC_IN_SFP_COPPER_LEN_OFST 8
#define	MC_CMD_FC_IN_SFP_DUAL_SPEED_OFST 12
#define	MC_CMD_FC_IN_SFP_PRESENT_OFST 16
#define	MC_CMD_FC_IN_SFP_TYPE_OFST 20

/* MC_CMD_FC_IN_DDR_TEST msgrequest */
#define	MC_CMD_FC_IN_DDR_TEST_LEN 8
/*            MC_CMD_FC_IN_CMD_OFST 0 */
#define	MC_CMD_FC_IN_DDR_TEST_HEADER_OFST 4
#define	MC_CMD_FC_IN_DDR_TEST_OP_LBN 0
#define	MC_CMD_FC_IN_DDR_TEST_OP_WIDTH 8
#define	MC_CMD_FC_OP_DDR_TEST_START 0x1 /* enum */
#define	MC_CMD_FC_OP_DDR_TEST_POLL 0x2 /* enum */

/* MC_CMD_FC_IN_DDR_TEST_START msgrequest */
#define	MC_CMD_FC_IN_DDR_TEST_START_LEN 12
/*            MC_CMD_FC_IN_CMD_OFST 0 */
/*            MC_CMD_FC_IN_DDR_TEST_HEADER_OFST 4 */
#define	MC_CMD_FC_IN_DDR_TEST_START_MASK_OFST 8
#define	MC_CMD_FC_IN_DDR_TEST_START_T0_LBN 0
#define	MC_CMD_FC_IN_DDR_TEST_START_T0_WIDTH 1
#define	MC_CMD_FC_IN_DDR_TEST_START_T1_LBN 1
#define	MC_CMD_FC_IN_DDR_TEST_START_T1_WIDTH 1
#define	MC_CMD_FC_IN_DDR_TEST_START_B0_LBN 2
#define	MC_CMD_FC_IN_DDR_TEST_START_B0_WIDTH 1
#define	MC_CMD_FC_IN_DDR_TEST_START_B1_LBN 3
#define	MC_CMD_FC_IN_DDR_TEST_START_B1_WIDTH 1

/* MC_CMD_FC_IN_DDR_TEST_POLL msgrequest */
#define	MC_CMD_FC_IN_DDR_TEST_POLL_LEN 8
#define	MC_CMD_FC_IN_DDR_TEST_CMD_OFST 0
/*            MC_CMD_FC_IN_DDR_TEST_HEADER_OFST 4 */

/* MC_CMD_FC_IN_GET_ASSERT msgrequest */
#define	MC_CMD_FC_IN_GET_ASSERT_LEN 4
/*            MC_CMD_FC_IN_CMD_OFST 0 */

/* MC_CMD_FC_IN_FPGA_BUILD msgrequest */
#define	MC_CMD_FC_IN_FPGA_BUILD_LEN 8
/*            MC_CMD_FC_IN_CMD_OFST 0 */
#define	MC_CMD_FC_IN_FPGA_BUILD_OP_OFST 4
#define	MC_CMD_FC_IN_FPGA_BUILD_BUILD 0x1 /* enum */
#define	MC_CMD_FC_IN_FPGA_BUILD_SERVICES 0x2 /* enum */
#define	MC_CMD_FC_IN_FPGA_BUILD_BSP_VERSION 0x3 /* enum */

/* MC_CMD_FC_IN_READ_MAP msgrequest */
#define	MC_CMD_FC_IN_READ_MAP_LEN 8
/*            MC_CMD_FC_IN_CMD_OFST 0 */
#define	MC_CMD_FC_IN_READ_MAP_HEADER_OFST 4
#define	MC_CMD_FC_IN_READ_MAP_OP_LBN 0
#define	MC_CMD_FC_IN_READ_MAP_OP_WIDTH 8
#define	MC_CMD_FC_OP_READ_MAP_COUNT 0x1 /* enum */
#define	MC_CMD_FC_OP_READ_MAP_INDEX 0x2 /* enum */

/* MC_CMD_FC_IN_READ_MAP_COUNT msgrequest */
#define	MC_CMD_FC_IN_READ_MAP_COUNT_LEN 8
/*            MC_CMD_FC_IN_CMD_OFST 0 */
/*            MC_CMD_FC_IN_READ_MAP_HEADER_OFST 4 */

/* MC_CMD_FC_IN_READ_MAP_INDEX msgrequest */
#define	MC_CMD_FC_IN_READ_MAP_INDEX_LEN 12
/*            MC_CMD_FC_IN_CMD_OFST 0 */
/*            MC_CMD_FC_IN_READ_MAP_HEADER_OFST 4 */
#define	MC_CMD_FC_IN_MAP_INDEX_OFST 8

/* MC_CMD_FC_IN_CAPABILITIES msgrequest */
#define	MC_CMD_FC_IN_CAPABILITIES_LEN 4
/*            MC_CMD_FC_IN_CMD_OFST 0 */

/* MC_CMD_FC_IN_GLOBAL_FLAGS msgrequest */
#define	MC_CMD_FC_IN_GLOBAL_FLAGS_LEN 8
/*            MC_CMD_FC_IN_CMD_OFST 0 */
#define	MC_CMD_FC_IN_GLOBAL_FLAGS_FLAGS_OFST 4
#define	MC_CMD_FC_IN_GLOBAL_FLAGS_RX_TUNING_CABLE_PLUGGED_IN_LBN 0
#define	MC_CMD_FC_IN_GLOBAL_FLAGS_RX_TUNING_CABLE_PLUGGED_IN_WIDTH 1
#define	MC_CMD_FC_IN_GLOBAL_FLAGS_RX_TUNING_LINK_MONITORING_LBN 1
#define	MC_CMD_FC_IN_GLOBAL_FLAGS_RX_TUNING_LINK_MONITORING_WIDTH 1
#define	MC_CMD_FC_IN_GLOBAL_FLAGS_DFE_ENABLE_LBN 2
#define	MC_CMD_FC_IN_GLOBAL_FLAGS_DFE_ENABLE_WIDTH 1
#define	MC_CMD_FC_IN_GLOBAL_FLAGS_1D_EYE_ENABLE_LBN 3
#define	MC_CMD_FC_IN_GLOBAL_FLAGS_1D_EYE_ENABLE_WIDTH 1
#define	MC_CMD_FC_IN_GLOBAL_FLAGS_1D_TUNING_ENABLE_LBN 4
#define	MC_CMD_FC_IN_GLOBAL_FLAGS_1D_TUNING_ENABLE_WIDTH 1
#define	MC_CMD_FC_IN_GLOBAL_FLAGS_OFFCAL_ENABLE_LBN 5
#define	MC_CMD_FC_IN_GLOBAL_FLAGS_OFFCAL_ENABLE_WIDTH 1

/* MC_CMD_FC_IN_IO_REL msgrequest */
#define	MC_CMD_FC_IN_IO_REL_LEN 8
/*            MC_CMD_FC_IN_CMD_OFST 0 */
#define	MC_CMD_FC_IN_IO_REL_HEADER_OFST 4
#define	MC_CMD_FC_IN_IO_REL_OP_LBN 0
#define	MC_CMD_FC_IN_IO_REL_OP_WIDTH 8
#define	MC_CMD_FC_IN_IO_REL_GET_ADDR 0x1 /* enum */
#define	MC_CMD_FC_IN_IO_REL_READ32 0x2 /* enum */
#define	MC_CMD_FC_IN_IO_REL_WRITE32 0x3 /* enum */
#define	MC_CMD_FC_IN_IO_REL_COMP_TYPE_LBN 8
#define	MC_CMD_FC_IN_IO_REL_COMP_TYPE_WIDTH 8
#define	MC_CMD_FC_COMP_TYPE_APP_ADDR_SPACE 0x1 /* enum */
#define	MC_CMD_FC_COMP_TYPE_FLASH 0x2 /* enum */

/* MC_CMD_FC_IN_IO_REL_GET_ADDR msgrequest */
#define	MC_CMD_FC_IN_IO_REL_GET_ADDR_LEN 8
/*            MC_CMD_FC_IN_CMD_OFST 0 */
/*            MC_CMD_FC_IN_IO_REL_HEADER_OFST 4 */

/* MC_CMD_FC_IN_IO_REL_READ32 msgrequest */
#define	MC_CMD_FC_IN_IO_REL_READ32_LEN 20
/*            MC_CMD_FC_IN_CMD_OFST 0 */
/*            MC_CMD_FC_IN_IO_REL_HEADER_OFST 4 */
#define	MC_CMD_FC_IN_IO_REL_READ32_ADDR_HI_OFST 8
#define	MC_CMD_FC_IN_IO_REL_READ32_ADDR_LO_OFST 12
#define	MC_CMD_FC_IN_IO_REL_READ32_NUMWORDS_OFST 16

/* MC_CMD_FC_IN_IO_REL_WRITE32 msgrequest */
#define	MC_CMD_FC_IN_IO_REL_WRITE32_LENMIN 20
#define	MC_CMD_FC_IN_IO_REL_WRITE32_LENMAX 252
#define	MC_CMD_FC_IN_IO_REL_WRITE32_LEN(num) (16+4*(num))
/*            MC_CMD_FC_IN_CMD_OFST 0 */
/*            MC_CMD_FC_IN_IO_REL_HEADER_OFST 4 */
#define	MC_CMD_FC_IN_IO_REL_WRITE32_ADDR_HI_OFST 8
#define	MC_CMD_FC_IN_IO_REL_WRITE32_ADDR_LO_OFST 12
#define	MC_CMD_FC_IN_IO_REL_WRITE32_BUFFER_OFST 16
#define	MC_CMD_FC_IN_IO_REL_WRITE32_BUFFER_LEN 4
#define	MC_CMD_FC_IN_IO_REL_WRITE32_BUFFER_MINNUM 1
#define	MC_CMD_FC_IN_IO_REL_WRITE32_BUFFER_MAXNUM 59

/* MC_CMD_FC_IN_UHLINK msgrequest */
#define	MC_CMD_FC_IN_UHLINK_LEN 8
/*            MC_CMD_FC_IN_CMD_OFST 0 */
#define	MC_CMD_FC_IN_UHLINK_HEADER_OFST 4
#define	MC_CMD_FC_IN_UHLINK_OP_LBN 0
#define	MC_CMD_FC_IN_UHLINK_OP_WIDTH 8
#define	MC_CMD_FC_OP_UHLINK_PHY 0x1 /* enum */
#define	MC_CMD_FC_OP_UHLINK_MAC 0x2 /* enum */
#define	MC_CMD_FC_OP_UHLINK_RX_EYE 0x3 /* enum */
#define	MC_CMD_FC_OP_UHLINK_DUMP_RX_EYE_PLOT 0x4 /* enum */
#define	MC_CMD_FC_OP_UHLINK_READ_RX_EYE_PLOT 0x5 /* enum */
#define	MC_CMD_FC_OP_UHLINK_RX_TUNE 0x6 /* enum */
#define	MC_CMD_FC_OP_UHLINK_LOOPBACK_SET 0x7 /* enum */
#define	MC_CMD_FC_OP_UHLINK_LOOPBACK_GET 0x8 /* enum */
#define	MC_CMD_FC_IN_UHLINK_PORT_TYPE_LBN 8
#define	MC_CMD_FC_IN_UHLINK_PORT_TYPE_WIDTH 8
#define	MC_CMD_FC_IN_UHLINK_PORT_IDX_LBN 16
#define	MC_CMD_FC_IN_UHLINK_PORT_IDX_WIDTH 8
#define	MC_CMD_FC_IN_UHLINK_CMD_FORMAT_LBN 24
#define	MC_CMD_FC_IN_UHLINK_CMD_FORMAT_WIDTH 8
#define	MC_CMD_FC_OP_UHLINK_CMD_FORMAT_DEFAULT 0x0 /* enum */
#define	MC_CMD_FC_OP_UHLINK_CMD_FORMAT_PORT_OVERRIDE 0x1 /* enum */

/* MC_CMD_FC_OP_UHLINK_PHY msgrequest */
#define	MC_CMD_FC_OP_UHLINK_PHY_LEN 8
/*            MC_CMD_FC_IN_CMD_OFST 0 */
/*            MC_CMD_FC_IN_UHLINK_HEADER_OFST 4 */

/* MC_CMD_FC_OP_UHLINK_MAC msgrequest */
#define	MC_CMD_FC_OP_UHLINK_MAC_LEN 8
/*            MC_CMD_FC_IN_CMD_OFST 0 */
/*            MC_CMD_FC_IN_UHLINK_HEADER_OFST 4 */

/* MC_CMD_FC_OP_UHLINK_RX_EYE msgrequest */
#define	MC_CMD_FC_OP_UHLINK_RX_EYE_LEN 12
/*            MC_CMD_FC_IN_CMD_OFST 0 */
/*            MC_CMD_FC_IN_UHLINK_HEADER_OFST 4 */
#define	MC_CMD_FC_OP_UHLINK_RX_EYE_INDEX_OFST 8
#define	MC_CMD_FC_UHLINK_RX_EYE_PER_BLOCK 0x30 /* enum */

/* MC_CMD_FC_OP_UHLINK_DUMP_RX_EYE_PLOT msgrequest */
#define	MC_CMD_FC_OP_UHLINK_DUMP_RX_EYE_PLOT_LEN 8
/*            MC_CMD_FC_IN_CMD_OFST 0 */
/*            MC_CMD_FC_IN_UHLINK_HEADER_OFST 4 */

/* MC_CMD_FC_OP_UHLINK_READ_RX_EYE_PLOT msgrequest */
#define	MC_CMD_FC_OP_UHLINK_READ_RX_EYE_PLOT_LEN 20
/*            MC_CMD_FC_IN_CMD_OFST 0 */
/*            MC_CMD_FC_IN_UHLINK_HEADER_OFST 4 */
#define	MC_CMD_FC_OP_UHLINK_READ_RX_EYE_PLOT_DC_GAIN_OFST 8
#define	MC_CMD_FC_OP_UHLINK_READ_RX_EYE_PLOT_EQ_CONTROL_OFST 12
#define	MC_CMD_FC_OP_UHLINK_READ_RX_EYE_PLOT_INDEX_OFST 16
#define	MC_CMD_FC_UHLINK_RX_EYE_PLOT_ROWS_PER_BLOCK 0x1e /* enum */

/* MC_CMD_FC_OP_UHLINK_RX_TUNE msgrequest */
#define	MC_CMD_FC_OP_UHLINK_RX_TUNE_LEN 8
/*            MC_CMD_FC_IN_CMD_OFST 0 */
/*            MC_CMD_FC_IN_UHLINK_HEADER_OFST 4 */

/* MC_CMD_FC_OP_UHLINK_LOOPBACK_SET msgrequest */
#define	MC_CMD_FC_OP_UHLINK_LOOPBACK_SET_LEN 16
/*            MC_CMD_FC_IN_CMD_OFST 0 */
/*            MC_CMD_FC_IN_UHLINK_HEADER_OFST 4 */
#define	MC_CMD_FC_OP_UHLINK_LOOPBACK_SET_TYPE_OFST 8
#define	MC_CMD_FC_UHLINK_LOOPBACK_TYPE_PCS_SERIAL 0x0 /* enum */
#define	MC_CMD_FC_UHLINK_LOOPBACK_TYPE_PMA_PRE_CDR 0x1 /* enum */
#define	MC_CMD_FC_UHLINK_LOOPBACK_TYPE_PMA_POST_CDR 0x2 /* enum */
#define	MC_CMD_FC_OP_UHLINK_LOOPBACK_SET_STATE_OFST 12
#define	MC_CMD_FC_UHLINK_LOOPBACK_STATE_OFF 0x0 /* enum */
#define	MC_CMD_FC_UHLINK_LOOPBACK_STATE_ON 0x1 /* enum */

/* MC_CMD_FC_OP_UHLINK_LOOPBACK_GET msgrequest */
#define	MC_CMD_FC_OP_UHLINK_LOOPBACK_GET_LEN 12
/*            MC_CMD_FC_IN_CMD_OFST 0 */
/*            MC_CMD_FC_IN_UHLINK_HEADER_OFST 4 */
#define	MC_CMD_FC_OP_UHLINK_LOOPBACK_GET_TYPE_OFST 8

/* MC_CMD_FC_IN_SET_LINK msgrequest */
#define	MC_CMD_FC_IN_SET_LINK_LEN 16
/*            MC_CMD_FC_IN_CMD_OFST 0 */
#define	MC_CMD_FC_IN_SET_LINK_MODE_OFST 4
#define	MC_CMD_FC_IN_SET_LINK_SPEED_OFST 8
#define	MC_CMD_FC_IN_SET_LINK_FLAGS_OFST 12
#define	MC_CMD_FC_IN_SET_LINK_LOWPOWER_LBN 0
#define	MC_CMD_FC_IN_SET_LINK_LOWPOWER_WIDTH 1
#define	MC_CMD_FC_IN_SET_LINK_POWEROFF_LBN 1
#define	MC_CMD_FC_IN_SET_LINK_POWEROFF_WIDTH 1
#define	MC_CMD_FC_IN_SET_LINK_TXDIS_LBN 2
#define	MC_CMD_FC_IN_SET_LINK_TXDIS_WIDTH 1

/* MC_CMD_FC_IN_LICENSE msgrequest */
#define	MC_CMD_FC_IN_LICENSE_LEN 8
/*            MC_CMD_FC_IN_CMD_OFST 0 */
#define	MC_CMD_FC_IN_LICENSE_OP_OFST 4
#define	MC_CMD_FC_IN_LICENSE_UPDATE_LICENSE 0x0 /* enum */
#define	MC_CMD_FC_IN_LICENSE_GET_KEY_STATS 0x1 /* enum */

/* MC_CMD_FC_IN_STARTUP msgrequest */
#define	MC_CMD_FC_IN_STARTUP_LEN 40
/*            MC_CMD_FC_IN_CMD_OFST 0 */
#define	MC_CMD_FC_IN_STARTUP_BASE_OFST 4
#define	MC_CMD_FC_IN_STARTUP_LENGTH_OFST 8
#define	MC_CMD_FC_IN_STARTUP_IDLENGTH_OFST 12
#define	MC_CMD_FC_IN_STARTUP_ID_OFST 16
#define	MC_CMD_FC_IN_STARTUP_ID_LEN 1
#define	MC_CMD_FC_IN_STARTUP_ID_NUM 24

/* MC_CMD_FC_IN_DMA msgrequest */
#define	MC_CMD_FC_IN_DMA_LEN 8
/*            MC_CMD_FC_IN_CMD_OFST 0 */
#define	MC_CMD_FC_IN_DMA_OP_OFST 4
#define	MC_CMD_FC_IN_DMA_STOP  0x0 /* enum */
#define	MC_CMD_FC_IN_DMA_READ  0x1 /* enum */

/* MC_CMD_FC_IN_DMA_STOP msgrequest */
#define	MC_CMD_FC_IN_DMA_STOP_LEN 12
/*            MC_CMD_FC_IN_CMD_OFST 0 */
/*            MC_CMD_FC_IN_DMA_OP_OFST 4 */
#define	MC_CMD_FC_IN_DMA_STOP_FC_HANDLE_OFST 8

/* MC_CMD_FC_IN_DMA_READ msgrequest */
#define	MC_CMD_FC_IN_DMA_READ_LEN 16
/*            MC_CMD_FC_IN_CMD_OFST 0 */
/*            MC_CMD_FC_IN_DMA_OP_OFST 4 */
#define	MC_CMD_FC_IN_DMA_READ_OFFSET_OFST 8
#define	MC_CMD_FC_IN_DMA_READ_LENGTH_OFST 12

/* MC_CMD_FC_IN_TIMED_READ msgrequest */
#define	MC_CMD_FC_IN_TIMED_READ_LEN 8
/*            MC_CMD_FC_IN_CMD_OFST 0 */
#define	MC_CMD_FC_IN_TIMED_READ_OP_OFST 4
#define	MC_CMD_FC_IN_TIMED_READ_SET  0x0 /* enum */
#define	MC_CMD_FC_IN_TIMED_READ_GET  0x1 /* enum */
#define	MC_CMD_FC_IN_TIMED_READ_CLEAR  0x2 /* enum */

/* MC_CMD_FC_IN_TIMED_READ_SET msgrequest */
#define	MC_CMD_FC_IN_TIMED_READ_SET_LEN 52
/*            MC_CMD_FC_IN_CMD_OFST 0 */
/*            MC_CMD_FC_IN_TIMED_READ_OP_OFST 4 */
#define	MC_CMD_FC_IN_TIMED_READ_SET_HOST_HANDLE_OFST 8
#define	MC_CMD_FC_IN_TIMED_READ_SET_HOST_DMA_ADDRESS_OFST 12
#define	MC_CMD_FC_IN_TIMED_READ_SET_HOST_DMA_ADDRESS_LEN 8
#define	MC_CMD_FC_IN_TIMED_READ_SET_HOST_DMA_ADDRESS_LO_OFST 12
#define	MC_CMD_FC_IN_TIMED_READ_SET_HOST_DMA_ADDRESS_HI_OFST 16
#define	MC_CMD_FC_IN_TIMED_READ_SET_AOE_ADDRESS_OFST 20
#define	MC_CMD_FC_IN_TIMED_READ_SET_AOE_ADDRESS_LEN 8
#define	MC_CMD_FC_IN_TIMED_READ_SET_AOE_ADDRESS_LO_OFST 20
#define	MC_CMD_FC_IN_TIMED_READ_SET_AOE_ADDRESS_HI_OFST 24
#define	MC_CMD_FC_IN_TIMED_READ_SET_AOE_LENGTH_OFST 28
#define	MC_CMD_FC_IN_TIMED_READ_SET_HOST_LENGTH_OFST 32
#define	MC_CMD_FC_IN_TIMED_READ_SET_OFFSET_OFST 36
#define	MC_CMD_FC_IN_TIMED_READ_SET_DATA_OFST 40
#define	MC_CMD_FC_IN_TIMED_READ_SET_FLAGS_OFST 44
#define	MC_CMD_FC_IN_TIMED_READ_SET_INDIRECT_LBN 0
#define	MC_CMD_FC_IN_TIMED_READ_SET_INDIRECT_WIDTH 1
#define	MC_CMD_FC_IN_TIMED_READ_SET_DOUBLE_LBN 1
#define	MC_CMD_FC_IN_TIMED_READ_SET_DOUBLE_WIDTH 1
#define	MC_CMD_FC_IN_TIMED_READ_SET_EVENT_LBN 2
#define	MC_CMD_FC_IN_TIMED_READ_SET_EVENT_WIDTH 1
#define	MC_CMD_FC_IN_TIMED_READ_SET_PREREAD_LBN 3
#define	MC_CMD_FC_IN_TIMED_READ_SET_PREREAD_WIDTH 2
#define	MC_CMD_FC_IN_TIMED_READ_SET_NONE  0x0 /* enum */
#define	MC_CMD_FC_IN_TIMED_READ_SET_READ  0x1 /* enum */
#define	MC_CMD_FC_IN_TIMED_READ_SET_WRITE  0x2 /* enum */
#define	MC_CMD_FC_IN_TIMED_READ_SET_READWRITE  0x3 /* enum */
#define	MC_CMD_FC_IN_TIMED_READ_SET_PERIOD_OFST 48

/* MC_CMD_FC_IN_TIMED_READ_GET msgrequest */
#define	MC_CMD_FC_IN_TIMED_READ_GET_LEN 12
/*            MC_CMD_FC_IN_CMD_OFST 0 */
/*            MC_CMD_FC_IN_TIMED_READ_OP_OFST 4 */
#define	MC_CMD_FC_IN_TIMED_READ_GET_FC_HANDLE_OFST 8

/* MC_CMD_FC_IN_TIMED_READ_CLEAR msgrequest */
#define	MC_CMD_FC_IN_TIMED_READ_CLEAR_LEN 12
/*            MC_CMD_FC_IN_CMD_OFST 0 */
/*            MC_CMD_FC_IN_TIMED_READ_OP_OFST 4 */
#define	MC_CMD_FC_IN_TIMED_READ_CLEAR_FC_HANDLE_OFST 8

/* MC_CMD_FC_IN_LOG msgrequest */
#define	MC_CMD_FC_IN_LOG_LEN 8
/*            MC_CMD_FC_IN_CMD_OFST 0 */
#define	MC_CMD_FC_IN_LOG_OP_OFST 4
#define	MC_CMD_FC_IN_LOG_ADDR_RANGE  0x0 /* enum */
#define	MC_CMD_FC_IN_LOG_JTAG_UART  0x1 /* enum */

/* MC_CMD_FC_IN_LOG_ADDR_RANGE msgrequest */
#define	MC_CMD_FC_IN_LOG_ADDR_RANGE_LEN 20
/*            MC_CMD_FC_IN_CMD_OFST 0 */
/*            MC_CMD_FC_IN_LOG_OP_OFST 4 */
#define	MC_CMD_FC_IN_LOG_ADDR_RANGE_OFFSET_OFST 8
#define	MC_CMD_FC_IN_LOG_ADDR_RANGE_LENGTH_OFST 12
#define	MC_CMD_FC_IN_LOG_ADDR_RANGE_ERASE_SIZE_OFST 16

/* MC_CMD_FC_IN_LOG_JTAG_UART msgrequest */
#define	MC_CMD_FC_IN_LOG_JTAG_UART_LEN 12
/*            MC_CMD_FC_IN_CMD_OFST 0 */
/*            MC_CMD_FC_IN_LOG_OP_OFST 4 */
#define	MC_CMD_FC_IN_LOG_JTAG_UART_ENABLE_OFST 8

/* MC_CMD_FC_IN_CLOCK msgrequest */
#define	MC_CMD_FC_IN_CLOCK_LEN 12
/*            MC_CMD_FC_IN_CMD_OFST 0 */
#define	MC_CMD_FC_IN_CLOCK_OP_OFST 4
#define	MC_CMD_FC_IN_CLOCK_GET_TIME  0x0 /* enum */
#define	MC_CMD_FC_IN_CLOCK_SET_TIME  0x1 /* enum */
#define	MC_CMD_FC_IN_CLOCK_ID_OFST 8
#define	MC_CMD_FC_IN_CLOCK_STATS  0x0 /* enum */
#define	MC_CMD_FC_IN_CLOCK_MAC  0x1 /* enum */

/* MC_CMD_FC_IN_CLOCK_GET_TIME msgrequest */
#define	MC_CMD_FC_IN_CLOCK_GET_TIME_LEN 12
/*            MC_CMD_FC_IN_CMD_OFST 0 */
/*            MC_CMD_FC_IN_CLOCK_OP_OFST 4 */
/*            MC_CMD_FC_IN_CLOCK_ID_OFST 8 */

/* MC_CMD_FC_IN_CLOCK_SET_TIME msgrequest */
#define	MC_CMD_FC_IN_CLOCK_SET_TIME_LEN 24
/*            MC_CMD_FC_IN_CMD_OFST 0 */
/*            MC_CMD_FC_IN_CLOCK_OP_OFST 4 */
/*            MC_CMD_FC_IN_CLOCK_ID_OFST 8 */
#define	MC_CMD_FC_IN_CLOCK_SET_TIME_SECONDS_OFST 12
#define	MC_CMD_FC_IN_CLOCK_SET_TIME_SECONDS_LEN 8
#define	MC_CMD_FC_IN_CLOCK_SET_TIME_SECONDS_LO_OFST 12
#define	MC_CMD_FC_IN_CLOCK_SET_TIME_SECONDS_HI_OFST 16
#define	MC_CMD_FC_IN_CLOCK_SET_TIME_NANOSECONDS_OFST 20

/* MC_CMD_FC_IN_DDR msgrequest */
#define	MC_CMD_FC_IN_DDR_LEN 12
/*            MC_CMD_FC_IN_CMD_OFST 0 */
#define	MC_CMD_FC_IN_DDR_OP_OFST 4
#define	MC_CMD_FC_IN_DDR_SET_SPD  0x0 /* enum */
#define	MC_CMD_FC_IN_DDR_GET_STATUS  0x1 /* enum */
#define	MC_CMD_FC_IN_DDR_BANK_OFST 8
#define	MC_CMD_FC_IN_DDR_BANK_B0  0x0 /* enum */
#define	MC_CMD_FC_IN_DDR_BANK_B1  0x1 /* enum */
#define	MC_CMD_FC_IN_DDR_BANK_T0  0x2 /* enum */
#define	MC_CMD_FC_IN_DDR_BANK_T1  0x3 /* enum */
#define	MC_CMD_FC_IN_DDR_NUM_BANKS  0x4 /* enum */

/* MC_CMD_FC_IN_DDR_SET_SPD msgrequest */
#define	MC_CMD_FC_IN_DDR_SET_SPD_LEN 148
/*            MC_CMD_FC_IN_CMD_OFST 0 */
/*            MC_CMD_FC_IN_DDR_OP_OFST 4 */
/*            MC_CMD_FC_IN_DDR_BANK_OFST 8 */
#define	MC_CMD_FC_IN_DDR_FLAGS_OFST 12
#define	MC_CMD_FC_IN_DDR_SET_SPD_ACTIVE  0x1 /* enum */
#define	MC_CMD_FC_IN_DDR_SPD_OFST 16
#define	MC_CMD_FC_IN_DDR_SPD_LEN 1
#define	MC_CMD_FC_IN_DDR_SPD_NUM 128
#define	MC_CMD_FC_IN_DDR_SPD_PAGE_ID_OFST 144

/* MC_CMD_FC_IN_DDR_GET_STATUS msgrequest */
#define	MC_CMD_FC_IN_DDR_GET_STATUS_LEN 12
/*            MC_CMD_FC_IN_CMD_OFST 0 */
/*            MC_CMD_FC_IN_DDR_OP_OFST 4 */
/*            MC_CMD_FC_IN_DDR_BANK_OFST 8 */

/* MC_CMD_FC_IN_TIMESTAMP msgrequest */
#define	MC_CMD_FC_IN_TIMESTAMP_LEN 8
/*            MC_CMD_FC_IN_CMD_OFST 0 */
#define	MC_CMD_FC_IN_TIMESTAMP_OP_OFST 4
#define	MC_CMD_FC_IN_TIMESTAMP_READ_TRANSMIT 0x0 /* enum */
#define	MC_CMD_FC_IN_TIMESTAMP_READ_SNAPSHOT 0x1 /* enum */
#define	MC_CMD_FC_IN_TIMESTAMP_CLEAR_TRANSMIT 0x2 /* enum */

/* MC_CMD_FC_IN_TIMESTAMP_READ_TRANSMIT msgrequest */
#define	MC_CMD_FC_IN_TIMESTAMP_READ_TRANSMIT_LEN 28
/*            MC_CMD_FC_IN_CMD_OFST 0 */
#define	MC_CMD_FC_IN_TIMESTAMP_READ_TRANSMIT_OP_OFST 4
#define	MC_CMD_FC_IN_TIMESTAMP_READ_TRANSMIT_FILTER_OFST 8
#define	MC_CMD_FC_IN_TIMESTAMP_READ_TRANSMIT_LATEST 0x0 /* enum */
#define	MC_CMD_FC_IN_TIMESTAMP_READ_TRANSMIT_MATCH 0x1 /* enum */
#define	MC_CMD_FC_IN_TIMESTAMP_READ_TRANSMIT_CLOCK_ID_OFST 12
#define	MC_CMD_FC_IN_TIMESTAMP_READ_TRANSMIT_CLOCK_ID_LEN 8
#define	MC_CMD_FC_IN_TIMESTAMP_READ_TRANSMIT_CLOCK_ID_LO_OFST 12
#define	MC_CMD_FC_IN_TIMESTAMP_READ_TRANSMIT_CLOCK_ID_HI_OFST 16
#define	MC_CMD_FC_IN_TIMESTAMP_READ_TRANSMIT_PORT_NUM_OFST 20
#define	MC_CMD_FC_IN_TIMESTAMP_READ_TRANSMIT_SEQ_NUM_OFST 24

/* MC_CMD_FC_IN_TIMESTAMP_READ_SNAPSHOT msgrequest */
#define	MC_CMD_FC_IN_TIMESTAMP_READ_SNAPSHOT_LEN 8
/*            MC_CMD_FC_IN_CMD_OFST 0 */
#define	MC_CMD_FC_IN_TIMESTAMP_READ_SNAPSHOT_OP_OFST 4

/* MC_CMD_FC_IN_TIMESTAMP_CLEAR_TRANSMIT msgrequest */
#define	MC_CMD_FC_IN_TIMESTAMP_CLEAR_TRANSMIT_LEN 8
/*            MC_CMD_FC_IN_CMD_OFST 0 */
#define	MC_CMD_FC_IN_TIMESTAMP_CLEAR_TRANSMIT_OP_OFST 4

/* MC_CMD_FC_IN_SPI msgrequest */
#define	MC_CMD_FC_IN_SPI_LEN 8
/*            MC_CMD_FC_IN_CMD_OFST 0 */
#define	MC_CMD_FC_IN_SPI_OP_OFST 4
#define	MC_CMD_FC_IN_SPI_READ 0x0 /* enum */
#define	MC_CMD_FC_IN_SPI_WRITE 0x1 /* enum */
#define	MC_CMD_FC_IN_SPI_ERASE 0x2 /* enum */

/* MC_CMD_FC_IN_SPI_READ msgrequest */
#define	MC_CMD_FC_IN_SPI_READ_LEN 16
/*            MC_CMD_FC_IN_CMD_OFST 0 */
#define	MC_CMD_FC_IN_SPI_READ_OP_OFST 4
#define	MC_CMD_FC_IN_SPI_READ_ADDR_OFST 8
#define	MC_CMD_FC_IN_SPI_READ_NUMBYTES_OFST 12

/* MC_CMD_FC_IN_SPI_WRITE msgrequest */
#define	MC_CMD_FC_IN_SPI_WRITE_LENMIN 16
#define	MC_CMD_FC_IN_SPI_WRITE_LENMAX 252
#define	MC_CMD_FC_IN_SPI_WRITE_LEN(num) (12+4*(num))
/*            MC_CMD_FC_IN_CMD_OFST 0 */
#define	MC_CMD_FC_IN_SPI_WRITE_OP_OFST 4
#define	MC_CMD_FC_IN_SPI_WRITE_ADDR_OFST 8
#define	MC_CMD_FC_IN_SPI_WRITE_BUFFER_OFST 12
#define	MC_CMD_FC_IN_SPI_WRITE_BUFFER_LEN 4
#define	MC_CMD_FC_IN_SPI_WRITE_BUFFER_MINNUM 1
#define	MC_CMD_FC_IN_SPI_WRITE_BUFFER_MAXNUM 60

/* MC_CMD_FC_IN_SPI_ERASE msgrequest */
#define	MC_CMD_FC_IN_SPI_ERASE_LEN 16
/*            MC_CMD_FC_IN_CMD_OFST 0 */
#define	MC_CMD_FC_IN_SPI_ERASE_OP_OFST 4
#define	MC_CMD_FC_IN_SPI_ERASE_ADDR_OFST 8
#define	MC_CMD_FC_IN_SPI_ERASE_NUMBYTES_OFST 12

/* MC_CMD_FC_IN_DIAG msgrequest */
#define	MC_CMD_FC_IN_DIAG_LEN 8
/*            MC_CMD_FC_IN_CMD_OFST 0 */
#define	MC_CMD_FC_IN_DIAG_OP_OFST 4
#define	MC_CMD_FC_IN_DIAG_POWER_NOISE 0x0 /* enum */
#define	MC_CMD_FC_IN_DIAG_DDR_SOAK 0x1 /* enum */
#define	MC_CMD_FC_IN_DIAG_DATAPATH_CTRL 0x2 /* enum */

/* MC_CMD_FC_IN_DIAG_POWER_NOISE msgrequest */
#define	MC_CMD_FC_IN_DIAG_POWER_NOISE_LEN 12
/*            MC_CMD_FC_IN_CMD_OFST 0 */
#define	MC_CMD_FC_IN_DIAG_POWER_NOISE_OP_OFST 4
#define	MC_CMD_FC_IN_DIAG_POWER_NOISE_SUB_OP_OFST 8
#define	MC_CMD_FC_IN_DIAG_POWER_NOISE_READ_CONFIG 0x0 /* enum */
#define	MC_CMD_FC_IN_DIAG_POWER_NOISE_WRITE_CONFIG 0x1 /* enum */

/* MC_CMD_FC_IN_DIAG_POWER_NOISE_READ_CONFIG msgrequest */
#define	MC_CMD_FC_IN_DIAG_POWER_NOISE_READ_CONFIG_LEN 12
/*            MC_CMD_FC_IN_CMD_OFST 0 */
#define	MC_CMD_FC_IN_DIAG_POWER_NOISE_READ_CONFIG_OP_OFST 4
#define	MC_CMD_FC_IN_DIAG_POWER_NOISE_READ_CONFIG_SUB_OP_OFST 8

/* MC_CMD_FC_IN_DIAG_POWER_NOISE_WRITE_CONFIG msgrequest */
#define	MC_CMD_FC_IN_DIAG_POWER_NOISE_WRITE_CONFIG_LEN 20
/*            MC_CMD_FC_IN_CMD_OFST 0 */
#define	MC_CMD_FC_IN_DIAG_POWER_NOISE_WRITE_CONFIG_OP_OFST 4
#define	MC_CMD_FC_IN_DIAG_POWER_NOISE_WRITE_CONFIG_SUB_OP_OFST 8
#define	MC_CMD_FC_IN_DIAG_POWER_NOISE_WRITE_CONFIG_TOGGLE_COUNT_OFST 12
#define	MC_CMD_FC_IN_DIAG_POWER_NOISE_WRITE_CONFIG_CLKEN_COUNT_OFST 16

/* MC_CMD_FC_IN_DIAG_DDR_SOAK msgrequest */
#define	MC_CMD_FC_IN_DIAG_DDR_SOAK_LEN 12
/*            MC_CMD_FC_IN_CMD_OFST 0 */
#define	MC_CMD_FC_IN_DIAG_DDR_SOAK_OP_OFST 4
#define	MC_CMD_FC_IN_DIAG_DDR_SOAK_SUB_OP_OFST 8
#define	MC_CMD_FC_IN_DIAG_DDR_SOAK_START 0x0 /* enum */
#define	MC_CMD_FC_IN_DIAG_DDR_SOAK_RESULT 0x1 /* enum */
#define	MC_CMD_FC_IN_DIAG_DDR_SOAK_STOP 0x2 /* enum */
#define	MC_CMD_FC_IN_DIAG_DDR_SOAK_ERROR 0x3 /* enum */

/* MC_CMD_FC_IN_DIAG_DDR_SOAK_START msgrequest */
#define	MC_CMD_FC_IN_DIAG_DDR_SOAK_START_LEN 24
/*            MC_CMD_FC_IN_CMD_OFST 0 */
#define	MC_CMD_FC_IN_DIAG_DDR_SOAK_START_OP_OFST 4
#define	MC_CMD_FC_IN_DIAG_DDR_SOAK_START_SUB_OP_OFST 8
#define	MC_CMD_FC_IN_DIAG_DDR_SOAK_START_BANK_MASK_OFST 12
#define	MC_CMD_FC_IN_DIAG_DDR_SOAK_START_TEST_PATTERN_OFST 16
#define	MC_CMD_FC_IN_DIAG_DDR_SOAK_START_ZEROS 0x0 /* enum */
#define	MC_CMD_FC_IN_DIAG_DDR_SOAK_START_ONES 0x1 /* enum */
#define	MC_CMD_FC_IN_DIAG_DDR_SOAK_START_TEST_TYPE_OFST 20
#define	MC_CMD_FC_IN_DIAG_DDR_SOAK_START_ONGOING_TEST 0x0 /* enum */
#define	MC_CMD_FC_IN_DIAG_DDR_SOAK_START_SINGLE_TEST 0x1 /* enum */

/* MC_CMD_FC_IN_DIAG_DDR_SOAK_RESULT msgrequest */
#define	MC_CMD_FC_IN_DIAG_DDR_SOAK_RESULT_LEN 16
/*            MC_CMD_FC_IN_CMD_OFST 0 */
#define	MC_CMD_FC_IN_DIAG_DDR_SOAK_RESULT_OP_OFST 4
#define	MC_CMD_FC_IN_DIAG_DDR_SOAK_RESULT_SUB_OP_OFST 8
#define	MC_CMD_FC_IN_DIAG_DDR_SOAK_RESULT_BANK_ID_OFST 12
#define	MC_CMD_FC_DDR_BANK0 0x0 /* enum */
#define	MC_CMD_FC_DDR_BANK1 0x1 /* enum */
#define	MC_CMD_FC_DDR_BANK2 0x2 /* enum */
#define	MC_CMD_FC_DDR_BANK3 0x3 /* enum */
#define	MC_CMD_FC_DDR_AOEMEM_MAX_BANKS 0x4 /* enum */

/* MC_CMD_FC_IN_DIAG_DDR_SOAK_STOP msgrequest */
#define	MC_CMD_FC_IN_DIAG_DDR_SOAK_STOP_LEN 16
/*            MC_CMD_FC_IN_CMD_OFST 0 */
#define	MC_CMD_FC_IN_DIAG_DDR_SOAK_STOP_OP_OFST 4
#define	MC_CMD_FC_IN_DIAG_DDR_SOAK_STOP_SUB_OP_OFST 8
#define	MC_CMD_FC_IN_DIAG_DDR_SOAK_STOP_BANK_MASK_OFST 12

/* MC_CMD_FC_IN_DIAG_DDR_SOAK_ERROR msgrequest */
#define	MC_CMD_FC_IN_DIAG_DDR_SOAK_ERROR_LEN 20
/*            MC_CMD_FC_IN_CMD_OFST 0 */
#define	MC_CMD_FC_IN_DIAG_DDR_SOAK_ERROR_OP_OFST 4
#define	MC_CMD_FC_IN_DIAG_DDR_SOAK_ERROR_SUB_OP_OFST 8
#define	MC_CMD_FC_IN_DIAG_DDR_SOAK_ERROR_BANK_MASK_OFST 12
#define	MC_CMD_FC_IN_DIAG_DDR_SOAK_ERROR_FLAG_ACTION_OFST 16
#define	MC_CMD_FC_IN_DIAG_DDR_SOAK_ERROR_CLEAR 0x0 /* enum */
#define	MC_CMD_FC_IN_DIAG_DDR_SOAK_ERROR_SET 0x1 /* enum */

/* MC_CMD_FC_IN_DIAG_DATAPATH_CTRL msgrequest */
#define	MC_CMD_FC_IN_DIAG_DATAPATH_CTRL_LEN 12
/*            MC_CMD_FC_IN_CMD_OFST 0 */
#define	MC_CMD_FC_IN_DIAG_DATAPATH_CTRL_OP_OFST 4
#define	MC_CMD_FC_IN_DIAG_DATAPATH_CTRL_SUB_OP_OFST 8
#define	MC_CMD_FC_IN_DIAG_DATAPATH_CTRL_SET_MODE 0x0 /* enum */
#define	MC_CMD_FC_IN_DIAG_DATAPATH_CTRL_RAW_CONFIG 0x1 /* enum */

/* MC_CMD_FC_IN_DIAG_DATAPATH_CTRL_SET_MODE msgrequest */
#define	MC_CMD_FC_IN_DIAG_DATAPATH_CTRL_SET_MODE_LEN 16
/*            MC_CMD_FC_IN_CMD_OFST 0 */
#define	MC_CMD_FC_IN_DIAG_DATAPATH_CTRL_SET_MODE_OP_OFST 4
#define	MC_CMD_FC_IN_DIAG_DATAPATH_CTRL_SET_MODE_SUB_OP_OFST 8
#define	MC_CMD_FC_IN_DIAG_DATAPATH_CTRL_SET_MODE_MODE_OFST 12
#define	MC_CMD_FC_IN_DIAG_DATAPATH_CTRL_SET_MODE_PASSTHROUGH 0x0 /* enum */
#define	MC_CMD_FC_IN_DIAG_DATAPATH_CTRL_SET_MODE_SNAKE 0x1 /* enum */

/* MC_CMD_FC_IN_DIAG_DATAPATH_CTRL_RAW_CONFIG msgrequest */
#define	MC_CMD_FC_IN_DIAG_DATAPATH_CTRL_RAW_CONFIG_LEN 24
/*            MC_CMD_FC_IN_CMD_OFST 0 */
#define	MC_CMD_FC_IN_DIAG_DATAPATH_CTRL_RAW_CONFIG_OP_OFST 4
#define	MC_CMD_FC_IN_DIAG_DATAPATH_CTRL_RAW_CONFIG_SUB_OP_OFST 8
#define	MC_CMD_FC_IN_DIAG_DATAPATH_CTRL_RAW_CONFIG_CONTROL1_OFST 12
#define	MC_CMD_FC_IN_DIAG_DATAPATH_CTRL_RAW_CONFIG_CONTROL2_OFST 16
#define	MC_CMD_FC_IN_DIAG_DATAPATH_CTRL_RAW_CONFIG_CONTROL3_OFST 20

/* MC_CMD_FC_OUT msgresponse */
#define	MC_CMD_FC_OUT_LEN 0

/* MC_CMD_FC_OUT_NULL msgresponse */
#define	MC_CMD_FC_OUT_NULL_LEN 0

/* MC_CMD_FC_OUT_READ32 msgresponse */
#define	MC_CMD_FC_OUT_READ32_LENMIN 4
#define	MC_CMD_FC_OUT_READ32_LENMAX 252
#define	MC_CMD_FC_OUT_READ32_LEN(num) (0+4*(num))
#define	MC_CMD_FC_OUT_READ32_BUFFER_OFST 0
#define	MC_CMD_FC_OUT_READ32_BUFFER_LEN 4
#define	MC_CMD_FC_OUT_READ32_BUFFER_MINNUM 1
#define	MC_CMD_FC_OUT_READ32_BUFFER_MAXNUM 63

/* MC_CMD_FC_OUT_WRITE32 msgresponse */
#define	MC_CMD_FC_OUT_WRITE32_LEN 0

/* MC_CMD_FC_OUT_TRC_READ msgresponse */
#define	MC_CMD_FC_OUT_TRC_READ_LEN 16
#define	MC_CMD_FC_OUT_TRC_READ_DATA_OFST 0
#define	MC_CMD_FC_OUT_TRC_READ_DATA_LEN 4
#define	MC_CMD_FC_OUT_TRC_READ_DATA_NUM 4

/* MC_CMD_FC_OUT_TRC_WRITE msgresponse */
#define	MC_CMD_FC_OUT_TRC_WRITE_LEN 0

/* MC_CMD_FC_OUT_GET_VERSION msgresponse */
#define	MC_CMD_FC_OUT_GET_VERSION_LEN 12
#define	MC_CMD_FC_OUT_GET_VERSION_FIRMWARE_OFST 0
#define	MC_CMD_FC_OUT_GET_VERSION_VERSION_OFST 4
#define	MC_CMD_FC_OUT_GET_VERSION_VERSION_LEN 8
#define	MC_CMD_FC_OUT_GET_VERSION_VERSION_LO_OFST 4
#define	MC_CMD_FC_OUT_GET_VERSION_VERSION_HI_OFST 8

/* MC_CMD_FC_OUT_TRC_RX_READ msgresponse */
#define	MC_CMD_FC_OUT_TRC_RX_READ_LEN 8
#define	MC_CMD_FC_OUT_TRC_RX_READ_DATA_OFST 0
#define	MC_CMD_FC_OUT_TRC_RX_READ_DATA_LEN 4
#define	MC_CMD_FC_OUT_TRC_RX_READ_DATA_NUM 2

/* MC_CMD_FC_OUT_TRC_RX_WRITE msgresponse */
#define	MC_CMD_FC_OUT_TRC_RX_WRITE_LEN 0

/* MC_CMD_FC_OUT_MAC_RECONFIGURE msgresponse */
#define	MC_CMD_FC_OUT_MAC_RECONFIGURE_LEN 0

/* MC_CMD_FC_OUT_MAC_SET_LINK msgresponse */
#define	MC_CMD_FC_OUT_MAC_SET_LINK_LEN 0

/* MC_CMD_FC_OUT_MAC_READ_STATUS msgresponse */
#define	MC_CMD_FC_OUT_MAC_READ_STATUS_LEN 4
#define	MC_CMD_FC_OUT_MAC_READ_STATUS_STATUS_OFST 0

/* MC_CMD_FC_OUT_MAC_GET_RX_STATS msgresponse */
#define	MC_CMD_FC_OUT_MAC_GET_RX_STATS_LEN ((((0-1+(64*MC_CMD_FC_MAC_RX_NSTATS))+1))>>3)
#define	MC_CMD_FC_OUT_MAC_GET_RX_STATS_STATISTICS_OFST 0
#define	MC_CMD_FC_OUT_MAC_GET_RX_STATS_STATISTICS_LEN 8
#define	MC_CMD_FC_OUT_MAC_GET_RX_STATS_STATISTICS_LO_OFST 0
#define	MC_CMD_FC_OUT_MAC_GET_RX_STATS_STATISTICS_HI_OFST 4
#define	MC_CMD_FC_OUT_MAC_GET_RX_STATS_STATISTICS_NUM MC_CMD_FC_MAC_RX_NSTATS
#define	MC_CMD_FC_MAC_RX_STATS_OCTETS  0x0 /* enum */
#define	MC_CMD_FC_MAC_RX_OCTETS_OK  0x1 /* enum */
#define	MC_CMD_FC_MAC_RX_ALIGNMENT_ERRORS  0x2 /* enum */
#define	MC_CMD_FC_MAC_RX_PAUSE_MAC_CTRL_FRAMES  0x3 /* enum */
#define	MC_CMD_FC_MAC_RX_FRAMES_OK  0x4 /* enum */
#define	MC_CMD_FC_MAC_RX_CRC_ERRORS  0x5 /* enum */
#define	MC_CMD_FC_MAC_RX_VLAN_OK  0x6 /* enum */
#define	MC_CMD_FC_MAC_RX_ERRORS  0x7 /* enum */
#define	MC_CMD_FC_MAC_RX_UCAST_PKTS  0x8 /* enum */
#define	MC_CMD_FC_MAC_RX_MULTICAST_PKTS  0x9 /* enum */
#define	MC_CMD_FC_MAC_RX_BROADCAST_PKTS  0xa /* enum */
#define	MC_CMD_FC_MAC_RX_STATS_DROP_EVENTS  0xb /* enum */
#define	MC_CMD_FC_MAC_RX_STATS_PKTS  0xc /* enum */
#define	MC_CMD_FC_MAC_RX_STATS_UNDERSIZE_PKTS  0xd /* enum */
#define	MC_CMD_FC_MAC_RX_STATS_PKTS_64  0xe /* enum */
#define	MC_CMD_FC_MAC_RX_STATS_PKTS_65_127  0xf /* enum */
#define	MC_CMD_FC_MAC_RX_STATS_PKTS_128_255  0x10 /* enum */
#define	MC_CMD_FC_MAC_RX_STATS_PKTS_256_511  0x11 /* enum */
#define	MC_CMD_FC_MAC_RX_STATS_PKTS_512_1023  0x12 /* enum */
#define	MC_CMD_FC_MAC_RX_STATS_PKTS_1024_1518  0x13 /* enum */
#define	MC_CMD_FC_MAC_RX_STATS_PKTS_1519_MAX  0x14 /* enum */
#define	MC_CMD_FC_MAC_RX_STATS_OVERSIZE_PKTS  0x15 /* enum */
#define	MC_CMD_FC_MAC_RX_STATS_JABBERS  0x16 /* enum */
#define	MC_CMD_FC_MAC_RX_STATS_FRAGMENTS  0x17 /* enum */
#define	MC_CMD_FC_MAC_RX_MAC_CONTROL_FRAMES  0x18 /* enum */
#define	MC_CMD_FC_MAC_RX_NSTATS  0x19 /* enum */

/* MC_CMD_FC_OUT_MAC_GET_TX_STATS msgresponse */
#define	MC_CMD_FC_OUT_MAC_GET_TX_STATS_LEN ((((0-1+(64*MC_CMD_FC_MAC_TX_NSTATS))+1))>>3)
#define	MC_CMD_FC_OUT_MAC_GET_TX_STATS_STATISTICS_OFST 0
#define	MC_CMD_FC_OUT_MAC_GET_TX_STATS_STATISTICS_LEN 8
#define	MC_CMD_FC_OUT_MAC_GET_TX_STATS_STATISTICS_LO_OFST 0
#define	MC_CMD_FC_OUT_MAC_GET_TX_STATS_STATISTICS_HI_OFST 4
#define	MC_CMD_FC_OUT_MAC_GET_TX_STATS_STATISTICS_NUM MC_CMD_FC_MAC_TX_NSTATS
#define	MC_CMD_FC_MAC_TX_STATS_OCTETS  0x0 /* enum */
#define	MC_CMD_FC_MAC_TX_OCTETS_OK  0x1 /* enum */
#define	MC_CMD_FC_MAC_TX_ALIGNMENT_ERRORS  0x2 /* enum */
#define	MC_CMD_FC_MAC_TX_PAUSE_MAC_CTRL_FRAMES  0x3 /* enum */
#define	MC_CMD_FC_MAC_TX_FRAMES_OK  0x4 /* enum */
#define	MC_CMD_FC_MAC_TX_CRC_ERRORS  0x5 /* enum */
#define	MC_CMD_FC_MAC_TX_VLAN_OK  0x6 /* enum */
#define	MC_CMD_FC_MAC_TX_ERRORS  0x7 /* enum */
#define	MC_CMD_FC_MAC_TX_UCAST_PKTS  0x8 /* enum */
#define	MC_CMD_FC_MAC_TX_MULTICAST_PKTS  0x9 /* enum */
#define	MC_CMD_FC_MAC_TX_BROADCAST_PKTS  0xa /* enum */
#define	MC_CMD_FC_MAC_TX_STATS_DROP_EVENTS  0xb /* enum */
#define	MC_CMD_FC_MAC_TX_STATS_PKTS  0xc /* enum */
#define	MC_CMD_FC_MAC_TX_STATS_UNDERSIZE_PKTS  0xd /* enum */
#define	MC_CMD_FC_MAC_TX_STATS_PKTS_64  0xe /* enum */
#define	MC_CMD_FC_MAC_TX_STATS_PKTS_65_127  0xf /* enum */
#define	MC_CMD_FC_MAC_TX_STATS_PKTS_128_255  0x10 /* enum */
#define	MC_CMD_FC_MAC_TX_STATS_PKTS_256_511  0x11 /* enum */
#define	MC_CMD_FC_MAC_TX_STATS_PKTS_512_1023  0x12 /* enum */
#define	MC_CMD_FC_MAC_TX_STATS_PKTS_1024_1518  0x13 /* enum */
#define	MC_CMD_FC_MAC_TX_STATS_PKTS_1519_TX_MTU  0x14 /* enum */
#define	MC_CMD_FC_MAC_TX_MAC_CONTROL_FRAMES  0x15 /* enum */
#define	MC_CMD_FC_MAC_TX_NSTATS  0x16 /* enum */

/* MC_CMD_FC_OUT_MAC_GET_STATS msgresponse */
#define	MC_CMD_FC_OUT_MAC_GET_STATS_LEN ((((0-1+(64*MC_CMD_FC_MAC_NSTATS_PER_BLOCK))+1))>>3)
#define	MC_CMD_FC_OUT_MAC_GET_STATS_STATISTICS_OFST 0
#define	MC_CMD_FC_OUT_MAC_GET_STATS_STATISTICS_LEN 8
#define	MC_CMD_FC_OUT_MAC_GET_STATS_STATISTICS_LO_OFST 0
#define	MC_CMD_FC_OUT_MAC_GET_STATS_STATISTICS_HI_OFST 4
#define	MC_CMD_FC_OUT_MAC_GET_STATS_STATISTICS_NUM MC_CMD_FC_MAC_NSTATS_PER_BLOCK

/* MC_CMD_FC_OUT_MAC msgresponse */
#define	MC_CMD_FC_OUT_MAC_LEN 0

/* MC_CMD_FC_OUT_SFP msgresponse */
#define	MC_CMD_FC_OUT_SFP_LEN 0

/* MC_CMD_FC_OUT_DDR_TEST_START msgresponse */
#define	MC_CMD_FC_OUT_DDR_TEST_START_LEN 0

/* MC_CMD_FC_OUT_DDR_TEST_POLL msgresponse */
#define	MC_CMD_FC_OUT_DDR_TEST_POLL_LEN 8
#define	MC_CMD_FC_OUT_DDR_TEST_POLL_STATUS_OFST 0
#define	MC_CMD_FC_OUT_DDR_TEST_POLL_CODE_LBN 0
#define	MC_CMD_FC_OUT_DDR_TEST_POLL_CODE_WIDTH 8
#define	MC_CMD_FC_OP_DDR_TEST_NONE 0x0 /* enum */
#define	MC_CMD_FC_OP_DDR_TEST_INPROGRESS 0x1 /* enum */
#define	MC_CMD_FC_OP_DDR_TEST_SUCCESS 0x2 /* enum */
#define	MC_CMD_FC_OP_DDR_TEST_TIMER_EXPIRED 0x3 /* enum */
#define	MC_CMD_FC_OUT_DDR_TEST_POLL_PRESENT_T0_LBN 11
#define	MC_CMD_FC_OUT_DDR_TEST_POLL_PRESENT_T0_WIDTH 1
#define	MC_CMD_FC_OUT_DDR_TEST_POLL_PRESENT_T1_LBN 10
#define	MC_CMD_FC_OUT_DDR_TEST_POLL_PRESENT_T1_WIDTH 1
#define	MC_CMD_FC_OUT_DDR_TEST_POLL_PRESENT_B0_LBN 9
#define	MC_CMD_FC_OUT_DDR_TEST_POLL_PRESENT_B0_WIDTH 1
#define	MC_CMD_FC_OUT_DDR_TEST_POLL_PRESENT_B1_LBN 8
#define	MC_CMD_FC_OUT_DDR_TEST_POLL_PRESENT_B1_WIDTH 1
#define	MC_CMD_FC_OUT_DDR_TEST_POLL_RESULT_OFST 4
#define	MC_CMD_FC_OUT_DDR_TEST_POLL_FPGA_SUPPORTS_T0_LBN 31
#define	MC_CMD_FC_OUT_DDR_TEST_POLL_FPGA_SUPPORTS_T0_WIDTH 1
#define	MC_CMD_FC_OUT_DDR_TEST_POLL_FPGA_SUPPORTS_T1_LBN 30
#define	MC_CMD_FC_OUT_DDR_TEST_POLL_FPGA_SUPPORTS_T1_WIDTH 1
#define	MC_CMD_FC_OUT_DDR_TEST_POLL_FPGA_SUPPORTS_B0_LBN 29
#define	MC_CMD_FC_OUT_DDR_TEST_POLL_FPGA_SUPPORTS_B0_WIDTH 1
#define	MC_CMD_FC_OUT_DDR_TEST_POLL_FPGA_SUPPORTS_B1_LBN 28
#define	MC_CMD_FC_OUT_DDR_TEST_POLL_FPGA_SUPPORTS_B1_WIDTH 1
#define	MC_CMD_FC_OUT_DDR_TEST_POLL_T0_LBN 15
#define	MC_CMD_FC_OUT_DDR_TEST_POLL_T0_WIDTH 5
#define	MC_CMD_FC_OUT_DDR_TEST_POLL_T1_LBN 10
#define	MC_CMD_FC_OUT_DDR_TEST_POLL_T1_WIDTH 5
#define	MC_CMD_FC_OUT_DDR_TEST_POLL_B0_LBN 5
#define	MC_CMD_FC_OUT_DDR_TEST_POLL_B0_WIDTH 5
#define	MC_CMD_FC_OUT_DDR_TEST_POLL_B1_LBN 0
#define	MC_CMD_FC_OUT_DDR_TEST_POLL_B1_WIDTH 5
#define	MC_CMD_FC_OUT_DDR_TEST_POLL_TEST_COMPLETE 0x0 /* enum */
#define	MC_CMD_FC_OUT_DDR_TEST_POLL_TEST_FAIL 0x1 /* enum */
#define	MC_CMD_FC_OUT_DDR_TEST_POLL_TEST_PASS 0x2 /* enum */
#define	MC_CMD_FC_OUT_DDR_TEST_POLL_CAL_FAIL 0x3 /* enum */
#define	MC_CMD_FC_OUT_DDR_TEST_POLL_CAL_SUCCESS 0x4 /* enum */

/* MC_CMD_FC_OUT_DDR_TEST msgresponse */
#define	MC_CMD_FC_OUT_DDR_TEST_LEN 0

/* MC_CMD_FC_OUT_GET_ASSERT msgresponse */
#define	MC_CMD_FC_OUT_GET_ASSERT_LEN 144
#define	MC_CMD_FC_OUT_GET_ASSERT_GLOBAL_FLAGS_OFST 0
#define	MC_CMD_FC_OUT_GET_ASSERT_STATE_LBN 8
#define	MC_CMD_FC_OUT_GET_ASSERT_STATE_WIDTH 8
#define	MC_CMD_FC_GET_ASSERT_FLAGS_STATE_CLEAR 0x0 /* enum */
#define	MC_CMD_FC_GET_ASSERT_FLAGS_STATE_NEW 0x1 /* enum */
#define	MC_CMD_FC_GET_ASSERT_FLAGS_STATE_NOTIFIED 0x2 /* enum */
#define	MC_CMD_FC_OUT_GET_ASSERT_TYPE_LBN 0
#define	MC_CMD_FC_OUT_GET_ASSERT_TYPE_WIDTH 8
#define	MC_CMD_FC_GET_ASSERT_FLAGS_TYPE_NONE 0x0 /* enum */
#define	MC_CMD_FC_GET_ASSERT_FLAGS_TYPE_EXCEPTION 0x1 /* enum */
#define	MC_CMD_FC_GET_ASSERT_FLAGS_TYPE_ASSERTION 0x2 /* enum */
#define	MC_CMD_FC_OUT_GET_ASSERT_SAVED_PC_OFFS_OFST 4
#define	MC_CMD_FC_OUT_GET_ASSERT_GP_REGS_OFFS_OFST 8
#define	MC_CMD_FC_OUT_GET_ASSERT_GP_REGS_OFFS_LEN 4
#define	MC_CMD_FC_OUT_GET_ASSERT_GP_REGS_OFFS_NUM 31
#define	MC_CMD_FC_OUT_GET_ASSERT_EXCEPTION_TYPE_OFFS_OFST 132
#define	MC_CMD_FC_OUT_GET_ASSERT_EXCEPTION_PC_ADDR_OFFS_OFST 136
#define	MC_CMD_FC_OUT_GET_ASSERT_EXCEPTION_BAD_ADDR_OFFS_OFST 140

/* MC_CMD_FC_OUT_FPGA_BUILD msgresponse */
#define	MC_CMD_FC_OUT_FPGA_BUILD_LEN 32
#define	MC_CMD_FC_OUT_FPGA_BUILD_COMPONENT_INFO_OFST 0
#define	MC_CMD_FC_OUT_FPGA_BUILD_IS_APPLICATION_LBN 31
#define	MC_CMD_FC_OUT_FPGA_BUILD_IS_APPLICATION_WIDTH 1
#define	MC_CMD_FC_OUT_FPGA_BUILD_IS_LICENSED_LBN 30
#define	MC_CMD_FC_OUT_FPGA_BUILD_IS_LICENSED_WIDTH 1
#define	MC_CMD_FC_OUT_FPGA_BUILD_COMPONENT_ID_LBN 16
#define	MC_CMD_FC_OUT_FPGA_BUILD_COMPONENT_ID_WIDTH 14
#define	MC_CMD_FC_OUT_FPGA_BUILD_VERSION_MAJOR_LBN 12
#define	MC_CMD_FC_OUT_FPGA_BUILD_VERSION_MAJOR_WIDTH 4
#define	MC_CMD_FC_OUT_FPGA_BUILD_VERSION_MINOR_LBN 4
#define	MC_CMD_FC_OUT_FPGA_BUILD_VERSION_MINOR_WIDTH 8
#define	MC_CMD_FC_OUT_FPGA_BUILD_BUILD_NUM_LBN 0
#define	MC_CMD_FC_OUT_FPGA_BUILD_BUILD_NUM_WIDTH 4
#define	MC_CMD_FC_OUT_FPGA_BUILD_TIMESTAMP_OFST 4
#define	MC_CMD_FC_OUT_FPGA_BUILD_PARAMETERS_OFST 8
#define	MC_CMD_FC_OUT_FPGA_BUILD_FPGA_TYPE_LBN 0
#define	MC_CMD_FC_OUT_FPGA_BUILD_FPGA_TYPE_WIDTH 8
#define	MC_CMD_FC_FPGA_TYPE_A7 0xa7 /* enum */
#define	MC_CMD_FC_FPGA_TYPE_A5 0xa5 /* enum */
#define	MC_CMD_FC_OUT_FPGA_BUILD_RESERVED1_LBN 8
#define	MC_CMD_FC_OUT_FPGA_BUILD_RESERVED1_WIDTH 10
#define	MC_CMD_FC_OUT_FPGA_BUILD_PTP_ENABLED_LBN 18
#define	MC_CMD_FC_OUT_FPGA_BUILD_PTP_ENABLED_WIDTH 1
#define	MC_CMD_FC_OUT_FPGA_BUILD_SODIMM1_RLDRAM_DEF_LBN 19
#define	MC_CMD_FC_OUT_FPGA_BUILD_SODIMM1_RLDRAM_DEF_WIDTH 1
#define	MC_CMD_FC_OUT_FPGA_BUILD_SODIMM2_RLDRAM_DEF_LBN 20
#define	MC_CMD_FC_OUT_FPGA_BUILD_SODIMM2_RLDRAM_DEF_WIDTH 1
#define	MC_CMD_FC_OUT_FPGA_BUILD_SODIMM3_RLDRAM_DEF_LBN 21
#define	MC_CMD_FC_OUT_FPGA_BUILD_SODIMM3_RLDRAM_DEF_WIDTH 1
#define	MC_CMD_FC_OUT_FPGA_BUILD_SODIMM4_RLDRAM_DEF_LBN 22
#define	MC_CMD_FC_OUT_FPGA_BUILD_SODIMM4_RLDRAM_DEF_WIDTH 1
#define	MC_CMD_FC_OUT_FPGA_BUILD_SODIMM_T0_DDR3_DEF_LBN 23
#define	MC_CMD_FC_OUT_FPGA_BUILD_SODIMM_T0_DDR3_DEF_WIDTH 1
#define	MC_CMD_FC_OUT_FPGA_BUILD_SODIMM_T1_DDR3_DEF_LBN 24
#define	MC_CMD_FC_OUT_FPGA_BUILD_SODIMM_T1_DDR3_DEF_WIDTH 1
#define	MC_CMD_FC_OUT_FPGA_BUILD_SODIMM_B0_DDR3_DEF_LBN 25
#define	MC_CMD_FC_OUT_FPGA_BUILD_SODIMM_B0_DDR3_DEF_WIDTH 1
#define	MC_CMD_FC_OUT_FPGA_BUILD_SODIMM_B1_DDR3_DEF_LBN 26
#define	MC_CMD_FC_OUT_FPGA_BUILD_SODIMM_B1_DDR3_DEF_WIDTH 1
#define	MC_CMD_FC_OUT_FPGA_BUILD_DDR3_ECC_ENABLED_LBN 27
#define	MC_CMD_FC_OUT_FPGA_BUILD_DDR3_ECC_ENABLED_WIDTH 1
#define	MC_CMD_FC_OUT_FPGA_BUILD_SODIMM_T1_QDR_DEF_LBN 28
#define	MC_CMD_FC_OUT_FPGA_BUILD_SODIMM_T1_QDR_DEF_WIDTH 1
#define	MC_CMD_FC_OUT_FPGA_BUILD_RESERVED2_LBN 29
#define	MC_CMD_FC_OUT_FPGA_BUILD_RESERVED2_WIDTH 2
#define	MC_CMD_FC_OUT_FPGA_BUILD_CRC_APPEND_LBN 31
#define	MC_CMD_FC_OUT_FPGA_BUILD_CRC_APPEND_WIDTH 1
#define	MC_CMD_FC_OUT_FPGA_BUILD_IDENTIFIER_OFST 12
#define	MC_CMD_FC_OUT_FPGA_BUILD_CHANGESET_LBN 0
#define	MC_CMD_FC_OUT_FPGA_BUILD_CHANGESET_WIDTH 16
#define	MC_CMD_FC_OUT_FPGA_BUILD_BUILD_FLAG_LBN 16
#define	MC_CMD_FC_OUT_FPGA_BUILD_BUILD_FLAG_WIDTH 1
#define	MC_CMD_FC_FPGA_BUILD_FLAG_INTERNAL 0x0 /* enum */
#define	MC_CMD_FC_FPGA_BUILD_FLAG_RELEASE 0x1 /* enum */
#define	MC_CMD_FC_OUT_FPGA_BUILD_RESERVED3_LBN 17
#define	MC_CMD_FC_OUT_FPGA_BUILD_RESERVED3_WIDTH 15
#define	MC_CMD_FC_OUT_FPGA_BUILD_VERSION_HI_OFST 16
#define	MC_CMD_FC_OUT_FPGA_BUILD_DEPLOYMENT_VERSION_MINOR_LBN 0
#define	MC_CMD_FC_OUT_FPGA_BUILD_DEPLOYMENT_VERSION_MINOR_WIDTH 16
#define	MC_CMD_FC_OUT_FPGA_BUILD_DEPLOYMENT_VERSION_MAJOR_LBN 16
#define	MC_CMD_FC_OUT_FPGA_BUILD_DEPLOYMENT_VERSION_MAJOR_WIDTH 16
#define	MC_CMD_FC_OUT_FPGA_BUILD_VERSION_LO_OFST 20
#define	MC_CMD_FC_OUT_FPGA_BUILD_DEPLOYMENT_VERSION_BUILD_LBN 0
#define	MC_CMD_FC_OUT_FPGA_BUILD_DEPLOYMENT_VERSION_BUILD_WIDTH 16
#define	MC_CMD_FC_OUT_FPGA_BUILD_DEPLOYMENT_VERSION_MICRO_LBN 16
#define	MC_CMD_FC_OUT_FPGA_BUILD_DEPLOYMENT_VERSION_MICRO_WIDTH 16
#define	MC_CMD_FC_OUT_FPGA_BUILD_RESERVED4_OFST 16
#define	MC_CMD_FC_OUT_FPGA_BUILD_RESERVED4_LEN 8
#define	MC_CMD_FC_OUT_FPGA_BUILD_RESERVED4_LO_OFST 16
#define	MC_CMD_FC_OUT_FPGA_BUILD_RESERVED4_HI_OFST 20
#define	MC_CMD_FC_OUT_FPGA_BUILD_REVISION_LO_OFST 24
#define	MC_CMD_FC_OUT_FPGA_BUILD_REVISION_HI_OFST 28
#define	MC_CMD_FC_OUT_FPGA_BUILD_REVISION_HIGH_LBN 0
#define	MC_CMD_FC_OUT_FPGA_BUILD_REVISION_HIGH_WIDTH 16

/* MC_CMD_FC_OUT_FPGA_SERVICES msgresponse */
#define	MC_CMD_FC_OUT_FPGA_SERVICES_LEN 32
#define	MC_CMD_FC_OUT_FPGA_SERVICES_COMPONENT_INFO_OFST 0
#define	MC_CMD_FC_OUT_FPGA_SERVICES_IS_APPLICATION_LBN 31
#define	MC_CMD_FC_OUT_FPGA_SERVICES_IS_APPLICATION_WIDTH 1
#define	MC_CMD_FC_OUT_FPGA_SERVICES_IS_LICENSED_LBN 30
#define	MC_CMD_FC_OUT_FPGA_SERVICES_IS_LICENSED_WIDTH 1
#define	MC_CMD_FC_OUT_FPGA_SERVICES_COMPONENT_ID_LBN 16
#define	MC_CMD_FC_OUT_FPGA_SERVICES_COMPONENT_ID_WIDTH 14
#define	MC_CMD_FC_OUT_FPGA_SERVICES_VERSION_MAJOR_LBN 12
#define	MC_CMD_FC_OUT_FPGA_SERVICES_VERSION_MAJOR_WIDTH 4
#define	MC_CMD_FC_OUT_FPGA_SERVICES_VERSION_MINOR_LBN 4
#define	MC_CMD_FC_OUT_FPGA_SERVICES_VERSION_MINOR_WIDTH 8
#define	MC_CMD_FC_OUT_FPGA_SERVICES_BUILD_NUM_LBN 0
#define	MC_CMD_FC_OUT_FPGA_SERVICES_BUILD_NUM_WIDTH 4
#define	MC_CMD_FC_OUT_FPGA_SERVICES_TIMESTAMP_OFST 4
#define	MC_CMD_FC_OUT_FPGA_SERVICES_PARAMETERS_OFST 8
#define	MC_CMD_FC_OUT_FPGA_SERVICES_FC_FLASH_BOOTED_LBN 8
#define	MC_CMD_FC_OUT_FPGA_SERVICES_FC_FLASH_BOOTED_WIDTH 1
#define	MC_CMD_FC_OUT_FPGA_SERVICES_NIC0_DEF_LBN 27
#define	MC_CMD_FC_OUT_FPGA_SERVICES_NIC0_DEF_WIDTH 1
#define	MC_CMD_FC_OUT_FPGA_SERVICES_NIC1_DEF_LBN 28
#define	MC_CMD_FC_OUT_FPGA_SERVICES_NIC1_DEF_WIDTH 1
#define	MC_CMD_FC_OUT_FPGA_SERVICES_SFP0_DEF_LBN 29
#define	MC_CMD_FC_OUT_FPGA_SERVICES_SFP0_DEF_WIDTH 1
#define	MC_CMD_FC_OUT_FPGA_SERVICES_SFP1_DEF_LBN 30
#define	MC_CMD_FC_OUT_FPGA_SERVICES_SFP1_DEF_WIDTH 1
#define	MC_CMD_FC_OUT_FPGA_SERVICES_RESERVED_LBN 31
#define	MC_CMD_FC_OUT_FPGA_SERVICES_RESERVED_WIDTH 1
#define	MC_CMD_FC_OUT_FPGA_SERVICES_IDENTIFIER_OFST 12
#define	MC_CMD_FC_OUT_FPGA_SERVICES_CHANGESET_LBN 0
#define	MC_CMD_FC_OUT_FPGA_SERVICES_CHANGESET_WIDTH 16
#define	MC_CMD_FC_OUT_FPGA_SERVICES_BUILD_FLAG_LBN 16
#define	MC_CMD_FC_OUT_FPGA_SERVICES_BUILD_FLAG_WIDTH 1
#define	MC_CMD_FC_OUT_FPGA_SERVICES_MEMORY_SIZE_OFST 16
#define	MC_CMD_FC_OUT_FPGA_SERVICES_MEMORY_SIZE_WIDTH_LBN 0
#define	MC_CMD_FC_OUT_FPGA_SERVICES_MEMORY_SIZE_WIDTH_WIDTH 16
#define	MC_CMD_FC_OUT_FPGA_SERVICES_MEMORY_SIZE_COUNT_LBN 16
#define	MC_CMD_FC_OUT_FPGA_SERVICES_MEMORY_SIZE_COUNT_WIDTH 16
#define	MC_CMD_FC_OUT_FPGA_SERVICES_INSTANCE_SIZE_OFST 20
#define	MC_CMD_FC_OUT_FPGA_SERVICES_INSTANCE_SIZE_WIDTH_LBN 0
#define	MC_CMD_FC_OUT_FPGA_SERVICES_INSTANCE_SIZE_WIDTH_WIDTH 16
#define	MC_CMD_FC_OUT_FPGA_SERVICES_INSTANCE_SIZE_COUNT_LBN 16
#define	MC_CMD_FC_OUT_FPGA_SERVICES_INSTANCE_SIZE_COUNT_WIDTH 16
#define	MC_CMD_FC_OUT_FPGA_SERVICES_REVISION_LO_OFST 24
#define	MC_CMD_FC_OUT_FPGA_SERVICES_REVISION_HI_OFST 28
#define	MC_CMD_FC_OUT_FPGA_SERVICES_REVISION_HIGH_LBN 0
#define	MC_CMD_FC_OUT_FPGA_SERVICES_REVISION_HIGH_WIDTH 16

/* MC_CMD_FC_OUT_BSP_VERSION msgresponse */
#define	MC_CMD_FC_OUT_BSP_VERSION_LEN 4
#define	MC_CMD_FC_OUT_BSP_VERSION_SYSID_OFST 0
#define	MC_CMD_FC_OUT_BSP_VERSION_VERSION_MAJOR_LBN 12
#define	MC_CMD_FC_OUT_BSP_VERSION_VERSION_MAJOR_WIDTH 4
#define	MC_CMD_FC_OUT_BSP_VERSION_VERSION_MINOR_LBN 4
#define	MC_CMD_FC_OUT_BSP_VERSION_VERSION_MINOR_WIDTH 8
#define	MC_CMD_FC_OUT_BSP_VERSION_BUILD_NUM_LBN 0
#define	MC_CMD_FC_OUT_BSP_VERSION_BUILD_NUM_WIDTH 4

/* MC_CMD_FC_OUT_READ_MAP_COUNT msgresponse */
#define	MC_CMD_FC_OUT_READ_MAP_COUNT_LEN 4
#define	MC_CMD_FC_OUT_READ_MAP_COUNT_NUM_MAPS_OFST 0

/* MC_CMD_FC_OUT_READ_MAP_INDEX msgresponse */
#define	MC_CMD_FC_OUT_READ_MAP_INDEX_LEN 164
#define	MC_CMD_FC_OUT_READ_MAP_INDEX_INDEX_OFST 0
#define	MC_CMD_FC_OUT_READ_MAP_INDEX_OPTIONS_OFST 4
#define	MC_CMD_FC_OUT_READ_MAP_INDEX_ALIGN_8  0x0 /* enum */
#define	MC_CMD_FC_OUT_READ_MAP_INDEX_ALIGN_16  0x1 /* enum */
#define	MC_CMD_FC_OUT_READ_MAP_INDEX_ALIGN_32  0x2 /* enum */
#define	MC_CMD_FC_OUT_READ_MAP_INDEX_ALIGN_64  0x3 /* enum */
#define	MC_CMD_FC_OUT_READ_MAP_INDEX_ALIGN_MASK  0x3 /* enum */
#define	MC_CMD_FC_OUT_READ_MAP_INDEX_PATH_FC  0x4 /* enum */
#define	MC_CMD_FC_OUT_READ_MAP_INDEX_PATH_MEM  0x8 /* enum */
#define	MC_CMD_FC_OUT_READ_MAP_INDEX_PERM_READ  0x10 /* enum */
#define	MC_CMD_FC_OUT_READ_MAP_INDEX_PERM_WRITE  0x20 /* enum */
#define	MC_CMD_FC_OUT_READ_MAP_INDEX_LICENSE_FREE  0x0 /* enum */
#define	MC_CMD_FC_OUT_READ_MAP_INDEX_LICENSE_LICENSED  0x40 /* enum */
#define	MC_CMD_FC_OUT_READ_MAP_INDEX_ADDRESS_OFST 8
#define	MC_CMD_FC_OUT_READ_MAP_INDEX_ADDRESS_LEN 8
#define	MC_CMD_FC_OUT_READ_MAP_INDEX_ADDRESS_LO_OFST 8
#define	MC_CMD_FC_OUT_READ_MAP_INDEX_ADDRESS_HI_OFST 12
#define	MC_CMD_FC_OUT_READ_MAP_INDEX_LEN_OFST 16
#define	MC_CMD_FC_OUT_READ_MAP_INDEX_LEN_LEN 8
#define	MC_CMD_FC_OUT_READ_MAP_INDEX_LEN_LO_OFST 16
#define	MC_CMD_FC_OUT_READ_MAP_INDEX_LEN_HI_OFST 20
#define	MC_CMD_FC_OUT_READ_MAP_INDEX_COMP_INFO_OFST 24
#define	MC_CMD_FC_OUT_READ_MAP_INDEX_LICENSE_DATE_OFST 28
#define	MC_CMD_FC_OUT_READ_MAP_INDEX_LICENSE_DATE_LEN 8
#define	MC_CMD_FC_OUT_READ_MAP_INDEX_LICENSE_DATE_LO_OFST 28
#define	MC_CMD_FC_OUT_READ_MAP_INDEX_LICENSE_DATE_HI_OFST 32
#define	MC_CMD_FC_OUT_READ_MAP_INDEX_NAME_OFST 36
#define	MC_CMD_FC_OUT_READ_MAP_INDEX_NAME_LEN 1
#define	MC_CMD_FC_OUT_READ_MAP_INDEX_NAME_NUM 128

/* MC_CMD_FC_OUT_READ_MAP msgresponse */
#define	MC_CMD_FC_OUT_READ_MAP_LEN 0

/* MC_CMD_FC_OUT_CAPABILITIES msgresponse */
#define	MC_CMD_FC_OUT_CAPABILITIES_LEN 8
#define	MC_CMD_FC_OUT_CAPABILITIES_INTERNAL_OFST 0
#define	MC_CMD_FC_OUT_CAPABILITIES_EXTERNAL_OFST 4

/* MC_CMD_FC_OUT_GLOBAL_FLAGS msgresponse */
#define	MC_CMD_FC_OUT_GLOBAL_FLAGS_LEN 4
#define	MC_CMD_FC_OUT_GLOBAL_FLAGS_FLAGS_OFST 0

/* MC_CMD_FC_OUT_IO_REL msgresponse */
#define	MC_CMD_FC_OUT_IO_REL_LEN 0

/* MC_CMD_FC_OUT_IO_REL_GET_ADDR msgresponse */
#define	MC_CMD_FC_OUT_IO_REL_GET_ADDR_LEN 8
#define	MC_CMD_FC_OUT_IO_REL_GET_ADDR_ADDR_HI_OFST 0
#define	MC_CMD_FC_OUT_IO_REL_GET_ADDR_ADDR_LO_OFST 4

/* MC_CMD_FC_OUT_IO_REL_READ32 msgresponse */
#define	MC_CMD_FC_OUT_IO_REL_READ32_LENMIN 4
#define	MC_CMD_FC_OUT_IO_REL_READ32_LENMAX 252
#define	MC_CMD_FC_OUT_IO_REL_READ32_LEN(num) (0+4*(num))
#define	MC_CMD_FC_OUT_IO_REL_READ32_BUFFER_OFST 0
#define	MC_CMD_FC_OUT_IO_REL_READ32_BUFFER_LEN 4
#define	MC_CMD_FC_OUT_IO_REL_READ32_BUFFER_MINNUM 1
#define	MC_CMD_FC_OUT_IO_REL_READ32_BUFFER_MAXNUM 63

/* MC_CMD_FC_OUT_IO_REL_WRITE32 msgresponse */
#define	MC_CMD_FC_OUT_IO_REL_WRITE32_LEN 0

/* MC_CMD_FC_OUT_UHLINK_PHY msgresponse */
#define	MC_CMD_FC_OUT_UHLINK_PHY_LEN 48
#define	MC_CMD_FC_OUT_UHLINK_PHY_TRC_TX_SETTINGS_0_OFST 0
#define	MC_CMD_FC_OUT_UHLINK_PHY_TRC_TX_VOD_LBN 0
#define	MC_CMD_FC_OUT_UHLINK_PHY_TRC_TX_VOD_WIDTH 16
#define	MC_CMD_FC_OUT_UHLINK_PHY_TRC_TX_PREEMP_1STPOSTTAP_LBN 16
#define	MC_CMD_FC_OUT_UHLINK_PHY_TRC_TX_PREEMP_1STPOSTTAP_WIDTH 16
#define	MC_CMD_FC_OUT_UHLINK_PHY_TRC_TX_SETTINGS_1_OFST 4
#define	MC_CMD_FC_OUT_UHLINK_PHY_TRC_TX_PREEMP_PRETAP_LBN 0
#define	MC_CMD_FC_OUT_UHLINK_PHY_TRC_TX_PREEMP_PRETAP_WIDTH 16
#define	MC_CMD_FC_OUT_UHLINK_PHY_TRC_TX_PREEMP_2NDPOSTTAP_LBN 16
#define	MC_CMD_FC_OUT_UHLINK_PHY_TRC_TX_PREEMP_2NDPOSTTAP_WIDTH 16
#define	MC_CMD_FC_OUT_UHLINK_PHY_TRC_RX_SETTINGS_OFST 8
#define	MC_CMD_FC_OUT_UHLINK_PHY_TRC_RX_DC_GAIN_LBN 0
#define	MC_CMD_FC_OUT_UHLINK_PHY_TRC_RX_DC_GAIN_WIDTH 16
#define	MC_CMD_FC_OUT_UHLINK_PHY_TRC_RX_EQ_CONTROL_LBN 16
#define	MC_CMD_FC_OUT_UHLINK_PHY_TRC_RX_EQ_CONTROL_WIDTH 16
#define	MC_CMD_FC_OUT_UHLINK_PHY_RX_EYE_OFST 12
#define	MC_CMD_FC_OUT_UHLINK_PHY_RX_EYE_WIDTH_LBN 0
#define	MC_CMD_FC_OUT_UHLINK_PHY_RX_EYE_WIDTH_WIDTH 16
#define	MC_CMD_FC_OUT_UHLINK_PHY_RX_EYE_HEIGHT_LBN 16
#define	MC_CMD_FC_OUT_UHLINK_PHY_RX_EYE_HEIGHT_WIDTH 16
#define	MC_CMD_FC_OUT_UHLINK_PHY_PCS_STATUS_OFST 16
#define	MC_CMD_FC_OUT_UHLINK_PHY_LINK_STATE_WORD_OFST 20
#define	MC_CMD_FC_OUT_UHLINK_PHY_LINK_STATE_LBN 0
#define	MC_CMD_FC_OUT_UHLINK_PHY_LINK_STATE_WIDTH 1
#define	MC_CMD_FC_OUT_UHLINK_PHY_LINK_CONFIGURED_LBN 1
#define	MC_CMD_FC_OUT_UHLINK_PHY_LINK_CONFIGURED_WIDTH 1
#define	MC_CMD_FC_OUT_UHLINK_PHY_SFP_PARAMS_OFST 24
#define	MC_CMD_FC_OUT_UHLINK_PHY_SFP_PARAMS_LEN 20
#define	MC_CMD_FC_OUT_UHLINK_PHY_SFP_SPEED_OFST 24
#define	MC_CMD_FC_OUT_UHLINK_PHY_SFP_COPPER_LEN_OFST 28
#define	MC_CMD_FC_OUT_UHLINK_PHY_SFP_DUAL_SPEED_OFST 32
#define	MC_CMD_FC_OUT_UHLINK_PHY_SFP_PRESENT_OFST 36
#define	MC_CMD_FC_OUT_UHLINK_PHY_SFP_TYPE_OFST 40
#define	MC_CMD_FC_OUT_UHLINK_PHY_PHY_CFG_OFST 44
#define	MC_CMD_FC_OUT_UHLINK_PHY_PHY_CFG_DFE_LBN 0
#define	MC_CMD_FC_OUT_UHLINK_PHY_PHY_CFG_DFE_WIDTH 1
#define	MC_CMD_FC_OUT_UHLINK_PHY_PHY_CFG_AEQ_LBN 1
#define	MC_CMD_FC_OUT_UHLINK_PHY_PHY_CFG_AEQ_WIDTH 1
#define	MC_CMD_FC_OUT_UHLINK_PHY_PHY_CFG_RX_TUNING_LBN 2
#define	MC_CMD_FC_OUT_UHLINK_PHY_PHY_CFG_RX_TUNING_WIDTH 1

/* MC_CMD_FC_OUT_UHLINK_MAC msgresponse */
#define	MC_CMD_FC_OUT_UHLINK_MAC_LEN 20
#define	MC_CMD_FC_OUT_UHLINK_MAC_CONFIG_OFST 0
#define	MC_CMD_FC_OUT_UHLINK_MAC_MTU_OFST 4
#define	MC_CMD_FC_OUT_UHLINK_MAC_IF_STATUS_OFST 8
#define	MC_CMD_FC_OUT_UHLINK_MAC_ADDR_OFST 12
#define	MC_CMD_FC_OUT_UHLINK_MAC_ADDR_LEN 8
#define	MC_CMD_FC_OUT_UHLINK_MAC_ADDR_LO_OFST 12
#define	MC_CMD_FC_OUT_UHLINK_MAC_ADDR_HI_OFST 16

/* MC_CMD_FC_OUT_UHLINK_RX_EYE msgresponse */
#define	MC_CMD_FC_OUT_UHLINK_RX_EYE_LEN ((((0-1+(32*MC_CMD_FC_UHLINK_RX_EYE_PER_BLOCK))+1))>>3)
#define	MC_CMD_FC_OUT_UHLINK_RX_EYE_RX_EYE_OFST 0
#define	MC_CMD_FC_OUT_UHLINK_RX_EYE_RX_EYE_LEN 4
#define	MC_CMD_FC_OUT_UHLINK_RX_EYE_RX_EYE_NUM MC_CMD_FC_UHLINK_RX_EYE_PER_BLOCK

/* MC_CMD_FC_OUT_UHLINK_DUMP_RX_EYE_PLOT msgresponse */
#define	MC_CMD_FC_OUT_UHLINK_DUMP_RX_EYE_PLOT_LEN 0

/* MC_CMD_FC_OUT_UHLINK_READ_RX_EYE_PLOT msgresponse */
#define	MC_CMD_FC_OUT_UHLINK_READ_RX_EYE_PLOT_LEN ((((32-1+(64*MC_CMD_FC_UHLINK_RX_EYE_PLOT_ROWS_PER_BLOCK))+1))>>3)
#define	MC_CMD_FC_OUT_UHLINK_READ_RX_EYE_PLOT_VALID_OFST 0
#define	MC_CMD_FC_OUT_UHLINK_READ_RX_EYE_PLOT_ROWS_OFST 4
#define	MC_CMD_FC_OUT_UHLINK_READ_RX_EYE_PLOT_ROWS_LEN 8
#define	MC_CMD_FC_OUT_UHLINK_READ_RX_EYE_PLOT_ROWS_LO_OFST 4
#define	MC_CMD_FC_OUT_UHLINK_READ_RX_EYE_PLOT_ROWS_HI_OFST 8
#define	MC_CMD_FC_OUT_UHLINK_READ_RX_EYE_PLOT_ROWS_NUM MC_CMD_FC_UHLINK_RX_EYE_PLOT_ROWS_PER_BLOCK

/* MC_CMD_FC_OUT_UHLINK_RX_TUNE msgresponse */
#define	MC_CMD_FC_OUT_UHLINK_RX_TUNE_LEN 0

/* MC_CMD_FC_OUT_UHLINK_LOOPBACK_SET msgresponse */
#define	MC_CMD_FC_OUT_UHLINK_LOOPBACK_SET_LEN 0

/* MC_CMD_FC_OUT_UHLINK_LOOPBACK_GET msgresponse */
#define	MC_CMD_FC_OUT_UHLINK_LOOPBACK_GET_LEN 4
#define	MC_CMD_FC_OUT_UHLINK_LOOPBACK_GET_STATE_OFST 0

/* MC_CMD_FC_OUT_UHLINK msgresponse */
#define	MC_CMD_FC_OUT_UHLINK_LEN 0

/* MC_CMD_FC_OUT_SET_LINK msgresponse */
#define	MC_CMD_FC_OUT_SET_LINK_LEN 0

/* MC_CMD_FC_OUT_LICENSE msgresponse */
#define	MC_CMD_FC_OUT_LICENSE_LEN 12
#define	MC_CMD_FC_OUT_LICENSE_VALID_KEYS_OFST 0
#define	MC_CMD_FC_OUT_LICENSE_INVALID_KEYS_OFST 4
#define	MC_CMD_FC_OUT_LICENSE_BLACKLISTED_KEYS_OFST 8

/* MC_CMD_FC_OUT_STARTUP msgresponse */
#define	MC_CMD_FC_OUT_STARTUP_LEN 4
#define	MC_CMD_FC_OUT_STARTUP_CAPABILITIES_OFST 0
#define	MC_CMD_FC_OUT_STARTUP_CAN_ACCESS_FLASH_LBN 0
#define	MC_CMD_FC_OUT_STARTUP_CAN_ACCESS_FLASH_WIDTH 1

/* MC_CMD_FC_OUT_DMA_READ msgresponse */
#define	MC_CMD_FC_OUT_DMA_READ_LENMIN 1
#define	MC_CMD_FC_OUT_DMA_READ_LENMAX 252
#define	MC_CMD_FC_OUT_DMA_READ_LEN(num) (0+1*(num))
#define	MC_CMD_FC_OUT_DMA_READ_DATA_OFST 0
#define	MC_CMD_FC_OUT_DMA_READ_DATA_LEN 1
#define	MC_CMD_FC_OUT_DMA_READ_DATA_MINNUM 1
#define	MC_CMD_FC_OUT_DMA_READ_DATA_MAXNUM 252

/* MC_CMD_FC_OUT_TIMED_READ_SET msgresponse */
#define	MC_CMD_FC_OUT_TIMED_READ_SET_LEN 4
#define	MC_CMD_FC_OUT_TIMED_READ_SET_FC_HANDLE_OFST 0

/* MC_CMD_FC_OUT_TIMED_READ_GET msgresponse */
#define	MC_CMD_FC_OUT_TIMED_READ_GET_LEN 52
#define	MC_CMD_FC_OUT_TIMED_READ_GET_HOST_HANDLE_OFST 0
#define	MC_CMD_FC_OUT_TIMED_READ_GET_HOST_DMA_ADDRESS_OFST 4
#define	MC_CMD_FC_OUT_TIMED_READ_GET_HOST_DMA_ADDRESS_LEN 8
#define	MC_CMD_FC_OUT_TIMED_READ_GET_HOST_DMA_ADDRESS_LO_OFST 4
#define	MC_CMD_FC_OUT_TIMED_READ_GET_HOST_DMA_ADDRESS_HI_OFST 8
#define	MC_CMD_FC_OUT_TIMED_READ_GET_AOE_ADDRESS_OFST 12
#define	MC_CMD_FC_OUT_TIMED_READ_GET_AOE_ADDRESS_LEN 8
#define	MC_CMD_FC_OUT_TIMED_READ_GET_AOE_ADDRESS_LO_OFST 12
#define	MC_CMD_FC_OUT_TIMED_READ_GET_AOE_ADDRESS_HI_OFST 16
#define	MC_CMD_FC_OUT_TIMED_READ_GET_AOE_LENGTH_OFST 20
#define	MC_CMD_FC_OUT_TIMED_READ_GET_HOST_LENGTH_OFST 24
#define	MC_CMD_FC_OUT_TIMED_READ_GET_FLAGS_OFST 28
#define	MC_CMD_FC_OUT_TIMED_READ_GET_PERIOD_OFST 32
#define	MC_CMD_FC_OUT_TIMED_READ_GET_CLOCK_START_OFST 36
#define	MC_CMD_FC_OUT_TIMED_READ_GET_CLOCK_START_LEN 8
#define	MC_CMD_FC_OUT_TIMED_READ_GET_CLOCK_START_LO_OFST 36
#define	MC_CMD_FC_OUT_TIMED_READ_GET_CLOCK_START_HI_OFST 40
#define	MC_CMD_FC_OUT_TIMED_READ_GET_CLOCK_END_OFST 44
#define	MC_CMD_FC_OUT_TIMED_READ_GET_CLOCK_END_LEN 8
#define	MC_CMD_FC_OUT_TIMED_READ_GET_CLOCK_END_LO_OFST 44
#define	MC_CMD_FC_OUT_TIMED_READ_GET_CLOCK_END_HI_OFST 48

/* MC_CMD_FC_OUT_LOG_ADDR_RANGE msgresponse */
#define	MC_CMD_FC_OUT_LOG_ADDR_RANGE_LEN 0

/* MC_CMD_FC_OUT_LOG msgresponse */
#define	MC_CMD_FC_OUT_LOG_LEN 0

/* MC_CMD_FC_OUT_CLOCK_GET_TIME msgresponse */
#define	MC_CMD_FC_OUT_CLOCK_GET_TIME_LEN 24
#define	MC_CMD_FC_OUT_CLOCK_GET_TIME_CLOCK_ID_OFST 0
#define	MC_CMD_FC_OUT_CLOCK_GET_TIME_SECONDS_OFST 4
#define	MC_CMD_FC_OUT_CLOCK_GET_TIME_SECONDS_LEN 8
#define	MC_CMD_FC_OUT_CLOCK_GET_TIME_SECONDS_LO_OFST 4
#define	MC_CMD_FC_OUT_CLOCK_GET_TIME_SECONDS_HI_OFST 8
#define	MC_CMD_FC_OUT_CLOCK_GET_TIME_NANOSECONDS_OFST 12
#define	MC_CMD_FC_OUT_CLOCK_GET_TIME_RANGE_OFST 16
#define	MC_CMD_FC_OUT_CLOCK_GET_TIME_PRECISION_OFST 20

/* MC_CMD_FC_OUT_CLOCK_SET_TIME msgresponse */
#define	MC_CMD_FC_OUT_CLOCK_SET_TIME_LEN 0

/* MC_CMD_FC_OUT_DDR_SET_SPD msgresponse */
#define	MC_CMD_FC_OUT_DDR_SET_SPD_LEN 0

/* MC_CMD_FC_OUT_DDR_GET_STATUS msgresponse */
#define	MC_CMD_FC_OUT_DDR_GET_STATUS_LEN 4
#define	MC_CMD_FC_OUT_DDR_GET_STATUS_FLAGS_OFST 0
#define	MC_CMD_FC_OUT_DDR_GET_STATUS_READY_LBN 0
#define	MC_CMD_FC_OUT_DDR_GET_STATUS_READY_WIDTH 1
#define	MC_CMD_FC_OUT_DDR_GET_STATUS_CALIBRATED_LBN 1
#define	MC_CMD_FC_OUT_DDR_GET_STATUS_CALIBRATED_WIDTH 1

/* MC_CMD_FC_OUT_TIMESTAMP_READ_TRANSMIT msgresponse */
#define	MC_CMD_FC_OUT_TIMESTAMP_READ_TRANSMIT_LEN 8
#define	MC_CMD_FC_OUT_TIMESTAMP_READ_TRANSMIT_SECONDS_OFST 0
#define	MC_CMD_FC_OUT_TIMESTAMP_READ_TRANSMIT_NANOSECONDS_OFST 4

/* MC_CMD_FC_OUT_TIMESTAMP_READ_SNAPSHOT msgresponse */
#define	MC_CMD_FC_OUT_TIMESTAMP_READ_SNAPSHOT_LENMIN 8
#define	MC_CMD_FC_OUT_TIMESTAMP_READ_SNAPSHOT_LENMAX 248
#define	MC_CMD_FC_OUT_TIMESTAMP_READ_SNAPSHOT_LEN(num) (0+8*(num))
#define	MC_CMD_FC_OUT_TIMESTAMP_READ_SNAPSHOT_SECONDS_OFST 0
#define	MC_CMD_FC_OUT_TIMESTAMP_READ_SNAPSHOT_NANOSECONDS_OFST 4
#define	MC_CMD_FC_OUT_TIMESTAMP_READ_SNAPSHOT_TIMESTAMP_OFST 0
#define	MC_CMD_FC_OUT_TIMESTAMP_READ_SNAPSHOT_TIMESTAMP_LEN 8
#define	MC_CMD_FC_OUT_TIMESTAMP_READ_SNAPSHOT_TIMESTAMP_LO_OFST 0
#define	MC_CMD_FC_OUT_TIMESTAMP_READ_SNAPSHOT_TIMESTAMP_HI_OFST 4
#define	MC_CMD_FC_OUT_TIMESTAMP_READ_SNAPSHOT_TIMESTAMP_MINNUM 0
#define	MC_CMD_FC_OUT_TIMESTAMP_READ_SNAPSHOT_TIMESTAMP_MAXNUM 31

/* MC_CMD_FC_OUT_SPI_READ msgresponse */
#define	MC_CMD_FC_OUT_SPI_READ_LENMIN 4
#define	MC_CMD_FC_OUT_SPI_READ_LENMAX 252
#define	MC_CMD_FC_OUT_SPI_READ_LEN(num) (0+4*(num))
#define	MC_CMD_FC_OUT_SPI_READ_BUFFER_OFST 0
#define	MC_CMD_FC_OUT_SPI_READ_BUFFER_LEN 4
#define	MC_CMD_FC_OUT_SPI_READ_BUFFER_MINNUM 1
#define	MC_CMD_FC_OUT_SPI_READ_BUFFER_MAXNUM 63

/* MC_CMD_FC_OUT_SPI_WRITE msgresponse */
#define	MC_CMD_FC_OUT_SPI_WRITE_LEN 0

/* MC_CMD_FC_OUT_SPI_ERASE msgresponse */
#define	MC_CMD_FC_OUT_SPI_ERASE_LEN 0

/* MC_CMD_FC_OUT_DIAG_POWER_NOISE_READ_CONFIG msgresponse */
#define	MC_CMD_FC_OUT_DIAG_POWER_NOISE_READ_CONFIG_LEN 8
#define	MC_CMD_FC_OUT_DIAG_POWER_NOISE_READ_CONFIG_TOGGLE_COUNT_OFST 0
#define	MC_CMD_FC_OUT_DIAG_POWER_NOISE_READ_CONFIG_CLKEN_COUNT_OFST 4

/* MC_CMD_FC_OUT_DIAG_POWER_NOISE_WRITE_CONFIG msgresponse */
#define	MC_CMD_FC_OUT_DIAG_POWER_NOISE_WRITE_CONFIG_LEN 0

/* MC_CMD_FC_OUT_DIAG_DDR_SOAK_START msgresponse */
#define	MC_CMD_FC_OUT_DIAG_DDR_SOAK_START_LEN 0

/* MC_CMD_FC_OUT_DIAG_DDR_SOAK_RESULT msgresponse */
#define	MC_CMD_FC_OUT_DIAG_DDR_SOAK_RESULT_LEN 8
#define	MC_CMD_FC_OUT_DIAG_DDR_SOAK_RESULT_STATUS_OFST 0
#define	MC_CMD_FC_OUT_DIAG_DDR_SOAK_RESULT_PASSED_LBN 0
#define	MC_CMD_FC_OUT_DIAG_DDR_SOAK_RESULT_PASSED_WIDTH 1
#define	MC_CMD_FC_OUT_DIAG_DDR_SOAK_RESULT_FAILED_LBN 1
#define	MC_CMD_FC_OUT_DIAG_DDR_SOAK_RESULT_FAILED_WIDTH 1
#define	MC_CMD_FC_OUT_DIAG_DDR_SOAK_RESULT_COMPLETED_LBN 2
#define	MC_CMD_FC_OUT_DIAG_DDR_SOAK_RESULT_COMPLETED_WIDTH 1
#define	MC_CMD_FC_OUT_DIAG_DDR_SOAK_RESULT_TIMEOUT_LBN 3
#define	MC_CMD_FC_OUT_DIAG_DDR_SOAK_RESULT_TIMEOUT_WIDTH 1
#define	MC_CMD_FC_OUT_DIAG_DDR_SOAK_RESULT_PNF_LBN 4
#define	MC_CMD_FC_OUT_DIAG_DDR_SOAK_RESULT_PNF_WIDTH 1
#define	MC_CMD_FC_OUT_DIAG_DDR_SOAK_RESULT_ERR_COUNT_OFST 4

/* MC_CMD_FC_OUT_DIAG_DDR_SOAK_STOP msgresponse */
#define	MC_CMD_FC_OUT_DIAG_DDR_SOAK_STOP_LEN 0

/* MC_CMD_FC_OUT_DIAG_DDR_SOAK_ERROR msgresponse */
#define	MC_CMD_FC_OUT_DIAG_DDR_SOAK_ERROR_LEN 0

/* MC_CMD_FC_OUT_DIAG_DATAPATH_CTRL_SET_MODE msgresponse */
#define	MC_CMD_FC_OUT_DIAG_DATAPATH_CTRL_SET_MODE_LEN 0

/* MC_CMD_FC_OUT_DIAG_DATAPATH_CTRL_RAW_CONFIG msgresponse */
#define	MC_CMD_FC_OUT_DIAG_DATAPATH_CTRL_RAW_CONFIG_LEN 0


/***********************************/
/* MC_CMD_AOE 
 * AOE operations (on MC rather than FC)
 */
#define	MC_CMD_AOE  0xa

/* MC_CMD_AOE_IN msgrequest */
#define	MC_CMD_AOE_IN_LEN 4
#define	MC_CMD_AOE_IN_OP_HDR_OFST 0
#define	MC_CMD_AOE_IN_OP_LBN 0
#define	MC_CMD_AOE_IN_OP_WIDTH 8
#define	MC_CMD_AOE_OP_INFO 0x1 /* enum */
#define	MC_CMD_AOE_OP_CURRENTS 0x2 /* enum */
#define	MC_CMD_AOE_OP_TEMPERATURES 0x3 /* enum */
#define	MC_CMD_AOE_OP_CPLD_IDLE 0x4 /* enum */
#define	MC_CMD_AOE_OP_CPLD_READ 0x5 /* enum */
#define	MC_CMD_AOE_OP_CPLD_WRITE 0x6 /* enum */
#define	MC_CMD_AOE_OP_CPLD_INSTRUCTION 0x7 /* enum */
#define	MC_CMD_AOE_OP_CPLD_REPROGRAM 0x8 /* enum */
#define	MC_CMD_AOE_OP_POWER 0x9 /* enum */
#define	MC_CMD_AOE_OP_LOAD 0xa /* enum */
#define	MC_CMD_AOE_OP_FAN_CONTROL 0xb /* enum */
#define	MC_CMD_AOE_OP_FAN_FAILURES 0xc /* enum */
#define	MC_CMD_AOE_OP_MAC_STATS 0xd /* enum */
#define	MC_CMD_AOE_OP_GET_PHY_MEDIA_INFO 0xe /* enum */
#define	MC_CMD_AOE_OP_JTAG_WRITE 0xf /* enum */
#define	MC_CMD_AOE_OP_FPGA_ACCESS 0x10 /* enum */
#define	MC_CMD_AOE_OP_SET_MTU_OFFSET 0x11 /* enum */
#define	MC_CMD_AOE_OP_LINK_STATE 0x12 /* enum */
#define	MC_CMD_AOE_OP_SIENA_STATS 0x13 /* enum */
#define	MC_CMD_AOE_OP_DDR 0x14 /* enum */
#define	MC_CMD_AOE_OP_FC 0x15 /* enum */
#define	MC_CMD_AOE_OP_DDR_ECC_STATUS 0x16 /* enum */
#define	MC_CMD_AOE_OP_MC_SPI_MASTER 0x17 /* enum */

/* MC_CMD_AOE_OUT msgresponse */
#define	MC_CMD_AOE_OUT_LEN 0

/* MC_CMD_AOE_IN_INFO msgrequest */
#define	MC_CMD_AOE_IN_INFO_LEN 4
#define	MC_CMD_AOE_IN_CMD_OFST 0

/* MC_CMD_AOE_IN_CURRENTS msgrequest */
#define	MC_CMD_AOE_IN_CURRENTS_LEN 4
/*            MC_CMD_AOE_IN_CMD_OFST 0 */

/* MC_CMD_AOE_IN_TEMPERATURES msgrequest */
#define	MC_CMD_AOE_IN_TEMPERATURES_LEN 4
/*            MC_CMD_AOE_IN_CMD_OFST 0 */

/* MC_CMD_AOE_IN_CPLD_IDLE msgrequest */
#define	MC_CMD_AOE_IN_CPLD_IDLE_LEN 4
/*            MC_CMD_AOE_IN_CMD_OFST 0 */

/* MC_CMD_AOE_IN_CPLD_READ msgrequest */
#define	MC_CMD_AOE_IN_CPLD_READ_LEN 12
/*            MC_CMD_AOE_IN_CMD_OFST 0 */
#define	MC_CMD_AOE_IN_CPLD_READ_REGISTER_OFST 4
#define	MC_CMD_AOE_IN_CPLD_READ_WIDTH_OFST 8

/* MC_CMD_AOE_IN_CPLD_WRITE msgrequest */
#define	MC_CMD_AOE_IN_CPLD_WRITE_LEN 16
/*            MC_CMD_AOE_IN_CMD_OFST 0 */
#define	MC_CMD_AOE_IN_CPLD_WRITE_REGISTER_OFST 4
#define	MC_CMD_AOE_IN_CPLD_WRITE_WIDTH_OFST 8
#define	MC_CMD_AOE_IN_CPLD_WRITE_VALUE_OFST 12

/* MC_CMD_AOE_IN_CPLD_INSTRUCTION msgrequest */
#define	MC_CMD_AOE_IN_CPLD_INSTRUCTION_LEN 8
/*            MC_CMD_AOE_IN_CMD_OFST 0 */
#define	MC_CMD_AOE_IN_CPLD_INSTRUCTION_INSTRUCTION_OFST 4

/* MC_CMD_AOE_IN_CPLD_REPROGRAM msgrequest */
#define	MC_CMD_AOE_IN_CPLD_REPROGRAM_LEN 8
/*            MC_CMD_AOE_IN_CMD_OFST 0 */
#define	MC_CMD_AOE_IN_CPLD_REPROGRAM_OP_OFST 4
#define	MC_CMD_AOE_IN_CPLD_REPROGRAM_REPROGRAM 0x1 /* enum */
#define	MC_CMD_AOE_IN_CPLD_REPROGRAM_REPROGRAM_EVENT 0x3 /* enum */
#define	MC_CMD_AOE_IN_CPLD_REPROGRAM_STATUS 0x4 /* enum */

/* MC_CMD_AOE_IN_POWER msgrequest */
#define	MC_CMD_AOE_IN_POWER_LEN 8
/*            MC_CMD_AOE_IN_CMD_OFST 0 */
#define	MC_CMD_AOE_IN_POWER_OP_OFST 4
#define	MC_CMD_AOE_IN_POWER_OFF  0x0 /* enum */
#define	MC_CMD_AOE_IN_POWER_ON  0x1 /* enum */
#define	MC_CMD_AOE_IN_POWER_CLEAR  0x2 /* enum */
#define	MC_CMD_AOE_IN_POWER_SHOW_CURRENT  0x3 /* enum */
#define	MC_CMD_AOE_IN_POWER_SHOW_PEAK  0x4 /* enum */
#define	MC_CMD_AOE_IN_POWER_DDR_LAST  0x5 /* enum */
#define	MC_CMD_AOE_IN_POWER_DDR_PEAK  0x6 /* enum */
#define	MC_CMD_AOE_IN_POWER_DDR_CLEAR  0x7 /* enum */

/* MC_CMD_AOE_IN_LOAD msgrequest */
#define	MC_CMD_AOE_IN_LOAD_LEN 8
/*            MC_CMD_AOE_IN_CMD_OFST 0 */
#define	MC_CMD_AOE_IN_LOAD_IMAGE_OFST 4

/* MC_CMD_AOE_IN_FAN_CONTROL msgrequest */
#define	MC_CMD_AOE_IN_FAN_CONTROL_LEN 8
/*            MC_CMD_AOE_IN_CMD_OFST 0 */
#define	MC_CMD_AOE_IN_FAN_CONTROL_REAL_RPM_OFST 4

/* MC_CMD_AOE_IN_FAN_FAILURES msgrequest */
#define	MC_CMD_AOE_IN_FAN_FAILURES_LEN 4
/*            MC_CMD_AOE_IN_CMD_OFST 0 */

/* MC_CMD_AOE_IN_MAC_STATS msgrequest */
#define	MC_CMD_AOE_IN_MAC_STATS_LEN 24
/*            MC_CMD_AOE_IN_CMD_OFST 0 */
#define	MC_CMD_AOE_IN_MAC_STATS_PORT_OFST 4
#define	MC_CMD_AOE_IN_MAC_STATS_DMA_ADDR_OFST 8
#define	MC_CMD_AOE_IN_MAC_STATS_DMA_ADDR_LEN 8
#define	MC_CMD_AOE_IN_MAC_STATS_DMA_ADDR_LO_OFST 8
#define	MC_CMD_AOE_IN_MAC_STATS_DMA_ADDR_HI_OFST 12
#define	MC_CMD_AOE_IN_MAC_STATS_CMD_OFST 16
#define	MC_CMD_AOE_IN_MAC_STATS_DMA_LBN 0
#define	MC_CMD_AOE_IN_MAC_STATS_DMA_WIDTH 1
#define	MC_CMD_AOE_IN_MAC_STATS_CLEAR_LBN 1
#define	MC_CMD_AOE_IN_MAC_STATS_CLEAR_WIDTH 1
#define	MC_CMD_AOE_IN_MAC_STATS_PERIODIC_CHANGE_LBN 2
#define	MC_CMD_AOE_IN_MAC_STATS_PERIODIC_CHANGE_WIDTH 1
#define	MC_CMD_AOE_IN_MAC_STATS_PERIODIC_ENABLE_LBN 3
#define	MC_CMD_AOE_IN_MAC_STATS_PERIODIC_ENABLE_WIDTH 1
#define	MC_CMD_AOE_IN_MAC_STATS_PERIODIC_CLEAR_LBN 4
#define	MC_CMD_AOE_IN_MAC_STATS_PERIODIC_CLEAR_WIDTH 1
#define	MC_CMD_AOE_IN_MAC_STATS_PERIODIC_NOEVENT_LBN 5
#define	MC_CMD_AOE_IN_MAC_STATS_PERIODIC_NOEVENT_WIDTH 1
#define	MC_CMD_AOE_IN_MAC_STATS_PERIOD_MS_LBN 16
#define	MC_CMD_AOE_IN_MAC_STATS_PERIOD_MS_WIDTH 16
#define	MC_CMD_AOE_IN_MAC_STATS_DMA_LEN_OFST 20

/* MC_CMD_AOE_IN_GET_PHY_MEDIA_INFO msgrequest */
#define	MC_CMD_AOE_IN_GET_PHY_MEDIA_INFO_LEN 12
/*            MC_CMD_AOE_IN_CMD_OFST 0 */
#define	MC_CMD_AOE_IN_GET_PHY_MEDIA_INFO_PORT_OFST 4
#define	MC_CMD_AOE_IN_GET_PHY_MEDIA_INFO_PAGE_OFST 8

/* MC_CMD_AOE_IN_JTAG_WRITE msgrequest */
#define	MC_CMD_AOE_IN_JTAG_WRITE_LENMIN 12
#define	MC_CMD_AOE_IN_JTAG_WRITE_LENMAX 252
#define	MC_CMD_AOE_IN_JTAG_WRITE_LEN(num) (8+4*(num))
/*            MC_CMD_AOE_IN_CMD_OFST 0 */
#define	MC_CMD_AOE_IN_JTAG_WRITE_DATALEN_OFST 4
#define	MC_CMD_AOE_IN_JTAG_WRITE_DATA_OFST 8
#define	MC_CMD_AOE_IN_JTAG_WRITE_DATA_LEN 4
#define	MC_CMD_AOE_IN_JTAG_WRITE_DATA_MINNUM 1
#define	MC_CMD_AOE_IN_JTAG_WRITE_DATA_MAXNUM 61

/* MC_CMD_AOE_IN_FPGA_ACCESS msgrequest */
#define	MC_CMD_AOE_IN_FPGA_ACCESS_LEN 8
/*            MC_CMD_AOE_IN_CMD_OFST 0 */
#define	MC_CMD_AOE_IN_FPGA_ACCESS_OP_OFST 4
#define	MC_CMD_AOE_IN_FPGA_ACCESS_ENABLE 0x1 /* enum */
#define	MC_CMD_AOE_IN_FPGA_ACCESS_DISABLE 0x2 /* enum */

/* MC_CMD_AOE_IN_SET_MTU_OFFSET msgrequest */
#define	MC_CMD_AOE_IN_SET_MTU_OFFSET_LEN 12
/*            MC_CMD_AOE_IN_CMD_OFST 0 */
#define	MC_CMD_AOE_IN_SET_MTU_OFFSET_PORT_OFST 4
#define	MC_CMD_AOE_IN_SET_MTU_OFFSET_ALL_EXTERNAL 0x8000 /* enum */
#define	MC_CMD_AOE_IN_SET_MTU_OFFSET_ALL_INTERNAL 0x4000 /* enum */
#define	MC_CMD_AOE_IN_SET_MTU_OFFSET_OFFSET_OFST 8

/* MC_CMD_AOE_IN_LINK_STATE msgrequest */
#define	MC_CMD_AOE_IN_LINK_STATE_LEN 8
/*            MC_CMD_AOE_IN_CMD_OFST 0 */
#define	MC_CMD_AOE_IN_LINK_STATE_MODE_OFST 4
#define	MC_CMD_AOE_IN_LINK_STATE_CONFIG_MODE_LBN 0
#define	MC_CMD_AOE_IN_LINK_STATE_CONFIG_MODE_WIDTH 8
#define	MC_CMD_AOE_IN_LINK_STATE_SIMPLE_SEPARATE  0x0 /* enum */
#define	MC_CMD_AOE_IN_LINK_STATE_SIMPLE_COMBINED  0x1 /* enum */
#define	MC_CMD_AOE_IN_LINK_STATE_DIAGNOSTIC  0x2 /* enum */
#define	MC_CMD_AOE_IN_LINK_STATE_CUSTOM  0x3 /* enum */
#define	MC_CMD_AOE_IN_LINK_STATE_OPERATION_LBN 8
#define	MC_CMD_AOE_IN_LINK_STATE_OPERATION_WIDTH 8
#define	MC_CMD_AOE_IN_LINK_STATE_OP_NONE  0x0 /* enum */
#define	MC_CMD_AOE_IN_LINK_STATE_OP_OR  0x1 /* enum */
#define	MC_CMD_AOE_IN_LINK_STATE_OP_AND  0x2 /* enum */
#define	MC_CMD_AOE_IN_LINK_STATE_SFP_MASK_LBN 16
#define	MC_CMD_AOE_IN_LINK_STATE_SFP_MASK_WIDTH 16

/* MC_CMD_AOE_IN_SIENA_STATS msgrequest */
#define	MC_CMD_AOE_IN_SIENA_STATS_LEN 8
/*            MC_CMD_AOE_IN_CMD_OFST 0 */
#define	MC_CMD_AOE_IN_SIENA_STATS_MODE_OFST 4
#define	MC_CMD_AOE_IN_SIENA_STATS_STATS_SIENA  0x0 /* enum */
#define	MC_CMD_AOE_IN_SIENA_STATS_STATS_AOE  0x1 /* enum */

/* MC_CMD_AOE_IN_DDR msgrequest */
#define	MC_CMD_AOE_IN_DDR_LEN 12
/*            MC_CMD_AOE_IN_CMD_OFST 0 */
#define	MC_CMD_AOE_IN_DDR_BANK_OFST 4
/*            Enum values, see field(s): */
/*               MC_CMD_FC_IN_DDR_BANK */
#define	MC_CMD_AOE_IN_DDR_SPD_PAGE_ID_OFST 8

/* MC_CMD_AOE_IN_FC msgrequest */
#define	MC_CMD_AOE_IN_FC_LEN 4
/*            MC_CMD_AOE_IN_CMD_OFST 0 */

/* MC_CMD_AOE_IN_DDR_ECC_STATUS msgrequest */
#define	MC_CMD_AOE_IN_DDR_ECC_STATUS_LEN 8
/*            MC_CMD_AOE_IN_CMD_OFST 0 */
#define	MC_CMD_AOE_IN_DDR_ECC_STATUS_BANK_OFST 4
/*            Enum values, see field(s): */
/*               MC_CMD_FC_IN_DDR_BANK */

/* MC_CMD_AOE_IN_MC_SPI_MASTER msgrequest */
#define	MC_CMD_AOE_IN_MC_SPI_MASTER_LEN 8
/*            MC_CMD_AOE_IN_CMD_OFST 0 */
#define	MC_CMD_AOE_IN_MC_SPI_MASTER_OP_OFST 4
#define	MC_CMD_AOE_IN_MC_SPI_MASTER_READ 0x0 /* enum */
#define	MC_CMD_AOE_IN_MC_SPI_MASTER_WRITE 0x1 /* enum */

/* MC_CMD_AOE_IN_MC_SPI_MASTER_READ msgrequest */
#define	MC_CMD_AOE_IN_MC_SPI_MASTER_READ_LEN 12
/*            MC_CMD_AOE_IN_CMD_OFST 0 */
#define	MC_CMD_AOE_IN_MC_SPI_MASTER_READ_OP_OFST 4
#define	MC_CMD_AOE_IN_MC_SPI_MASTER_READ_OFFSET_OFST 8

/* MC_CMD_AOE_IN_MC_SPI_MASTER_WRITE msgrequest */
#define	MC_CMD_AOE_IN_MC_SPI_MASTER_WRITE_LEN 16
/*            MC_CMD_AOE_IN_CMD_OFST 0 */
#define	MC_CMD_AOE_IN_MC_SPI_MASTER_WRITE_OP_OFST 4
#define	MC_CMD_AOE_IN_MC_SPI_MASTER_WRITE_OFFSET_OFST 8
#define	MC_CMD_AOE_IN_MC_SPI_MASTER_WRITE_DATA_OFST 12

/* MC_CMD_AOE_OUT_INFO msgresponse */
#define	MC_CMD_AOE_OUT_INFO_LEN 44
#define	MC_CMD_AOE_OUT_INFO_CPLD_IDCODE_OFST 0
#define	MC_CMD_AOE_OUT_INFO_CPLD_VERSION_OFST 4
#define	MC_CMD_AOE_OUT_INFO_FPGA_IDCODE_OFST 8
#define	MC_CMD_AOE_OUT_INFO_FPGA_VERSION_OFST 12
#define	MC_CMD_AOE_OUT_INFO_FPGA_TYPE_OFST 16
#define	MC_CMD_AOE_OUT_INFO_FPGA_STATE_OFST 20
#define	MC_CMD_AOE_OUT_INFO_FPGA_IMAGE_OFST 24
#define	MC_CMD_AOE_OUT_INFO_FC_STATE_OFST 28
#define	MC_CMD_AOE_OUT_INFO_WATCHDOG 0x1 /* enum */
#define	MC_CMD_AOE_OUT_INFO_COMMS 0x2 /* enum */
#define	MC_CMD_AOE_OUT_INFO_FLAGS_OFST 32
#define	MC_CMD_AOE_OUT_INFO_PEG_POWER 0x1 /* enum */
#define	MC_CMD_AOE_OUT_INFO_CPLD_GOOD 0x2 /* enum */
#define	MC_CMD_AOE_OUT_INFO_FPGA_GOOD 0x4 /* enum */
#define	MC_CMD_AOE_OUT_INFO_FPGA_POWER 0x8 /* enum */
#define	MC_CMD_AOE_OUT_INFO_BAD_SODIMM 0x10 /* enum */
#define	MC_CMD_AOE_OUT_INFO_HAS_BYTEBLASTER 0x20 /* enum */
#define	MC_CMD_AOE_OUT_INFO_BOARD_REVISION_OFST 36
#define	MC_CMD_AOE_OUT_INFO_UNKNOWN  0x0 /* enum */
#define	MC_CMD_AOE_OUT_INFO_R1_0  0x10 /* enum */
#define	MC_CMD_AOE_OUT_INFO_R1_1  0x11 /* enum */
#define	MC_CMD_AOE_OUT_INFO_R1_2  0x12 /* enum */
#define	MC_CMD_AOE_OUT_INFO_FC_BOOT_RESULT_OFST 40
#define	MC_CMD_AOE_OUT_INFO_FC_BOOT_FAIL_NO_ERROR 0x0 /* enum */
#define	MC_CMD_AOE_OUT_INFO_FC_BOOT_FAIL_BAD_ADDRESS 0x1 /* enum */
#define	MC_CMD_AOE_OUT_INFO_FC_BOOT_FAIL_BAD_MAGIC 0x2 /* enum */
#define	MC_CMD_AOE_OUT_INFO_FC_BOOT_FAIL_BAD_TEXT 0x3 /* enum */
#define	MC_CMD_AOE_OUT_INFO_FC_BOOT_FAIL_BAD_CHECKSUM 0x4 /* enum */
#define	MC_CMD_AOE_OUT_INFO_FC_BOOT_FAIL_BAD_BSP 0x5 /* enum */
#define	MC_CMD_AOE_OUT_INFO_FC_BOOT_APP_EXECUTE 0x80 /* enum */
#define	MC_CMD_AOE_OUT_INFO_FC_BOOT_NO_BOOTROM 0xff /* enum */

/* MC_CMD_AOE_OUT_CURRENTS msgresponse */
#define	MC_CMD_AOE_OUT_CURRENTS_LEN 68
#define	MC_CMD_AOE_OUT_CURRENTS_VALUES_OFST 0
#define	MC_CMD_AOE_OUT_CURRENTS_VALUES_LEN 4
#define	MC_CMD_AOE_OUT_CURRENTS_VALUES_NUM 17
#define	MC_CMD_AOE_OUT_CURRENTS_I_2V5 0x0 /* enum */
#define	MC_CMD_AOE_OUT_CURRENTS_I_1V8 0x1 /* enum */
#define	MC_CMD_AOE_OUT_CURRENTS_I_GXB 0x2 /* enum */
#define	MC_CMD_AOE_OUT_CURRENTS_I_PGM 0x3 /* enum */
#define	MC_CMD_AOE_OUT_CURRENTS_I_XCVR 0x4 /* enum */
#define	MC_CMD_AOE_OUT_CURRENTS_I_1V5 0x5 /* enum */
#define	MC_CMD_AOE_OUT_CURRENTS_V_3V3 0x6 /* enum */
#define	MC_CMD_AOE_OUT_CURRENTS_V_1V5 0x7 /* enum */
#define	MC_CMD_AOE_OUT_CURRENTS_I_IN 0x8 /* enum */
#define	MC_CMD_AOE_OUT_CURRENTS_I_OUT 0x9 /* enum */
#define	MC_CMD_AOE_OUT_CURRENTS_V_IN 0xa /* enum */
#define	MC_CMD_AOE_OUT_CURRENTS_I_OUT_DDR1 0xb /* enum */
#define	MC_CMD_AOE_OUT_CURRENTS_V_OUT_DDR1 0xc /* enum */
#define	MC_CMD_AOE_OUT_CURRENTS_I_OUT_DDR2 0xd /* enum */
#define	MC_CMD_AOE_OUT_CURRENTS_V_OUT_DDR2 0xe /* enum */
#define	MC_CMD_AOE_OUT_CURRENTS_I_OUT_DDR3 0xf /* enum */
#define	MC_CMD_AOE_OUT_CURRENTS_V_OUT_DDR3 0x10 /* enum */

/* MC_CMD_AOE_OUT_TEMPERATURES msgresponse */
#define	MC_CMD_AOE_OUT_TEMPERATURES_LEN 40
#define	MC_CMD_AOE_OUT_TEMPERATURES_VALUES_OFST 0
#define	MC_CMD_AOE_OUT_TEMPERATURES_VALUES_LEN 4
#define	MC_CMD_AOE_OUT_TEMPERATURES_VALUES_NUM 10
#define	MC_CMD_AOE_OUT_TEMPERATURES_MAIN_0 0x0 /* enum */
#define	MC_CMD_AOE_OUT_TEMPERATURES_MAIN_1 0x1 /* enum */
#define	MC_CMD_AOE_OUT_TEMPERATURES_IND_0 0x2 /* enum */
#define	MC_CMD_AOE_OUT_TEMPERATURES_IND_1 0x3 /* enum */
#define	MC_CMD_AOE_OUT_TEMPERATURES_VCCIO1 0x4 /* enum */
#define	MC_CMD_AOE_OUT_TEMPERATURES_VCCIO2 0x5 /* enum */
#define	MC_CMD_AOE_OUT_TEMPERATURES_VCCIO3 0x6 /* enum */
#define	MC_CMD_AOE_OUT_TEMPERATURES_PSU 0x7 /* enum */
#define	MC_CMD_AOE_OUT_TEMPERATURES_FPGA 0x8 /* enum */
#define	MC_CMD_AOE_OUT_TEMPERATURES_SIENA 0x9 /* enum */

/* MC_CMD_AOE_OUT_CPLD_READ msgresponse */
#define	MC_CMD_AOE_OUT_CPLD_READ_LEN 4
#define	MC_CMD_AOE_OUT_CPLD_READ_VALUE_OFST 0

/* MC_CMD_AOE_OUT_FAN_FAILURES msgresponse */
#define	MC_CMD_AOE_OUT_FAN_FAILURES_LENMIN 4
#define	MC_CMD_AOE_OUT_FAN_FAILURES_LENMAX 252
#define	MC_CMD_AOE_OUT_FAN_FAILURES_LEN(num) (0+4*(num))
#define	MC_CMD_AOE_OUT_FAN_FAILURES_COUNT_OFST 0
#define	MC_CMD_AOE_OUT_FAN_FAILURES_COUNT_LEN 4
#define	MC_CMD_AOE_OUT_FAN_FAILURES_COUNT_MINNUM 1
#define	MC_CMD_AOE_OUT_FAN_FAILURES_COUNT_MAXNUM 63

/* MC_CMD_AOE_OUT_CPLD_REPROGRAM msgresponse */
#define	MC_CMD_AOE_OUT_CPLD_REPROGRAM_LEN 4
#define	MC_CMD_AOE_OUT_CPLD_REPROGRAM_STATUS_OFST 0

/* MC_CMD_AOE_OUT_MAC_STATS_DMA msgresponse */
#define	MC_CMD_AOE_OUT_MAC_STATS_DMA_LEN 0

/* MC_CMD_AOE_OUT_MAC_STATS_NO_DMA msgresponse */
#define	MC_CMD_AOE_OUT_MAC_STATS_NO_DMA_LEN (((MC_CMD_MAC_NSTATS*64))>>3)
#define	MC_CMD_AOE_OUT_MAC_STATS_NO_DMA_STATISTICS_OFST 0
#define	MC_CMD_AOE_OUT_MAC_STATS_NO_DMA_STATISTICS_LEN 8
#define	MC_CMD_AOE_OUT_MAC_STATS_NO_DMA_STATISTICS_LO_OFST 0
#define	MC_CMD_AOE_OUT_MAC_STATS_NO_DMA_STATISTICS_HI_OFST 4
#define	MC_CMD_AOE_OUT_MAC_STATS_NO_DMA_STATISTICS_NUM MC_CMD_MAC_NSTATS

/* MC_CMD_AOE_OUT_GET_PHY_MEDIA_INFO msgresponse */
#define	MC_CMD_AOE_OUT_GET_PHY_MEDIA_INFO_LENMIN 5
#define	MC_CMD_AOE_OUT_GET_PHY_MEDIA_INFO_LENMAX 252
#define	MC_CMD_AOE_OUT_GET_PHY_MEDIA_INFO_LEN(num) (4+1*(num))
#define	MC_CMD_AOE_OUT_GET_PHY_MEDIA_INFO_DATALEN_OFST 0
#define	MC_CMD_AOE_OUT_GET_PHY_MEDIA_INFO_DATA_OFST 4
#define	MC_CMD_AOE_OUT_GET_PHY_MEDIA_INFO_DATA_LEN 1
#define	MC_CMD_AOE_OUT_GET_PHY_MEDIA_INFO_DATA_MINNUM 1
#define	MC_CMD_AOE_OUT_GET_PHY_MEDIA_INFO_DATA_MAXNUM 248

/* MC_CMD_AOE_OUT_JTAG_WRITE msgresponse */
#define	MC_CMD_AOE_OUT_JTAG_WRITE_LENMIN 12
#define	MC_CMD_AOE_OUT_JTAG_WRITE_LENMAX 252
#define	MC_CMD_AOE_OUT_JTAG_WRITE_LEN(num) (8+4*(num))
#define	MC_CMD_AOE_OUT_JTAG_WRITE_DATALEN_OFST 0
#define	MC_CMD_AOE_OUT_JTAG_WRITE_PAD_OFST 4
#define	MC_CMD_AOE_OUT_JTAG_WRITE_DATA_OFST 8
#define	MC_CMD_AOE_OUT_JTAG_WRITE_DATA_LEN 4
#define	MC_CMD_AOE_OUT_JTAG_WRITE_DATA_MINNUM 1
#define	MC_CMD_AOE_OUT_JTAG_WRITE_DATA_MAXNUM 61

/* MC_CMD_AOE_OUT_FPGA_ACCESS msgresponse */
#define	MC_CMD_AOE_OUT_FPGA_ACCESS_LEN 0

/* MC_CMD_AOE_OUT_DDR msgresponse */
#define	MC_CMD_AOE_OUT_DDR_LENMIN 17
#define	MC_CMD_AOE_OUT_DDR_LENMAX 252
#define	MC_CMD_AOE_OUT_DDR_LEN(num) (16+1*(num))
#define	MC_CMD_AOE_OUT_DDR_FLAGS_OFST 0
#define	MC_CMD_AOE_OUT_DDR_PRESENT_LBN 0
#define	MC_CMD_AOE_OUT_DDR_PRESENT_WIDTH 1
#define	MC_CMD_AOE_OUT_DDR_POWERED_LBN 1
#define	MC_CMD_AOE_OUT_DDR_POWERED_WIDTH 1
#define	MC_CMD_AOE_OUT_DDR_OPERATIONAL_LBN 2
#define	MC_CMD_AOE_OUT_DDR_OPERATIONAL_WIDTH 1
#define	MC_CMD_AOE_OUT_DDR_CAPACITY_OFST 4
#define	MC_CMD_AOE_OUT_DDR_TYPE_OFST 8
#define	MC_CMD_AOE_OUT_DDR_VOLTAGE_OFST 12
#define	MC_CMD_AOE_OUT_DDR_SPD_OFST 16
#define	MC_CMD_AOE_OUT_DDR_SPD_LEN 1
#define	MC_CMD_AOE_OUT_DDR_SPD_MINNUM 1
#define	MC_CMD_AOE_OUT_DDR_SPD_MAXNUM 236

/* MC_CMD_AOE_OUT_LINK_STATE msgresponse */
#define	MC_CMD_AOE_OUT_LINK_STATE_LEN 0

/* MC_CMD_AOE_OUT_FC msgresponse */
#define	MC_CMD_AOE_OUT_FC_LEN 0

/* MC_CMD_AOE_OUT_DDR_ECC_STATUS msgresponse */
#define	MC_CMD_AOE_OUT_DDR_ECC_STATUS_LEN 8
#define	MC_CMD_AOE_OUT_DDR_ECC_STATUS_FLAGS_OFST 0
#define	MC_CMD_AOE_OUT_DDR_ECC_STATUS_VALID_LBN 0
#define	MC_CMD_AOE_OUT_DDR_ECC_STATUS_VALID_WIDTH 1
#define	MC_CMD_AOE_OUT_DDR_ECC_STATUS_STATUS_OFST 4
#define	MC_CMD_AOE_OUT_DDR_ECC_STATUS_SBE_LBN 0
#define	MC_CMD_AOE_OUT_DDR_ECC_STATUS_SBE_WIDTH 1
#define	MC_CMD_AOE_OUT_DDR_ECC_STATUS_DBE_LBN 1
#define	MC_CMD_AOE_OUT_DDR_ECC_STATUS_DBE_WIDTH 1
#define	MC_CMD_AOE_OUT_DDR_ECC_STATUS_CORDROP_LBN 2
#define	MC_CMD_AOE_OUT_DDR_ECC_STATUS_CORDROP_WIDTH 1
#define	MC_CMD_AOE_OUT_DDR_ECC_STATUS_SBE_COUNT_LBN 8
#define	MC_CMD_AOE_OUT_DDR_ECC_STATUS_SBE_COUNT_WIDTH 8
#define	MC_CMD_AOE_OUT_DDR_ECC_STATUS_DBE_COUNT_LBN 16
#define	MC_CMD_AOE_OUT_DDR_ECC_STATUS_DBE_COUNT_WIDTH 8
#define	MC_CMD_AOE_OUT_DDR_ECC_STATUS_CORDROP_COUNT_LBN 24
#define	MC_CMD_AOE_OUT_DDR_ECC_STATUS_CORDROP_COUNT_WIDTH 8

/* MC_CMD_AOE_OUT_MC_SPI_MASTER_READ msgresponse */
#define	MC_CMD_AOE_OUT_MC_SPI_MASTER_READ_LEN 4
#define	MC_CMD_AOE_OUT_MC_SPI_MASTER_READ_DATA_OFST 0

/* MC_CMD_AOE_OUT_MC_SPI_MASTER_WRITE msgresponse */
#define	MC_CMD_AOE_OUT_MC_SPI_MASTER_WRITE_LEN 0

/* MC_CMD_AOE_OUT_MC_SPI_MASTER msgresponse */
#define	MC_CMD_AOE_OUT_MC_SPI_MASTER_LEN 0


/***********************************/
/* MC_CMD_PTP 
 * Perform PTP operation
 */
#define	MC_CMD_PTP  0xb

/* MC_CMD_PTP_IN msgrequest */
#define	MC_CMD_PTP_IN_LEN 1
#define	MC_CMD_PTP_IN_OP_OFST 0
#define	MC_CMD_PTP_IN_OP_LEN 1
#define	MC_CMD_PTP_OP_ENABLE 0x1 /* enum */
#define	MC_CMD_PTP_OP_DISABLE 0x2 /* enum */
#define	MC_CMD_PTP_OP_TRANSMIT 0x3 /* enum */
#define	MC_CMD_PTP_OP_READ_NIC_TIME 0x4 /* enum */
#define	MC_CMD_PTP_OP_STATUS 0x5 /* enum */
#define	MC_CMD_PTP_OP_ADJUST 0x6 /* enum */
#define	MC_CMD_PTP_OP_SYNCHRONIZE 0x7 /* enum */
#define	MC_CMD_PTP_OP_MANFTEST_BASIC 0x8 /* enum */
#define	MC_CMD_PTP_OP_MANFTEST_PACKET 0x9 /* enum */
#define	MC_CMD_PTP_OP_RESET_STATS 0xa /* enum */
#define	MC_CMD_PTP_OP_DEBUG 0xb /* enum */
#define	MC_CMD_PTP_OP_FPGAREAD 0xc /* enum */
#define	MC_CMD_PTP_OP_FPGAWRITE 0xd /* enum */
#define	MC_CMD_PTP_OP_CLOCK_OFFSET_ADJUST 0xe /* enum */
#define	MC_CMD_PTP_OP_CLOCK_FREQ_ADJUST 0xf /* enum */
#define	MC_CMD_PTP_OP_RX_SET_VLAN_FILTER 0x10 /* enum */
#define	MC_CMD_PTP_OP_RX_SET_UUID_FILTER 0x11 /* enum */
#define	MC_CMD_PTP_OP_RX_SET_DOMAIN_FILTER 0x12 /* enum */
#define	MC_CMD_PTP_OP_SET_CLK_SRC 0x13 /* enum */
#define	MC_CMD_PTP_OP_RST_CLK 0x14 /* enum */
#define	MC_CMD_PTP_OP_PPS_ENABLE 0x15 /* enum */
#define	MC_CMD_PTP_OP_MAX 0x16 /* enum */

/* MC_CMD_PTP_IN_ENABLE msgrequest */
#define	MC_CMD_PTP_IN_ENABLE_LEN 16
#define	MC_CMD_PTP_IN_CMD_OFST 0
#define	MC_CMD_PTP_IN_PERIPH_ID_OFST 4
#define	MC_CMD_PTP_IN_ENABLE_QUEUE_OFST 8
#define	MC_CMD_PTP_IN_ENABLE_MODE_OFST 12
#define	MC_CMD_PTP_MODE_V1 0x0 /* enum */
#define	MC_CMD_PTP_MODE_V1_VLAN 0x1 /* enum */
#define	MC_CMD_PTP_MODE_V2 0x2 /* enum */
#define	MC_CMD_PTP_MODE_V2_VLAN 0x3 /* enum */
#define	MC_CMD_PTP_MODE_V2_ENHANCED 0x4 /* enum */

/* MC_CMD_PTP_IN_DISABLE msgrequest */
#define	MC_CMD_PTP_IN_DISABLE_LEN 8
/*            MC_CMD_PTP_IN_CMD_OFST 0 */
/*            MC_CMD_PTP_IN_PERIPH_ID_OFST 4 */

/* MC_CMD_PTP_IN_TRANSMIT msgrequest */
#define	MC_CMD_PTP_IN_TRANSMIT_LENMIN 13
#define	MC_CMD_PTP_IN_TRANSMIT_LENMAX 252
#define	MC_CMD_PTP_IN_TRANSMIT_LEN(num) (12+1*(num))
/*            MC_CMD_PTP_IN_CMD_OFST 0 */
/*            MC_CMD_PTP_IN_PERIPH_ID_OFST 4 */
#define	MC_CMD_PTP_IN_TRANSMIT_LENGTH_OFST 8
#define	MC_CMD_PTP_IN_TRANSMIT_PACKET_OFST 12
#define	MC_CMD_PTP_IN_TRANSMIT_PACKET_LEN 1
#define	MC_CMD_PTP_IN_TRANSMIT_PACKET_MINNUM 1
#define	MC_CMD_PTP_IN_TRANSMIT_PACKET_MAXNUM 240

/* MC_CMD_PTP_IN_READ_NIC_TIME msgrequest */
#define	MC_CMD_PTP_IN_READ_NIC_TIME_LEN 8
/*            MC_CMD_PTP_IN_CMD_OFST 0 */
/*            MC_CMD_PTP_IN_PERIPH_ID_OFST 4 */

/* MC_CMD_PTP_IN_STATUS msgrequest */
#define	MC_CMD_PTP_IN_STATUS_LEN 8
/*            MC_CMD_PTP_IN_CMD_OFST 0 */
/*            MC_CMD_PTP_IN_PERIPH_ID_OFST 4 */

/* MC_CMD_PTP_IN_ADJUST msgrequest */
#define	MC_CMD_PTP_IN_ADJUST_LEN 24
/*            MC_CMD_PTP_IN_CMD_OFST 0 */
/*            MC_CMD_PTP_IN_PERIPH_ID_OFST 4 */
#define	MC_CMD_PTP_IN_ADJUST_FREQ_OFST 8
#define	MC_CMD_PTP_IN_ADJUST_FREQ_LEN 8
#define	MC_CMD_PTP_IN_ADJUST_FREQ_LO_OFST 8
#define	MC_CMD_PTP_IN_ADJUST_FREQ_HI_OFST 12
#define	MC_CMD_PTP_IN_ADJUST_BITS 0x28 /* enum */
#define	MC_CMD_PTP_IN_ADJUST_SECONDS_OFST 16
#define	MC_CMD_PTP_IN_ADJUST_NANOSECONDS_OFST 20

/* MC_CMD_PTP_IN_SYNCHRONIZE msgrequest */
#define	MC_CMD_PTP_IN_SYNCHRONIZE_LEN 20
/*            MC_CMD_PTP_IN_CMD_OFST 0 */
/*            MC_CMD_PTP_IN_PERIPH_ID_OFST 4 */
#define	MC_CMD_PTP_IN_SYNCHRONIZE_NUMTIMESETS_OFST 8
#define	MC_CMD_PTP_IN_SYNCHRONIZE_START_ADDR_OFST 12
#define	MC_CMD_PTP_IN_SYNCHRONIZE_START_ADDR_LEN 8
#define	MC_CMD_PTP_IN_SYNCHRONIZE_START_ADDR_LO_OFST 12
#define	MC_CMD_PTP_IN_SYNCHRONIZE_START_ADDR_HI_OFST 16

/* MC_CMD_PTP_IN_MANFTEST_BASIC msgrequest */
#define	MC_CMD_PTP_IN_MANFTEST_BASIC_LEN 8
/*            MC_CMD_PTP_IN_CMD_OFST 0 */
/*            MC_CMD_PTP_IN_PERIPH_ID_OFST 4 */

/* MC_CMD_PTP_IN_MANFTEST_PACKET msgrequest */
#define	MC_CMD_PTP_IN_MANFTEST_PACKET_LEN 12
/*            MC_CMD_PTP_IN_CMD_OFST 0 */
/*            MC_CMD_PTP_IN_PERIPH_ID_OFST 4 */
#define	MC_CMD_PTP_IN_MANFTEST_PACKET_TEST_ENABLE_OFST 8

/* MC_CMD_PTP_IN_RESET_STATS msgrequest */
#define	MC_CMD_PTP_IN_RESET_STATS_LEN 8
/*            MC_CMD_PTP_IN_CMD_OFST 0 */
/*            MC_CMD_PTP_IN_PERIPH_ID_OFST 4 */

/* MC_CMD_PTP_IN_DEBUG msgrequest */
#define	MC_CMD_PTP_IN_DEBUG_LEN 12
/*            MC_CMD_PTP_IN_CMD_OFST 0 */
/*            MC_CMD_PTP_IN_PERIPH_ID_OFST 4 */
#define	MC_CMD_PTP_IN_DEBUG_DEBUG_PARAM_OFST 8

/* MC_CMD_PTP_IN_FPGAREAD msgrequest */
#define	MC_CMD_PTP_IN_FPGAREAD_LEN 16
/*            MC_CMD_PTP_IN_CMD_OFST 0 */
/*            MC_CMD_PTP_IN_PERIPH_ID_OFST 4 */
#define	MC_CMD_PTP_IN_FPGAREAD_ADDR_OFST 8
#define	MC_CMD_PTP_IN_FPGAREAD_NUMBYTES_OFST 12

/* MC_CMD_PTP_IN_FPGAWRITE msgrequest */
#define	MC_CMD_PTP_IN_FPGAWRITE_LENMIN 13
#define	MC_CMD_PTP_IN_FPGAWRITE_LENMAX 252
#define	MC_CMD_PTP_IN_FPGAWRITE_LEN(num) (12+1*(num))
/*            MC_CMD_PTP_IN_CMD_OFST 0 */
/*            MC_CMD_PTP_IN_PERIPH_ID_OFST 4 */
#define	MC_CMD_PTP_IN_FPGAWRITE_ADDR_OFST 8
#define	MC_CMD_PTP_IN_FPGAWRITE_BUFFER_OFST 12
#define	MC_CMD_PTP_IN_FPGAWRITE_BUFFER_LEN 1
#define	MC_CMD_PTP_IN_FPGAWRITE_BUFFER_MINNUM 1
#define	MC_CMD_PTP_IN_FPGAWRITE_BUFFER_MAXNUM 240

/* MC_CMD_PTP_IN_CLOCK_OFFSET_ADJUST msgrequest */
#define	MC_CMD_PTP_IN_CLOCK_OFFSET_ADJUST_LEN 16
/*            MC_CMD_PTP_IN_CMD_OFST 0 */
/*            MC_CMD_PTP_IN_PERIPH_ID_OFST 4 */
#define	MC_CMD_PTP_IN_CLOCK_OFFSET_ADJUST_SECONDS_OFST 8
#define	MC_CMD_PTP_IN_CLOCK_OFFSET_ADJUST_NANOSECONDS_OFST 12

/* MC_CMD_PTP_IN_CLOCK_FREQ_ADJUST msgrequest */
#define	MC_CMD_PTP_IN_CLOCK_FREQ_ADJUST_LEN 16
/*            MC_CMD_PTP_IN_CMD_OFST 0 */
/*            MC_CMD_PTP_IN_PERIPH_ID_OFST 4 */
#define	MC_CMD_PTP_IN_CLOCK_FREQ_ADJUST_FREQ_OFST 8
#define	MC_CMD_PTP_IN_CLOCK_FREQ_ADJUST_FREQ_LEN 8
#define	MC_CMD_PTP_IN_CLOCK_FREQ_ADJUST_FREQ_LO_OFST 8
#define	MC_CMD_PTP_IN_CLOCK_FREQ_ADJUST_FREQ_HI_OFST 12
/*               MC_CMD_PTP_IN_ADJUST_BITS 0x28 */

/* MC_CMD_PTP_IN_RX_SET_VLAN_FILTER msgrequest */
#define	MC_CMD_PTP_IN_RX_SET_VLAN_FILTER_LEN 24
/*            MC_CMD_PTP_IN_CMD_OFST 0 */
/*            MC_CMD_PTP_IN_PERIPH_ID_OFST 4 */
#define	MC_CMD_PTP_IN_RX_SET_VLAN_FILTER_NUM_VLAN_TAGS_OFST 8
#define	MC_CMD_PTP_IN_RX_SET_VLAN_FILTER_VLAN_TAG_OFST 12
#define	MC_CMD_PTP_IN_RX_SET_VLAN_FILTER_VLAN_TAG_LEN 4
#define	MC_CMD_PTP_IN_RX_SET_VLAN_FILTER_VLAN_TAG_NUM 3

/* MC_CMD_PTP_IN_RX_SET_UUID_FILTER msgrequest */
#define	MC_CMD_PTP_IN_RX_SET_UUID_FILTER_LEN 20
/*            MC_CMD_PTP_IN_CMD_OFST 0 */
/*            MC_CMD_PTP_IN_PERIPH_ID_OFST 4 */
#define	MC_CMD_PTP_IN_RX_SET_UUID_FILTER_ENABLE_OFST 8
#define	MC_CMD_PTP_IN_RX_SET_UUID_FILTER_UUID_OFST 12
#define	MC_CMD_PTP_IN_RX_SET_UUID_FILTER_UUID_LEN 8
#define	MC_CMD_PTP_IN_RX_SET_UUID_FILTER_UUID_LO_OFST 12
#define	MC_CMD_PTP_IN_RX_SET_UUID_FILTER_UUID_HI_OFST 16

/* MC_CMD_PTP_IN_RX_SET_DOMAIN_FILTER msgrequest */
#define	MC_CMD_PTP_IN_RX_SET_DOMAIN_FILTER_LEN 16
/*            MC_CMD_PTP_IN_CMD_OFST 0 */
/*            MC_CMD_PTP_IN_PERIPH_ID_OFST 4 */
#define	MC_CMD_PTP_IN_RX_SET_DOMAIN_FILTER_ENABLE_OFST 8
#define	MC_CMD_PTP_IN_RX_SET_DOMAIN_FILTER_DOMAIN_OFST 12

/* MC_CMD_PTP_IN_PPS_ENABLE msgrequest */
#define	MC_CMD_PTP_IN_PPS_ENABLE_LEN 12
/*            MC_CMD_PTP_IN_CMD_OFST 0 */
#define	MC_CMD_PTP_IN_PPS_ENABLE_OP_OFST 4
#define	MC_CMD_PTP_ENABLE_PPS 0x0 /* enum */
#define	MC_CMD_PTP_DISABLE_PPS 0x1 /* enum */
#define	MC_CMD_PTP_IN_PPS_ENABLE_QUEUE_ID_OFST 8

/* MC_CMD_PTP_OUT msgresponse */
#define	MC_CMD_PTP_OUT_LEN 0

/* MC_CMD_PTP_OUT_TRANSMIT msgresponse */
#define	MC_CMD_PTP_OUT_TRANSMIT_LEN 8
#define	MC_CMD_PTP_OUT_TRANSMIT_SECONDS_OFST 0
#define	MC_CMD_PTP_OUT_TRANSMIT_NANOSECONDS_OFST 4

/* MC_CMD_PTP_OUT_READ_NIC_TIME msgresponse */
#define	MC_CMD_PTP_OUT_READ_NIC_TIME_LEN 8
#define	MC_CMD_PTP_OUT_READ_NIC_TIME_SECONDS_OFST 0
#define	MC_CMD_PTP_OUT_READ_NIC_TIME_NANOSECONDS_OFST 4

/* MC_CMD_PTP_OUT_STATUS msgresponse */
#define	MC_CMD_PTP_OUT_STATUS_LEN 64
#define	MC_CMD_PTP_OUT_STATUS_CLOCK_FREQ_OFST 0
#define	MC_CMD_PTP_OUT_STATUS_STATS_TX_OFST 4
#define	MC_CMD_PTP_OUT_STATUS_STATS_RX_OFST 8
#define	MC_CMD_PTP_OUT_STATUS_STATS_TS_OFST 12
#define	MC_CMD_PTP_OUT_STATUS_STATS_FM_OFST 16
#define	MC_CMD_PTP_OUT_STATUS_STATS_NFM_OFST 20
#define	MC_CMD_PTP_OUT_STATUS_STATS_PPS_OFLOW_OFST 24
#define	MC_CMD_PTP_OUT_STATUS_STATS_PPS_BAD_OFST 28
#define	MC_CMD_PTP_OUT_STATUS_STATS_PPS_PER_MIN_OFST 32
#define	MC_CMD_PTP_OUT_STATUS_STATS_PPS_PER_MAX_OFST 36
#define	MC_CMD_PTP_OUT_STATUS_STATS_PPS_PER_LAST_OFST 40
#define	MC_CMD_PTP_OUT_STATUS_STATS_PPS_PER_MEAN_OFST 44
#define	MC_CMD_PTP_OUT_STATUS_STATS_PPS_OFF_MIN_OFST 48
#define	MC_CMD_PTP_OUT_STATUS_STATS_PPS_OFF_MAX_OFST 52
#define	MC_CMD_PTP_OUT_STATUS_STATS_PPS_OFF_LAST_OFST 56
#define	MC_CMD_PTP_OUT_STATUS_STATS_PPS_OFF_MEAN_OFST 60

/* MC_CMD_PTP_OUT_SYNCHRONIZE msgresponse */
#define	MC_CMD_PTP_OUT_SYNCHRONIZE_LENMIN 20
#define	MC_CMD_PTP_OUT_SYNCHRONIZE_LENMAX 240
#define	MC_CMD_PTP_OUT_SYNCHRONIZE_LEN(num) (0+20*(num))
#define	MC_CMD_PTP_OUT_SYNCHRONIZE_TIMESET_OFST 0
#define	MC_CMD_PTP_OUT_SYNCHRONIZE_TIMESET_LEN 20
#define	MC_CMD_PTP_OUT_SYNCHRONIZE_TIMESET_MINNUM 1
#define	MC_CMD_PTP_OUT_SYNCHRONIZE_TIMESET_MAXNUM 12
#define	MC_CMD_PTP_OUT_SYNCHRONIZE_HOSTSTART_OFST 0
#define	MC_CMD_PTP_OUT_SYNCHRONIZE_SECONDS_OFST 4
#define	MC_CMD_PTP_OUT_SYNCHRONIZE_NANOSECONDS_OFST 8
#define	MC_CMD_PTP_OUT_SYNCHRONIZE_HOSTEND_OFST 12
#define	MC_CMD_PTP_OUT_SYNCHRONIZE_WAITNS_OFST 16

/* MC_CMD_PTP_OUT_MANFTEST_BASIC msgresponse */
#define	MC_CMD_PTP_OUT_MANFTEST_BASIC_LEN 8
#define	MC_CMD_PTP_OUT_MANFTEST_BASIC_TEST_RESULT_OFST 0
#define	MC_CMD_PTP_MANF_SUCCESS 0x0 /* enum */
#define	MC_CMD_PTP_MANF_FPGA_LOAD 0x1 /* enum */
#define	MC_CMD_PTP_MANF_FPGA_VERSION 0x2 /* enum */
#define	MC_CMD_PTP_MANF_FPGA_REGISTERS 0x3 /* enum */
#define	MC_CMD_PTP_MANF_OSCILLATOR 0x4 /* enum */
#define	MC_CMD_PTP_MANF_TIMESTAMPS 0x5 /* enum */
#define	MC_CMD_PTP_MANF_PACKET_COUNT 0x6 /* enum */
#define	MC_CMD_PTP_MANF_FILTER_COUNT 0x7 /* enum */
#define	MC_CMD_PTP_MANF_PACKET_ENOUGH 0x8 /* enum */
#define	MC_CMD_PTP_MANF_GPIO_TRIGGER 0x9 /* enum */
#define	MC_CMD_PTP_OUT_MANFTEST_BASIC_TEST_EXTOSC_OFST 4

/* MC_CMD_PTP_OUT_MANFTEST_PACKET msgresponse */
#define	MC_CMD_PTP_OUT_MANFTEST_PACKET_LEN 12
#define	MC_CMD_PTP_OUT_MANFTEST_PACKET_TEST_RESULT_OFST 0
#define	MC_CMD_PTP_OUT_MANFTEST_PACKET_TEST_FPGACOUNT_OFST 4
#define	MC_CMD_PTP_OUT_MANFTEST_PACKET_TEST_FILTERCOUNT_OFST 8

/* MC_CMD_PTP_OUT_FPGAREAD msgresponse */
#define	MC_CMD_PTP_OUT_FPGAREAD_LENMIN 1
#define	MC_CMD_PTP_OUT_FPGAREAD_LENMAX 252
#define	MC_CMD_PTP_OUT_FPGAREAD_LEN(num) (0+1*(num))
#define	MC_CMD_PTP_OUT_FPGAREAD_BUFFER_OFST 0
#define	MC_CMD_PTP_OUT_FPGAREAD_BUFFER_LEN 1
#define	MC_CMD_PTP_OUT_FPGAREAD_BUFFER_MINNUM 1
#define	MC_CMD_PTP_OUT_FPGAREAD_BUFFER_MAXNUM 252


/***********************************/
/* MC_CMD_CSR_READ32 
 * Read 32bit words from the indirect memory map.
 */
#define	MC_CMD_CSR_READ32  0xc

/* MC_CMD_CSR_READ32_IN msgrequest */
#define	MC_CMD_CSR_READ32_IN_LEN 12
#define	MC_CMD_CSR_READ32_IN_ADDR_OFST 0
#define	MC_CMD_CSR_READ32_IN_STEP_OFST 4
#define	MC_CMD_CSR_READ32_IN_NUMWORDS_OFST 8

/* MC_CMD_CSR_READ32_OUT msgresponse */
#define	MC_CMD_CSR_READ32_OUT_LENMIN 4
#define	MC_CMD_CSR_READ32_OUT_LENMAX 252
#define	MC_CMD_CSR_READ32_OUT_LEN(num) (0+4*(num))
#define	MC_CMD_CSR_READ32_OUT_BUFFER_OFST 0
#define	MC_CMD_CSR_READ32_OUT_BUFFER_LEN 4
#define	MC_CMD_CSR_READ32_OUT_BUFFER_MINNUM 1
#define	MC_CMD_CSR_READ32_OUT_BUFFER_MAXNUM 63


/***********************************/
/* MC_CMD_CSR_WRITE32 
 * Write 32bit dwords to the indirect memory map.
 */
#define	MC_CMD_CSR_WRITE32  0xd

/* MC_CMD_CSR_WRITE32_IN msgrequest */
#define	MC_CMD_CSR_WRITE32_IN_LENMIN 12
#define	MC_CMD_CSR_WRITE32_IN_LENMAX 252
#define	MC_CMD_CSR_WRITE32_IN_LEN(num) (8+4*(num))
#define	MC_CMD_CSR_WRITE32_IN_ADDR_OFST 0
#define	MC_CMD_CSR_WRITE32_IN_STEP_OFST 4
#define	MC_CMD_CSR_WRITE32_IN_BUFFER_OFST 8
#define	MC_CMD_CSR_WRITE32_IN_BUFFER_LEN 4
#define	MC_CMD_CSR_WRITE32_IN_BUFFER_MINNUM 1
#define	MC_CMD_CSR_WRITE32_IN_BUFFER_MAXNUM 61

/* MC_CMD_CSR_WRITE32_OUT msgresponse */
#define	MC_CMD_CSR_WRITE32_OUT_LEN 4
#define	MC_CMD_CSR_WRITE32_OUT_STATUS_OFST 0


/***********************************/
/* MC_CMD_HP 
 * HP specific commands.
 */
#define	MC_CMD_HP  0x54

/* MC_CMD_HP_IN msgrequest */
#define	MC_CMD_HP_IN_LEN 16
#define	MC_CMD_HP_IN_SUBCMD_OFST 0
#define	MC_CMD_HP_IN_OCSD_SUBCMD 0x0 /* enum */
#define	MC_CMD_HP_IN_LAST_SUBCMD 0x0 /* enum */
#define	MC_CMD_HP_IN_OCSD_ADDR_OFST 4
#define	MC_CMD_HP_IN_OCSD_ADDR_LEN 8
#define	MC_CMD_HP_IN_OCSD_ADDR_LO_OFST 4
#define	MC_CMD_HP_IN_OCSD_ADDR_HI_OFST 8
#define	MC_CMD_HP_IN_OCSD_INTERVAL_OFST 12

/* MC_CMD_HP_OUT msgresponse */
#define	MC_CMD_HP_OUT_LEN 4
#define	MC_CMD_HP_OUT_OCSD_STATUS_OFST 0
#define	MC_CMD_HP_OUT_OCSD_STOPPED 0x1 /* enum */
#define	MC_CMD_HP_OUT_OCSD_STARTED 0x2 /* enum */
#define	MC_CMD_HP_OUT_OCSD_ALREADY_STARTED 0x3 /* enum */


/***********************************/
/* MC_CMD_STACKINFO 
 * Get stack information.
 */
#define	MC_CMD_STACKINFO  0xf

/* MC_CMD_STACKINFO_IN msgrequest */
#define	MC_CMD_STACKINFO_IN_LEN 0

/* MC_CMD_STACKINFO_OUT msgresponse */
#define	MC_CMD_STACKINFO_OUT_LENMIN 12
#define	MC_CMD_STACKINFO_OUT_LENMAX 252
#define	MC_CMD_STACKINFO_OUT_LEN(num) (0+12*(num))
#define	MC_CMD_STACKINFO_OUT_THREAD_INFO_OFST 0
#define	MC_CMD_STACKINFO_OUT_THREAD_INFO_LEN 12
#define	MC_CMD_STACKINFO_OUT_THREAD_INFO_MINNUM 1
#define	MC_CMD_STACKINFO_OUT_THREAD_INFO_MAXNUM 21


/***********************************/
/* MC_CMD_MDIO_READ 
 * MDIO register read.
 */
#define	MC_CMD_MDIO_READ  0x10

/* MC_CMD_MDIO_READ_IN msgrequest */
#define	MC_CMD_MDIO_READ_IN_LEN 16
#define	MC_CMD_MDIO_READ_IN_BUS_OFST 0
#define	MC_CMD_MDIO_BUS_INTERNAL 0x0 /* enum */
#define	MC_CMD_MDIO_BUS_EXTERNAL 0x1 /* enum */
#define	MC_CMD_MDIO_READ_IN_PRTAD_OFST 4
#define	MC_CMD_MDIO_READ_IN_DEVAD_OFST 8
#define	MC_CMD_MDIO_CLAUSE22 0x20 /* enum */
#define	MC_CMD_MDIO_READ_IN_ADDR_OFST 12

/* MC_CMD_MDIO_READ_OUT msgresponse */
#define	MC_CMD_MDIO_READ_OUT_LEN 8
#define	MC_CMD_MDIO_READ_OUT_VALUE_OFST 0
#define	MC_CMD_MDIO_READ_OUT_STATUS_OFST 4
#define	MC_CMD_MDIO_STATUS_GOOD 0x8 /* enum */


/***********************************/
/* MC_CMD_MDIO_WRITE 
 * MDIO register write.
 */
#define	MC_CMD_MDIO_WRITE  0x11

/* MC_CMD_MDIO_WRITE_IN msgrequest */
#define	MC_CMD_MDIO_WRITE_IN_LEN 20
#define	MC_CMD_MDIO_WRITE_IN_BUS_OFST 0
/*               MC_CMD_MDIO_BUS_INTERNAL 0x0 */
/*               MC_CMD_MDIO_BUS_EXTERNAL 0x1 */
#define	MC_CMD_MDIO_WRITE_IN_PRTAD_OFST 4
#define	MC_CMD_MDIO_WRITE_IN_DEVAD_OFST 8
/*               MC_CMD_MDIO_CLAUSE22 0x20 */
#define	MC_CMD_MDIO_WRITE_IN_ADDR_OFST 12
#define	MC_CMD_MDIO_WRITE_IN_VALUE_OFST 16

/* MC_CMD_MDIO_WRITE_OUT msgresponse */
#define	MC_CMD_MDIO_WRITE_OUT_LEN 4
#define	MC_CMD_MDIO_WRITE_OUT_STATUS_OFST 0
/*               MC_CMD_MDIO_STATUS_GOOD 0x8 */


/***********************************/
/* MC_CMD_DBI_WRITE 
 * Write DBI register(s).
 */
#define	MC_CMD_DBI_WRITE  0x12

/* MC_CMD_DBI_WRITE_IN msgrequest */
#define	MC_CMD_DBI_WRITE_IN_LENMIN 12
#define	MC_CMD_DBI_WRITE_IN_LENMAX 252
#define	MC_CMD_DBI_WRITE_IN_LEN(num) (0+12*(num))
#define	MC_CMD_DBI_WRITE_IN_DBIWROP_OFST 0
#define	MC_CMD_DBI_WRITE_IN_DBIWROP_LEN 12
#define	MC_CMD_DBI_WRITE_IN_DBIWROP_MINNUM 1
#define	MC_CMD_DBI_WRITE_IN_DBIWROP_MAXNUM 21

/* MC_CMD_DBI_WRITE_OUT msgresponse */
#define	MC_CMD_DBI_WRITE_OUT_LEN 0

/* MC_CMD_DBIWROP_TYPEDEF structuredef */
#define	MC_CMD_DBIWROP_TYPEDEF_LEN 12
#define	MC_CMD_DBIWROP_TYPEDEF_ADDRESS_OFST 0
#define	MC_CMD_DBIWROP_TYPEDEF_ADDRESS_LBN 0
#define	MC_CMD_DBIWROP_TYPEDEF_ADDRESS_WIDTH 32
#define	MC_CMD_DBIWROP_TYPEDEF_BYTE_MASK_OFST 4
#define	MC_CMD_DBIWROP_TYPEDEF_BYTE_MASK_LBN 32
#define	MC_CMD_DBIWROP_TYPEDEF_BYTE_MASK_WIDTH 32
#define	MC_CMD_DBIWROP_TYPEDEF_VALUE_OFST 8
#define	MC_CMD_DBIWROP_TYPEDEF_VALUE_LBN 64
#define	MC_CMD_DBIWROP_TYPEDEF_VALUE_WIDTH 32


/***********************************/
/* MC_CMD_PORT_READ32 
 * Read a 32-bit register from the indirect port register map.
 */
#define	MC_CMD_PORT_READ32  0x14

/* MC_CMD_PORT_READ32_IN msgrequest */
#define	MC_CMD_PORT_READ32_IN_LEN 4
#define	MC_CMD_PORT_READ32_IN_ADDR_OFST 0

/* MC_CMD_PORT_READ32_OUT msgresponse */
#define	MC_CMD_PORT_READ32_OUT_LEN 8
#define	MC_CMD_PORT_READ32_OUT_VALUE_OFST 0
#define	MC_CMD_PORT_READ32_OUT_STATUS_OFST 4


/***********************************/
/* MC_CMD_PORT_WRITE32 
 * Write a 32-bit register to the indirect port register map.
 */
#define	MC_CMD_PORT_WRITE32  0x15

/* MC_CMD_PORT_WRITE32_IN msgrequest */
#define	MC_CMD_PORT_WRITE32_IN_LEN 8
#define	MC_CMD_PORT_WRITE32_IN_ADDR_OFST 0
#define	MC_CMD_PORT_WRITE32_IN_VALUE_OFST 4

/* MC_CMD_PORT_WRITE32_OUT msgresponse */
#define	MC_CMD_PORT_WRITE32_OUT_LEN 4
#define	MC_CMD_PORT_WRITE32_OUT_STATUS_OFST 0


/***********************************/
/* MC_CMD_PORT_READ128 
 * Read a 128-bit register from the indirect port register map.
 */
#define	MC_CMD_PORT_READ128  0x16

/* MC_CMD_PORT_READ128_IN msgrequest */
#define	MC_CMD_PORT_READ128_IN_LEN 4
#define	MC_CMD_PORT_READ128_IN_ADDR_OFST 0

/* MC_CMD_PORT_READ128_OUT msgresponse */
#define	MC_CMD_PORT_READ128_OUT_LEN 20
#define	MC_CMD_PORT_READ128_OUT_VALUE_OFST 0
#define	MC_CMD_PORT_READ128_OUT_VALUE_LEN 16
#define	MC_CMD_PORT_READ128_OUT_STATUS_OFST 16


/***********************************/
/* MC_CMD_PORT_WRITE128 
 * Write a 128-bit register to the indirect port register map.
 */
#define	MC_CMD_PORT_WRITE128  0x17

/* MC_CMD_PORT_WRITE128_IN msgrequest */
#define	MC_CMD_PORT_WRITE128_IN_LEN 20
#define	MC_CMD_PORT_WRITE128_IN_ADDR_OFST 0
#define	MC_CMD_PORT_WRITE128_IN_VALUE_OFST 4
#define	MC_CMD_PORT_WRITE128_IN_VALUE_LEN 16

/* MC_CMD_PORT_WRITE128_OUT msgresponse */
#define	MC_CMD_PORT_WRITE128_OUT_LEN 4
#define	MC_CMD_PORT_WRITE128_OUT_STATUS_OFST 0


/***********************************/
/* MC_CMD_GET_BOARD_CFG 
 * Returns the MC firmware configuration structure.
 */
#define	MC_CMD_GET_BOARD_CFG  0x18

/* MC_CMD_GET_BOARD_CFG_IN msgrequest */
#define	MC_CMD_GET_BOARD_CFG_IN_LEN 0

/* MC_CMD_GET_BOARD_CFG_OUT msgresponse */
#define	MC_CMD_GET_BOARD_CFG_OUT_LENMIN 96
#define	MC_CMD_GET_BOARD_CFG_OUT_LENMAX 136
#define	MC_CMD_GET_BOARD_CFG_OUT_LEN(num) (72+2*(num))
#define	MC_CMD_GET_BOARD_CFG_OUT_BOARD_TYPE_OFST 0
#define	MC_CMD_GET_BOARD_CFG_OUT_BOARD_NAME_OFST 4
#define	MC_CMD_GET_BOARD_CFG_OUT_BOARD_NAME_LEN 32
#define	MC_CMD_GET_BOARD_CFG_OUT_CAPABILITIES_PORT0_OFST 36
#define	MC_CMD_CAPABILITIES_SMALL_BUF_TBL_LBN 0x0 /* enum */
#define	MC_CMD_CAPABILITIES_SMALL_BUF_TBL_WIDTH 0x1 /* enum */
#define	MC_CMD_CAPABILITIES_TURBO_LBN 0x1 /* enum */
#define	MC_CMD_CAPABILITIES_TURBO_WIDTH 0x1 /* enum */
#define	MC_CMD_CAPABILITIES_TURBO_ACTIVE_LBN 0x2 /* enum */
#define	MC_CMD_CAPABILITIES_TURBO_ACTIVE_WIDTH 0x1 /* enum */
#define	MC_CMD_CAPABILITIES_PTP_LBN 0x3 /* enum */
#define	MC_CMD_CAPABILITIES_PTP_WIDTH 0x1 /* enum */
#define	MC_CMD_CAPABILITIES_AOE_LBN 0x4 /* enum */
#define	MC_CMD_CAPABILITIES_AOE_WIDTH 0x1 /* enum */
#define	MC_CMD_CAPABILITIES_AOE_ACTIVE_LBN 0x5 /* enum */
#define	MC_CMD_CAPABILITIES_AOE_ACTIVE_WIDTH 0x1 /* enum */
#define	MC_CMD_CAPABILITIES_FC_ACTIVE_LBN 0x6 /* enum */
#define	MC_CMD_CAPABILITIES_FC_ACTIVE_WIDTH 0x1 /* enum */
#define	MC_CMD_GET_BOARD_CFG_OUT_CAPABILITIES_PORT1_OFST 40
/*            Enum values, see field(s): */
/*               CAPABILITIES_PORT0 */
#define	MC_CMD_GET_BOARD_CFG_OUT_MAC_ADDR_BASE_PORT0_OFST 44
#define	MC_CMD_GET_BOARD_CFG_OUT_MAC_ADDR_BASE_PORT0_LEN 6
#define	MC_CMD_GET_BOARD_CFG_OUT_MAC_ADDR_BASE_PORT1_OFST 50
#define	MC_CMD_GET_BOARD_CFG_OUT_MAC_ADDR_BASE_PORT1_LEN 6
#define	MC_CMD_GET_BOARD_CFG_OUT_MAC_COUNT_PORT0_OFST 56
#define	MC_CMD_GET_BOARD_CFG_OUT_MAC_COUNT_PORT1_OFST 60
#define	MC_CMD_GET_BOARD_CFG_OUT_MAC_STRIDE_PORT0_OFST 64
#define	MC_CMD_GET_BOARD_CFG_OUT_MAC_STRIDE_PORT1_OFST 68
#define	MC_CMD_GET_BOARD_CFG_OUT_FW_SUBTYPE_LIST_OFST 72
#define	MC_CMD_GET_BOARD_CFG_OUT_FW_SUBTYPE_LIST_LEN 2
#define	MC_CMD_GET_BOARD_CFG_OUT_FW_SUBTYPE_LIST_MINNUM 12
#define	MC_CMD_GET_BOARD_CFG_OUT_FW_SUBTYPE_LIST_MAXNUM 32


/***********************************/
/* MC_CMD_DBI_READX 
 * Read DBI register(s).
 */
#define	MC_CMD_DBI_READX  0x19

/* MC_CMD_DBI_READX_IN msgrequest */
#define	MC_CMD_DBI_READX_IN_LENMIN 8
#define	MC_CMD_DBI_READX_IN_LENMAX 248
#define	MC_CMD_DBI_READX_IN_LEN(num) (0+8*(num))
#define	MC_CMD_DBI_READX_IN_DBIRDOP_OFST 0
#define	MC_CMD_DBI_READX_IN_DBIRDOP_LEN 8
#define	MC_CMD_DBI_READX_IN_DBIRDOP_LO_OFST 0
#define	MC_CMD_DBI_READX_IN_DBIRDOP_HI_OFST 4
#define	MC_CMD_DBI_READX_IN_DBIRDOP_MINNUM 1
#define	MC_CMD_DBI_READX_IN_DBIRDOP_MAXNUM 31

/* MC_CMD_DBI_READX_OUT msgresponse */
#define	MC_CMD_DBI_READX_OUT_LENMIN 4
#define	MC_CMD_DBI_READX_OUT_LENMAX 252
#define	MC_CMD_DBI_READX_OUT_LEN(num) (0+4*(num))
#define	MC_CMD_DBI_READX_OUT_VALUE_OFST 0
#define	MC_CMD_DBI_READX_OUT_VALUE_LEN 4
#define	MC_CMD_DBI_READX_OUT_VALUE_MINNUM 1
#define	MC_CMD_DBI_READX_OUT_VALUE_MAXNUM 63


/***********************************/
/* MC_CMD_SET_RAND_SEED 
 * Set the 16byte seed for the MC pseudo-random generator.
 */
#define	MC_CMD_SET_RAND_SEED  0x1a

/* MC_CMD_SET_RAND_SEED_IN msgrequest */
#define	MC_CMD_SET_RAND_SEED_IN_LEN 16
#define	MC_CMD_SET_RAND_SEED_IN_SEED_OFST 0
#define	MC_CMD_SET_RAND_SEED_IN_SEED_LEN 16

/* MC_CMD_SET_RAND_SEED_OUT msgresponse */
#define	MC_CMD_SET_RAND_SEED_OUT_LEN 0


/***********************************/
/* MC_CMD_LTSSM_HIST 
 * Retrieve the history of the PCIE LTSSM.
 */
#define	MC_CMD_LTSSM_HIST  0x1b

/* MC_CMD_LTSSM_HIST_IN msgrequest */
#define	MC_CMD_LTSSM_HIST_IN_LEN 0

/* MC_CMD_LTSSM_HIST_OUT msgresponse */
#define	MC_CMD_LTSSM_HIST_OUT_LENMIN 0
#define	MC_CMD_LTSSM_HIST_OUT_LENMAX 252
#define	MC_CMD_LTSSM_HIST_OUT_LEN(num) (0+4*(num))
#define	MC_CMD_LTSSM_HIST_OUT_DATA_OFST 0
#define	MC_CMD_LTSSM_HIST_OUT_DATA_LEN 4
#define	MC_CMD_LTSSM_HIST_OUT_DATA_MINNUM 0
#define	MC_CMD_LTSSM_HIST_OUT_DATA_MAXNUM 63


/***********************************/
/* MC_CMD_DRV_ATTACH 
 * Inform MCPU that this port is managed on the host.
 */
#define	MC_CMD_DRV_ATTACH  0x1c

/* MC_CMD_DRV_ATTACH_IN msgrequest */
#define	MC_CMD_DRV_ATTACH_IN_LEN 8
#define	MC_CMD_DRV_ATTACH_IN_NEW_STATE_OFST 0
#define	MC_CMD_DRV_ATTACH_IN_UPDATE_OFST 4

/* MC_CMD_DRV_ATTACH_OUT msgresponse */
#define	MC_CMD_DRV_ATTACH_OUT_LEN 4
#define	MC_CMD_DRV_ATTACH_OUT_OLD_STATE_OFST 0


/***********************************/
/* MC_CMD_SHMUART 
 * Route UART output to circular buffer in shared memory instead.
 */
#define	MC_CMD_SHMUART  0x1f

/* MC_CMD_SHMUART_IN msgrequest */
#define	MC_CMD_SHMUART_IN_LEN 4
#define	MC_CMD_SHMUART_IN_FLAG_OFST 0

/* MC_CMD_SHMUART_OUT msgresponse */
#define	MC_CMD_SHMUART_OUT_LEN 0


/***********************************/
/* MC_CMD_PORT_RESET 
 * Generic per-port reset.
 */
#define	MC_CMD_PORT_RESET  0x20

/* MC_CMD_PORT_RESET_IN msgrequest */
#define	MC_CMD_PORT_RESET_IN_LEN 0

/* MC_CMD_PORT_RESET_OUT msgresponse */
#define	MC_CMD_PORT_RESET_OUT_LEN 0


/***********************************/
/* MC_CMD_PCIE_CREDITS 
 * Read instantaneous and minimum flow control thresholds.
 */
#define	MC_CMD_PCIE_CREDITS  0x21

/* MC_CMD_PCIE_CREDITS_IN msgrequest */
#define	MC_CMD_PCIE_CREDITS_IN_LEN 8
#define	MC_CMD_PCIE_CREDITS_IN_POLL_PERIOD_OFST 0
#define	MC_CMD_PCIE_CREDITS_IN_WIPE_OFST 4

/* MC_CMD_PCIE_CREDITS_OUT msgresponse */
#define	MC_CMD_PCIE_CREDITS_OUT_LEN 16
#define	MC_CMD_PCIE_CREDITS_OUT_CURRENT_P_HDR_OFST 0
#define	MC_CMD_PCIE_CREDITS_OUT_CURRENT_P_HDR_LEN 2
#define	MC_CMD_PCIE_CREDITS_OUT_CURRENT_P_DATA_OFST 2
#define	MC_CMD_PCIE_CREDITS_OUT_CURRENT_P_DATA_LEN 2
#define	MC_CMD_PCIE_CREDITS_OUT_CURRENT_NP_HDR_OFST 4
#define	MC_CMD_PCIE_CREDITS_OUT_CURRENT_NP_HDR_LEN 2
#define	MC_CMD_PCIE_CREDITS_OUT_CURRENT_NP_DATA_OFST 6
#define	MC_CMD_PCIE_CREDITS_OUT_CURRENT_NP_DATA_LEN 2
#define	MC_CMD_PCIE_CREDITS_OUT_MINIMUM_P_HDR_OFST 8
#define	MC_CMD_PCIE_CREDITS_OUT_MINIMUM_P_HDR_LEN 2
#define	MC_CMD_PCIE_CREDITS_OUT_MINIMUM_P_DATA_OFST 10
#define	MC_CMD_PCIE_CREDITS_OUT_MINIMUM_P_DATA_LEN 2
#define	MC_CMD_PCIE_CREDITS_OUT_MINIMUM_NP_HDR_OFST 12
#define	MC_CMD_PCIE_CREDITS_OUT_MINIMUM_NP_HDR_LEN 2
#define	MC_CMD_PCIE_CREDITS_OUT_MINIMUM_NP_DATA_OFST 14
#define	MC_CMD_PCIE_CREDITS_OUT_MINIMUM_NP_DATA_LEN 2


/***********************************/
/* MC_CMD_RXD_MONITOR 
 * Get histogram of RX queue fill level.
 */
#define	MC_CMD_RXD_MONITOR  0x22

/* MC_CMD_RXD_MONITOR_IN msgrequest */
#define	MC_CMD_RXD_MONITOR_IN_LEN 12
#define	MC_CMD_RXD_MONITOR_IN_QID_OFST 0
#define	MC_CMD_RXD_MONITOR_IN_POLL_PERIOD_OFST 4
#define	MC_CMD_RXD_MONITOR_IN_WIPE_OFST 8

/* MC_CMD_RXD_MONITOR_OUT msgresponse */
#define	MC_CMD_RXD_MONITOR_OUT_LEN 80
#define	MC_CMD_RXD_MONITOR_OUT_QID_OFST 0
#define	MC_CMD_RXD_MONITOR_OUT_RING_FILL_OFST 4
#define	MC_CMD_RXD_MONITOR_OUT_CACHE_FILL_OFST 8
#define	MC_CMD_RXD_MONITOR_OUT_RING_LT_1_OFST 12
#define	MC_CMD_RXD_MONITOR_OUT_RING_LT_2_OFST 16
#define	MC_CMD_RXD_MONITOR_OUT_RING_LT_4_OFST 20
#define	MC_CMD_RXD_MONITOR_OUT_RING_LT_8_OFST 24
#define	MC_CMD_RXD_MONITOR_OUT_RING_LT_16_OFST 28
#define	MC_CMD_RXD_MONITOR_OUT_RING_LT_32_OFST 32
#define	MC_CMD_RXD_MONITOR_OUT_RING_LT_64_OFST 36
#define	MC_CMD_RXD_MONITOR_OUT_RING_LT_128_OFST 40
#define	MC_CMD_RXD_MONITOR_OUT_RING_LT_256_OFST 44
#define	MC_CMD_RXD_MONITOR_OUT_RING_GE_256_OFST 48
#define	MC_CMD_RXD_MONITOR_OUT_CACHE_LT_1_OFST 52
#define	MC_CMD_RXD_MONITOR_OUT_CACHE_LT_2_OFST 56
#define	MC_CMD_RXD_MONITOR_OUT_CACHE_LT_4_OFST 60
#define	MC_CMD_RXD_MONITOR_OUT_CACHE_LT_8_OFST 64
#define	MC_CMD_RXD_MONITOR_OUT_CACHE_LT_16_OFST 68
#define	MC_CMD_RXD_MONITOR_OUT_CACHE_LT_32_OFST 72
#define	MC_CMD_RXD_MONITOR_OUT_CACHE_GE_32_OFST 76


/***********************************/
/* MC_CMD_PUTS 
 * puts(3) implementation over MCDI
 */
#define	MC_CMD_PUTS  0x23

/* MC_CMD_PUTS_IN msgrequest */
#define	MC_CMD_PUTS_IN_LENMIN 13
#define	MC_CMD_PUTS_IN_LENMAX 252
#define	MC_CMD_PUTS_IN_LEN(num) (12+1*(num))
#define	MC_CMD_PUTS_IN_DEST_OFST 0
#define	MC_CMD_PUTS_IN_UART_LBN 0
#define	MC_CMD_PUTS_IN_UART_WIDTH 1
#define	MC_CMD_PUTS_IN_PORT_LBN 1
#define	MC_CMD_PUTS_IN_PORT_WIDTH 1
#define	MC_CMD_PUTS_IN_DHOST_OFST 4
#define	MC_CMD_PUTS_IN_DHOST_LEN 6
#define	MC_CMD_PUTS_IN_STRING_OFST 12
#define	MC_CMD_PUTS_IN_STRING_LEN 1
#define	MC_CMD_PUTS_IN_STRING_MINNUM 1
#define	MC_CMD_PUTS_IN_STRING_MAXNUM 240

/* MC_CMD_PUTS_OUT msgresponse */
#define	MC_CMD_PUTS_OUT_LEN 0


/***********************************/
/* MC_CMD_GET_PHY_CFG 
 * Report PHY configuration.
 */
#define	MC_CMD_GET_PHY_CFG  0x24

/* MC_CMD_GET_PHY_CFG_IN msgrequest */
#define	MC_CMD_GET_PHY_CFG_IN_LEN 0

/* MC_CMD_GET_PHY_CFG_OUT msgresponse */
#define	MC_CMD_GET_PHY_CFG_OUT_LEN 72
#define	MC_CMD_GET_PHY_CFG_OUT_FLAGS_OFST 0
#define	MC_CMD_GET_PHY_CFG_OUT_PRESENT_LBN 0
#define	MC_CMD_GET_PHY_CFG_OUT_PRESENT_WIDTH 1
#define	MC_CMD_GET_PHY_CFG_OUT_BIST_CABLE_SHORT_LBN 1
#define	MC_CMD_GET_PHY_CFG_OUT_BIST_CABLE_SHORT_WIDTH 1
#define	MC_CMD_GET_PHY_CFG_OUT_BIST_CABLE_LONG_LBN 2
#define	MC_CMD_GET_PHY_CFG_OUT_BIST_CABLE_LONG_WIDTH 1
#define	MC_CMD_GET_PHY_CFG_OUT_LOWPOWER_LBN 3
#define	MC_CMD_GET_PHY_CFG_OUT_LOWPOWER_WIDTH 1
#define	MC_CMD_GET_PHY_CFG_OUT_POWEROFF_LBN 4
#define	MC_CMD_GET_PHY_CFG_OUT_POWEROFF_WIDTH 1
#define	MC_CMD_GET_PHY_CFG_OUT_TXDIS_LBN 5
#define	MC_CMD_GET_PHY_CFG_OUT_TXDIS_WIDTH 1
#define	MC_CMD_GET_PHY_CFG_OUT_BIST_LBN 6
#define	MC_CMD_GET_PHY_CFG_OUT_BIST_WIDTH 1
#define	MC_CMD_GET_PHY_CFG_OUT_TYPE_OFST 4
#define	MC_CMD_GET_PHY_CFG_OUT_SUPPORTED_CAP_OFST 8
#define	MC_CMD_PHY_CAP_10HDX_LBN 1
#define	MC_CMD_PHY_CAP_10HDX_WIDTH 1
#define	MC_CMD_PHY_CAP_10FDX_LBN 2
#define	MC_CMD_PHY_CAP_10FDX_WIDTH 1
#define	MC_CMD_PHY_CAP_100HDX_LBN 3
#define	MC_CMD_PHY_CAP_100HDX_WIDTH 1
#define	MC_CMD_PHY_CAP_100FDX_LBN 4
#define	MC_CMD_PHY_CAP_100FDX_WIDTH 1
#define	MC_CMD_PHY_CAP_1000HDX_LBN 5
#define	MC_CMD_PHY_CAP_1000HDX_WIDTH 1
#define	MC_CMD_PHY_CAP_1000FDX_LBN 6
#define	MC_CMD_PHY_CAP_1000FDX_WIDTH 1
#define	MC_CMD_PHY_CAP_10000FDX_LBN 7
#define	MC_CMD_PHY_CAP_10000FDX_WIDTH 1
#define	MC_CMD_PHY_CAP_PAUSE_LBN 8
#define	MC_CMD_PHY_CAP_PAUSE_WIDTH 1
#define	MC_CMD_PHY_CAP_ASYM_LBN 9
#define	MC_CMD_PHY_CAP_ASYM_WIDTH 1
#define	MC_CMD_PHY_CAP_AN_LBN 10
#define	MC_CMD_PHY_CAP_AN_WIDTH 1
#define	MC_CMD_PHY_CAP_DDM_LBN 12
#define	MC_CMD_PHY_CAP_DDM_WIDTH 1
#define	MC_CMD_GET_PHY_CFG_OUT_CHANNEL_OFST 12
#define	MC_CMD_GET_PHY_CFG_OUT_PRT_OFST 16
#define	MC_CMD_GET_PHY_CFG_OUT_STATS_MASK_OFST 20
#define	MC_CMD_GET_PHY_CFG_OUT_NAME_OFST 24
#define	MC_CMD_GET_PHY_CFG_OUT_NAME_LEN 20
#define	MC_CMD_GET_PHY_CFG_OUT_MEDIA_TYPE_OFST 44
#define	MC_CMD_MEDIA_XAUI 0x1 /* enum */
#define	MC_CMD_MEDIA_CX4 0x2 /* enum */
#define	MC_CMD_MEDIA_KX4 0x3 /* enum */
#define	MC_CMD_MEDIA_XFP 0x4 /* enum */
#define	MC_CMD_MEDIA_SFP_PLUS 0x5 /* enum */
#define	MC_CMD_MEDIA_BASE_T 0x6 /* enum */
#define	MC_CMD_GET_PHY_CFG_OUT_MMD_MASK_OFST 48
#define	MC_CMD_MMD_CLAUSE22 0x0 /* enum */
#define	MC_CMD_MMD_CLAUSE45_PMAPMD 0x1 /* enum */
#define	MC_CMD_MMD_CLAUSE45_WIS 0x2 /* enum */
#define	MC_CMD_MMD_CLAUSE45_PCS 0x3 /* enum */
#define	MC_CMD_MMD_CLAUSE45_PHYXS 0x4 /* enum */
#define	MC_CMD_MMD_CLAUSE45_DTEXS 0x5 /* enum */
#define	MC_CMD_MMD_CLAUSE45_TC 0x6 /* enum */
#define	MC_CMD_MMD_CLAUSE45_AN 0x7 /* enum */
#define	MC_CMD_MMD_CLAUSE45_C22EXT 0x1d /* enum */
#define	MC_CMD_MMD_CLAUSE45_VEND1 0x1e /* enum */
#define	MC_CMD_MMD_CLAUSE45_VEND2 0x1f /* enum */
#define	MC_CMD_GET_PHY_CFG_OUT_REVISION_OFST 52
#define	MC_CMD_GET_PHY_CFG_OUT_REVISION_LEN 20


/***********************************/
/* MC_CMD_START_BIST 
 * Start a BIST test on the PHY.
 */
#define	MC_CMD_START_BIST  0x25

/* MC_CMD_START_BIST_IN msgrequest */
#define	MC_CMD_START_BIST_IN_LEN 4
#define	MC_CMD_START_BIST_IN_TYPE_OFST 0
#define	MC_CMD_PHY_BIST_CABLE_SHORT 0x1 /* enum */
#define	MC_CMD_PHY_BIST_CABLE_LONG 0x2 /* enum */
#define	MC_CMD_BPX_SERDES_BIST 0x3 /* enum */
#define	MC_CMD_MC_LOOPBACK_BIST 0x4 /* enum */
#define	MC_CMD_PHY_BIST 0x5 /* enum */

/* MC_CMD_START_BIST_OUT msgresponse */
#define	MC_CMD_START_BIST_OUT_LEN 0


/***********************************/
/* MC_CMD_POLL_BIST 
 * Poll for BIST completion.
 */
#define	MC_CMD_POLL_BIST  0x26

/* MC_CMD_POLL_BIST_IN msgrequest */
#define	MC_CMD_POLL_BIST_IN_LEN 0

/* MC_CMD_POLL_BIST_OUT msgresponse */
#define	MC_CMD_POLL_BIST_OUT_LEN 8
#define	MC_CMD_POLL_BIST_OUT_RESULT_OFST 0
#define	MC_CMD_POLL_BIST_RUNNING 0x1 /* enum */
#define	MC_CMD_POLL_BIST_PASSED 0x2 /* enum */
#define	MC_CMD_POLL_BIST_FAILED 0x3 /* enum */
#define	MC_CMD_POLL_BIST_TIMEOUT 0x4 /* enum */
#define	MC_CMD_POLL_BIST_OUT_PRIVATE_OFST 4

/* MC_CMD_POLL_BIST_OUT_SFT9001 msgresponse */
#define	MC_CMD_POLL_BIST_OUT_SFT9001_LEN 36
/*            MC_CMD_POLL_BIST_OUT_RESULT_OFST 0 */
/*            Enum values, see field(s): */
/*               MC_CMD_POLL_BIST_OUT/MC_CMD_POLL_BIST_OUT_RESULT */
#define	MC_CMD_POLL_BIST_OUT_SFT9001_CABLE_LENGTH_A_OFST 4
#define	MC_CMD_POLL_BIST_OUT_SFT9001_CABLE_LENGTH_B_OFST 8
#define	MC_CMD_POLL_BIST_OUT_SFT9001_CABLE_LENGTH_C_OFST 12
#define	MC_CMD_POLL_BIST_OUT_SFT9001_CABLE_LENGTH_D_OFST 16
#define	MC_CMD_POLL_BIST_OUT_SFT9001_CABLE_STATUS_A_OFST 20
#define	MC_CMD_POLL_BIST_SFT9001_PAIR_OK 0x1 /* enum */
#define	MC_CMD_POLL_BIST_SFT9001_PAIR_OPEN 0x2 /* enum */
#define	MC_CMD_POLL_BIST_SFT9001_INTRA_PAIR_SHORT 0x3 /* enum */
#define	MC_CMD_POLL_BIST_SFT9001_INTER_PAIR_SHORT 0x4 /* enum */
#define	MC_CMD_POLL_BIST_SFT9001_PAIR_BUSY 0x9 /* enum */
#define	MC_CMD_POLL_BIST_OUT_SFT9001_CABLE_STATUS_B_OFST 24
/*            Enum values, see field(s): */
/*               CABLE_STATUS_A */
#define	MC_CMD_POLL_BIST_OUT_SFT9001_CABLE_STATUS_C_OFST 28
/*            Enum values, see field(s): */
/*               CABLE_STATUS_A */
#define	MC_CMD_POLL_BIST_OUT_SFT9001_CABLE_STATUS_D_OFST 32
/*            Enum values, see field(s): */
/*               CABLE_STATUS_A */

/* MC_CMD_POLL_BIST_OUT_MRSFP msgresponse */
#define	MC_CMD_POLL_BIST_OUT_MRSFP_LEN 8
/*            MC_CMD_POLL_BIST_OUT_RESULT_OFST 0 */
/*            Enum values, see field(s): */
/*               MC_CMD_POLL_BIST_OUT/MC_CMD_POLL_BIST_OUT_RESULT */
#define	MC_CMD_POLL_BIST_OUT_MRSFP_TEST_OFST 4
#define	MC_CMD_POLL_BIST_MRSFP_TEST_COMPLETE 0x0 /* enum */
#define	MC_CMD_POLL_BIST_MRSFP_TEST_BUS_SWITCH_OFF_I2C_WRITE 0x1 /* enum */
#define	MC_CMD_POLL_BIST_MRSFP_TEST_BUS_SWITCH_OFF_I2C_NO_ACCESS_IO_EXP 0x2 /* enum */
#define	MC_CMD_POLL_BIST_MRSFP_TEST_BUS_SWITCH_OFF_I2C_NO_ACCESS_MODULE 0x3 /* enum */
#define	MC_CMD_POLL_BIST_MRSFP_TEST_IO_EXP_I2C_CONFIGURE 0x4 /* enum */
#define	MC_CMD_POLL_BIST_MRSFP_TEST_BUS_SWITCH_I2C_NO_CROSSTALK 0x5 /* enum */
#define	MC_CMD_POLL_BIST_MRSFP_TEST_MODULE_PRESENCE 0x6 /* enum */
#define	MC_CMD_POLL_BIST_MRSFP_TEST_MODULE_ID_I2C_ACCESS 0x7 /* enum */
#define	MC_CMD_POLL_BIST_MRSFP_TEST_MODULE_ID_SANE_VALUE 0x8 /* enum */


/***********************************/
/* MC_CMD_FLUSH_RX_QUEUES 
 * Flush receive queue(s).
 */
#define	MC_CMD_FLUSH_RX_QUEUES  0x27

/* MC_CMD_FLUSH_RX_QUEUES_IN msgrequest */
#define	MC_CMD_FLUSH_RX_QUEUES_IN_LENMIN 4
#define	MC_CMD_FLUSH_RX_QUEUES_IN_LENMAX 252
#define	MC_CMD_FLUSH_RX_QUEUES_IN_LEN(num) (0+4*(num))
#define	MC_CMD_FLUSH_RX_QUEUES_IN_QID_OFST_OFST 0
#define	MC_CMD_FLUSH_RX_QUEUES_IN_QID_OFST_LEN 4
#define	MC_CMD_FLUSH_RX_QUEUES_IN_QID_OFST_MINNUM 1
#define	MC_CMD_FLUSH_RX_QUEUES_IN_QID_OFST_MAXNUM 63

/* MC_CMD_FLUSH_RX_QUEUES_OUT msgresponse */
#define	MC_CMD_FLUSH_RX_QUEUES_OUT_LEN 0


/***********************************/
/* MC_CMD_GET_LOOPBACK_MODES 
 * Get port's loopback modes.
 */
#define	MC_CMD_GET_LOOPBACK_MODES  0x28

/* MC_CMD_GET_LOOPBACK_MODES_IN msgrequest */
#define	MC_CMD_GET_LOOPBACK_MODES_IN_LEN 0

/* MC_CMD_GET_LOOPBACK_MODES_OUT msgresponse */
#define	MC_CMD_GET_LOOPBACK_MODES_OUT_LEN 32
#define	MC_CMD_GET_LOOPBACK_MODES_OUT_100M_OFST 0
#define	MC_CMD_GET_LOOPBACK_MODES_OUT_100M_LEN 8
#define	MC_CMD_GET_LOOPBACK_MODES_OUT_100M_LO_OFST 0
#define	MC_CMD_GET_LOOPBACK_MODES_OUT_100M_HI_OFST 4
#define	MC_CMD_LOOPBACK_NONE  0x0 /* enum */
#define	MC_CMD_LOOPBACK_DATA  0x1 /* enum */
#define	MC_CMD_LOOPBACK_GMAC  0x2 /* enum */
#define	MC_CMD_LOOPBACK_XGMII 0x3 /* enum */
#define	MC_CMD_LOOPBACK_XGXS  0x4 /* enum */
#define	MC_CMD_LOOPBACK_XAUI  0x5 /* enum */
#define	MC_CMD_LOOPBACK_GMII  0x6 /* enum */
#define	MC_CMD_LOOPBACK_SGMII  0x7 /* enum */
#define	MC_CMD_LOOPBACK_XGBR  0x8 /* enum */
#define	MC_CMD_LOOPBACK_XFI  0x9 /* enum */
#define	MC_CMD_LOOPBACK_XAUI_FAR  0xa /* enum */
#define	MC_CMD_LOOPBACK_GMII_FAR  0xb /* enum */
#define	MC_CMD_LOOPBACK_SGMII_FAR  0xc /* enum */
#define	MC_CMD_LOOPBACK_XFI_FAR  0xd /* enum */
#define	MC_CMD_LOOPBACK_GPHY  0xe /* enum */
#define	MC_CMD_LOOPBACK_PHYXS  0xf /* enum */
#define	MC_CMD_LOOPBACK_PCS  0x10 /* enum */
#define	MC_CMD_LOOPBACK_PMAPMD  0x11 /* enum */
#define	MC_CMD_LOOPBACK_XPORT  0x12 /* enum */
#define	MC_CMD_LOOPBACK_XGMII_WS  0x13 /* enum */
#define	MC_CMD_LOOPBACK_XAUI_WS  0x14 /* enum */
#define	MC_CMD_LOOPBACK_XAUI_WS_FAR  0x15 /* enum */
#define	MC_CMD_LOOPBACK_XAUI_WS_NEAR  0x16 /* enum */
#define	MC_CMD_LOOPBACK_GMII_WS  0x17 /* enum */
#define	MC_CMD_LOOPBACK_XFI_WS  0x18 /* enum */
#define	MC_CMD_LOOPBACK_XFI_WS_FAR  0x19 /* enum */
#define	MC_CMD_LOOPBACK_PHYXS_WS  0x1a /* enum */
#define	MC_CMD_LOOPBACK_AOE_INT_NEAR  0x23 /* enum */
#define	MC_CMD_GET_LOOPBACK_MODES_OUT_1G_OFST 8
#define	MC_CMD_GET_LOOPBACK_MODES_OUT_1G_LEN 8
#define	MC_CMD_GET_LOOPBACK_MODES_OUT_1G_LO_OFST 8
#define	MC_CMD_GET_LOOPBACK_MODES_OUT_1G_HI_OFST 12
/*            Enum values, see field(s): */
/*               100M */
#define	MC_CMD_GET_LOOPBACK_MODES_OUT_10G_OFST 16
#define	MC_CMD_GET_LOOPBACK_MODES_OUT_10G_LEN 8
#define	MC_CMD_GET_LOOPBACK_MODES_OUT_10G_LO_OFST 16
#define	MC_CMD_GET_LOOPBACK_MODES_OUT_10G_HI_OFST 20
/*            Enum values, see field(s): */
/*               100M */
#define	MC_CMD_GET_LOOPBACK_MODES_OUT_SUGGESTED_OFST 24
#define	MC_CMD_GET_LOOPBACK_MODES_OUT_SUGGESTED_LEN 8
#define	MC_CMD_GET_LOOPBACK_MODES_OUT_SUGGESTED_LO_OFST 24
#define	MC_CMD_GET_LOOPBACK_MODES_OUT_SUGGESTED_HI_OFST 28
/*            Enum values, see field(s): */
/*               100M */


/***********************************/
/* MC_CMD_GET_LINK 
 * Read the unified MAC/PHY link state.
 */
#define	MC_CMD_GET_LINK  0x29

/* MC_CMD_GET_LINK_IN msgrequest */
#define	MC_CMD_GET_LINK_IN_LEN 0

/* MC_CMD_GET_LINK_OUT msgresponse */
#define	MC_CMD_GET_LINK_OUT_LEN 28
#define	MC_CMD_GET_LINK_OUT_CAP_OFST 0
#define	MC_CMD_GET_LINK_OUT_LP_CAP_OFST 4
#define	MC_CMD_GET_LINK_OUT_LINK_SPEED_OFST 8
#define	MC_CMD_GET_LINK_OUT_LOOPBACK_MODE_OFST 12
/*            Enum values, see field(s): */
/*               MC_CMD_GET_LOOPBACK_MODES/MC_CMD_GET_LOOPBACK_MODES_OUT/100M */
#define	MC_CMD_GET_LINK_OUT_FLAGS_OFST 16
#define	MC_CMD_GET_LINK_OUT_LINK_UP_LBN 0
#define	MC_CMD_GET_LINK_OUT_LINK_UP_WIDTH 1
#define	MC_CMD_GET_LINK_OUT_FULL_DUPLEX_LBN 1
#define	MC_CMD_GET_LINK_OUT_FULL_DUPLEX_WIDTH 1
#define	MC_CMD_GET_LINK_OUT_BPX_LINK_LBN 2
#define	MC_CMD_GET_LINK_OUT_BPX_LINK_WIDTH 1
#define	MC_CMD_GET_LINK_OUT_PHY_LINK_LBN 3
#define	MC_CMD_GET_LINK_OUT_PHY_LINK_WIDTH 1
#define	MC_CMD_GET_LINK_OUT_LINK_FAULT_RX_LBN 6
#define	MC_CMD_GET_LINK_OUT_LINK_FAULT_RX_WIDTH 1
#define	MC_CMD_GET_LINK_OUT_LINK_FAULT_TX_LBN 7
#define	MC_CMD_GET_LINK_OUT_LINK_FAULT_TX_WIDTH 1
#define	MC_CMD_GET_LINK_OUT_FCNTL_OFST 20
#define	MC_CMD_FCNTL_OFF 0x0 /* enum */
#define	MC_CMD_FCNTL_RESPOND 0x1 /* enum */
#define	MC_CMD_FCNTL_BIDIR 0x2 /* enum */
#define	MC_CMD_GET_LINK_OUT_MAC_FAULT_OFST 24
#define	MC_CMD_MAC_FAULT_XGMII_LOCAL_LBN 0
#define	MC_CMD_MAC_FAULT_XGMII_LOCAL_WIDTH 1
#define	MC_CMD_MAC_FAULT_XGMII_REMOTE_LBN 1
#define	MC_CMD_MAC_FAULT_XGMII_REMOTE_WIDTH 1
#define	MC_CMD_MAC_FAULT_SGMII_REMOTE_LBN 2
#define	MC_CMD_MAC_FAULT_SGMII_REMOTE_WIDTH 1
#define	MC_CMD_MAC_FAULT_PENDING_RECONFIG_LBN 3
#define	MC_CMD_MAC_FAULT_PENDING_RECONFIG_WIDTH 1


/***********************************/
/* MC_CMD_SET_LINK 
 * Write the unified MAC/PHY link configuration.
 */
#define	MC_CMD_SET_LINK  0x2a

/* MC_CMD_SET_LINK_IN msgrequest */
#define	MC_CMD_SET_LINK_IN_LEN 16
#define	MC_CMD_SET_LINK_IN_CAP_OFST 0
#define	MC_CMD_SET_LINK_IN_FLAGS_OFST 4
#define	MC_CMD_SET_LINK_IN_LOWPOWER_LBN 0
#define	MC_CMD_SET_LINK_IN_LOWPOWER_WIDTH 1
#define	MC_CMD_SET_LINK_IN_POWEROFF_LBN 1
#define	MC_CMD_SET_LINK_IN_POWEROFF_WIDTH 1
#define	MC_CMD_SET_LINK_IN_TXDIS_LBN 2
#define	MC_CMD_SET_LINK_IN_TXDIS_WIDTH 1
#define	MC_CMD_SET_LINK_IN_LOOPBACK_MODE_OFST 8
/*            Enum values, see field(s): */
/*               MC_CMD_GET_LOOPBACK_MODES/MC_CMD_GET_LOOPBACK_MODES_OUT/100M */
#define	MC_CMD_SET_LINK_IN_LOOPBACK_SPEED_OFST 12

/* MC_CMD_SET_LINK_OUT msgresponse */
#define	MC_CMD_SET_LINK_OUT_LEN 0


/***********************************/
/* MC_CMD_SET_ID_LED 
 * Set indentification LED state.
 */
#define	MC_CMD_SET_ID_LED  0x2b

/* MC_CMD_SET_ID_LED_IN msgrequest */
#define	MC_CMD_SET_ID_LED_IN_LEN 4
#define	MC_CMD_SET_ID_LED_IN_STATE_OFST 0
#define	MC_CMD_LED_OFF  0x0 /* enum */
#define	MC_CMD_LED_ON  0x1 /* enum */
#define	MC_CMD_LED_DEFAULT  0x2 /* enum */

/* MC_CMD_SET_ID_LED_OUT msgresponse */
#define	MC_CMD_SET_ID_LED_OUT_LEN 0


/***********************************/
/* MC_CMD_SET_MAC 
 * Set MAC configuration.
 */
#define	MC_CMD_SET_MAC  0x2c

/* MC_CMD_SET_MAC_IN msgrequest */
#define	MC_CMD_SET_MAC_IN_LEN 24
#define	MC_CMD_SET_MAC_IN_MTU_OFST 0
#define	MC_CMD_SET_MAC_IN_DRAIN_OFST 4
#define	MC_CMD_SET_MAC_IN_ADDR_OFST 8
#define	MC_CMD_SET_MAC_IN_ADDR_LEN 8
#define	MC_CMD_SET_MAC_IN_ADDR_LO_OFST 8
#define	MC_CMD_SET_MAC_IN_ADDR_HI_OFST 12
#define	MC_CMD_SET_MAC_IN_REJECT_OFST 16
#define	MC_CMD_SET_MAC_IN_REJECT_UNCST_LBN 0
#define	MC_CMD_SET_MAC_IN_REJECT_UNCST_WIDTH 1
#define	MC_CMD_SET_MAC_IN_REJECT_BRDCST_LBN 1
#define	MC_CMD_SET_MAC_IN_REJECT_BRDCST_WIDTH 1
#define	MC_CMD_SET_MAC_IN_FCNTL_OFST 20
/*               MC_CMD_FCNTL_OFF 0x0 */
/*               MC_CMD_FCNTL_RESPOND 0x1 */
/*               MC_CMD_FCNTL_BIDIR 0x2 */
#define	MC_CMD_FCNTL_AUTO 0x3 /* enum */

/* MC_CMD_SET_MAC_OUT msgresponse */
#define	MC_CMD_SET_MAC_OUT_LEN 0


/***********************************/
/* MC_CMD_PHY_STATS 
 * Get generic PHY statistics.
 */
#define	MC_CMD_PHY_STATS  0x2d

/* MC_CMD_PHY_STATS_IN msgrequest */
#define	MC_CMD_PHY_STATS_IN_LEN 8
#define	MC_CMD_PHY_STATS_IN_DMA_ADDR_OFST 0
#define	MC_CMD_PHY_STATS_IN_DMA_ADDR_LEN 8
#define	MC_CMD_PHY_STATS_IN_DMA_ADDR_LO_OFST 0
#define	MC_CMD_PHY_STATS_IN_DMA_ADDR_HI_OFST 4

/* MC_CMD_PHY_STATS_OUT_DMA msgresponse */
#define	MC_CMD_PHY_STATS_OUT_DMA_LEN 0

/* MC_CMD_PHY_STATS_OUT_NO_DMA msgresponse */
#define	MC_CMD_PHY_STATS_OUT_NO_DMA_LEN (((MC_CMD_PHY_NSTATS*32))>>3)
#define	MC_CMD_PHY_STATS_OUT_NO_DMA_STATISTICS_OFST 0
#define	MC_CMD_PHY_STATS_OUT_NO_DMA_STATISTICS_LEN 4
#define	MC_CMD_PHY_STATS_OUT_NO_DMA_STATISTICS_NUM MC_CMD_PHY_NSTATS
#define	MC_CMD_OUI  0x0 /* enum */
#define	MC_CMD_PMA_PMD_LINK_UP  0x1 /* enum */
#define	MC_CMD_PMA_PMD_RX_FAULT  0x2 /* enum */
#define	MC_CMD_PMA_PMD_TX_FAULT  0x3 /* enum */
#define	MC_CMD_PMA_PMD_SIGNAL  0x4 /* enum */
#define	MC_CMD_PMA_PMD_SNR_A  0x5 /* enum */
#define	MC_CMD_PMA_PMD_SNR_B  0x6 /* enum */
#define	MC_CMD_PMA_PMD_SNR_C  0x7 /* enum */
#define	MC_CMD_PMA_PMD_SNR_D  0x8 /* enum */
#define	MC_CMD_PCS_LINK_UP  0x9 /* enum */
#define	MC_CMD_PCS_RX_FAULT  0xa /* enum */
#define	MC_CMD_PCS_TX_FAULT  0xb /* enum */
#define	MC_CMD_PCS_BER  0xc /* enum */
#define	MC_CMD_PCS_BLOCK_ERRORS  0xd /* enum */
#define	MC_CMD_PHYXS_LINK_UP  0xe /* enum */
#define	MC_CMD_PHYXS_RX_FAULT  0xf /* enum */
#define	MC_CMD_PHYXS_TX_FAULT  0x10 /* enum */
#define	MC_CMD_PHYXS_ALIGN  0x11 /* enum */
#define	MC_CMD_PHYXS_SYNC  0x12 /* enum */
#define	MC_CMD_AN_LINK_UP  0x13 /* enum */
#define	MC_CMD_AN_COMPLETE  0x14 /* enum */
#define	MC_CMD_AN_10GBT_STATUS  0x15 /* enum */
#define	MC_CMD_CL22_LINK_UP  0x16 /* enum */
#define	MC_CMD_PHY_NSTATS  0x17 /* enum */


/***********************************/
/* MC_CMD_MAC_STATS 
 * Get generic MAC statistics.
 */
#define	MC_CMD_MAC_STATS  0x2e

/* MC_CMD_MAC_STATS_IN msgrequest */
#define	MC_CMD_MAC_STATS_IN_LEN 16
#define	MC_CMD_MAC_STATS_IN_DMA_ADDR_OFST 0
#define	MC_CMD_MAC_STATS_IN_DMA_ADDR_LEN 8
#define	MC_CMD_MAC_STATS_IN_DMA_ADDR_LO_OFST 0
#define	MC_CMD_MAC_STATS_IN_DMA_ADDR_HI_OFST 4
#define	MC_CMD_MAC_STATS_IN_CMD_OFST 8
#define	MC_CMD_MAC_STATS_IN_DMA_LBN 0
#define	MC_CMD_MAC_STATS_IN_DMA_WIDTH 1
#define	MC_CMD_MAC_STATS_IN_CLEAR_LBN 1
#define	MC_CMD_MAC_STATS_IN_CLEAR_WIDTH 1
#define	MC_CMD_MAC_STATS_IN_PERIODIC_CHANGE_LBN 2
#define	MC_CMD_MAC_STATS_IN_PERIODIC_CHANGE_WIDTH 1
#define	MC_CMD_MAC_STATS_IN_PERIODIC_ENABLE_LBN 3
#define	MC_CMD_MAC_STATS_IN_PERIODIC_ENABLE_WIDTH 1
#define	MC_CMD_MAC_STATS_IN_PERIODIC_CLEAR_LBN 4
#define	MC_CMD_MAC_STATS_IN_PERIODIC_CLEAR_WIDTH 1
#define	MC_CMD_MAC_STATS_IN_PERIODIC_NOEVENT_LBN 5
#define	MC_CMD_MAC_STATS_IN_PERIODIC_NOEVENT_WIDTH 1
#define	MC_CMD_MAC_STATS_IN_PERIOD_MS_LBN 16
#define	MC_CMD_MAC_STATS_IN_PERIOD_MS_WIDTH 16
#define	MC_CMD_MAC_STATS_IN_DMA_LEN_OFST 12

/* MC_CMD_MAC_STATS_OUT_DMA msgresponse */
#define	MC_CMD_MAC_STATS_OUT_DMA_LEN 0

/* MC_CMD_MAC_STATS_OUT_NO_DMA msgresponse */
#define	MC_CMD_MAC_STATS_OUT_NO_DMA_LEN (((MC_CMD_MAC_NSTATS*64))>>3)
#define	MC_CMD_MAC_STATS_OUT_NO_DMA_STATISTICS_OFST 0
#define	MC_CMD_MAC_STATS_OUT_NO_DMA_STATISTICS_LEN 8
#define	MC_CMD_MAC_STATS_OUT_NO_DMA_STATISTICS_LO_OFST 0
#define	MC_CMD_MAC_STATS_OUT_NO_DMA_STATISTICS_HI_OFST 4
#define	MC_CMD_MAC_STATS_OUT_NO_DMA_STATISTICS_NUM MC_CMD_MAC_NSTATS
#define	MC_CMD_MAC_GENERATION_START  0x0 /* enum */
#define	MC_CMD_MAC_TX_PKTS  0x1 /* enum */
#define	MC_CMD_MAC_TX_PAUSE_PKTS  0x2 /* enum */
#define	MC_CMD_MAC_TX_CONTROL_PKTS  0x3 /* enum */
#define	MC_CMD_MAC_TX_UNICAST_PKTS  0x4 /* enum */
#define	MC_CMD_MAC_TX_MULTICAST_PKTS  0x5 /* enum */
#define	MC_CMD_MAC_TX_BROADCAST_PKTS  0x6 /* enum */
#define	MC_CMD_MAC_TX_BYTES  0x7 /* enum */
#define	MC_CMD_MAC_TX_BAD_BYTES  0x8 /* enum */
#define	MC_CMD_MAC_TX_LT64_PKTS  0x9 /* enum */
#define	MC_CMD_MAC_TX_64_PKTS  0xa /* enum */
#define	MC_CMD_MAC_TX_65_TO_127_PKTS  0xb /* enum */
#define	MC_CMD_MAC_TX_128_TO_255_PKTS  0xc /* enum */
#define	MC_CMD_MAC_TX_256_TO_511_PKTS  0xd /* enum */
#define	MC_CMD_MAC_TX_512_TO_1023_PKTS  0xe /* enum */
#define	MC_CMD_MAC_TX_1024_TO_15XX_PKTS  0xf /* enum */
#define	MC_CMD_MAC_TX_15XX_TO_JUMBO_PKTS  0x10 /* enum */
#define	MC_CMD_MAC_TX_GTJUMBO_PKTS  0x11 /* enum */
#define	MC_CMD_MAC_TX_BAD_FCS_PKTS  0x12 /* enum */
#define	MC_CMD_MAC_TX_SINGLE_COLLISION_PKTS  0x13 /* enum */
#define	MC_CMD_MAC_TX_MULTIPLE_COLLISION_PKTS  0x14 /* enum */
#define	MC_CMD_MAC_TX_EXCESSIVE_COLLISION_PKTS  0x15 /* enum */
#define	MC_CMD_MAC_TX_LATE_COLLISION_PKTS  0x16 /* enum */
#define	MC_CMD_MAC_TX_DEFERRED_PKTS  0x17 /* enum */
#define	MC_CMD_MAC_TX_EXCESSIVE_DEFERRED_PKTS  0x18 /* enum */
#define	MC_CMD_MAC_TX_NON_TCPUDP_PKTS  0x19 /* enum */
#define	MC_CMD_MAC_TX_MAC_SRC_ERR_PKTS  0x1a /* enum */
#define	MC_CMD_MAC_TX_IP_SRC_ERR_PKTS  0x1b /* enum */
#define	MC_CMD_MAC_RX_PKTS  0x1c /* enum */
#define	MC_CMD_MAC_RX_PAUSE_PKTS  0x1d /* enum */
#define	MC_CMD_MAC_RX_GOOD_PKTS  0x1e /* enum */
#define	MC_CMD_MAC_RX_CONTROL_PKTS  0x1f /* enum */
#define	MC_CMD_MAC_RX_UNICAST_PKTS  0x20 /* enum */
#define	MC_CMD_MAC_RX_MULTICAST_PKTS  0x21 /* enum */
#define	MC_CMD_MAC_RX_BROADCAST_PKTS  0x22 /* enum */
#define	MC_CMD_MAC_RX_BYTES  0x23 /* enum */
#define	MC_CMD_MAC_RX_BAD_BYTES  0x24 /* enum */
#define	MC_CMD_MAC_RX_64_PKTS  0x25 /* enum */
#define	MC_CMD_MAC_RX_65_TO_127_PKTS  0x26 /* enum */
#define	MC_CMD_MAC_RX_128_TO_255_PKTS  0x27 /* enum */
#define	MC_CMD_MAC_RX_256_TO_511_PKTS  0x28 /* enum */
#define	MC_CMD_MAC_RX_512_TO_1023_PKTS  0x29 /* enum */
#define	MC_CMD_MAC_RX_1024_TO_15XX_PKTS  0x2a /* enum */
#define	MC_CMD_MAC_RX_15XX_TO_JUMBO_PKTS  0x2b /* enum */
#define	MC_CMD_MAC_RX_GTJUMBO_PKTS  0x2c /* enum */
#define	MC_CMD_MAC_RX_UNDERSIZE_PKTS  0x2d /* enum */
#define	MC_CMD_MAC_RX_BAD_FCS_PKTS  0x2e /* enum */
#define	MC_CMD_MAC_RX_OVERFLOW_PKTS  0x2f /* enum */
#define	MC_CMD_MAC_RX_FALSE_CARRIER_PKTS  0x30 /* enum */
#define	MC_CMD_MAC_RX_SYMBOL_ERROR_PKTS  0x31 /* enum */
#define	MC_CMD_MAC_RX_ALIGN_ERROR_PKTS  0x32 /* enum */
#define	MC_CMD_MAC_RX_LENGTH_ERROR_PKTS  0x33 /* enum */
#define	MC_CMD_MAC_RX_INTERNAL_ERROR_PKTS  0x34 /* enum */
#define	MC_CMD_MAC_RX_JABBER_PKTS  0x35 /* enum */
#define	MC_CMD_MAC_RX_NODESC_DROPS  0x36 /* enum */
#define	MC_CMD_MAC_RX_LANES01_CHAR_ERR  0x37 /* enum */
#define	MC_CMD_MAC_RX_LANES23_CHAR_ERR  0x38 /* enum */
#define	MC_CMD_MAC_RX_LANES01_DISP_ERR  0x39 /* enum */
#define	MC_CMD_MAC_RX_LANES23_DISP_ERR  0x3a /* enum */
#define	MC_CMD_MAC_RX_MATCH_FAULT  0x3b /* enum */
#define	MC_CMD_GMAC_DMABUF_START  0x40 /* enum */
#define	MC_CMD_GMAC_DMABUF_END    0x5f /* enum */
#define	MC_CMD_MAC_GENERATION_END 0x60 /* enum */
#define	MC_CMD_MAC_NSTATS  0x61 /* enum */


/***********************************/
/* MC_CMD_SRIOV 
 * to be documented
 */
#define	MC_CMD_SRIOV  0x30

/* MC_CMD_SRIOV_IN msgrequest */
#define	MC_CMD_SRIOV_IN_LEN 12
#define	MC_CMD_SRIOV_IN_ENABLE_OFST 0
#define	MC_CMD_SRIOV_IN_VI_BASE_OFST 4
#define	MC_CMD_SRIOV_IN_VF_COUNT_OFST 8

/* MC_CMD_SRIOV_OUT msgresponse */
#define	MC_CMD_SRIOV_OUT_LEN 8
#define	MC_CMD_SRIOV_OUT_VI_SCALE_OFST 0
#define	MC_CMD_SRIOV_OUT_VF_TOTAL_OFST 4

/* MC_CMD_MEMCPY_RECORD_TYPEDEF structuredef */
#define	MC_CMD_MEMCPY_RECORD_TYPEDEF_LEN 32
#define	MC_CMD_MEMCPY_RECORD_TYPEDEF_NUM_RECORDS_OFST 0
#define	MC_CMD_MEMCPY_RECORD_TYPEDEF_NUM_RECORDS_LBN 0
#define	MC_CMD_MEMCPY_RECORD_TYPEDEF_NUM_RECORDS_WIDTH 32
#define	MC_CMD_MEMCPY_RECORD_TYPEDEF_TO_RID_OFST 4
#define	MC_CMD_MEMCPY_RECORD_TYPEDEF_TO_RID_LBN 32
#define	MC_CMD_MEMCPY_RECORD_TYPEDEF_TO_RID_WIDTH 32
#define	MC_CMD_MEMCPY_RECORD_TYPEDEF_TO_ADDR_OFST 8
#define	MC_CMD_MEMCPY_RECORD_TYPEDEF_TO_ADDR_LEN 8
#define	MC_CMD_MEMCPY_RECORD_TYPEDEF_TO_ADDR_LO_OFST 8
#define	MC_CMD_MEMCPY_RECORD_TYPEDEF_TO_ADDR_HI_OFST 12
#define	MC_CMD_MEMCPY_RECORD_TYPEDEF_TO_ADDR_LBN 64
#define	MC_CMD_MEMCPY_RECORD_TYPEDEF_TO_ADDR_WIDTH 64
#define	MC_CMD_MEMCPY_RECORD_TYPEDEF_FROM_RID_OFST 16
#define	MC_CMD_MEMCPY_RECORD_TYPEDEF_RID_INLINE 0x100 /* enum */
#define	MC_CMD_MEMCPY_RECORD_TYPEDEF_FROM_RID_LBN 128
#define	MC_CMD_MEMCPY_RECORD_TYPEDEF_FROM_RID_WIDTH 32
#define	MC_CMD_MEMCPY_RECORD_TYPEDEF_FROM_ADDR_OFST 20
#define	MC_CMD_MEMCPY_RECORD_TYPEDEF_FROM_ADDR_LEN 8
#define	MC_CMD_MEMCPY_RECORD_TYPEDEF_FROM_ADDR_LO_OFST 20
#define	MC_CMD_MEMCPY_RECORD_TYPEDEF_FROM_ADDR_HI_OFST 24
#define	MC_CMD_MEMCPY_RECORD_TYPEDEF_FROM_ADDR_LBN 160
#define	MC_CMD_MEMCPY_RECORD_TYPEDEF_FROM_ADDR_WIDTH 64
#define	MC_CMD_MEMCPY_RECORD_TYPEDEF_LENGTH_OFST 28
#define	MC_CMD_MEMCPY_RECORD_TYPEDEF_LENGTH_LBN 224
#define	MC_CMD_MEMCPY_RECORD_TYPEDEF_LENGTH_WIDTH 32


/***********************************/
/* MC_CMD_MEMCPY 
 * Perform memory copy operation.
 */
#define	MC_CMD_MEMCPY  0x31

/* MC_CMD_MEMCPY_IN msgrequest */
#define	MC_CMD_MEMCPY_IN_LENMIN 32
#define	MC_CMD_MEMCPY_IN_LENMAX 224
#define	MC_CMD_MEMCPY_IN_LEN(num) (0+32*(num))
#define	MC_CMD_MEMCPY_IN_RECORD_OFST 0
#define	MC_CMD_MEMCPY_IN_RECORD_LEN 32
#define	MC_CMD_MEMCPY_IN_RECORD_MINNUM 1
#define	MC_CMD_MEMCPY_IN_RECORD_MAXNUM 7

/* MC_CMD_MEMCPY_OUT msgresponse */
#define	MC_CMD_MEMCPY_OUT_LEN 0


/***********************************/
/* MC_CMD_WOL_FILTER_SET 
 * Set a WoL filter.
 */
#define	MC_CMD_WOL_FILTER_SET  0x32

/* MC_CMD_WOL_FILTER_SET_IN msgrequest */
#define	MC_CMD_WOL_FILTER_SET_IN_LEN 192
#define	MC_CMD_WOL_FILTER_SET_IN_FILTER_MODE_OFST 0
#define	MC_CMD_FILTER_MODE_SIMPLE    0x0 /* enum */
#define	MC_CMD_FILTER_MODE_STRUCTURED 0xffffffff /* enum */
#define	MC_CMD_WOL_FILTER_SET_IN_WOL_TYPE_OFST 4
#define	MC_CMD_WOL_TYPE_MAGIC      0x0 /* enum */
#define	MC_CMD_WOL_TYPE_WIN_MAGIC 0x2 /* enum */
#define	MC_CMD_WOL_TYPE_IPV4_SYN   0x3 /* enum */
#define	MC_CMD_WOL_TYPE_IPV6_SYN   0x4 /* enum */
#define	MC_CMD_WOL_TYPE_BITMAP     0x5 /* enum */
#define	MC_CMD_WOL_TYPE_LINK       0x6 /* enum */
#define	MC_CMD_WOL_TYPE_MAX        0x7 /* enum */
#define	MC_CMD_WOL_FILTER_SET_IN_DATA_OFST 8
#define	MC_CMD_WOL_FILTER_SET_IN_DATA_LEN 4
#define	MC_CMD_WOL_FILTER_SET_IN_DATA_NUM 46

/* MC_CMD_WOL_FILTER_SET_IN_MAGIC msgrequest */
#define	MC_CMD_WOL_FILTER_SET_IN_MAGIC_LEN 16
/*            MC_CMD_WOL_FILTER_SET_IN_FILTER_MODE_OFST 0 */
/*            MC_CMD_WOL_FILTER_SET_IN_WOL_TYPE_OFST 4 */
#define	MC_CMD_WOL_FILTER_SET_IN_MAGIC_MAC_OFST 8
#define	MC_CMD_WOL_FILTER_SET_IN_MAGIC_MAC_LEN 8
#define	MC_CMD_WOL_FILTER_SET_IN_MAGIC_MAC_LO_OFST 8
#define	MC_CMD_WOL_FILTER_SET_IN_MAGIC_MAC_HI_OFST 12

/* MC_CMD_WOL_FILTER_SET_IN_IPV4_SYN msgrequest */
#define	MC_CMD_WOL_FILTER_SET_IN_IPV4_SYN_LEN 20
/*            MC_CMD_WOL_FILTER_SET_IN_FILTER_MODE_OFST 0 */
/*            MC_CMD_WOL_FILTER_SET_IN_WOL_TYPE_OFST 4 */
#define	MC_CMD_WOL_FILTER_SET_IN_IPV4_SYN_SRC_IP_OFST 8
#define	MC_CMD_WOL_FILTER_SET_IN_IPV4_SYN_DST_IP_OFST 12
#define	MC_CMD_WOL_FILTER_SET_IN_IPV4_SYN_SRC_PORT_OFST 16
#define	MC_CMD_WOL_FILTER_SET_IN_IPV4_SYN_SRC_PORT_LEN 2
#define	MC_CMD_WOL_FILTER_SET_IN_IPV4_SYN_DST_PORT_OFST 18
#define	MC_CMD_WOL_FILTER_SET_IN_IPV4_SYN_DST_PORT_LEN 2

/* MC_CMD_WOL_FILTER_SET_IN_IPV6_SYN msgrequest */
#define	MC_CMD_WOL_FILTER_SET_IN_IPV6_SYN_LEN 44
/*            MC_CMD_WOL_FILTER_SET_IN_FILTER_MODE_OFST 0 */
/*            MC_CMD_WOL_FILTER_SET_IN_WOL_TYPE_OFST 4 */
#define	MC_CMD_WOL_FILTER_SET_IN_IPV6_SYN_SRC_IP_OFST 8
#define	MC_CMD_WOL_FILTER_SET_IN_IPV6_SYN_SRC_IP_LEN 16
#define	MC_CMD_WOL_FILTER_SET_IN_IPV6_SYN_DST_IP_OFST 24
#define	MC_CMD_WOL_FILTER_SET_IN_IPV6_SYN_DST_IP_LEN 16
#define	MC_CMD_WOL_FILTER_SET_IN_IPV6_SYN_SRC_PORT_OFST 40
#define	MC_CMD_WOL_FILTER_SET_IN_IPV6_SYN_SRC_PORT_LEN 2
#define	MC_CMD_WOL_FILTER_SET_IN_IPV6_SYN_DST_PORT_OFST 42
#define	MC_CMD_WOL_FILTER_SET_IN_IPV6_SYN_DST_PORT_LEN 2

/* MC_CMD_WOL_FILTER_SET_IN_BITMAP msgrequest */
#define	MC_CMD_WOL_FILTER_SET_IN_BITMAP_LEN 187
/*            MC_CMD_WOL_FILTER_SET_IN_FILTER_MODE_OFST 0 */
/*            MC_CMD_WOL_FILTER_SET_IN_WOL_TYPE_OFST 4 */
#define	MC_CMD_WOL_FILTER_SET_IN_BITMAP_MASK_OFST 8
#define	MC_CMD_WOL_FILTER_SET_IN_BITMAP_MASK_LEN 48
#define	MC_CMD_WOL_FILTER_SET_IN_BITMAP_BITMAP_OFST 56
#define	MC_CMD_WOL_FILTER_SET_IN_BITMAP_BITMAP_LEN 128
#define	MC_CMD_WOL_FILTER_SET_IN_BITMAP_LEN_OFST 184
#define	MC_CMD_WOL_FILTER_SET_IN_BITMAP_LEN_LEN 1
#define	MC_CMD_WOL_FILTER_SET_IN_BITMAP_LAYER3_OFST 185
#define	MC_CMD_WOL_FILTER_SET_IN_BITMAP_LAYER3_LEN 1
#define	MC_CMD_WOL_FILTER_SET_IN_BITMAP_LAYER4_OFST 186
#define	MC_CMD_WOL_FILTER_SET_IN_BITMAP_LAYER4_LEN 1

/* MC_CMD_WOL_FILTER_SET_IN_LINK msgrequest */
#define	MC_CMD_WOL_FILTER_SET_IN_LINK_LEN 12
/*            MC_CMD_WOL_FILTER_SET_IN_FILTER_MODE_OFST 0 */
/*            MC_CMD_WOL_FILTER_SET_IN_WOL_TYPE_OFST 4 */
#define	MC_CMD_WOL_FILTER_SET_IN_LINK_MASK_OFST 8
#define	MC_CMD_WOL_FILTER_SET_IN_LINK_UP_LBN 0
#define	MC_CMD_WOL_FILTER_SET_IN_LINK_UP_WIDTH 1
#define	MC_CMD_WOL_FILTER_SET_IN_LINK_DOWN_LBN 1
#define	MC_CMD_WOL_FILTER_SET_IN_LINK_DOWN_WIDTH 1

/* MC_CMD_WOL_FILTER_SET_OUT msgresponse */
#define	MC_CMD_WOL_FILTER_SET_OUT_LEN 4
#define	MC_CMD_WOL_FILTER_SET_OUT_FILTER_ID_OFST 0


/***********************************/
/* MC_CMD_WOL_FILTER_REMOVE 
 * Remove a WoL filter.
 */
#define	MC_CMD_WOL_FILTER_REMOVE  0x33

/* MC_CMD_WOL_FILTER_REMOVE_IN msgrequest */
#define	MC_CMD_WOL_FILTER_REMOVE_IN_LEN 4
#define	MC_CMD_WOL_FILTER_REMOVE_IN_FILTER_ID_OFST 0

/* MC_CMD_WOL_FILTER_REMOVE_OUT msgresponse */
#define	MC_CMD_WOL_FILTER_REMOVE_OUT_LEN 0


/***********************************/
/* MC_CMD_WOL_FILTER_RESET 
 * Reset (i.e. remove all) WoL filters.
 */
#define	MC_CMD_WOL_FILTER_RESET  0x34

/* MC_CMD_WOL_FILTER_RESET_IN msgrequest */
#define	MC_CMD_WOL_FILTER_RESET_IN_LEN 4
#define	MC_CMD_WOL_FILTER_RESET_IN_MASK_OFST 0
#define	MC_CMD_WOL_FILTER_RESET_IN_WAKE_FILTERS 0x1 /* enum */
#define	MC_CMD_WOL_FILTER_RESET_IN_LIGHTSOUT_OFFLOADS 0x2 /* enum */

/* MC_CMD_WOL_FILTER_RESET_OUT msgresponse */
#define	MC_CMD_WOL_FILTER_RESET_OUT_LEN 0


/***********************************/
/* MC_CMD_SET_MCAST_HASH 
 * Set the MCASH hash value.
 */
#define	MC_CMD_SET_MCAST_HASH  0x35

/* MC_CMD_SET_MCAST_HASH_IN msgrequest */
#define	MC_CMD_SET_MCAST_HASH_IN_LEN 32
#define	MC_CMD_SET_MCAST_HASH_IN_HASH0_OFST 0
#define	MC_CMD_SET_MCAST_HASH_IN_HASH0_LEN 16
#define	MC_CMD_SET_MCAST_HASH_IN_HASH1_OFST 16
#define	MC_CMD_SET_MCAST_HASH_IN_HASH1_LEN 16

/* MC_CMD_SET_MCAST_HASH_OUT msgresponse */
#define	MC_CMD_SET_MCAST_HASH_OUT_LEN 0


/***********************************/
/* MC_CMD_NVRAM_TYPES 
 * Get virtual NVRAM partitions information.
 */
#define	MC_CMD_NVRAM_TYPES  0x36

/* MC_CMD_NVRAM_TYPES_IN msgrequest */
#define	MC_CMD_NVRAM_TYPES_IN_LEN 0

/* MC_CMD_NVRAM_TYPES_OUT msgresponse */
#define	MC_CMD_NVRAM_TYPES_OUT_LEN 4
#define	MC_CMD_NVRAM_TYPES_OUT_TYPES_OFST 0
#define	MC_CMD_NVRAM_TYPE_DISABLED_CALLISTO 0x0 /* enum */
#define	MC_CMD_NVRAM_TYPE_MC_FW 0x1 /* enum */
#define	MC_CMD_NVRAM_TYPE_MC_FW_BACKUP 0x2 /* enum */
#define	MC_CMD_NVRAM_TYPE_STATIC_CFG_PORT0 0x3 /* enum */
#define	MC_CMD_NVRAM_TYPE_STATIC_CFG_PORT1 0x4 /* enum */
#define	MC_CMD_NVRAM_TYPE_DYNAMIC_CFG_PORT0 0x5 /* enum */
#define	MC_CMD_NVRAM_TYPE_DYNAMIC_CFG_PORT1 0x6 /* enum */
#define	MC_CMD_NVRAM_TYPE_EXP_ROM 0x7 /* enum */
#define	MC_CMD_NVRAM_TYPE_EXP_ROM_CFG_PORT0 0x8 /* enum */
#define	MC_CMD_NVRAM_TYPE_EXP_ROM_CFG_PORT1 0x9 /* enum */
#define	MC_CMD_NVRAM_TYPE_PHY_PORT0 0xa /* enum */
#define	MC_CMD_NVRAM_TYPE_PHY_PORT1 0xb /* enum */
#define	MC_CMD_NVRAM_TYPE_LOG 0xc /* enum */
#define	MC_CMD_NVRAM_TYPE_FPGA 0xd /* enum */
#define	MC_CMD_NVRAM_TYPE_FPGA_BACKUP 0xe /* enum */
#define	MC_CMD_NVRAM_TYPE_FC_FW 0xf /* enum */
#define	MC_CMD_NVRAM_TYPE_FC_FW_BACKUP 0x10 /* enum */
#define	MC_CMD_NVRAM_TYPE_CPLD 0x11 /* enum */
#define	MC_CMD_NVRAM_TYPE_LICENSE 0x12 /* enum */
#define	MC_CMD_NVRAM_TYPE_FC_LOG 0x13 /* enum */
#define	MC_CMD_NVRAM_TYPE_FC_EXTRA 0x14 /* enum */


/***********************************/
/* MC_CMD_NVRAM_INFO 
 * Read info about a virtual NVRAM partition.
 */
#define	MC_CMD_NVRAM_INFO  0x37

/* MC_CMD_NVRAM_INFO_IN msgrequest */
#define	MC_CMD_NVRAM_INFO_IN_LEN 4
#define	MC_CMD_NVRAM_INFO_IN_TYPE_OFST 0
/*            Enum values, see field(s): */
/*               MC_CMD_NVRAM_TYPES/MC_CMD_NVRAM_TYPES_OUT/TYPES */

/* MC_CMD_NVRAM_INFO_OUT msgresponse */
#define	MC_CMD_NVRAM_INFO_OUT_LEN 24
#define	MC_CMD_NVRAM_INFO_OUT_TYPE_OFST 0
/*            Enum values, see field(s): */
/*               MC_CMD_NVRAM_TYPES/MC_CMD_NVRAM_TYPES_OUT/TYPES */
#define	MC_CMD_NVRAM_INFO_OUT_SIZE_OFST 4
#define	MC_CMD_NVRAM_INFO_OUT_ERASESIZE_OFST 8
#define	MC_CMD_NVRAM_INFO_OUT_FLAGS_OFST 12
#define	MC_CMD_NVRAM_INFO_OUT_PROTECTED_LBN 0
#define	MC_CMD_NVRAM_INFO_OUT_PROTECTED_WIDTH 1
#define	MC_CMD_NVRAM_INFO_OUT_PHYSDEV_OFST 16
#define	MC_CMD_NVRAM_INFO_OUT_PHYSADDR_OFST 20


/***********************************/
/* MC_CMD_NVRAM_UPDATE_START 
 * Start a group of update operations on a virtual NVRAM partition.
 */
#define	MC_CMD_NVRAM_UPDATE_START  0x38

/* MC_CMD_NVRAM_UPDATE_START_IN msgrequest */
#define	MC_CMD_NVRAM_UPDATE_START_IN_LEN 4
#define	MC_CMD_NVRAM_UPDATE_START_IN_TYPE_OFST 0
/*            Enum values, see field(s): */
/*               MC_CMD_NVRAM_TYPES/MC_CMD_NVRAM_TYPES_OUT/TYPES */

/* MC_CMD_NVRAM_UPDATE_START_OUT msgresponse */
#define	MC_CMD_NVRAM_UPDATE_START_OUT_LEN 0


/***********************************/
/* MC_CMD_NVRAM_READ 
 * Read data from a virtual NVRAM partition.
 */
#define	MC_CMD_NVRAM_READ  0x39

/* MC_CMD_NVRAM_READ_IN msgrequest */
#define	MC_CMD_NVRAM_READ_IN_LEN 12
#define	MC_CMD_NVRAM_READ_IN_TYPE_OFST 0
/*            Enum values, see field(s): */
/*               MC_CMD_NVRAM_TYPES/MC_CMD_NVRAM_TYPES_OUT/TYPES */
#define	MC_CMD_NVRAM_READ_IN_OFFSET_OFST 4
#define	MC_CMD_NVRAM_READ_IN_LENGTH_OFST 8

/* MC_CMD_NVRAM_READ_OUT msgresponse */
#define	MC_CMD_NVRAM_READ_OUT_LENMIN 1
#define	MC_CMD_NVRAM_READ_OUT_LENMAX 252
#define	MC_CMD_NVRAM_READ_OUT_LEN(num) (0+1*(num))
#define	MC_CMD_NVRAM_READ_OUT_READ_BUFFER_OFST 0
#define	MC_CMD_NVRAM_READ_OUT_READ_BUFFER_LEN 1
#define	MC_CMD_NVRAM_READ_OUT_READ_BUFFER_MINNUM 1
#define	MC_CMD_NVRAM_READ_OUT_READ_BUFFER_MAXNUM 252


/***********************************/
/* MC_CMD_NVRAM_WRITE 
 * Write data to a virtual NVRAM partition.
 */
#define	MC_CMD_NVRAM_WRITE  0x3a

/* MC_CMD_NVRAM_WRITE_IN msgrequest */
#define	MC_CMD_NVRAM_WRITE_IN_LENMIN 13
#define	MC_CMD_NVRAM_WRITE_IN_LENMAX 252
#define	MC_CMD_NVRAM_WRITE_IN_LEN(num) (12+1*(num))
#define	MC_CMD_NVRAM_WRITE_IN_TYPE_OFST 0
/*            Enum values, see field(s): */
/*               MC_CMD_NVRAM_TYPES/MC_CMD_NVRAM_TYPES_OUT/TYPES */
#define	MC_CMD_NVRAM_WRITE_IN_OFFSET_OFST 4
#define	MC_CMD_NVRAM_WRITE_IN_LENGTH_OFST 8
#define	MC_CMD_NVRAM_WRITE_IN_WRITE_BUFFER_OFST 12
#define	MC_CMD_NVRAM_WRITE_IN_WRITE_BUFFER_LEN 1
#define	MC_CMD_NVRAM_WRITE_IN_WRITE_BUFFER_MINNUM 1
#define	MC_CMD_NVRAM_WRITE_IN_WRITE_BUFFER_MAXNUM 240

/* MC_CMD_NVRAM_WRITE_OUT msgresponse */
#define	MC_CMD_NVRAM_WRITE_OUT_LEN 0


/***********************************/
/* MC_CMD_NVRAM_ERASE 
 * Erase sector(s) from a virtual NVRAM partition.
 */
#define	MC_CMD_NVRAM_ERASE  0x3b

/* MC_CMD_NVRAM_ERASE_IN msgrequest */
#define	MC_CMD_NVRAM_ERASE_IN_LEN 12
#define	MC_CMD_NVRAM_ERASE_IN_TYPE_OFST 0
/*            Enum values, see field(s): */
/*               MC_CMD_NVRAM_TYPES/MC_CMD_NVRAM_TYPES_OUT/TYPES */
#define	MC_CMD_NVRAM_ERASE_IN_OFFSET_OFST 4
#define	MC_CMD_NVRAM_ERASE_IN_LENGTH_OFST 8

/* MC_CMD_NVRAM_ERASE_OUT msgresponse */
#define	MC_CMD_NVRAM_ERASE_OUT_LEN 0


/***********************************/
/* MC_CMD_NVRAM_UPDATE_FINISH 
 * Finish a group of update operations on a virtual NVRAM partition.
 */
#define	MC_CMD_NVRAM_UPDATE_FINISH  0x3c

/* MC_CMD_NVRAM_UPDATE_FINISH_IN msgrequest */
#define	MC_CMD_NVRAM_UPDATE_FINISH_IN_LEN 8
#define	MC_CMD_NVRAM_UPDATE_FINISH_IN_TYPE_OFST 0
/*            Enum values, see field(s): */
/*               MC_CMD_NVRAM_TYPES/MC_CMD_NVRAM_TYPES_OUT/TYPES */
#define	MC_CMD_NVRAM_UPDATE_FINISH_IN_REBOOT_OFST 4

/* MC_CMD_NVRAM_UPDATE_FINISH_OUT msgresponse */
#define	MC_CMD_NVRAM_UPDATE_FINISH_OUT_LEN 0


/***********************************/
/* MC_CMD_REBOOT 
 * Reboot the MC.
 */
#define	MC_CMD_REBOOT  0x3d

/* MC_CMD_REBOOT_IN msgrequest */
#define	MC_CMD_REBOOT_IN_LEN 4
#define	MC_CMD_REBOOT_IN_FLAGS_OFST 0
#define	MC_CMD_REBOOT_FLAGS_AFTER_ASSERTION 0x1 /* enum */

/* MC_CMD_REBOOT_OUT msgresponse */
#define	MC_CMD_REBOOT_OUT_LEN 0


/***********************************/
/* MC_CMD_SCHEDINFO 
 * Request scheduler info.
 */
#define	MC_CMD_SCHEDINFO  0x3e

/* MC_CMD_SCHEDINFO_IN msgrequest */
#define	MC_CMD_SCHEDINFO_IN_LEN 0

/* MC_CMD_SCHEDINFO_OUT msgresponse */
#define	MC_CMD_SCHEDINFO_OUT_LENMIN 4
#define	MC_CMD_SCHEDINFO_OUT_LENMAX 252
#define	MC_CMD_SCHEDINFO_OUT_LEN(num) (0+4*(num))
#define	MC_CMD_SCHEDINFO_OUT_DATA_OFST 0
#define	MC_CMD_SCHEDINFO_OUT_DATA_LEN 4
#define	MC_CMD_SCHEDINFO_OUT_DATA_MINNUM 1
#define	MC_CMD_SCHEDINFO_OUT_DATA_MAXNUM 63


/***********************************/
/* MC_CMD_REBOOT_MODE 
 */
#define	MC_CMD_REBOOT_MODE  0x3f

/* MC_CMD_REBOOT_MODE_IN msgrequest */
#define	MC_CMD_REBOOT_MODE_IN_LEN 4
#define	MC_CMD_REBOOT_MODE_IN_VALUE_OFST 0
#define	MC_CMD_REBOOT_MODE_NORMAL 0x0 /* enum */
#define	MC_CMD_REBOOT_MODE_SNAPPER 0x3 /* enum */

/* MC_CMD_REBOOT_MODE_OUT msgresponse */
#define	MC_CMD_REBOOT_MODE_OUT_LEN 4
#define	MC_CMD_REBOOT_MODE_OUT_VALUE_OFST 0


/***********************************/
/* MC_CMD_SENSOR_INFO 
 * Returns information about every available sensor.
 */
#define	MC_CMD_SENSOR_INFO  0x41

/* MC_CMD_SENSOR_INFO_IN msgrequest */
#define	MC_CMD_SENSOR_INFO_IN_LEN 0

/* MC_CMD_SENSOR_INFO_OUT msgresponse */
#define	MC_CMD_SENSOR_INFO_OUT_LENMIN 12
#define	MC_CMD_SENSOR_INFO_OUT_LENMAX 252
#define	MC_CMD_SENSOR_INFO_OUT_LEN(num) (4+8*(num))
#define	MC_CMD_SENSOR_INFO_OUT_MASK_OFST 0
#define	MC_CMD_SENSOR_CONTROLLER_TEMP  0x0 /* enum */
#define	MC_CMD_SENSOR_PHY_COMMON_TEMP  0x1 /* enum */
#define	MC_CMD_SENSOR_CONTROLLER_COOLING  0x2 /* enum */
#define	MC_CMD_SENSOR_PHY0_TEMP  0x3 /* enum */
#define	MC_CMD_SENSOR_PHY0_COOLING  0x4 /* enum */
#define	MC_CMD_SENSOR_PHY1_TEMP  0x5 /* enum */
#define	MC_CMD_SENSOR_PHY1_COOLING  0x6 /* enum */
#define	MC_CMD_SENSOR_IN_1V0  0x7 /* enum */
#define	MC_CMD_SENSOR_IN_1V2  0x8 /* enum */
#define	MC_CMD_SENSOR_IN_1V8  0x9 /* enum */
#define	MC_CMD_SENSOR_IN_2V5  0xa /* enum */
#define	MC_CMD_SENSOR_IN_3V3  0xb /* enum */
#define	MC_CMD_SENSOR_IN_12V0  0xc /* enum */
#define	MC_CMD_SENSOR_IN_1V2A  0xd /* enum */
#define	MC_CMD_SENSOR_IN_VREF  0xe /* enum */
#define	MC_CMD_SENSOR_OUT_VAOE  0xf /* enum */
#define	MC_CMD_SENSOR_AOE_TEMP  0x10 /* enum */
#define	MC_CMD_SENSOR_PSU_AOE_TEMP  0x11 /* enum */
#define	MC_CMD_SENSOR_PSU_TEMP  0x12 /* enum */
#define	MC_CMD_SENSOR_FAN_0  0x13 /* enum */
#define	MC_CMD_SENSOR_FAN_1  0x14 /* enum */
#define	MC_CMD_SENSOR_FAN_2  0x15 /* enum */
#define	MC_CMD_SENSOR_FAN_3  0x16 /* enum */
#define	MC_CMD_SENSOR_FAN_4  0x17 /* enum */
#define	MC_CMD_SENSOR_IN_VAOE  0x18 /* enum */
#define	MC_CMD_SENSOR_OUT_IAOE  0x19 /* enum */
#define	MC_CMD_SENSOR_IN_IAOE  0x1a /* enum */
#define	MC_CMD_SENSOR_NIC_POWER  0x1b /* enum */
#define	MC_CMD_SENSOR_ENTRY_OFST 4
#define	MC_CMD_SENSOR_ENTRY_LEN 8
#define	MC_CMD_SENSOR_ENTRY_LO_OFST 4
#define	MC_CMD_SENSOR_ENTRY_HI_OFST 8
#define	MC_CMD_SENSOR_ENTRY_MINNUM 1
#define	MC_CMD_SENSOR_ENTRY_MAXNUM 31

/* MC_CMD_SENSOR_INFO_ENTRY_TYPEDEF structuredef */
#define	MC_CMD_SENSOR_INFO_ENTRY_TYPEDEF_LEN 8
#define	MC_CMD_SENSOR_INFO_ENTRY_TYPEDEF_MIN1_OFST 0
#define	MC_CMD_SENSOR_INFO_ENTRY_TYPEDEF_MIN1_LEN 2
#define	MC_CMD_SENSOR_INFO_ENTRY_TYPEDEF_MIN1_LBN 0
#define	MC_CMD_SENSOR_INFO_ENTRY_TYPEDEF_MIN1_WIDTH 16
#define	MC_CMD_SENSOR_INFO_ENTRY_TYPEDEF_MAX1_OFST 2
#define	MC_CMD_SENSOR_INFO_ENTRY_TYPEDEF_MAX1_LEN 2
#define	MC_CMD_SENSOR_INFO_ENTRY_TYPEDEF_MAX1_LBN 16
#define	MC_CMD_SENSOR_INFO_ENTRY_TYPEDEF_MAX1_WIDTH 16
#define	MC_CMD_SENSOR_INFO_ENTRY_TYPEDEF_MIN2_OFST 4
#define	MC_CMD_SENSOR_INFO_ENTRY_TYPEDEF_MIN2_LEN 2
#define	MC_CMD_SENSOR_INFO_ENTRY_TYPEDEF_MIN2_LBN 32
#define	MC_CMD_SENSOR_INFO_ENTRY_TYPEDEF_MIN2_WIDTH 16
#define	MC_CMD_SENSOR_INFO_ENTRY_TYPEDEF_MAX2_OFST 6
#define	MC_CMD_SENSOR_INFO_ENTRY_TYPEDEF_MAX2_LEN 2
#define	MC_CMD_SENSOR_INFO_ENTRY_TYPEDEF_MAX2_LBN 48
#define	MC_CMD_SENSOR_INFO_ENTRY_TYPEDEF_MAX2_WIDTH 16


/***********************************/
/* MC_CMD_READ_SENSORS 
 * Returns the current reading from each sensor.
 */
#define	MC_CMD_READ_SENSORS  0x42

/* MC_CMD_READ_SENSORS_IN msgrequest */
#define	MC_CMD_READ_SENSORS_IN_LEN 8
#define	MC_CMD_READ_SENSORS_IN_DMA_ADDR_OFST 0
#define	MC_CMD_READ_SENSORS_IN_DMA_ADDR_LEN 8
#define	MC_CMD_READ_SENSORS_IN_DMA_ADDR_LO_OFST 0
#define	MC_CMD_READ_SENSORS_IN_DMA_ADDR_HI_OFST 4

/* MC_CMD_READ_SENSORS_OUT msgresponse */
#define	MC_CMD_READ_SENSORS_OUT_LEN 0

/* MC_CMD_SENSOR_VALUE_ENTRY_TYPEDEF structuredef */
#define	MC_CMD_SENSOR_VALUE_ENTRY_TYPEDEF_LEN 3
#define	MC_CMD_SENSOR_VALUE_ENTRY_TYPEDEF_VALUE_OFST 0
#define	MC_CMD_SENSOR_VALUE_ENTRY_TYPEDEF_VALUE_LEN 2
#define	MC_CMD_SENSOR_VALUE_ENTRY_TYPEDEF_VALUE_LBN 0
#define	MC_CMD_SENSOR_VALUE_ENTRY_TYPEDEF_VALUE_WIDTH 16
#define	MC_CMD_SENSOR_VALUE_ENTRY_TYPEDEF_STATE_OFST 2
#define	MC_CMD_SENSOR_VALUE_ENTRY_TYPEDEF_STATE_LEN 1
#define	MC_CMD_SENSOR_STATE_OK  0x0 /* enum */
#define	MC_CMD_SENSOR_STATE_WARNING  0x1 /* enum */
#define	MC_CMD_SENSOR_STATE_FATAL  0x2 /* enum */
#define	MC_CMD_SENSOR_STATE_BROKEN  0x3 /* enum */
#define	MC_CMD_SENSOR_VALUE_ENTRY_TYPEDEF_STATE_LBN 16
#define	MC_CMD_SENSOR_VALUE_ENTRY_TYPEDEF_STATE_WIDTH 8


/***********************************/
/* MC_CMD_GET_PHY_STATE 
 * Report current state of PHY.
 */
#define	MC_CMD_GET_PHY_STATE  0x43

/* MC_CMD_GET_PHY_STATE_IN msgrequest */
#define	MC_CMD_GET_PHY_STATE_IN_LEN 0

/* MC_CMD_GET_PHY_STATE_OUT msgresponse */
#define	MC_CMD_GET_PHY_STATE_OUT_LEN 4
#define	MC_CMD_GET_PHY_STATE_OUT_STATE_OFST 0
#define	MC_CMD_PHY_STATE_OK 0x1 /* enum */
#define	MC_CMD_PHY_STATE_ZOMBIE 0x2 /* enum */


/***********************************/
/* MC_CMD_SETUP_8021QBB 
 * 802.1Qbb control.
 */
#define	MC_CMD_SETUP_8021QBB  0x44

/* MC_CMD_SETUP_8021QBB_IN msgrequest */
#define	MC_CMD_SETUP_8021QBB_IN_LEN 32
#define	MC_CMD_SETUP_8021QBB_IN_TXQS_OFST 0
#define	MC_CMD_SETUP_8021QBB_IN_TXQS_LEN 32

/* MC_CMD_SETUP_8021QBB_OUT msgresponse */
#define	MC_CMD_SETUP_8021QBB_OUT_LEN 0


/***********************************/
/* MC_CMD_WOL_FILTER_GET 
 * Retrieve ID of any WoL filters.
 */
#define	MC_CMD_WOL_FILTER_GET  0x45

/* MC_CMD_WOL_FILTER_GET_IN msgrequest */
#define	MC_CMD_WOL_FILTER_GET_IN_LEN 0

/* MC_CMD_WOL_FILTER_GET_OUT msgresponse */
#define	MC_CMD_WOL_FILTER_GET_OUT_LEN 4
#define	MC_CMD_WOL_FILTER_GET_OUT_FILTER_ID_OFST 0


/***********************************/
/* MC_CMD_ADD_LIGHTSOUT_OFFLOAD 
 * Add a protocol offload to NIC for lights-out state.
 */
#define	MC_CMD_ADD_LIGHTSOUT_OFFLOAD  0x46

/* MC_CMD_ADD_LIGHTSOUT_OFFLOAD_IN msgrequest */
#define	MC_CMD_ADD_LIGHTSOUT_OFFLOAD_IN_LENMIN 8
#define	MC_CMD_ADD_LIGHTSOUT_OFFLOAD_IN_LENMAX 252
#define	MC_CMD_ADD_LIGHTSOUT_OFFLOAD_IN_LEN(num) (4+4*(num))
#define	MC_CMD_ADD_LIGHTSOUT_OFFLOAD_IN_PROTOCOL_OFST 0
#define	MC_CMD_LIGHTSOUT_OFFLOAD_PROTOCOL_ARP 0x1 /* enum */
#define	MC_CMD_LIGHTSOUT_OFFLOAD_PROTOCOL_NS  0x2 /* enum */
#define	MC_CMD_ADD_LIGHTSOUT_OFFLOAD_IN_DATA_OFST 4
#define	MC_CMD_ADD_LIGHTSOUT_OFFLOAD_IN_DATA_LEN 4
#define	MC_CMD_ADD_LIGHTSOUT_OFFLOAD_IN_DATA_MINNUM 1
#define	MC_CMD_ADD_LIGHTSOUT_OFFLOAD_IN_DATA_MAXNUM 62

/* MC_CMD_ADD_LIGHTSOUT_OFFLOAD_IN_ARP msgrequest */
#define	MC_CMD_ADD_LIGHTSOUT_OFFLOAD_IN_ARP_LEN 14
/*            MC_CMD_ADD_LIGHTSOUT_OFFLOAD_IN_PROTOCOL_OFST 0 */
#define	MC_CMD_ADD_LIGHTSOUT_OFFLOAD_IN_ARP_MAC_OFST 4
#define	MC_CMD_ADD_LIGHTSOUT_OFFLOAD_IN_ARP_MAC_LEN 6
#define	MC_CMD_ADD_LIGHTSOUT_OFFLOAD_IN_ARP_IP_OFST 10

/* MC_CMD_ADD_LIGHTSOUT_OFFLOAD_IN_NS msgrequest */
#define	MC_CMD_ADD_LIGHTSOUT_OFFLOAD_IN_NS_LEN 42
/*            MC_CMD_ADD_LIGHTSOUT_OFFLOAD_IN_PROTOCOL_OFST 0 */
#define	MC_CMD_ADD_LIGHTSOUT_OFFLOAD_IN_NS_MAC_OFST 4
#define	MC_CMD_ADD_LIGHTSOUT_OFFLOAD_IN_NS_MAC_LEN 6
#define	MC_CMD_ADD_LIGHTSOUT_OFFLOAD_IN_NS_SNIPV6_OFST 10
#define	MC_CMD_ADD_LIGHTSOUT_OFFLOAD_IN_NS_SNIPV6_LEN 16
#define	MC_CMD_ADD_LIGHTSOUT_OFFLOAD_IN_NS_IPV6_OFST 26
#define	MC_CMD_ADD_LIGHTSOUT_OFFLOAD_IN_NS_IPV6_LEN 16

/* MC_CMD_ADD_LIGHTSOUT_OFFLOAD_OUT msgresponse */
#define	MC_CMD_ADD_LIGHTSOUT_OFFLOAD_OUT_LEN 4
#define	MC_CMD_ADD_LIGHTSOUT_OFFLOAD_OUT_FILTER_ID_OFST 0


/***********************************/
/* MC_CMD_REMOVE_LIGHTSOUT_OFFLOAD 
 * Remove a protocol offload from NIC for lights-out state.
 */
#define	MC_CMD_REMOVE_LIGHTSOUT_OFFLOAD  0x47

/* MC_CMD_REMOVE_LIGHTSOUT_OFFLOAD_IN msgrequest */
#define	MC_CMD_REMOVE_LIGHTSOUT_OFFLOAD_IN_LEN 8
#define	MC_CMD_REMOVE_LIGHTSOUT_OFFLOAD_IN_PROTOCOL_OFST 0
#define	MC_CMD_REMOVE_LIGHTSOUT_OFFLOAD_IN_FILTER_ID_OFST 4

/* MC_CMD_REMOVE_LIGHTSOUT_OFFLOAD_OUT msgresponse */
#define	MC_CMD_REMOVE_LIGHTSOUT_OFFLOAD_OUT_LEN 0


/***********************************/
/* MC_CMD_MAC_RESET_RESTORE 
 * Restore MAC after block reset.
 */
#define	MC_CMD_MAC_RESET_RESTORE  0x48

/* MC_CMD_MAC_RESET_RESTORE_IN msgrequest */
#define	MC_CMD_MAC_RESET_RESTORE_IN_LEN 0

/* MC_CMD_MAC_RESET_RESTORE_OUT msgresponse */
#define	MC_CMD_MAC_RESET_RESTORE_OUT_LEN 0


/***********************************/
/* MC_CMD_TESTASSERT  
 */
#define	MC_CMD_TESTASSERT   0x49

/* MC_CMD_TESTASSERT_IN msgrequest */
#define	MC_CMD_TESTASSERT_IN_LEN 0

/* MC_CMD_TESTASSERT_OUT msgresponse */
#define	MC_CMD_TESTASSERT_OUT_LEN 0


/***********************************/
/* MC_CMD_WORKAROUND 
 * Enable/Disable a given workaround.
 */
#define	MC_CMD_WORKAROUND  0x4a

/* MC_CMD_WORKAROUND_IN msgrequest */
#define	MC_CMD_WORKAROUND_IN_LEN 8
#define	MC_CMD_WORKAROUND_IN_TYPE_OFST 0
#define	MC_CMD_WORKAROUND_BUG17230 0x1 /* enum */
#define	MC_CMD_WORKAROUND_IN_ENABLED_OFST 4

/* MC_CMD_WORKAROUND_OUT msgresponse */
#define	MC_CMD_WORKAROUND_OUT_LEN 0


/***********************************/
/* MC_CMD_GET_PHY_MEDIA_INFO 
 * Read media-specific data from PHY.
 */
#define	MC_CMD_GET_PHY_MEDIA_INFO  0x4b

/* MC_CMD_GET_PHY_MEDIA_INFO_IN msgrequest */
#define	MC_CMD_GET_PHY_MEDIA_INFO_IN_LEN 4
#define	MC_CMD_GET_PHY_MEDIA_INFO_IN_PAGE_OFST 0

/* MC_CMD_GET_PHY_MEDIA_INFO_OUT msgresponse */
#define	MC_CMD_GET_PHY_MEDIA_INFO_OUT_LENMIN 5
#define	MC_CMD_GET_PHY_MEDIA_INFO_OUT_LENMAX 252
#define	MC_CMD_GET_PHY_MEDIA_INFO_OUT_LEN(num) (4+1*(num))
#define	MC_CMD_GET_PHY_MEDIA_INFO_OUT_DATALEN_OFST 0
#define	MC_CMD_GET_PHY_MEDIA_INFO_OUT_DATA_OFST 4
#define	MC_CMD_GET_PHY_MEDIA_INFO_OUT_DATA_LEN 1
#define	MC_CMD_GET_PHY_MEDIA_INFO_OUT_DATA_MINNUM 1
#define	MC_CMD_GET_PHY_MEDIA_INFO_OUT_DATA_MAXNUM 248


/***********************************/
/* MC_CMD_NVRAM_TEST 
 * Test a particular NVRAM partition.
 */
#define	MC_CMD_NVRAM_TEST  0x4c

/* MC_CMD_NVRAM_TEST_IN msgrequest */
#define	MC_CMD_NVRAM_TEST_IN_LEN 4
#define	MC_CMD_NVRAM_TEST_IN_TYPE_OFST 0
/*            Enum values, see field(s): */
/*               MC_CMD_NVRAM_TYPES/MC_CMD_NVRAM_TYPES_OUT/TYPES */

/* MC_CMD_NVRAM_TEST_OUT msgresponse */
#define	MC_CMD_NVRAM_TEST_OUT_LEN 4
#define	MC_CMD_NVRAM_TEST_OUT_RESULT_OFST 0
#define	MC_CMD_NVRAM_TEST_PASS 0x0 /* enum */
#define	MC_CMD_NVRAM_TEST_FAIL 0x1 /* enum */
#define	MC_CMD_NVRAM_TEST_NOTSUPP 0x2 /* enum */


/***********************************/
/* MC_CMD_MRSFP_TWEAK 
 * Read status and/or set parameters for the 'mrsfp' driver.
 */
#define	MC_CMD_MRSFP_TWEAK  0x4d

/* MC_CMD_MRSFP_TWEAK_IN_EQ_CONFIG msgrequest */
#define	MC_CMD_MRSFP_TWEAK_IN_EQ_CONFIG_LEN 16
#define	MC_CMD_MRSFP_TWEAK_IN_EQ_CONFIG_TXEQ_LEVEL_OFST 0
#define	MC_CMD_MRSFP_TWEAK_IN_EQ_CONFIG_TXEQ_DT_CFG_OFST 4
#define	MC_CMD_MRSFP_TWEAK_IN_EQ_CONFIG_RXEQ_BOOST_OFST 8
#define	MC_CMD_MRSFP_TWEAK_IN_EQ_CONFIG_RXEQ_DT_CFG_OFST 12

/* MC_CMD_MRSFP_TWEAK_IN_READ_ONLY msgrequest */
#define	MC_CMD_MRSFP_TWEAK_IN_READ_ONLY_LEN 0

/* MC_CMD_MRSFP_TWEAK_OUT msgresponse */
#define	MC_CMD_MRSFP_TWEAK_OUT_LEN 12
#define	MC_CMD_MRSFP_TWEAK_OUT_IOEXP_INPUTS_OFST 0
#define	MC_CMD_MRSFP_TWEAK_OUT_IOEXP_OUTPUTS_OFST 4
#define	MC_CMD_MRSFP_TWEAK_OUT_IOEXP_DIRECTION_OFST 8
#define	MC_CMD_MRSFP_TWEAK_OUT_IOEXP_DIRECTION_OUT 0x0 /* enum */
#define	MC_CMD_MRSFP_TWEAK_OUT_IOEXP_DIRECTION_IN 0x1 /* enum */


/***********************************/
/* MC_CMD_SENSOR_SET_LIMS 
 * Adjusts the sensor limits.
 */
#define	MC_CMD_SENSOR_SET_LIMS  0x4e

/* MC_CMD_SENSOR_SET_LIMS_IN msgrequest */
#define	MC_CMD_SENSOR_SET_LIMS_IN_LEN 20
#define	MC_CMD_SENSOR_SET_LIMS_IN_SENSOR_OFST 0
/*            Enum values, see field(s): */
/*               MC_CMD_SENSOR_INFO/MC_CMD_SENSOR_INFO_OUT/MASK */
#define	MC_CMD_SENSOR_SET_LIMS_IN_LOW0_OFST 4
#define	MC_CMD_SENSOR_SET_LIMS_IN_HI0_OFST 8
#define	MC_CMD_SENSOR_SET_LIMS_IN_LOW1_OFST 12
#define	MC_CMD_SENSOR_SET_LIMS_IN_HI1_OFST 16

/* MC_CMD_SENSOR_SET_LIMS_OUT msgresponse */
#define	MC_CMD_SENSOR_SET_LIMS_OUT_LEN 0


/***********************************/
/* MC_CMD_GET_RESOURCE_LIMITS 
 */
#define	MC_CMD_GET_RESOURCE_LIMITS  0x4f

/* MC_CMD_GET_RESOURCE_LIMITS_IN msgrequest */
#define	MC_CMD_GET_RESOURCE_LIMITS_IN_LEN 0

/* MC_CMD_GET_RESOURCE_LIMITS_OUT msgresponse */
#define	MC_CMD_GET_RESOURCE_LIMITS_OUT_LEN 16
#define	MC_CMD_GET_RESOURCE_LIMITS_OUT_BUFTBL_OFST 0
#define	MC_CMD_GET_RESOURCE_LIMITS_OUT_EVQ_OFST 4
#define	MC_CMD_GET_RESOURCE_LIMITS_OUT_RXQ_OFST 8
#define	MC_CMD_GET_RESOURCE_LIMITS_OUT_TXQ_OFST 12


/***********************************/
/* MC_CMD_CLP 
 * CLP support operations
 */
#define	MC_CMD_CLP  0x56

/* MC_CMD_CLP_IN msgrequest */
#define	MC_CMD_CLP_IN_LEN 4
#define	MC_CMD_CLP_IN_OP_OFST 0
#define	MC_CMD_CLP_OP_DEFAULT 0x1 /* enum */
#define	MC_CMD_CLP_OP_SET_MAC 0x2 /* enum */
#define	MC_CMD_CLP_OP_GET_MAC 0x3 /* enum */
#define	MC_CMD_CLP_OP_SET_BOOT 0x4 /* enum */
#define	MC_CMD_CLP_OP_GET_BOOT 0x5 /* enum */

/* MC_CMD_CLP_OUT msgresponse */
#define	MC_CMD_CLP_OUT_LEN 0

/* MC_CMD_CLP_IN_DEFAULT msgrequest */
#define	MC_CMD_CLP_IN_DEFAULT_LEN 4
/*            MC_CMD_CLP_IN_OP_OFST 0 */

/* MC_CMD_CLP_OUT_DEFAULT msgresponse */
#define	MC_CMD_CLP_OUT_DEFAULT_LEN 0

/* MC_CMD_CLP_IN_SET_MAC msgrequest */
#define	MC_CMD_CLP_IN_SET_MAC_LEN 12
/*            MC_CMD_CLP_IN_OP_OFST 0 */
#define	MC_CMD_CLP_IN_SET_MAC_ADDR_OFST 4
#define	MC_CMD_CLP_IN_SET_MAC_ADDR_LEN 6
#define	MC_CMD_CLP_IN_SET_MAC_RESERVED_OFST 10
#define	MC_CMD_CLP_IN_SET_MAC_RESERVED_LEN 2

/* MC_CMD_CLP_OUT_SET_MAC msgresponse */
#define	MC_CMD_CLP_OUT_SET_MAC_LEN 0

/* MC_CMD_CLP_IN_GET_MAC msgrequest */
#define	MC_CMD_CLP_IN_GET_MAC_LEN 4
/*            MC_CMD_CLP_IN_OP_OFST 0 */

/* MC_CMD_CLP_OUT_GET_MAC msgresponse */
#define	MC_CMD_CLP_OUT_GET_MAC_LEN 8
#define	MC_CMD_CLP_OUT_GET_MAC_ADDR_OFST 0
#define	MC_CMD_CLP_OUT_GET_MAC_ADDR_LEN 6
#define	MC_CMD_CLP_OUT_GET_MAC_RESERVED_OFST 6
#define	MC_CMD_CLP_OUT_GET_MAC_RESERVED_LEN 2

/* MC_CMD_CLP_IN_SET_BOOT msgrequest */
#define	MC_CMD_CLP_IN_SET_BOOT_LEN 5
/*            MC_CMD_CLP_IN_OP_OFST 0 */
#define	MC_CMD_CLP_IN_SET_BOOT_FLAG_OFST 4
#define	MC_CMD_CLP_IN_SET_BOOT_FLAG_LEN 1

/* MC_CMD_CLP_OUT_SET_BOOT msgresponse */
#define	MC_CMD_CLP_OUT_SET_BOOT_LEN 0

/* MC_CMD_CLP_IN_GET_BOOT msgrequest */
#define	MC_CMD_CLP_IN_GET_BOOT_LEN 4
/*            MC_CMD_CLP_IN_OP_OFST 0 */

/* MC_CMD_CLP_OUT_GET_BOOT msgresponse */
#define	MC_CMD_CLP_OUT_GET_BOOT_LEN 4
#define	MC_CMD_CLP_OUT_GET_BOOT_FLAG_OFST 0
#define	MC_CMD_CLP_OUT_GET_BOOT_FLAG_LEN 1
#define	MC_CMD_CLP_OUT_GET_BOOT_RESERVED_OFST 1
#define	MC_CMD_CLP_OUT_GET_BOOT_RESERVED_LEN 3

/* MC_CMD_RESOURCE_SPECIFIER enum */
#define	MC_CMD_RESOURCE_INSTANCE_ANY 0xffffffff /* enum */
#define	MC_CMD_RESOURCE_INSTANCE_NONE 0xfffffffe /* enum */


/***********************************/
/* MC_CMD_INIT_EVQ 
 */
#define	MC_CMD_INIT_EVQ  0x50

/* MC_CMD_INIT_EVQ_IN msgrequest */
#define	MC_CMD_INIT_EVQ_IN_LENMIN 36
#define	MC_CMD_INIT_EVQ_IN_LENMAX 540
#define	MC_CMD_INIT_EVQ_IN_LEN(num) (28+8*(num))
#define	MC_CMD_INIT_EVQ_IN_SIZE_OFST 0
#define	MC_CMD_INIT_EVQ_IN_INSTANCE_OFST 4
#define	MC_CMD_INIT_EVQ_IN_TMR_LOAD_OFST 8
#define	MC_CMD_INIT_EVQ_IN_TMR_RELOAD_OFST 12
#define	MC_CMD_INIT_EVQ_IN_FLAGS_OFST 16
#define	MC_CMD_INIT_EVQ_IN_FLAG_INTERRUPTING_LBN 0
#define	MC_CMD_INIT_EVQ_IN_FLAG_INTERRUPTING_WIDTH 1
#define	MC_CMD_INIT_EVQ_IN_FLAG_RPTR_DOS_LBN 1
#define	MC_CMD_INIT_EVQ_IN_FLAG_RPTR_DOS_WIDTH 1
#define	MC_CMD_INIT_EVQ_IN_TMR_MODE_OFST 20
#define	MC_CMD_INIT_EVQ_IN_TMR_MODE_DIS 0x0 /* enum */
#define	MC_CMD_INIT_EVQ_IN_TMR_IMMED_START 0x1 /* enum */
#define	MC_CMD_INIT_EVQ_IN_TMR_TRIG_START 0x2 /* enum */
#define	MC_CMD_INIT_EVQ_IN_TMR_INT_HLDOFF 0x3 /* enum */
#define	MC_CMD_INIT_EVQ_IN_TARGET_EVQ_OFST 24
#define	MC_CMD_INIT_EVQ_IN_IRQ_NUM_OFST 24
#define	MC_CMD_INIT_EVQ_IN_DMA_ADDR_OFST 28
#define	MC_CMD_INIT_EVQ_IN_DMA_ADDR_LEN 8
#define	MC_CMD_INIT_EVQ_IN_DMA_ADDR_LO_OFST 28
#define	MC_CMD_INIT_EVQ_IN_DMA_ADDR_HI_OFST 32
#define	MC_CMD_INIT_EVQ_IN_DMA_ADDR_MINNUM 1
#define	MC_CMD_INIT_EVQ_IN_DMA_ADDR_MAXNUM 64

/* MC_CMD_INIT_EVQ_OUT msgresponse */
#define	MC_CMD_INIT_EVQ_OUT_LEN 4
#define	MC_CMD_INIT_EVQ_OUT_IRQ_OFST 0

/* QUEUE_CRC_MODE structuredef */
#define	QUEUE_CRC_MODE_LEN 1
#define	QUEUE_CRC_MODE_MODE_LBN 0
#define	QUEUE_CRC_MODE_MODE_WIDTH 4
#define	QUEUE_CRC_MODE_NONE  0x0 /* enum */
#define	QUEUE_CRC_MODE_FCOE  0x1 /* enum */
#define	QUEUE_CRC_MODE_ISCSI_HDR  0x2 /* enum */
#define	QUEUE_CRC_MODE_ISCSI  0x3 /* enum */
#define	QUEUE_CRC_MODE_FCOIPOE  0x4 /* enum */
#define	QUEUE_CRC_MODE_MPA  0x5 /* enum */
#define	QUEUE_CRC_MODE_SPARE_LBN 4
#define	QUEUE_CRC_MODE_SPARE_WIDTH 4


/***********************************/
/* MC_CMD_INIT_RXQ 
 */
#define	MC_CMD_INIT_RXQ  0x51

/* MC_CMD_INIT_RXQ_IN msgrequest */
#define	MC_CMD_INIT_RXQ_IN_LENMIN 32
#define	MC_CMD_INIT_RXQ_IN_LENMAX 248
#define	MC_CMD_INIT_RXQ_IN_LEN(num) (24+8*(num))
#define	MC_CMD_INIT_RXQ_IN_SIZE_OFST 0
#define	MC_CMD_INIT_RXQ_IN_TARGET_EVQ_OFST 4
#define	MC_CMD_INIT_RXQ_IN_LABEL_OFST 8
#define	MC_CMD_INIT_RXQ_IN_INSTANCE_OFST 12
#define	MC_CMD_INIT_RXQ_IN_FLAGS_OFST 16
#define	MC_CMD_INIT_RXQ_IN_FLAG_BUFF_MODE_LBN 0
#define	MC_CMD_INIT_RXQ_IN_FLAG_BUFF_MODE_WIDTH 1
#define	MC_CMD_INIT_RXQ_IN_FLAG_HDR_SPLIT_LBN 1
#define	MC_CMD_INIT_RXQ_IN_FLAG_HDR_SPLIT_WIDTH 1
#define	MC_CMD_INIT_RXQ_IN_FLAG_PKT_EDIT_LBN 2
#define	MC_CMD_INIT_RXQ_IN_FLAG_PKT_EDIT_WIDTH 1
#define	MC_CMD_INIT_RXQ_IN_CRC_MODE_LBN 3
#define	MC_CMD_INIT_RXQ_IN_CRC_MODE_WIDTH 4
#define	MC_CMD_INIT_RXQ_IN_OWNER_ID_OFST 20
#define	MC_CMD_INIT_RXQ_IN_DMA_ADDR_OFST 24
#define	MC_CMD_INIT_RXQ_IN_DMA_ADDR_LEN 8
#define	MC_CMD_INIT_RXQ_IN_DMA_ADDR_LO_OFST 24
#define	MC_CMD_INIT_RXQ_IN_DMA_ADDR_HI_OFST 28
#define	MC_CMD_INIT_RXQ_IN_DMA_ADDR_MINNUM 1
#define	MC_CMD_INIT_RXQ_IN_DMA_ADDR_MAXNUM 28

/* MC_CMD_INIT_RXQ_OUT msgresponse */
#define	MC_CMD_INIT_RXQ_OUT_LEN 0


/***********************************/
/* MC_CMD_INIT_TXQ 
 */
#define	MC_CMD_INIT_TXQ  0x52

/* MC_CMD_INIT_TXQ_IN msgrequest */
#define	MC_CMD_INIT_TXQ_IN_LENMIN 32
#define	MC_CMD_INIT_TXQ_IN_LENMAX 248
#define	MC_CMD_INIT_TXQ_IN_LEN(num) (24+8*(num))
#define	MC_CMD_INIT_TXQ_IN_SIZE_OFST 0
#define	MC_CMD_INIT_TXQ_IN_TARGET_EVQ_OFST 4
#define	MC_CMD_INIT_TXQ_IN_LABEL_OFST 8
#define	MC_CMD_INIT_TXQ_IN_INSTANCE_OFST 12
#define	MC_CMD_INIT_TXQ_IN_FLAGS_OFST 16
#define	MC_CMD_INIT_TXQ_IN_FLAG_BUFF_MODE_LBN 0
#define	MC_CMD_INIT_TXQ_IN_FLAG_BUFF_MODE_WIDTH 1
#define	MC_CMD_INIT_TXQ_IN_FLAG_IP_CSUM_DIS_LBN 1
#define	MC_CMD_INIT_TXQ_IN_FLAG_IP_CSUM_DIS_WIDTH 1
#define	MC_CMD_INIT_TXQ_IN_FLAG_TCP_CSUM_DIS_LBN 2
#define	MC_CMD_INIT_TXQ_IN_FLAG_TCP_CSUM_DIS_WIDTH 1
#define	MC_CMD_INIT_TXQ_IN_FLAG_TCP_UDP_ONLY_LBN 3
#define	MC_CMD_INIT_TXQ_IN_FLAG_TCP_UDP_ONLY_WIDTH 1
#define	MC_CMD_INIT_TXQ_IN_CRC_MODE_LBN 4
#define	MC_CMD_INIT_TXQ_IN_CRC_MODE_WIDTH 4
#define	MC_CMD_INIT_TXQ_IN_OWNER_ID_OFST 20
#define	MC_CMD_INIT_TXQ_IN_DMA_ADDR_OFST 24
#define	MC_CMD_INIT_TXQ_IN_DMA_ADDR_LEN 8
#define	MC_CMD_INIT_TXQ_IN_DMA_ADDR_LO_OFST 24
#define	MC_CMD_INIT_TXQ_IN_DMA_ADDR_HI_OFST 28
#define	MC_CMD_INIT_TXQ_IN_DMA_ADDR_MINNUM 1
#define	MC_CMD_INIT_TXQ_IN_DMA_ADDR_MAXNUM 28

/* MC_CMD_INIT_TXQ_OUT msgresponse */
#define	MC_CMD_INIT_TXQ_OUT_LEN 0


/***********************************/
/* MC_CMD_FINI_EVQ 
 */
#define	MC_CMD_FINI_EVQ  0x55

/* MC_CMD_FINI_EVQ_IN msgrequest */
#define	MC_CMD_FINI_EVQ_IN_LEN 4
#define	MC_CMD_FINI_EVQ_IN_INSTANCE_OFST 0

/* MC_CMD_FINI_EVQ_OUT msgresponse */
#define	MC_CMD_FINI_EVQ_OUT_LEN 0


/***********************************/
/* MC_CMD_FINI_RXQ 
 */
#define	MC_CMD_FINI_RXQ  0x56

/* MC_CMD_FINI_RXQ_IN msgrequest */
#define	MC_CMD_FINI_RXQ_IN_LEN 4
#define	MC_CMD_FINI_RXQ_IN_INSTANCE_OFST 0

/* MC_CMD_FINI_RXQ_OUT msgresponse */
#define	MC_CMD_FINI_RXQ_OUT_LEN 0


/***********************************/
/* MC_CMD_FINI_TXQ 
 */
#define	MC_CMD_FINI_TXQ  0x57

/* MC_CMD_FINI_TXQ_IN msgrequest */
#define	MC_CMD_FINI_TXQ_IN_LEN 4
#define	MC_CMD_FINI_TXQ_IN_INSTANCE_OFST 0

/* MC_CMD_FINI_TXQ_OUT msgresponse */
#define	MC_CMD_FINI_TXQ_OUT_LEN 0


/***********************************/
/* MC_CMD_DRIVER_EVENT 
 */
#define	MC_CMD_DRIVER_EVENT  0x5a

/* MC_CMD_DRIVER_EVENT_IN msgrequest */
#define	MC_CMD_DRIVER_EVENT_IN_LEN 12
#define	MC_CMD_DRIVER_EVENT_IN_EVQ_OFST 0
#define	MC_CMD_DRIVER_EVENT_IN_DATA_OFST 4
#define	MC_CMD_DRIVER_EVENT_IN_DATA_LEN 8
#define	MC_CMD_DRIVER_EVENT_IN_DATA_LO_OFST 4
#define	MC_CMD_DRIVER_EVENT_IN_DATA_HI_OFST 8


/***********************************/
/* MC_CMD_PROXY_CMD 
 */
#define	MC_CMD_PROXY_CMD  0x5b

/* MC_CMD_PROXY_CMD_IN msgrequest */
#define	MC_CMD_PROXY_CMD_IN_LEN 4
#define	MC_CMD_PROXY_CMD_IN_TARGET_OFST 0


/***********************************/
/* MC_CMD_ALLOC_OWNER_IDS 
 */
#define	MC_CMD_ALLOC_OWNER_IDS  0x54

/* MC_CMD_ALLOC_OWNER_IDS_IN msgrequest */
#define	MC_CMD_ALLOC_OWNER_IDS_IN_LEN 4
#define	MC_CMD_ALLOC_OWNER_IDS_IN_NIDS_OFST 0

/* MC_CMD_ALLOC_OWNER_IDS_OUT msgresponse */
#define	MC_CMD_ALLOC_OWNER_IDS_OUT_LEN 12
#define	MC_CMD_ALLOC_OWNER_IDS_OUT_HANDLE_OFST 0
#define	MC_CMD_ALLOC_OWNER_IDS_OUT_NIDS_OFST 4
#define	MC_CMD_ALLOC_OWNER_IDS_OUT_BASE_OFST 8


/***********************************/
/* MC_CMD_FREE_OWNER_IDS 
 */
#define	MC_CMD_FREE_OWNER_IDS  0x59

/* MC_CMD_FREE_OWNER_IDS_IN msgrequest */
#define	MC_CMD_FREE_OWNER_IDS_IN_LEN 4
#define	MC_CMD_FREE_OWNER_IDS_IN_HANDLE_OFST 0

/* MC_CMD_FREE_OWNER_IDS_OUT msgresponse */
#define	MC_CMD_FREE_OWNER_IDS_OUT_LEN 0


/***********************************/
/* MC_CMD_ALLOC_BUFTBL_CHUNK 
 */
#define	MC_CMD_ALLOC_BUFTBL_CHUNK  0x5c

/* MC_CMD_ALLOC_BUFTBL_CHUNK_IN msgrequest */
#define	MC_CMD_ALLOC_BUFTBL_CHUNK_IN_LEN 8
#define	MC_CMD_ALLOC_BUFTBL_CHUNK_IN_OWNER_OFST 0
#define	MC_CMD_ALLOC_BUFTBL_CHUNK_IN_PAGE_SIZE_OFST 4

/* MC_CMD_ALLOC_BUFTBL_CHUNK_OUT msgresponse */
#define	MC_CMD_ALLOC_BUFTBL_CHUNK_OUT_LEN 12
#define	MC_CMD_ALLOC_BUFTBL_CHUNK_OUT_HANDLE_OFST 0
#define	MC_CMD_ALLOC_BUFTBL_CHUNK_OUT_NUMENTRIES_OFST 4
#define	MC_CMD_ALLOC_BUFTBL_CHUNK_OUT_ID_OFST 8


/***********************************/
/* MC_CMD_PROGRAM_BUFTBL_ENTRIES 
 */
#define	MC_CMD_PROGRAM_BUFTBL_ENTRIES  0x5d

/* MC_CMD_PROGRAM_BUFTBL_ENTRIES_IN msgrequest */
#define	MC_CMD_PROGRAM_BUFTBL_ENTRIES_IN_LENMIN 20
#define	MC_CMD_PROGRAM_BUFTBL_ENTRIES_IN_LENMAX 252
#define	MC_CMD_PROGRAM_BUFTBL_ENTRIES_IN_LEN(num) (12+8*(num))
#define	MC_CMD_PROGRAM_BUFTBL_ENTRIES_IN_HANDLE_OFST 0
#define	MC_CMD_PROGRAM_BUFTBL_ENTRIES_IN_FIRSTID_OFST 4
#define	MC_CMD_PROGRAM_BUFTBL_ENTRIES_IN_NUMENTRIES_OFST 8
#define	MC_CMD_PROGRAM_BUFTBL_ENTRIES_IN_ENTRY_OFST 12
#define	MC_CMD_PROGRAM_BUFTBL_ENTRIES_IN_ENTRY_LEN 8
#define	MC_CMD_PROGRAM_BUFTBL_ENTRIES_IN_ENTRY_LO_OFST 12
#define	MC_CMD_PROGRAM_BUFTBL_ENTRIES_IN_ENTRY_HI_OFST 16
#define	MC_CMD_PROGRAM_BUFTBL_ENTRIES_IN_ENTRY_MINNUM 1
#define	MC_CMD_PROGRAM_BUFTBL_ENTRIES_IN_ENTRY_MAXNUM 30

/* MC_CMD_PROGRAM_BUFTBL_ENTRIES_OUT msgresponse */
#define	MC_CMD_PROGRAM_BUFTBL_ENTRIES_OUT_LEN 0


/***********************************/
/* MC_CMD_FREE_BUFTBL_CHUNK 
 */
#define	MC_CMD_FREE_BUFTBL_CHUNK  0x5e

/* MC_CMD_FREE_BUFTBL_CHUNK_IN msgrequest */
#define	MC_CMD_FREE_BUFTBL_CHUNK_IN_LEN 4
#define	MC_CMD_FREE_BUFTBL_CHUNK_IN_HANDLE_OFST 0

/* MC_CMD_FREE_BUFTBL_CHUNK_OUT msgresponse */
#define	MC_CMD_FREE_BUFTBL_CHUNK_OUT_LEN 0


/***********************************/
/* MC_CMD_GET_PF_COUNT 
 */
#define	MC_CMD_GET_PF_COUNT  0x60

/* MC_CMD_GET_PF_COUNT_IN msgrequest */
#define	MC_CMD_GET_PF_COUNT_IN_LEN 0

/* MC_CMD_GET_PF_COUNT_OUT msgresponse */
#define	MC_CMD_GET_PF_COUNT_OUT_LEN 1
#define	MC_CMD_GET_PF_COUNT_OUT_PF_COUNT_OFST 0
#define	MC_CMD_GET_PF_COUNT_OUT_PF_COUNT_LEN 1


/***********************************/
/* MC_CMD_FILTER_OP 
 */
#define	MC_CMD_FILTER_OP  0x61

/* MC_CMD_FILTER_OP_IN msgrequest */
#define	MC_CMD_FILTER_OP_IN_LEN 100
#define	MC_CMD_FILTER_OP_IN_OP_OFST 0
#define	MC_CMD_FILTER_OP_IN_OP_INSERT  0x0 /* enum */
#define	MC_CMD_FILTER_OP_IN_OP_REMOVE  0x1 /* enum */
#define	MC_CMD_FILTER_OP_IN_OP_SUBSCRIBE  0x2 /* enum */
#define	MC_CMD_FILTER_OP_IN_OP_UNSUBSCRIBE  0x3 /* enum */
#define	MC_CMD_FILTER_OP_IN_HANDLE_OFST 4
#define	MC_CMD_FILTER_OP_IN_MATCH_FIELDS_OFST 8
#define	MC_CMD_FILTER_OP_IN_MATCH_SRC_IP_LBN 0
#define	MC_CMD_FILTER_OP_IN_MATCH_SRC_IP_WIDTH 1
#define	MC_CMD_FILTER_OP_IN_MATCH_DST_IP_LBN 1
#define	MC_CMD_FILTER_OP_IN_MATCH_DST_IP_WIDTH 1
#define	MC_CMD_FILTER_OP_IN_MATCH_SRC_MAC_LBN 2
#define	MC_CMD_FILTER_OP_IN_MATCH_SRC_MAC_WIDTH 1
#define	MC_CMD_FILTER_OP_IN_MATCH_SRC_PORT_LBN 3
#define	MC_CMD_FILTER_OP_IN_MATCH_SRC_PORT_WIDTH 1
#define	MC_CMD_FILTER_OP_IN_MATCH_DST_MAC_LBN 4
#define	MC_CMD_FILTER_OP_IN_MATCH_DST_MAC_WIDTH 1
#define	MC_CMD_FILTER_OP_IN_MATCH_DST_PORT_LBN 5
#define	MC_CMD_FILTER_OP_IN_MATCH_DST_PORT_WIDTH 1
#define	MC_CMD_FILTER_OP_IN_MATCH_ETHER_TYPE_LBN 6
#define	MC_CMD_FILTER_OP_IN_MATCH_ETHER_TYPE_WIDTH 1
#define	MC_CMD_FILTER_OP_IN_MATCH_INNER_VLAN_LBN 7
#define	MC_CMD_FILTER_OP_IN_MATCH_INNER_VLAN_WIDTH 1
#define	MC_CMD_FILTER_OP_IN_MATCH_OUTER_VLAN_LBN 8
#define	MC_CMD_FILTER_OP_IN_MATCH_OUTER_VLAN_WIDTH 1
#define	MC_CMD_FILTER_OP_IN_MATCH_IP_PROTO_LBN 9
#define	MC_CMD_FILTER_OP_IN_MATCH_IP_PROTO_WIDTH 1
#define	MC_CMD_FILTER_OP_IN_MATCH_FWDEF0_LBN 10
#define	MC_CMD_FILTER_OP_IN_MATCH_FWDEF0_WIDTH 1
#define	MC_CMD_FILTER_OP_IN_MATCH_FWDEF1_LBN 11
#define	MC_CMD_FILTER_OP_IN_MATCH_FWDEF1_WIDTH 1
#define	MC_CMD_FILTER_OP_IN_RX_DEST_OFST 12
#define	MC_CMD_FILTER_OP_IN_RX_DEST_DROP  0x0 /* enum */
#define	MC_CMD_FILTER_OP_IN_RX_DEST_HOST  0x1 /* enum */
#define	MC_CMD_FILTER_OP_IN_RX_DEST_MC  0x2 /* enum */
#define	MC_CMD_FILTER_OP_IN_RX_DEST_TX0  0x3 /* enum */
#define	MC_CMD_FILTER_OP_IN_RX_DEST_TX1  0x4 /* enum */
#define	MC_CMD_FILTER_OP_IN_RX_QUEUE_OFST 16
#define	MC_CMD_FILTER_OP_IN_RX_FLAGS_OFST 20
#define	MC_CMD_FILTER_OP_IN_RX_FLAG_RSS_LBN 0
#define	MC_CMD_FILTER_OP_IN_RX_FLAG_RSS_WIDTH 1
#define	MC_CMD_FILTER_OP_IN_RSS_CONTEXT_OFST 24
#define	MC_CMD_FILTER_OP_IN_TX_DOMAIN_OFST 28
#define	MC_CMD_FILTER_OP_IN_TX_DEST_OFST 32
#define	MC_CMD_FILTER_OP_IN_TX_DEST_MAC_LBN 0
#define	MC_CMD_FILTER_OP_IN_TX_DEST_MAC_WIDTH 1
#define	MC_CMD_FILTER_OP_IN_TX_DEST_PM_LBN 1
#define	MC_CMD_FILTER_OP_IN_TX_DEST_PM_WIDTH 1
#define	MC_CMD_FILTER_OP_IN_SRC_MAC_OFST 36
#define	MC_CMD_FILTER_OP_IN_SRC_MAC_LEN 6
#define	MC_CMD_FILTER_OP_IN_SRC_PORT_OFST 42
#define	MC_CMD_FILTER_OP_IN_SRC_PORT_LEN 2
#define	MC_CMD_FILTER_OP_IN_DST_MAC_OFST 44
#define	MC_CMD_FILTER_OP_IN_DST_MAC_LEN 6
#define	MC_CMD_FILTER_OP_IN_DST_PORT_OFST 50
#define	MC_CMD_FILTER_OP_IN_DST_PORT_LEN 2
#define	MC_CMD_FILTER_OP_IN_ETHER_TYPE_OFST 52
#define	MC_CMD_FILTER_OP_IN_ETHER_TYPE_LEN 2
#define	MC_CMD_FILTER_OP_IN_INNER_VLAN_OFST 54
#define	MC_CMD_FILTER_OP_IN_INNER_VLAN_LEN 2
#define	MC_CMD_FILTER_OP_IN_OUTER_VLAN_OFST 56
#define	MC_CMD_FILTER_OP_IN_OUTER_VLAN_LEN 2
#define	MC_CMD_FILTER_OP_IN_IP_PROTO_OFST 58
#define	MC_CMD_FILTER_OP_IN_IP_PROTO_LEN 2
#define	MC_CMD_FILTER_OP_IN_FWDEF0_OFST 60
#define	MC_CMD_FILTER_OP_IN_FWDEF1_OFST 64
#define	MC_CMD_FILTER_OP_IN_SRC_IP_OFST 68
#define	MC_CMD_FILTER_OP_IN_SRC_IP_LEN 16
#define	MC_CMD_FILTER_OP_IN_DST_IP_OFST 84
#define	MC_CMD_FILTER_OP_IN_DST_IP_LEN 16

/* MC_CMD_FILTER_OP_OUT msgresponse */
#define	MC_CMD_FILTER_OP_OUT_LEN 8
#define	MC_CMD_FILTER_OP_OUT_OP_OFST 0
#define	MC_CMD_FILTER_OP_OUT_OP_INSERT  0x0 /* enum */
#define	MC_CMD_FILTER_OP_OUT_OP_REMOVE  0x1 /* enum */
#define	MC_CMD_FILTER_OP_OUT_OP_SUBSCRIBE  0x2 /* enum */
#define	MC_CMD_FILTER_OP_OUT_OP_UNSUBSCRIBE  0x3 /* enum */
#define	MC_CMD_FILTER_OP_OUT_HANDLE_OFST 4


/***********************************/
/* MC_CMD_SET_PF_COUNT 
 */
#define	MC_CMD_SET_PF_COUNT  0x62

/* MC_CMD_SET_PF_COUNT_IN msgrequest */
#define	MC_CMD_SET_PF_COUNT_IN_LEN 4
#define	MC_CMD_SET_PF_COUNT_IN_PF_COUNT_OFST 0

/* MC_CMD_SET_PF_COUNT_OUT msgresponse */
#define	MC_CMD_SET_PF_COUNT_OUT_LEN 0


/***********************************/
/* MC_CMD_GET_PORT_ASSIGNMENT 
 */
#define	MC_CMD_GET_PORT_ASSIGNMENT  0x63

/* MC_CMD_GET_PORT_ASSIGNMENT_IN msgrequest */
#define	MC_CMD_GET_PORT_ASSIGNMENT_IN_LEN 0

/* MC_CMD_GET_PORT_ASSIGNMENT_OUT msgresponse */
#define	MC_CMD_GET_PORT_ASSIGNMENT_OUT_LEN 4
#define	MC_CMD_GET_PORT_ASSIGNMENT_OUT_PORT_OFST 0


/***********************************/
/* MC_CMD_SET_PORT_ASSIGNMENT 
 */
#define	MC_CMD_SET_PORT_ASSIGNMENT  0x64

/* MC_CMD_SET_PORT_ASSIGNMENT_IN msgrequest */
#define	MC_CMD_SET_PORT_ASSIGNMENT_IN_LEN 4
#define	MC_CMD_SET_PORT_ASSIGNMENT_IN_PORT_OFST 0

/* MC_CMD_SET_PORT_ASSIGNMENT_OUT msgresponse */
#define	MC_CMD_SET_PORT_ASSIGNMENT_OUT_LEN 0


/***********************************/
/* MC_CMD_ALLOC_VIS 
 */
#define	MC_CMD_ALLOC_VIS  0x65

/* MC_CMD_ALLOC_VIS_IN msgrequest */
#define	MC_CMD_ALLOC_VIS_IN_LEN 4
#define	MC_CMD_ALLOC_VIS_IN_VI_COUNT_OFST 0

/* MC_CMD_ALLOC_VIS_OUT msgresponse */
#define	MC_CMD_ALLOC_VIS_OUT_LEN 0


/***********************************/
/* MC_CMD_FREE_VIS 
 */
#define	MC_CMD_FREE_VIS  0x66

/* MC_CMD_FREE_VIS_IN msgrequest */
#define	MC_CMD_FREE_VIS_IN_LEN 0

/* MC_CMD_FREE_VIS_OUT msgresponse */
#define	MC_CMD_FREE_VIS_OUT_LEN 0


/***********************************/
/* MC_CMD_GET_SRIOV_CFG 
 */
#define	MC_CMD_GET_SRIOV_CFG  0x67

/* MC_CMD_GET_SRIOV_CFG_IN msgrequest */
#define	MC_CMD_GET_SRIOV_CFG_IN_LEN 0

/* MC_CMD_GET_SRIOV_CFG_OUT msgresponse */
#define	MC_CMD_GET_SRIOV_CFG_OUT_LEN 20
#define	MC_CMD_GET_SRIOV_CFG_OUT_VF_CURRENT_OFST 0
#define	MC_CMD_GET_SRIOV_CFG_OUT_VF_MAX_OFST 4
#define	MC_CMD_GET_SRIOV_CFG_OUT_FLAGS_OFST 8
#define	MC_CMD_GET_SRIOV_CFG_OUT_VF_ENABLED_LBN 0
#define	MC_CMD_GET_SRIOV_CFG_OUT_VF_ENABLED_WIDTH 1
#define	MC_CMD_GET_SRIOV_CFG_OUT_VF_OFFSET_OFST 12
#define	MC_CMD_GET_SRIOV_CFG_OUT_VF_STRIDE_OFST 16


/***********************************/
/* MC_CMD_SET_SRIOV_CFG 
 */
#define	MC_CMD_SET_SRIOV_CFG  0x68

/* MC_CMD_SET_SRIOV_CFG_IN msgrequest */
#define	MC_CMD_SET_SRIOV_CFG_IN_LEN 20
#define	MC_CMD_SET_SRIOV_CFG_IN_VF_CURRENT_OFST 0
#define	MC_CMD_SET_SRIOV_CFG_IN_VF_MAX_OFST 4
#define	MC_CMD_SET_SRIOV_CFG_IN_FLAGS_OFST 8
#define	MC_CMD_SET_SRIOV_CFG_IN_VF_ENABLED_LBN 0
#define	MC_CMD_SET_SRIOV_CFG_IN_VF_ENABLED_WIDTH 1
#define	MC_CMD_SET_SRIOV_CFG_IN_VF_OFFSET_OFST 12
#define	MC_CMD_SET_SRIOV_CFG_IN_VF_STRIDE_OFST 16

/* MC_CMD_SET_SRIOV_CFG_OUT msgresponse */
#define	MC_CMD_SET_SRIOV_CFG_OUT_LEN 0


/***********************************/
/* MC_CMD_GET_VI_COUNT 
 */
#define	MC_CMD_GET_VI_COUNT  0x69

/* MC_CMD_GET_VI_COUNT_IN msgrequest */
#define	MC_CMD_GET_VI_COUNT_IN_LEN 0

/* MC_CMD_GET_VI_COUNT_OUT msgresponse */
#define	MC_CMD_GET_VI_COUNT_OUT_LEN 4
#define	MC_CMD_GET_VI_COUNT_OUT_VI_COUNT_OFST 0


/***********************************/
/* MC_CMD_GET_VECTOR_CFG 
 */
#define	MC_CMD_GET_VECTOR_CFG  0x70

/* MC_CMD_GET_VECTOR_CFG_IN msgrequest */
#define	MC_CMD_GET_VECTOR_CFG_IN_LEN 0

/* MC_CMD_GET_VECTOR_CFG_OUT msgresponse */
#define	MC_CMD_GET_VECTOR_CFG_OUT_LEN 12
#define	MC_CMD_GET_VECTOR_CFG_OUT_VEC_BASE_OFST 0
#define	MC_CMD_GET_VECTOR_CFG_OUT_VECS_PER_PF_OFST 4
#define	MC_CMD_GET_VECTOR_CFG_OUT_VECS_PER_VF_OFST 8


/***********************************/
/* MC_CMD_SET_VECTOR_CFG 
 */
#define	MC_CMD_SET_VECTOR_CFG  0x71

/* MC_CMD_SET_VECTOR_CFG_IN msgrequest */
#define	MC_CMD_SET_VECTOR_CFG_IN_LEN 12
#define	MC_CMD_SET_VECTOR_CFG_IN_VEC_BASE_OFST 0
#define	MC_CMD_SET_VECTOR_CFG_IN_VECS_PER_PF_OFST 4
#define	MC_CMD_SET_VECTOR_CFG_IN_VECS_PER_VF_OFST 8

/* MC_CMD_SET_VECTOR_CFG_OUT msgresponse */
#define	MC_CMD_SET_VECTOR_CFG_OUT_LEN 0


/***********************************/
/* MC_CMD_ALLOC_PIOBUF 
 */
#define	MC_CMD_ALLOC_PIOBUF  0x72

/* MC_CMD_ALLOC_PIOBUF_IN msgrequest */
#define	MC_CMD_ALLOC_PIOBUF_IN_LEN 0

/* MC_CMD_ALLOC_PIOBUF_OUT msgresponse */
#define	MC_CMD_ALLOC_PIOBUF_OUT_LEN 4
#define	MC_CMD_ALLOC_PIOBUF_OUT_PIOBUF_HANDLE_OFST 0


/***********************************/
/* MC_CMD_FREE_PIOBUF 
 */
#define	MC_CMD_FREE_PIOBUF  0x73

/* MC_CMD_FREE_PIOBUF_IN msgrequest */
#define	MC_CMD_FREE_PIOBUF_IN_LEN 4
#define	MC_CMD_FREE_PIOBUF_IN_PIOBUF_HANDLE_OFST 0

/* MC_CMD_FREE_PIOBUF_OUT msgresponse */
#define	MC_CMD_FREE_PIOBUF_OUT_LEN 0


/***********************************/
/* MC_CMD_V2_EXTN 
 */
#define	MC_CMD_V2_EXTN  0x7f

/* MC_CMD_V2_EXTN_IN msgrequest */
#define	MC_CMD_V2_EXTN_IN_LEN 4
#define	MC_CMD_V2_EXTN_IN_EXTENDED_CMD_LBN 0
#define	MC_CMD_V2_EXTN_IN_EXTENDED_CMD_WIDTH 15
#define	MC_CMD_V2_EXTN_IN_UNUSED_LBN 15
#define	MC_CMD_V2_EXTN_IN_UNUSED_WIDTH 1
#define	MC_CMD_V2_EXTN_IN_ACTUAL_LEN_LBN 16
#define	MC_CMD_V2_EXTN_IN_ACTUAL_LEN_WIDTH 10
#define	MC_CMD_V2_EXTN_IN_UNUSED2_LBN 26
#define	MC_CMD_V2_EXTN_IN_UNUSED2_WIDTH 6


/***********************************/
/* MC_CMD_TCM_BUCKET_ALLOC 
 */
#define	MC_CMD_TCM_BUCKET_ALLOC  0x80

/* MC_CMD_TCM_BUCKET_ALLOC_IN msgrequest */
#define	MC_CMD_TCM_BUCKET_ALLOC_IN_LEN 0

/* MC_CMD_TCM_BUCKET_ALLOC_OUT msgresponse */
#define	MC_CMD_TCM_BUCKET_ALLOC_OUT_LEN 4
#define	MC_CMD_TCM_BUCKET_ALLOC_OUT_BUCKET_OFST 0


/***********************************/
/* MC_CMD_TCM_BUCKET_FREE 
 */
#define	MC_CMD_TCM_BUCKET_FREE  0x81

/* MC_CMD_TCM_BUCKET_FREE_IN msgrequest */
#define	MC_CMD_TCM_BUCKET_FREE_IN_LEN 4
#define	MC_CMD_TCM_BUCKET_FREE_IN_BUCKET_OFST 0

/* MC_CMD_TCM_BUCKET_FREE_OUT msgresponse */
#define	MC_CMD_TCM_BUCKET_FREE_OUT_LEN 0


/***********************************/
/* MC_CMD_TCM_BUCKET_INIT 
 */
#define	MC_CMD_TCM_BUCKET_INIT  0x82

/* MC_CMD_TCM_BUCKET_INIT_IN msgrequest */
#define	MC_CMD_TCM_BUCKET_INIT_IN_LEN 8
#define	MC_CMD_TCM_BUCKET_INIT_IN_BUCKET_OFST 0
#define	MC_CMD_TCM_BUCKET_INIT_IN_RATE_OFST 4

/* MC_CMD_TCM_BUCKET_INIT_OUT msgresponse */
#define	MC_CMD_TCM_BUCKET_INIT_OUT_LEN 0


/***********************************/
/* MC_CMD_TCM_TXQ_INIT 
 */
#define	MC_CMD_TCM_TXQ_INIT  0x83

/* MC_CMD_TCM_TXQ_INIT_IN msgrequest */
#define	MC_CMD_TCM_TXQ_INIT_IN_LEN 28
#define	MC_CMD_TCM_TXQ_INIT_IN_QID_OFST 0
#define	MC_CMD_TCM_TXQ_INIT_IN_LABEL_OFST 4
#define	MC_CMD_TCM_TXQ_INIT_IN_PQ_FLAGS_OFST 8
#define	MC_CMD_TCM_TXQ_INIT_IN_RP_BKT_OFST 12
#define	MC_CMD_TCM_TXQ_INIT_IN_MAX_BKT1_OFST 16
#define	MC_CMD_TCM_TXQ_INIT_IN_MAX_BKT2_OFST 20
#define	MC_CMD_TCM_TXQ_INIT_IN_MIN_BKT_OFST 24

/* MC_CMD_TCM_TXQ_INIT_OUT msgresponse */
#define	MC_CMD_TCM_TXQ_INIT_OUT_LEN 0

#endif /* _SIENA_MC_DRIVER_PCOL_H */
