/*
 * Structures and definitions for SCSI commands to Direct Access Devices
 */

/*
 * Some lines of this file come from a file of the name "scsi.h"
 * distributed by OSF as part of mach2.5,
 *  so the following disclaimer has been kept.
 *
 * Copyright 1990 by Open Software Foundation,
 * Grenoble, FRANCE
 *
 * 		All Rights Reserved
 *
 *   Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice appears in all copies and
 * that both the copyright notice and this permission notice appear in
 * supporting documentation, and that the name of OSF or Open Software
 * Foundation not be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior
 * permission.
 *
 *   OSF DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS,
 * IN NO EVENT SHALL OSF BE LIABLE FOR ANY SPECIAL, INDIRECT, OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN ACTION OF CONTRACT,
 * NEGLIGENCE, OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * Largely written by Julian Elischer (julian@tfs.com)
 * for TRW Financial Systems.
 *
 * TRW Financial Systems, in accordance with their agreement with Carnegie
 * Mellon University, makes this software available to CMU to distribute
 * or use in any manner that they see fit as long as this message is kept with
 * the software. For this reason TFS also grants any other persons or
 * organisations permission to use or modify this software.
 *
 * TFS supplies this software to be publicly redistributed
 * on the understanding that TFS is not responsible for the correct
 * functioning of this software in any circumstances.
 *
 * Ported to run under 386BSD by Julian Elischer (julian@tfs.com) Sept 1992
 *
 * $FreeBSD$
 */

#ifndef	_SCSI_SCSI_DA_H
#define _SCSI_SCSI_DA_H 1

#include <sys/cdefs.h>

struct scsi_rezero_unit
{
	u_int8_t opcode;
#define SRZU_LUN_MASK 0xE0
	u_int8_t byte2;
	u_int8_t reserved[3];
	u_int8_t control;
};

/*
 * NOTE:  The lower three bits of byte2 of the format CDB are the same as
 * the lower three bits of byte2 of the read defect data CDB, below.
 */
struct scsi_format_unit
{
	u_int8_t opcode;
	u_int8_t byte2;
#define FU_FORMAT_MASK	SRDD10_DLIST_FORMAT_MASK
#define FU_BLOCK_FORMAT	SRDD10_BLOCK_FORMAT
#define FU_BFI_FORMAT	SRDD10_BYTES_FROM_INDEX_FORMAT
#define FU_PHYS_FORMAT	SRDD10_PHYSICAL_SECTOR_FORMAT
#define FU_CMPLST	0x08
#define FU_FMT_DATA	0x10
	u_int8_t vendor_specific;
	u_int8_t interleave[2];
	u_int8_t control;
};

struct scsi_reassign_blocks
{
	u_int8_t opcode;
	u_int8_t byte2;
	u_int8_t unused[3];
	u_int8_t control;
};

struct scsi_read_defect_data_10
{
	u_int8_t opcode;

	/* 
	 * The most significant 3 bits are the LUN, the other 5 are
	 * reserved.
	 */
#define SRDD10_LUN_MASK 0xE0
	u_int8_t byte2;
#define SRDD10_GLIST 0x08
#define SRDD10_PLIST 0x10
#define SRDD10_DLIST_FORMAT_MASK 0x07
#define SRDD10_BLOCK_FORMAT            0x00
#define SRDD10_BYTES_FROM_INDEX_FORMAT 0x04
#define SRDD10_PHYSICAL_SECTOR_FORMAT  0x05
	u_int8_t format;

	u_int8_t reserved[4];

	u_int8_t alloc_length[2];

	u_int8_t control;
};

struct scsi_read_defect_data_12
{
	u_int8_t opcode;

	/* 
	 * The most significant 3 bits are the LUN, the other 5 are
	 * reserved.
	 */
#define SRDD12_LUN_MASK 0xE0
	u_int8_t byte2;

#define SRDD12_GLIST 0x08
#define SRDD12_PLIST 0x10
#define SRDD12_DLIST_FORMAT_MASK 0x07
#define SRDD12_BLOCK_FORMAT            0x00
#define SRDD12_BYTES_FROM_INDEX_FORMAT 0x04
#define SRDD12_PHYSICAL_SECTOR_FORMAT  0x05
	u_int8_t format;

