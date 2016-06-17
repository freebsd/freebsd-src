/* 
 * File...........: linux/drivers/s390/block/dasd_diag.h
 * Author(s)......: Holger Smolinski <Holger.Smolinski@de.ibm.com>
 * Based on.......: linux/drivers/s390/block/mdisk.h
 * ...............: by Hartmunt Penner <hpenner@de.ibm.com>
 * Bugreports.to..: <Linux390@de.ibm.com>
 * (C) IBM Corporation, IBM Deutschland Entwicklung GmbH, 1999,2000
 *
 * $Revision: 1.7 $
 *
 * History of changes
 *
 */

#define MDSK_WRITE_REQ 0x01
#define MDSK_READ_REQ  0x02

#define INIT_BIO        0x00
#define RW_BIO          0x01
#define TERM_BIO        0x02

#define DEV_CLASS_FBA   0x01
#define DEV_CLASS_ECKD  0x04
#define DEV_CLASS_CKD   0x04

typedef struct diag_bio_t {
	u8 type;
	u8 status;
	u16 spare1;
	u32 block_number;
	u32 alet;
	u32 buffer;
} __attribute__ ((packed, aligned (8)))

    diag_bio_t;

typedef struct diag_init_io_t {
	u16 dev_nr;
	u16 spare1[11];
	u32 block_size;
	u32 offset;
	u32 start_block;
	u32 end_block;
	u32 spare2[6];
} __attribute__ ((packed, aligned (8)))

    diag_init_io_t;

typedef struct diag_rw_io_t {
	u16 dev_nr;
	u16 spare1[11];
	u8 key;
	u8 flags;
	u16 spare2;
	u32 block_count;
	u32 alet;
	u32 bio_list;
	u32 interrupt_params;
	u32 spare3[5];
} __attribute__ ((packed, aligned (8)))

    diag_rw_io_t;

int dasd_diag_init (void);
void dasd_diag_cleanup (void);
