/*-
 * Structure and function declarations for the
 * SCSI Sequential Access Peripheral driver for CAM.
 *
 * Copyright (c) 1999, 2000 Matthew Jacob
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification, immediately at the beginning of the file.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/cam/scsi/scsi_sa.h,v 1.10.18.1 2008/11/25 02:59:29 kensmith Exp $
 */

#ifndef	_SCSI_SCSI_SA_H
#define _SCSI_SCSI_SA_H 1

#include <sys/cdefs.h>

struct scsi_read_block_limits
{
	u_int8_t opcode;
	u_int8_t byte2;
	u_int8_t unused[3];
	u_int8_t control;
};

struct scsi_read_block_limits_data
{
	u_int8_t gran;
#define	RBL_GRAN_MASK	0x1F
#define RBL_GRAN(rblim) ((rblim)->gran & RBL_GRAN_MASK)
	u_int8_t maximum[3];
	u_int8_t minimum[2];
};

struct scsi_sa_rw
{
	u_int8_t opcode;
        u_int8_t sli_fixed;
#define SAR_SLI		0x02
#define SARW_FIXED	0x01
	u_int8_t length[3];
        u_int8_t control;
};

struct scsi_load_unload
{
	u_int8_t opcode;
        u_int8_t immediate;
#define SLU_IMMED	0x01
	u_int8_t reserved[2];
	u_int8_t eot_reten_load;
#define SLU_EOT		0x04
#define SLU_RETEN	0x02
#define SLU_LOAD	0x01
        u_int8_t control;
};

struct scsi_rewind
{
	u_int8_t opcode;
        u_int8_t immediate;
#define SREW_IMMED	0x01
	u_int8_t reserved[3];
        u_int8_t control;
};

typedef enum {
	SS_BLOCKS,
	SS_FILEMARKS,
	SS_SEQFILEMARKS,
	SS_EOD,
	SS_SETMARKS,
	SS_SEQSETMARKS
} scsi_space_code;

struct scsi_space
{
	u_int8_t opcode;
        u_int8_t code;
#define SREW_IMMED	0x01
	u_int8_t count[3];
        u_int8_t control;
};

struct scsi_write_filemarks
{
	u_int8_t opcode;
        u_int8_t byte2;
#define SWFMRK_IMMED	0x01
#define SWFMRK_WSMK	0x02
	u_int8_t num_marks[3];
        u_int8_t control;
};

/*
 * Reserve and release unit have the same exact cdb format, but different
 * opcodes.
 */
struct scsi_reserve_release_unit
{
	u_int8_t opcode;
	u_int8_t lun_thirdparty;
#define SRRU_LUN_MASK	0xE0
#define SRRU_3RD_PARTY	0x10
#define SRRU_3RD_SHAMT	1
#define SRRU_3RD_MASK	0xE
	u_int8_t reserved[3];
	u_int8_t control;
};

/*
 * Erase a tape
 */
struct scsi_erase
{
	u_int8_t opcode;
	u_int8_t lun_imm_long;
#define SE_LUN_MASK	0xE0
#define SE_LONG		0x1
#define SE_IMMED	0x2
	u_int8_t reserved[3];
	u_int8_t control;
};

/*
 * Dev specific mode page masks.
 */
#define SMH_SA_WP		0x80
#define	SMH_SA_BUF_MODE_MASK	0x70
#define SMH_SA_BUF_MODE_NOBUF	0x00
#define SMH_SA_BUF_MODE_SIBUF	0x10	/* Single-Initiator buffering */
#define SMH_SA_BUF_MODE_MIBUF	0x20	/* Multi-Initiator buffering */
#define SMH_SA_SPEED_MASK	0x0F
#define SMH_SA_SPEED_DEFAULT	0x00

/*
 * Sequential-access specific mode page numbers.
 */
#define SA_DEVICE_CONFIGURATION_PAGE	0x10
#define SA_MEDIUM_PARTITION_PAGE_1	0x11
#define SA_MEDIUM_PARTITION_PAGE_2	0x12
#define SA_MEDIUM_PARTITION_PAGE_3	0x13
#define SA_MEDIUM_PARTITION_PAGE_4	0x14
#define SA_DATA_COMPRESSION_PAGE	0x0f	/* SCSI-3 */

/*
 * Mode page definitions.
 */

