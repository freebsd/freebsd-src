/*
 * HISTORY
 * $Log: scsi_disk.h,v $
 * Revision 1.2  1992/10/13  03:14:21  julian
 * added the load-eject field in 'start/stop' for removable devices.
 *
 * Revision 1.1  1992/09/26  22:11:29  julian
 * Initial revision
 *
 *
 * PATCHES MAGIC                LEVEL   PATCH THAT GOT US HERE
 * --------------------         -----   ----------------------
 * CURRENT PATCH LEVEL:         1       00098
 * --------------------         -----   ----------------------
 *
 * 16 Feb 93	Julian Elischer		ADDED for SCSI system
 * 
 */

/*
 * SCSI interface description
 */

/*
 * Some lines of this file comes from a file of the name "scsi.h"
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
 */

/*
 * Ported to run under 386BSD by Julian Elischer (julian@tfs.com) Sept 1992
 */

/*
 * SCSI command format
 */


struct scsi_reassign_blocks
{
	u_char	op_code;
	u_char	:5;
	u_char	lun:3;	
	u_char	unused[3];
	u_char	link:1;
	u_char	flag:1;
	u_char	:6;
};

struct scsi_rw
{
	u_char	op_code;
	u_char	addr_2:5;	/* Most significant */
	u_char	lun:3;
	u_char	addr_1;
	u_char	addr_0;		/* least significant */
	u_char	length;
	u_char	link:1;
	u_char	flag:1;
	u_char	:6;
};

struct scsi_rw_big
{
	u_char	op_code;
	u_char	rel_addr:1;
	u_char	:4;	/* Most significant */
	u_char	lun:3;
	u_char	addr_3;
	u_char	addr_2;
	u_char	addr_1;
	u_char	addr_0;		/* least significant */
	u_char	reserved;;
	u_char	length2;
	u_char	length1;
	u_char	link:1;
	u_char	flag:1;
	u_char	:4;
	u_char	vendor:2;
};

struct scsi_read_capacity
{
	u_char	op_code;
	u_char	:5;
	u_char	lun:3;
	u_char	addr_3;	/* Most Significant */
	u_char	addr_2;
	u_char	addr_1;
	u_char	addr_0;	/* Least Significant */
	u_char	unused[3];
	u_char	link:1;
	u_char	flag:1;
	u_char	:6;	
};

struct scsi_start_stop
{
	u_char	op_code;
	u_char	:5;
	u_char	lun:3;
	u_char	unused[2];
	u_char	start:1;
	u_char	loej:1;
	u_char	:6;
	u_char	link:1;
	u_char	flag:1;
	u_char	:6;
};



/*
 * Opcodes
 */

#define	REASSIGN_BLOCKS		0x07
#define	READ_COMMAND		0x08
#define WRITE_COMMAND		0x0a
#define MODE_SELECT		0x15
#define MODE_SENSE		0x1a
#define START_STOP		0x1b
#define PREVENT_ALLOW		0x1e
#define	READ_CAPACITY		0x25
#define	READ_BIG		0x28
#define WRITE_BIG		0x2a



struct scsi_read_cap_data
{
	u_char	addr_3;	/* Most significant */
	u_char	addr_2;
	u_char	addr_1;
	u_char	addr_0;	/* Least significant */
	u_char	length_3;	/* Most significant */
	u_char	length_2;
	u_char	length_1;
	u_char	length_0;	/* Least significant */
};

struct scsi_reassign_blocks_data
{
	u_char	reserved[2];
	u_char	length_msb;
	u_char	length_lsb;
	struct
	{
		u_char	dlbaddr_3;	/* defect logical block address (MSB) */
		u_char	dlbaddr_2;
		u_char	dlbaddr_1;
		u_char	dlbaddr_0;	/* defect logical block address (LSB) */
	} defect_descriptor[1];
};

union	disk_pages /* this is the structure copied from osf */
{
	struct page_disk_format {
	   u_char pg_code:6;	/* page code (should be 3)	      */
	   u_char :2;		
	   u_char pg_length;	/* page length (should be 0x16)	      */
	   u_char trk_z_1;	/* tracks per zone (MSB)	      */
	   u_char trk_z_0;	/* tracks per zone (LSB)	      */
	   u_char alt_sec_1;	/* alternate sectors per zone (MSB)   */
	   u_char alt_sec_0;	/* alternate sectors per zone (LSB)   */
	   u_char alt_trk_z_1;	/* alternate tracks per zone (MSB)    */
	   u_char alt_trk_z_0;	/* alternate tracks per zone (LSB)    */
	   u_char alt_trk_v_1;	/* alternate tracks per volume (MSB)  */
	   u_char alt_trk_v_0;	/* alternate tracks per volume (LSB)  */
	   u_char ph_sec_t_1;	/* physical sectors per track (MSB)   */
	   u_char ph_sec_t_0;	/* physical sectors per track (LSB)   */
	   u_char bytes_s_1;	/* bytes per sector (MSB)	      */
	   u_char bytes_s_0;	/* bytes per sector (LSB)	      */
	   u_char interleave_1;/* interleave (MSB)		      */
	   u_char interleave_0;/* interleave (LSB)		      */
	   u_char trk_skew_1;	/* track skew factor (MSB)	      */
	   u_char trk_skew_0;	/* track skew factor (LSB)	      */
	   u_char cyl_skew_1;	/* cylinder skew (MSB)		      */
	   u_char cyl_skew_0;	/* cylinder skew (LSB)		      */
	   u_char reserved1:4;
	   u_char surf:1;
	   u_char rmb:1;
	   u_char hsec:1;
	   u_char ssec:1;
	   u_char reserved2;
	   u_char reserved3;
	} disk_format;
	struct page_rigid_geometry {
	   u_char pg_code:7;	/* page code (should be 4)	      */
	   u_char mbone:1;	/* must be one			      */
	   u_char pg_length;	/* page length (should be 0x16)	      */
	   u_char ncyl_2;	/* number of cylinders (MSB)	      */
	   u_char ncyl_1;	/* number of cylinders 		      */
	   u_char ncyl_0;	/* number of cylinders (LSB)	      */
	   u_char nheads;	/* number of heads 		      */
	   u_char st_cyl_wp_2;	/* starting cyl., write precomp (MSB) */
	   u_char st_cyl_wp_1;	/* starting cyl., write precomp	      */
	   u_char st_cyl_wp_0;	/* starting cyl., write precomp (LSB) */
	   u_char st_cyl_rwc_2;/* starting cyl., red. write cur (MSB)*/
	   u_char st_cyl_rwc_1;/* starting cyl., red. write cur      */
	   u_char st_cyl_rwc_0;/* starting cyl., red. write cur (LSB)*/
	   u_char driv_step_1;	/* drive step rate (MSB)	      */
	   u_char driv_step_0;	/* drive step rate (LSB)	      */
	   u_char land_zone_2;	/* landing zone cylinder (MSB)	      */
	   u_char land_zone_1;	/* landing zone cylinder 	      */
	   u_char land_zone_0;	/* landing zone cylinder (LSB)	      */
	   u_char reserved1;
	   u_char reserved2;
	   u_char reserved3;
    	} rigid_geometry;
} ;