	u_int8_t reserved[4];

	u_int8_t alloc_length[4];

	u_int8_t control;
	
};


/*
 * Opcodes
 */
#define REZERO_UNIT		0x01
#define FORMAT_UNIT		0x04
#define	REASSIGN_BLOCKS		0x07
#define MODE_SELECT		0x15
#define MODE_SENSE		0x1a
#define READ_DEFECT_DATA_10	0x37
#define READ_DEFECT_DATA_12	0xb7

struct format_defect_list_header
{
	u_int8_t reserved;
	u_int8_t byte2;
#define FU_DLH_VS	0x01
#define FU_DLH_IMMED	0x02
#define FU_DLH_DSP	0x04
#define FU_DLH_IP	0x08
#define FU_DLH_STPF	0x10
#define FU_DLH_DCRT	0x20
#define FU_DLH_DPRY	0x40
#define FU_DLH_FOV	0x80
	u_int8_t defect_list_length[2];
};

struct format_ipat_descriptor
{
	u_int8_t byte1;
#define	FU_INIT_NO_HDR		0x00
#define FU_INIT_LBA_MSB		0x40
#define FU_INIT_LBA_EACH	0x80
#define FU_INIT_SI		0x20
	u_int8_t pattern_type;
#define FU_INIT_PAT_DEFAULT	0x00
#define FU_INIT_PAT_REPEAT	0x01
	u_int8_t pat_length[2];
};

struct scsi_reassign_blocks_data
{
	u_int8_t reserved[2];
	u_int8_t length[2];
	struct {
		u_int8_t dlbaddr[4];	/* defect logical block address */
	} defect_descriptor[1];
};


/*
 * This is the list header for the READ DEFECT DATA(10) command above.
 * It may be a bit wrong to append the 10 at the end of the data structure,
 * since it's only 4 bytes but it does tie it to the 10 byte command.
 */
struct scsi_read_defect_data_hdr_10
{
	u_int8_t reserved;
#define SRDDH10_GLIST 0x08
#define SRDDH10_PLIST 0x10
#define SRDDH10_DLIST_FORMAT_MASK 0x07
#define SRDDH10_BLOCK_FORMAT            0x00
#define SRDDH10_BYTES_FROM_INDEX_FORMAT 0x04
#define SRDDH10_PHYSICAL_SECTOR_FORMAT  0x05
	u_int8_t format;
	u_int8_t length[2];
};

struct scsi_defect_desc_block
{
	u_int8_t address[4];
};

struct scsi_defect_desc_bytes_from_index
{
	u_int8_t cylinder[3];
	u_int8_t head;
	u_int8_t bytes_from_index[4];
};

struct scsi_defect_desc_phys_sector
{
	u_int8_t cylinder[3];
	u_int8_t head;
	u_int8_t sector[4];
};

struct scsi_read_defect_data_hdr_12
{
	u_int8_t reserved;
#define SRDDH12_GLIST 0x08
#define SRDDH12_PLIST 0x10
#define SRDDH12_DLIST_FORMAT_MASK 0x07
#define SRDDH12_BLOCK_FORMAT            0x00
#define SRDDH12_BYTES_FROM_INDEX_FORMAT 0x04
#define SRDDH12_PHYSICAL_SECTOR_FORMAT  0x05
	u_int8_t format;
	u_int8_t length[4];
};

