/*-
 * Copyright (c) 2003-2009 RMI Corporation
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
 * 3. Neither the name of RMI Corporation, nor the names of its contributors,
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
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
 * RMI_BSD */
#ifndef _RMI_MSGRING_H_
#define _RMI_MSGRING_H_

#include <mips/rmi/xlrconfig.h>

#define MSGRNG_TX_BUF_REG 0
#define MSGRNG_RX_BUF_REG 1

#define MSGRNG_MSG_STATUS_REG 2
#define MSGRNG_MSG_CONFIG_REG 3

#define MSGRNG_MSG_BUCKSIZE_REG 4

#define MSGRNG_CC_0_REG  16
#define MSGRNG_CC_1_REG  17
#define MSGRNG_CC_2_REG  18
#define MSGRNG_CC_3_REG  19
#define MSGRNG_CC_4_REG  20
#define MSGRNG_CC_5_REG  21
#define MSGRNG_CC_6_REG  22
#define MSGRNG_CC_7_REG  23
#define MSGRNG_CC_8_REG  24
#define MSGRNG_CC_9_REG  25
#define MSGRNG_CC_10_REG 26
#define MSGRNG_CC_11_REG 27
#define MSGRNG_CC_12_REG 28
#define MSGRNG_CC_13_REG 29
#define MSGRNG_CC_14_REG 30
#define MSGRNG_CC_15_REG 31

#define msgrng_read_status() read_c2_register32(MSGRNG_MSG_STATUS_REG, 0)

#define msgrng_read_config() read_c2_register32(MSGRNG_MSG_CONFIG_REG, 0)
#define msgrng_write_config(value) write_c2_register32(MSGRNG_MSG_CONFIG_REG, 0, value)

#define msgrng_read_bucksize(bucket) read_c2_register32(MSGRNG_MSG_BUCKSIZE_REG, bucket)
#define msgrng_write_bucksize(bucket, value) write_c2_register32(MSGRNG_MSG_BUCKSIZE_REG, bucket, value)

#define msgrng_read_cc(reg, pri) read_c2_register32(reg, pri)
#define msgrng_write_cc(reg, value, pri) write_c2_register32(reg, pri, value)

#define msgrng_load_rx_msg0() read_c2_register64(MSGRNG_RX_BUF_REG, 0)
#define msgrng_load_rx_msg1() read_c2_register64(MSGRNG_RX_BUF_REG, 1)
#define msgrng_load_rx_msg2() read_c2_register64(MSGRNG_RX_BUF_REG, 2)
#define msgrng_load_rx_msg3() read_c2_register64(MSGRNG_RX_BUF_REG, 3)

#define msgrng_load_tx_msg0(value) write_c2_register64(MSGRNG_TX_BUF_REG, 0, value)
#define msgrng_load_tx_msg1(value) write_c2_register64(MSGRNG_TX_BUF_REG, 1, value)
#define msgrng_load_tx_msg2(value) write_c2_register64(MSGRNG_TX_BUF_REG, 2, value)
#define msgrng_load_tx_msg3(value) write_c2_register64(MSGRNG_TX_BUF_REG, 3, value)

/* Station IDs */
#define MSGRNG_STNID_CPU0  0x00
#define MSGRNG_STNID_CPU1  0x08
#define MSGRNG_STNID_CPU2  0x10
#define MSGRNG_STNID_CPU3  0x18
#define MSGRNG_STNID_CPU4  0x20
#define MSGRNG_STNID_CPU5  0x28
#define MSGRNG_STNID_CPU6  0x30
#define MSGRNG_STNID_CPU7  0x38
#define MSGRNG_STNID_XGS0_TX 64
#define MSGRNG_STNID_XMAC0_00_TX 64
#define MSGRNG_STNID_XMAC0_01_TX 65
#define MSGRNG_STNID_XMAC0_02_TX 66
#define MSGRNG_STNID_XMAC0_03_TX 67
#define MSGRNG_STNID_XMAC0_04_TX 68
#define MSGRNG_STNID_XMAC0_05_TX 69
#define MSGRNG_STNID_XMAC0_06_TX 70
#define MSGRNG_STNID_XMAC0_07_TX 71
#define MSGRNG_STNID_XMAC0_08_TX 72
#define MSGRNG_STNID_XMAC0_09_TX 73
#define MSGRNG_STNID_XMAC0_10_TX 74
#define MSGRNG_STNID_XMAC0_11_TX 75
#define MSGRNG_STNID_XMAC0_12_TX 76
#define MSGRNG_STNID_XMAC0_13_TX 77
#define MSGRNG_STNID_XMAC0_14_TX 78
#define MSGRNG_STNID_XMAC0_15_TX 79

