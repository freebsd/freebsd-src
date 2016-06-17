/* Driver for SCM Microsystems USB-ATAPI cable
 * Header File
 *
 * $Id: shuttle_usbat.h,v 1.5 2000/09/17 14:44:52 groovyjava Exp $
 *
 * Current development and maintenance by:
 *   (c) 2000 Robert Baruch (autophile@dol.net)
 *
 * See scm.c for more explanation
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef _USB_SHUTTLE_USBAT_H
#define _USB_SHUTTLE_USBAT_H

#define USBAT_EPP_PORT		0x10
#define USBAT_EPP_REGISTER	0x30
#define USBAT_ATA		0x40
#define USBAT_ISA		0x50

/* SCM User I/O Data registers */

#define USBAT_UIO_EPAD		0x80 // Enable Peripheral Control Signals
#define USBAT_UIO_CDT		0x40 // Card Detect (Read Only)
				     // CDT = ACKD & !UI1 & !UI0
#define USBAT_UIO_1		0x20 // I/O 1
#define USBAT_UIO_0		0x10 // I/O 0
#define USBAT_UIO_EPP_ATA	0x08 // 1=EPP mode, 0=ATA mode
#define USBAT_UIO_UI1		0x04 // Input 1
#define USBAT_UIO_UI0		0x02 // Input 0
#define USBAT_UIO_INTR_ACK	0x01 // Interrupt (ATA & ISA)/Acknowledge (EPP)

/* SCM User I/O Enable registers */

#define USBAT_UIO_DRVRST	0x80 // Reset Peripheral
#define USBAT_UIO_ACKD		0x40 // Enable Card Detect
#define USBAT_UIO_OE1		0x20 // I/O 1 set=output/clr=input
				     // If ACKD=1, set OE1 to 1 also.
#define USBAT_UIO_OE0		0x10 // I/O 0 set=output/clr=input
#define USBAT_UIO_ADPRST	0x01 // Reset SCM chip

/* USBAT-specific commands */

extern int usbat_read(struct us_data *us, unsigned char access,
	unsigned char reg, unsigned char *content);
extern int usbat_write(struct us_data *us, unsigned char access,
	unsigned char reg, unsigned char content);
extern int usbat_read_block(struct us_data *us, unsigned char access,
	unsigned char reg, unsigned char *content, unsigned short len,
	int use_sg);
extern int usbat_write_block(struct us_data *us, unsigned char access,
	unsigned char reg, unsigned char *content, unsigned short len,
	int use_sg, int minutes);
extern int usbat_multiple_write(struct us_data *us, unsigned char access,
	unsigned char *registers, unsigned char *data_out,
	unsigned short num_registers);
extern int usbat_read_user_io(struct us_data *us, unsigned char *data_flags);
extern int usbat_write_user_io(struct us_data *us,
	unsigned char enable_flags, unsigned char data_flags);

/* HP 8200e stuff */

extern int hp8200e_transport(Scsi_Cmnd *srb, struct us_data *us);
extern int init_8200e(struct us_data *us);

#endif