union	disk_pages /* this is the structure copied from osf */
{
	struct format_device_page {
		u_int8_t pg_code;	/* page code (should be 3)	      */
#define	SMS_FORMAT_DEVICE_PAGE	0x03	/* only 6 bits valid */
		u_int8_t pg_length;	/* page length (should be 0x16)	      */
#define	SMS_FORMAT_DEVICE_PLEN	0x16
		u_int8_t trk_z_1;	/* tracks per zone (MSB)	      */
		u_int8_t trk_z_0;	/* tracks per zone (LSB)	      */
		u_int8_t alt_sec_1;	/* alternate sectors per zone (MSB)   */
		u_int8_t alt_sec_0;	/* alternate sectors per zone (LSB)   */
		u_int8_t alt_trk_z_1;	/* alternate tracks per zone (MSB)    */
		u_int8_t alt_trk_z_0;	/* alternate tracks per zone (LSB)    */
		u_int8_t alt_trk_v_1;	/* alternate tracks per volume (MSB)  */
		u_int8_t alt_trk_v_0;	/* alternate tracks per volume (LSB)  */
		u_int8_t ph_sec_t_1;	/* physical sectors per track (MSB)   */
		u_int8_t ph_sec_t_0;	/* physical sectors per track (LSB)   */
		u_int8_t bytes_s_1;	/* bytes per sector (MSB)	      */
		u_int8_t bytes_s_0;	/* bytes per sector (LSB)	      */
		u_int8_t interleave_1;	/* interleave (MSB)		      */
		u_int8_t interleave_0;	/* interleave (LSB)		      */
		u_int8_t trk_skew_1;	/* track skew factor (MSB)	      */
		u_int8_t trk_skew_0;	/* track skew factor (LSB)	      */
		u_int8_t cyl_skew_1;	/* cylinder skew (MSB)		      */
		u_int8_t cyl_skew_0;	/* cylinder skew (LSB)		      */
		u_int8_t flags;		/* various */
#define			DISK_FMT_SURF	0x10
#define	       		DISK_FMT_RMB	0x20
#define			DISK_FMT_HSEC	0x40
#define			DISK_FMT_SSEC	0x80
		u_int8_t reserved21;
		u_int8_t reserved22;
		u_int8_t reserved23;
	} format_device;
	struct rigid_geometry_page {
		u_int8_t pg_code;	/* page code (should be 4)	      */
#define SMS_RIGID_GEOMETRY_PAGE 0x04
		u_int8_t pg_length;	/* page length (should be 0x16)	      */
#define SMS_RIGID_GEOMETRY_PLEN 0x16		
		u_int8_t ncyl_2;	/* number of cylinders (MSB)	      */
		u_int8_t ncyl_1;	/* number of cylinders 		      */
		u_int8_t ncyl_0;	/* number of cylinders (LSB)	      */
		u_int8_t nheads;	/* number of heads 		      */
		u_int8_t st_cyl_wp_2;	/* starting cyl., write precomp (MSB) */
		u_int8_t st_cyl_wp_1;	/* starting cyl., write precomp	      */
		u_int8_t st_cyl_wp_0;	/* starting cyl., write precomp (LSB) */
		u_int8_t st_cyl_rwc_2;	/* starting cyl., red. write cur (MSB)*/
		u_int8_t st_cyl_rwc_1;	/* starting cyl., red. write cur      */
		u_int8_t st_cyl_rwc_0;	/* starting cyl., red. write cur (LSB)*/
		u_int8_t driv_step_1;	/* drive step rate (MSB)	      */
		u_int8_t driv_step_0;	/* drive step rate (LSB)	      */
		u_int8_t land_zone_2;	/* landing zone cylinder (MSB)	      */
		u_int8_t land_zone_1;	/* landing zone cylinder 	      */
		u_int8_t land_zone_0;	/* landing zone cylinder (LSB)	      */
		u_int8_t rpl;		/* rotational position locking (2 bits) */
		u_int8_t rot_offset;	/* rotational offset */
		u_int8_t reserved19;
		u_int8_t medium_rot_rate_1; /* medium rotation rate (RPM) (MSB) */
		u_int8_t medium_rot_rate_0; /* medium rotation rate (RPM) (LSB) */
		u_int8_t reserved22;
		u_int8_t reserved23;
    	} rigid_geometry;
	struct flexible_disk_page {
		u_int8_t pg_code;	/* page code (should be 5)	      */
#define SMS_FLEXIBLE_GEOMETRY_PAGE 0x05
		u_int8_t pg_length;	/* page length (should be 0x1E)	      */
#define SMS_FLEXIBLE_GEOMETRY_PLEN 0x1E
		u_int8_t xfr_rate_1;	/* transfer rate (MSB)		      */
		u_int8_t xfr_rate_0;	/* transfer rate (LSB)		      */
		u_int8_t nheads;	/* number of heads 		      */
		u_int8_t sec_per_track;	/* Sectors per track		      */
		u_int8_t bytes_s_1;	/* bytes per sector (MSB)	      */
		u_int8_t bytes_s_0;	/* bytes per sector (LSB)	      */
		u_int8_t ncyl_1;	/* number of cylinders (MSB)	      */
		u_int8_t ncyl_0;	/* number of cylinders (LSB)	      */
		u_int8_t st_cyl_wp_1;	/* starting cyl., write precomp (MSB) */
		u_int8_t st_cyl_wp_0;	/* starting cyl., write precomp (LSB) */
		u_int8_t st_cyl_rwc_1;	/* starting cyl., red. write cur (MSB)*/
		u_int8_t st_cyl_rwc_0;	/* starting cyl., red. write cur (LSB)*/		
		u_int8_t driv_step_1;	/* drive step rate (MSB)	      */
		u_int8_t driv_step_0;	/* drive step rate (LSB)	      */
		u_int8_t driv_step_pw;	/* drive step pulse width	      */
		u_int8_t head_stl_del_1;/* Head settle delay (MSB)	      */
		u_int8_t head_stl_del_0;/* Head settle delay (LSB)	      */
		u_int8_t motor_on_del;	/* Motor on delay		      */
		u_int8_t motor_off_del;	/* Motor off delay		      */
		u_int8_t trdy_ssn_mo;	/* XXX ??? */
		u_int8_t spc;		/* XXX ??? */
		u_int8_t write_comp;	/* Write compensation */
		u_int8_t head_load_del; /* Head load delay */
		u_int8_t head_uload_del;/* Head un-load delay */
		u_int8_t pin32_pin2;
		u_int8_t pin4_pint1;
		u_int8_t medium_rot_rate_1; /* medium rotation rate (RPM) (MSB) */
		u_int8_t medium_rot_rate_0; /* medium rotation rate (RPM) (LSB) */		
		u_int8_t reserved30;
		u_int8_t reserved31;
    	} flexible_disk;	
};