#define MSGRNG_STNID_XGS1_TX 80
#define MSGRNG_STNID_XMAC1_00_TX 80
#define MSGRNG_STNID_XMAC1_01_TX 81
#define MSGRNG_STNID_XMAC1_02_TX 82
#define MSGRNG_STNID_XMAC1_03_TX 83
#define MSGRNG_STNID_XMAC1_04_TX 84
#define MSGRNG_STNID_XMAC1_05_TX 85
#define MSGRNG_STNID_XMAC1_06_TX 86
#define MSGRNG_STNID_XMAC1_07_TX 87
#define MSGRNG_STNID_XMAC1_08_TX 88
#define MSGRNG_STNID_XMAC1_09_TX 89
#define MSGRNG_STNID_XMAC1_10_TX 90
#define MSGRNG_STNID_XMAC1_11_TX 91
#define MSGRNG_STNID_XMAC1_12_TX 92
#define MSGRNG_STNID_XMAC1_13_TX 93
#define MSGRNG_STNID_XMAC1_14_TX 94
#define MSGRNG_STNID_XMAC1_15_TX 95

#define MSGRNG_STNID_GMAC 96
#define MSGRNG_STNID_GMACJFR_0  96
#define MSGRNG_STNID_GMACRFR_0  97
#define MSGRNG_STNID_GMACTX0    98
#define MSGRNG_STNID_GMACTX1    99
#define MSGRNG_STNID_GMACTX2    100
#define MSGRNG_STNID_GMACTX3    101
#define MSGRNG_STNID_GMACJFR_1  102
#define MSGRNG_STNID_GMACRFR_1  103

#define MSGRNG_STNID_DMA      104
#define MSGRNG_STNID_DMA_0    104
#define MSGRNG_STNID_DMA_1    105
#define MSGRNG_STNID_DMA_2    106
#define MSGRNG_STNID_DMA_3    107

#define MSGRNG_STNID_XGS0FR 112
#define MSGRNG_STNID_XMAC0JFR 112
#define MSGRNG_STNID_XMAC0RFR 113

#define MSGRNG_STNID_XGS1FR 114
#define MSGRNG_STNID_XMAC1JFR 114
#define MSGRNG_STNID_XMAC1RFR 115
#define MSGRNG_STNID_SEC 120
#define MSGRNG_STNID_SEC0 120
#define MSGRNG_STNID_SEC1 121
#define MSGRNG_STNID_SEC2 122
#define MSGRNG_STNID_SEC3 123
#define MSGRNG_STNID_PK0  124
#define MSGRNG_STNID_SEC_RSA 124
#define MSGRNG_STNID_SEC_RSVD0 125
#define MSGRNG_STNID_SEC_RSVD1 126
#define MSGRNG_STNID_SEC_RSVD2 127