/* See SCSI-II spec 9.3.3.1 */
struct scsi_dev_conf_page {
	u_int8_t pagecode;	/* 0x10 */
	u_int8_t pagelength;	/* 0x0e */
	u_int8_t byte2;		/* CAP, CAF, Active Format */
	u_int8_t active_partition;
	u_int8_t wb_full_ratio;
	u_int8_t rb_empty_ratio;
	u_int8_t wrdelay_time[2];
	u_int8_t byte8;
#define	SA_DBR			0x80	/* data buffer recovery */
#define	SA_BIS			0x40	/* block identifiers supported */
#define	SA_RSMK			0x20	/* report setmarks */
#define	SA_AVC			0x10	/* automatic velocity control */
#define	SA_SOCF_MASK		0xc0	/* stop on consecutive formats */
#define	SA_RBO			0x20	/* recover buffer order */
#define	SA_REW			0x10	/* report early warning */
	u_int8_t gap_size;
	u_int8_t byte10;
	u_int8_t ew_bufsize[3];
	u_int8_t sel_comp_alg;
#define	SA_COMP_NONE		0x00
#define	SA_COMP_DEFAULT		0x01
	/* the following is 'reserved' in SCSI-2 but is defined in SSC-r22 */
	u_int8_t extra_wp;
#define	SA_ASOC_WP		0x04	/* Associated Write Protect */
#define	SA_PERS_WP		0x02	/* Persistent Write Protect */
#define	SA_PERM_WP		0x01	/* Permanent Write Protect */
};

/* from SCSI-3: SSC-Rev10 (6/97) */
struct scsi_data_compression_page {
	u_int8_t page_code;	/* 0x0f */
	u_int8_t page_length;	/* 0x0e */
	u_int8_t dce_and_dcc;
#define SA_DCP_DCE		0x80 	/* Data compression enable */
#define SA_DCP_DCC		0x40	/* Data compression capable */
	u_int8_t dde_and_red;
#define SA_DCP_DDE		0x80	/* Data decompression enable */
#define SA_DCP_RED_MASK		0x60	/* Report Exception on Decomp. */
#define SA_DCP_RED_SHAMT	5
#define SA_DCP_RED_0		0x00
#define SA_DCP_RED_1		0x20
#define SA_DCP_RED_2		0x40
	u_int8_t comp_algorithm[4];
	u_int8_t decomp_algorithm[4];
	u_int8_t reserved[4];
};

typedef union {
	struct { u_int8_t pagecode, pagelength; } hdr;
	struct scsi_dev_conf_page dconf;
	struct scsi_data_compression_page dcomp;
} sa_comp_t;

struct scsi_tape_read_position {
	u_int8_t opcode;		/* READ_POSITION */
	u_int8_t byte1;			/* set LSB to read hardware block pos */
	u_int8_t reserved[8];
};

struct scsi_tape_position_data	{	/* Short Form */
	u_int8_t flags;
#define	SA_RPOS_BOP		0x80	/* Beginning of Partition */
#define	SA_RPOS_EOP		0x40	/* End of Partition */
#define	SA_RPOS_BCU		0x20	/* Block Count Unknown (SCSI3) */
#define	SA_RPOS_BYCU		0x10	/* Byte Count Unknown (SCSI3) */
#define	SA_RPOS_BPU		0x04	/* Block Position Unknown */
#define	SA_RPOS_PERR		0x02	/* Position Error (SCSI3) */
#define	SA_RPOS_UNCERTAIN	SA_RPOS_BPU
	u_int8_t partition;
	u_int8_t reserved[2];
	u_int8_t firstblk[4];
	u_int8_t lastblk[4];
	u_int8_t reserved2;
	u_int8_t nbufblk[3];
	u_int8_t nbufbyte[4];
};

struct scsi_tape_locate {
	u_int8_t opcode;
	u_int8_t byte1;
#define	SA_SPOS_IMMED		0x01
#define	SA_SPOS_CP		0x02
#define	SA_SPOS_BT		0x04
	u_int8_t reserved1;
	u_int8_t blkaddr[4];
	u_int8_t reserved2;
	u_int8_t partition;
	u_int8_t control;
};

/*
 * Opcodes
 */
#define REWIND			0x01
#define READ_BLOCK_LIMITS	0x05
#define SA_READ			0x08
#define SA_WRITE		0x0A
#define WRITE_FILEMARKS		0x10
#define SPACE			0x11
#define RESERVE_UNIT		0x16
#define RELEASE_UNIT		0x17
#define ERASE			0x19
#define LOAD_UNLOAD		0x1B
#define	LOCATE			0x2B
#define	READ_POSITION		0x34

/*
 * Tape specific density codes- only enough of them here to recognize
 * some specific older units so we can choose 2FM@EOD or FIXED blocksize
 * quirks.
 */