struct scsi_da_rw_recovery_page {
	u_int8_t page_code;
#define SMS_RW_ERROR_RECOVERY_PAGE	0x01
	u_int8_t page_length;
	u_int8_t byte3;
#define SMS_RWER_AWRE			0x80
#define SMS_RWER_ARRE			0x40
#define SMS_RWER_TB			0x20
#define SMS_RWER_RC			0x10
#define SMS_RWER_EER			0x08
#define SMS_RWER_PER			0x04
#define SMS_RWER_DTE			0x02
#define SMS_RWER_DCR			0x01
	u_int8_t read_retry_count;
	u_int8_t correction_span;
	u_int8_t head_offset_count;
	u_int8_t data_strobe_offset_cnt;
	u_int8_t reserved;
	u_int8_t write_retry_count;
	u_int8_t reserved2;
	u_int8_t recovery_time_limit[2];
};

__BEGIN_DECLS
/*
 * XXX This is only left out of the kernel build to silence warnings.  If,
 * for some reason this function is used in the kernel, the ifdefs should
 * be moved so it is included both in the kernel and userland.
 */
#ifndef _KERNEL
void scsi_format_unit(struct ccb_scsiio *csio, u_int32_t retries,
		      void (*cbfcnp)(struct cam_periph *, union ccb *),
		      u_int8_t tag_action, u_int8_t byte2, u_int16_t ileave,
		      u_int8_t *data_ptr, u_int32_t dxfer_len,
		      u_int8_t sense_len, u_int32_t timeout);

#endif /* !_KERNEL */
__END_DECLS

#endif /* _SCSI_SCSI_DA_H */