#define MSGRNG_STNID_GMAC1      80
#define MSGRNG_STNID_GMAC1_FR_0   81
#define MSGRNG_STNID_GMAC1_TX0  82
#define MSGRNG_STNID_GMAC1_TX1  83
#define MSGRNG_STNID_GMAC1_TX2  84
#define MSGRNG_STNID_GMAC1_TX3  85
#define MSGRNG_STNID_GMAC1_FR_1   87
#define MSGRNG_STNID_GMAC0      96
#define MSGRNG_STNID_GMAC0_FR_0   97
#define MSGRNG_STNID_GMAC0_TX0  98
#define MSGRNG_STNID_GMAC0_TX1  99
#define MSGRNG_STNID_GMAC0_TX2  100
#define MSGRNG_STNID_GMAC0_TX3  101
#define MSGRNG_STNID_GMAC0_FR_1   103
#define MSGRNG_STNID_CMP_0      108
#define MSGRNG_STNID_CMP_1      109
#define MSGRNG_STNID_CMP_2      110
#define MSGRNG_STNID_CMP_3      111
#define MSGRNG_STNID_PCIE_0     116
#define MSGRNG_STNID_PCIE_1     117
#define MSGRNG_STNID_PCIE_2     118
#define MSGRNG_STNID_PCIE_3     119
#define MSGRNG_STNID_XLS_PK0    121

#define MSGRNG_CODE_MAC 0
#define MSGRNG_CODE_XGMAC 2
#define MSGRNG_CODE_SEC 0
#define MSGRNG_CODE_BOOT_WAKEUP 200
#define MSGRNG_CODE_SPI4 3

static inline int 
msgrng_xgmac_stid_rfr(int id)
{
	return !id ? MSGRNG_STNID_XMAC0RFR : MSGRNG_STNID_XMAC1RFR;
}

static inline int 
msgrng_xgmac_stid_jfr(int id)
{
	return !id ? MSGRNG_STNID_XMAC0JFR : MSGRNG_STNID_XMAC1JFR;
}

static inline int 
msgrng_xgmac_stid_tx(int id)
{
	return !id ? MSGRNG_STNID_XMAC0_00_TX : MSGRNG_STNID_XMAC1_00_TX;
}

static inline int 
msgrng_gmac_stid_rfr(int id)
{
	return (MSGRNG_STNID_GMACRFR_0);
}

static inline int 
msgrng_gmac_stid_rfr_split_mode(int id)
{
	return ((id >> 1) ? MSGRNG_STNID_GMACRFR_1 : MSGRNG_STNID_GMACRFR_0);
}

static inline int 
msgrng_gmac_stid_jfr(int id)
{
	return MSGRNG_STNID_GMACJFR_0;
}

static inline int 
msgrng_gmac_stid_jfr_split_mode(int id)
{
	return ((id >> 1) ? MSGRNG_STNID_GMACJFR_1 : MSGRNG_STNID_GMACJFR_0);
}

static inline int 
msgrng_gmac_stid_tx(int id)
{
	return (MSGRNG_STNID_GMACTX0 + id);
}

static inline void 
msgrng_send(unsigned int stid)
{
	__asm__ volatile (
	             ".set push\n"
	             ".set noreorder\n"
	             "sync\n"
	    //       "msgsnd %0\n"
	             "move  $8, %0\n"
	             "c2    0x80001\n"
	             ".set pop\n"
	    ::       "r" (stid):"$8"
	);
}

static inline void 
msgrng_receive(unsigned int pri)
{
	__asm__ volatile (
	             ".set push\n"
	             ".set noreorder\n"
	    //       "msgld %0\n"
	             "move $8, %0\n"
	             "c2   0x80002\n"
	             ".set pop\n"
	    ::       "r" (pri):"$8"
	);
}
static inline void 
msgrng_wait(unsigned int mask)
{
	__asm__ volatile (
	             ".set push\n"
	             ".set noreorder\n"
	    //       "msgwait %0\n"
	             "move $8, %0\n"
	             "c2   0x80003\n"
	             ".set pop\n"
	    ::       "r" (mask):"$8"
	);
}

#define msgrng_enable(flags)                        \
do {                                                \
  __asm__ volatile (                                \
		    ".set push\n\t"                 \
		    ".set reorder\n\t"              \
		    ".set noat\n\t"                 \
		    "mfc0 %0, $12\n\t"              \
		    "li  $8, 0x40000001\n\t"        \
		    "or  $1, %0, $8\n\t"            \
		    "xori $1, 1\n\t"                \
		    ".set noreorder\n\t"            \
		    "mtc0 $1, $12\n\t"              \
		    ".set\tpop\n\t"                 \
		    : "=r" (flags)                  \
		    :                               \
		    : "$8"                          \
		    );                              \
} while (0)

