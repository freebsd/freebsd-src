/*
 * Structure and function declartaions for the
 * SCSI Sequential Access Peripheral driver for CAM.
 *
 * Copyright (c) 1997 Justin T. Gibbs
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
 *      $Id$
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
#define SA_DATA_COMPRESSION_PAGE	0x0f
#define SA_DEVICE_CONFIGURATION_PAGE	0x10
#define SA_MEDIUM_PARTITION_PAGE_1	0x11
#define SA_MEDIUM_PARTITION_PAGE_2	0x12
#define SA_MEDIUM_PARTITION_PAGE_3	0x13
#define SA_MEDIUM_PARTITION_PAGE_4	0x14

/*
 * Mode page definitions.
 */

struct scsi_data_compression_page {
	u_int8_t page_code;
	u_int8_t page_length;
#define SA_DCP_DCE		0x80 	/* Data compression enable */
#define SA_DCP_DCC		0x40	/* Data compression capable */
	u_int8_t dce_and_dcc;
#define SA_DCP_DDE		0x80	/* Data decompression enable */
#define SA_DCP_RED_MASK		0x60	/* Report Exception on Decomp. */
#define SA_DCP_RED_SHAMT	5
#define SA_DCP_RED_0		0x00
#define SA_DCP_RED_1		0x20
#define SA_DCP_RED_2		0x40
	u_int8_t dde_and_red;
	u_int8_t comp_algorithm[4];
	u_int8_t decomp_algorithm[4];
	u_int8_t reserved[4];
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
__END_DECLS

#endif /* _SCSI_SCSI_SA_H */