#define SCSI_DENSITY_HALFINCH_800	0x01
#define SCSI_DENSITY_HALFINCH_1600	0x02
#define SCSI_DENSITY_HALFINCH_6250	0x03
#define SCSI_DENSITY_HALFINCH_6250C	0xC3	/* HP Compressed 6250 */
#define SCSI_DENSITY_QIC_11_4TRK	0x04
#define SCSI_DENSITY_QIC_11_9TRK	0x84	/* Vendor Unique Emulex */
#define SCSI_DENSITY_QIC_24		0x05
#define SCSI_DENSITY_HALFINCH_PE	0x06
#define SCSI_DENSITY_QIC_120		0x0f
#define SCSI_DENSITY_QIC_150		0x10    
#define	SCSI_DENSITY_QIC_525_320	0x11
#define	SCSI_DENSITY_QIC_1320		0x12
#define	SCSI_DENSITY_QIC_2GB		0x22
#define	SCSI_DENSITY_QIC_4GB		0x26
#define	SCSI_DENSITY_QIC_3080		0x29

__BEGIN_DECLS
void	scsi_read_block_limits(struct ccb_scsiio *, u_int32_t,
			       void (*cbfcnp)(struct cam_periph *, union ccb *),
			       u_int8_t, struct scsi_read_block_limits_data *,
			       u_int8_t , u_int32_t);

void	scsi_sa_read_write(struct ccb_scsiio *csio, u_int32_t retries,
			   void (*cbfcnp)(struct cam_periph *, union ccb *),
			   u_int8_t tag_action, int readop, int sli,
			   int fixed, u_int32_t length, u_int8_t *data_ptr,
			   u_int32_t dxfer_len, u_int8_t sense_len,
			   u_int32_t timeout);

void	scsi_rewind(struct ccb_scsiio *csio, u_int32_t retries,
		    void (*cbfcnp)(struct cam_periph *, union ccb *),
		    u_int8_t tag_action, int immediate, u_int8_t sense_len,
		    u_int32_t timeout);

void	scsi_space(struct ccb_scsiio *csio, u_int32_t retries,
		   void (*cbfcnp)(struct cam_periph *, union ccb *),
		   u_int8_t tag_action, scsi_space_code code,
		   u_int32_t count, u_int8_t sense_len, u_int32_t timeout);

void	scsi_load_unload(struct ccb_scsiio *csio, u_int32_t retries,         
			 void (*cbfcnp)(struct cam_periph *, union ccb *),   
			 u_int8_t tag_action, int immediate,   int eot,
			 int reten, int load, u_int8_t sense_len,
			 u_int32_t timeout);
	
void	scsi_write_filemarks(struct ccb_scsiio *csio, u_int32_t retries,
			     void (*cbfcnp)(struct cam_periph *, union ccb *),
			     u_int8_t tag_action, int immediate, int setmark,
			     u_int32_t num_marks, u_int8_t sense_len,
			     u_int32_t timeout);

void	scsi_reserve_release_unit(struct ccb_scsiio *csio, u_int32_t retries,
				  void (*cbfcnp)(struct cam_periph *,
				  union ccb *), u_int8_t tag_action,	
				  int third_party, int third_party_id,
				  u_int8_t sense_len, u_int32_t timeout,
				  int reserve);

void	scsi_erase(struct ccb_scsiio *csio, u_int32_t retries,
		   void (*cbfcnp)(struct cam_periph *, union ccb *),
		   u_int8_t tag_action, int immediate, int long_erase,
		   u_int8_t sense_len, u_int32_t timeout);

void	scsi_data_comp_page(struct scsi_data_compression_page *page,
			    u_int8_t dce, u_int8_t dde, u_int8_t red,
			    u_int32_t comp_algorithm,
			    u_int32_t decomp_algorithm);

void	scsi_read_position(struct ccb_scsiio *csio, u_int32_t retries,
                           void (*cbfcnp)(struct cam_periph *, union ccb *),
                           u_int8_t tag_action, int hardsoft,
                           struct scsi_tape_position_data *sbp,
                           u_int8_t sense_len, u_int32_t timeout);

void	scsi_set_position(struct ccb_scsiio *csio, u_int32_t retries,
                         void (*cbfcnp)(struct cam_periph *, union ccb *),
                         u_int8_t tag_action, int hardsoft, u_int32_t blkno,
                         u_int8_t sense_len, u_int32_t timeout);
__END_DECLS

#endif /* _SCSI_SCSI_SA_H */