#define msgrng_disable(flags) __asm__ volatile (    \
                 "mtc0 %0, $12" : : "r" (flags))

#define msgrng_flags_save(flags) msgrng_enable(flags)
#define msgrng_flags_restore(flags) msgrng_disable(flags)

struct msgrng_msg {
	__uint64_t msg0;
	__uint64_t msg1;
	__uint64_t msg2;
	__uint64_t msg3;
};

static inline void 
message_send_block_fast(int size, unsigned int code, unsigned int stid,
    unsigned long long msg0, unsigned long long msg1,
    unsigned long long msg2, unsigned long long msg3)
{
	__asm__ __volatile__(".set push\n"
	            ".set noreorder\n"
	            ".set mips64\n"
	            "dmtc2 %1, $0, 0\n"
	            "dmtc2 %2, $0, 1\n"
	            "dmtc2 %3, $0, 2\n"
	            "dmtc2 %4, $0, 3\n"
	            "move $8, %0\n"
	            "1: c2 0x80001\n"
	            "mfc2 $8, $2\n"
	            "andi $8, $8, 0x6\n"
	            "bnez $8, 1b\n"
	            "move $8, %0\n"
	            ".set pop\n"
	    :
	    :       "r"(((size - 1) << 16) | (code << 8) | stid), "r"(msg0), "r"(msg1), "r"(msg2), "r"(msg3)
	    :       "$8"
	);
}

#define message_receive_fast(bucket, size, code, stid, msg0, msg1, msg2, msg3)      \
        ( { unsigned int _status=0, _tmp=0;                     \
           msgrng_receive(bucket);                              \
           while ( (_status=msgrng_read_status()) & 0x08) ;     \
           _tmp = _status & 0x30;                               \
           if (__builtin_expect((!_tmp), 1)) {                  \
                 (size)=((_status & 0xc0)>>6)+1;                \
                 (code)=(_status & 0xff00)>>8;                  \
                 (stid)=(_status & 0x7f0000)>>16;               \
                 (msg0)=msgrng_load_rx_msg0();                  \
                 (msg1)=msgrng_load_rx_msg1();                  \
                 (msg2)=msgrng_load_rx_msg2();                  \
                 (msg3)=msgrng_load_rx_msg3();                  \
                 _tmp=0;                                        \
                }                                               \
           _tmp;                                                \
        } )

static __inline__ int 
message_send(unsigned int size, unsigned int code,
    unsigned int stid, struct msgrng_msg *msg)
{
	unsigned int dest = 0;
	unsigned long long status = 0;
	int i = 0;

	msgrng_load_tx_msg0(msg->msg0);
	msgrng_load_tx_msg1(msg->msg1);
	msgrng_load_tx_msg2(msg->msg2);
	msgrng_load_tx_msg3(msg->msg3);

	dest = ((size - 1) << 16) | (code << 8) | (stid);

	//dbg_msg("Sending msg<%Lx,%Lx,%Lx,%Lx> to dest = %x\n",
	    //msg->msg0, msg->msg1, msg->msg2, msg->msg3, dest);

	msgrng_send(dest);

	for (i = 0; i < 16; i++) {
		status = msgrng_read_status();
		//dbg_msg("status = %Lx\n", status);

		if (status & 0x6) {
			continue;
		} else
			break;
	}
	if (i == 16) {
		if (dest == 0x61)
			//dbg_msg("Processor %x: Unable to send msg to %llx\n", processor_id(), dest);
		return status & 0x6;
	}
	return msgrng_read_status() & 0x06;
}

static __inline__ int 
message_send_retry(unsigned int size, unsigned int code,
    unsigned int stid, struct msgrng_msg *msg)
{
	int res = 0;
	int retry = 0;

	for (;;) {
		res = message_send(size, code, stid, msg);
		/* retry a pending fail */
		if (res & 0x02)
			continue;
		/* credit fail */
		if (res & 0x04)
			retry++;
		else
			break;
		if (retry == 4)
			return res & 0x06;
	}

	return 0;
}

static __inline__ int 
message_receive(int pri, int *size, int *code, int *src_id,
    struct msgrng_msg *msg)
{
	int res = message_receive_fast(pri, *size, *code, *src_id, msg->msg0, msg->msg1, msg->msg2, msg->msg3);

#ifdef MSGRING_DUMP_MESSAGES
	if (!res) {
		dbg_msg("Received msg <%llx, %llx, %llx, %llx> <%d,%d,%d>\n",
		    msg->msg0, msg->msg1, msg->msg2, msg->msg3,
		    *size, *code, *src_id);
	}
#endif

	return res;
}

#define MSGRNG_STN_RX_QSIZE 256

struct stn_cc {
	unsigned short counters[16][8];
};

struct bucket_size {
	unsigned short bucket[128];
};

extern struct bucket_size bucket_sizes;

extern struct stn_cc cc_table_cpu_0;
extern struct stn_cc cc_table_cpu_1;
extern struct stn_cc cc_table_cpu_2;
extern struct stn_cc cc_table_cpu_3;
extern struct stn_cc cc_table_cpu_4;
extern struct stn_cc cc_table_cpu_5;
extern struct stn_cc cc_table_cpu_6;
extern struct stn_cc cc_table_cpu_7;
extern struct stn_cc cc_table_xgs_0;
extern struct stn_cc cc_table_xgs_1;
extern struct stn_cc cc_table_gmac;
extern struct stn_cc cc_table_dma;
extern struct stn_cc cc_table_sec;

extern struct bucket_size xls_bucket_sizes;

extern struct stn_cc xls_cc_table_cpu_0;
extern struct stn_cc xls_cc_table_cpu_1;
extern struct stn_cc xls_cc_table_cpu_2;
extern struct stn_cc xls_cc_table_cpu_3;
extern struct stn_cc xls_cc_table_gmac0;
extern struct stn_cc xls_cc_table_gmac1;
extern struct stn_cc xls_cc_table_cmp;
extern struct stn_cc xls_cc_table_pcie;
extern struct stn_cc xls_cc_table_dma;
extern struct stn_cc xls_cc_table_sec;


#define msgrng_access_save(lock, mflags) do {                \
  if (rmi_spin_mutex_safe) mtx_lock_spin(lock);              \
  msgrng_flags_save(mflags);                                 \
 }while(0)

#define msgrng_access_restore(lock, mflags) do {             \
  msgrng_flags_restore(mflags);                              \
  if (rmi_spin_mutex_safe) mtx_unlock_spin(lock);            \
 }while(0)

#define msgrng_access_enable(mflags) do {   \
  critical_enter();                         \
  msgrng_flags_save(mflags);                \
} while(0)

#define msgrng_access_disable(mflags) do {   \
  msgrng_flags_restore(mflags);              \
  critical_exit();                           \
} while(0)

/*
 * NOTE: this is not stationid/8, ie the station numbers below are just
 * for internal use
 */
enum {
	TX_STN_CPU_0,
	TX_STN_CPU_1,
	TX_STN_CPU_2,
	TX_STN_CPU_3,
	TX_STN_CPU_4,
	TX_STN_CPU_5,
	TX_STN_CPU_6,
	TX_STN_CPU_7,
	TX_STN_GMAC,
	TX_STN_DMA,
	TX_STN_XGS_0,
	TX_STN_XGS_1,
	TX_STN_SAE,
	TX_STN_GMAC0,
	TX_STN_GMAC1,
	TX_STN_CDE,
	TX_STN_PCIE,
	TX_STN_INVALID,
	MAX_TX_STNS
};

extern int 
register_msgring_handler(int major,
    void (*action) (int, int, int, int, struct msgrng_msg *, void *),
    void *dev_id);
	extern void xlr_msgring_cpu_init(void);

	extern void xlr_msgring_config(void);

#define cpu_to_msgring_bucket(cpu) ((((cpu) >> 2)<<3)|((cpu) & 0x03))

#endif
