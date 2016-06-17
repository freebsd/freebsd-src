/*
 * Driver for USB Scanners (linux-2.4)
 *
 * Copyright (C) 1999, 2000, 2001, 2002 David E. Nelson
 * Previously maintained by Brian Beattie
 *
 * Current maintainer: Henning Meier-Geinitz <henning@meier-geinitz.de>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */ 

/*
 * For documentation, see Documentation/usb/scanner.txt.
 * Website: http://www.meier-geinitz.de/kernel/
 * Please contact the maintainer if your scanner is not detected by this
 * driver automatically.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <asm/uaccess.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/ioctl.h>
#include <linux/sched.h>
#include <linux/smp_lock.h>
#include <linux/devfs_fs_kernel.h>
#include <linux/usb_scanner_ioctl.h>

// #define DEBUG

#define DRIVER_VERSION "0.4.16"
#define DRIVER_DESC "USB Scanner Driver"

#include <linux/usb.h>

static __s32 vendor=-1, product=-1, read_timeout=0;

MODULE_AUTHOR("Henning Meier-Geinitz, henning@meier-geinitz.de");
MODULE_DESCRIPTION(DRIVER_DESC" "DRIVER_VERSION);
MODULE_LICENSE("GPL");

MODULE_PARM(vendor, "i");
MODULE_PARM_DESC(vendor, "User specified USB idVendor");

MODULE_PARM(product, "i");
MODULE_PARM_DESC(product, "User specified USB idProduct");

MODULE_PARM(read_timeout, "i");
MODULE_PARM_DESC(read_timeout, "User specified read timeout in seconds");


/* WARNING: These DATA_DUMP's can produce a lot of data. Caveat Emptor. */
// #define RD_DATA_DUMP /* Enable to dump data - limited to 24 bytes */
// #define WR_DATA_DUMP /* DEBUG does not have to be defined. */

static struct usb_device_id scanner_device_ids [] = {
	/* Acer (now Benq) */
	{ USB_DEVICE(0x04a5, 0x1a20) },	/* Prisa 310U */
	{ USB_DEVICE(0x04a5, 0x1a2a) },	/* Another 620U */
	{ USB_DEVICE(0x04a5, 0x2022) },	/* 340U */
	{ USB_DEVICE(0x04a5, 0x2040) },	/* 620U (!) */
	{ USB_DEVICE(0x04a5, 0x2060) },	/* 620U & 640U (!)*/
	{ USB_DEVICE(0x04a5, 0x207e) },	/* 640BU */
	{ USB_DEVICE(0x04a5, 0x20b0) },	/* Benq 4300 */
	{ USB_DEVICE(0x04a5, 0x20be) },	/* Unknown - Oliver Schwartz */
	{ USB_DEVICE(0x04a5, 0x20c0) }, /* 1240UT, 1240U */
	{ USB_DEVICE(0x04a5, 0x20de) },	/* S2W 3300U */
	{ USB_DEVICE(0x04a5, 0x20fc) }, /* Benq 5000 */
	{ USB_DEVICE(0x04a5, 0x20fe) },	/* Benq 5300 */
	/* Agfa */
	{ USB_DEVICE(0x06bd, 0x0001) },	/* SnapScan 1212U */
	{ USB_DEVICE(0x06bd, 0x0002) },	/* SnapScan 1236U */
	{ USB_DEVICE(0x06bd, 0x0100) },	/* SnapScan Touch */
	{ USB_DEVICE(0x06bd, 0x2061) },	/* Another SnapScan 1212U (?)*/
	{ USB_DEVICE(0x06bd, 0x208d) }, /* Snapscan e40 */
	{ USB_DEVICE(0x06bd, 0x208f) }, /* SnapScan e50*/
	{ USB_DEVICE(0x06bd, 0x2091) }, /* SnapScan e20 */
	{ USB_DEVICE(0x06bd, 0x2093) }, /* SnapScan e10*/
	{ USB_DEVICE(0x06bd, 0x2095) }, /* SnapScan e25 */
	{ USB_DEVICE(0x06bd, 0x2097) }, /* SnapScan e26 */
	{ USB_DEVICE(0x06bd, 0x20fd) }, /* SnapScan e52*/
	{ USB_DEVICE(0x06bd, 0x20ff) }, /* SnapScan e42*/
	/* Artec */
	{ USB_DEVICE(0x05d8, 0x4001) },	/* Ultima 2000 */
	{ USB_DEVICE(0x05d8, 0x4002) }, /* Ultima 2000 (GT6801 based) */
	{ USB_DEVICE(0x05d8, 0x4003) }, /* E+ 48U */
	{ USB_DEVICE(0x05d8, 0x4004) }, /* E+ Pro */
	/* Avision */
	{ USB_DEVICE(0x0638, 0x0268) }, /* iVina 1200U */
	{ USB_DEVICE(0x0638, 0x0a10) },	/* iVina FB1600 (=Umax Astra 4500) */
	{ USB_DEVICE(0x0638, 0x0a20) }, /* iVina FB1800 (=Umax Astra 4700) */
	/* Benq: see Acer */
	/* Brother */
	{ USB_DEVICE(0x04f9, 0x010f) },	/* MFC 5100C */
	{ USB_DEVICE(0x04f9, 0x0111) },	/* MFC 6800 */
	/* Canon */
	{ USB_DEVICE(0x04a9, 0x2201) }, /* CanoScan FB320U */
	{ USB_DEVICE(0x04a9, 0x2202) }, /* CanoScan FB620U */
	{ USB_DEVICE(0x04a9, 0x2204) }, /* CanoScan FB630U/FB636U */
	{ USB_DEVICE(0x04a9, 0x2205) }, /* CanoScan FB1210U */
	{ USB_DEVICE(0x04a9, 0x2206) }, /* CanoScan N650U/N656U */
	{ USB_DEVICE(0x04a9, 0x2207) }, /* CanoScan N1220U */
	{ USB_DEVICE(0x04a9, 0x2208) }, /* CanoScan D660U */ 
	{ USB_DEVICE(0x04a9, 0x220a) },	/* CanoScan D2400UF */
	{ USB_DEVICE(0x04a9, 0x220b) }, /* CanoScan D646U */
	{ USB_DEVICE(0x04a9, 0x220c) },	/* CanoScan D1250U2 */
	{ USB_DEVICE(0x04a9, 0x220d) }, /* CanoScan N670U/N676U/LIDE 20 */
	{ USB_DEVICE(0x04a9, 0x220e) }, /* CanoScan N1240U/LIDE 30 */
	{ USB_DEVICE(0x04a9, 0x220f) },	/* CanoScan 8000F */
	{ USB_DEVICE(0x04a9, 0x2210) },	/* CanoScan 9900F */
	{ USB_DEVICE(0x04a9, 0x2212) },	/* CanoScan 5000F */
	{ USB_DEVICE(0x04a9, 0x2213) },	/* LIDE 50 */
	{ USB_DEVICE(0x04a9, 0x2215) },	/* CanoScan 3000 */
	{ USB_DEVICE(0x04a9, 0x3042) }, /* FS4000US */
	/* Colorado -- See Primax/Colorado below */
	/* Compaq */
	{ USB_DEVICE(0x049f, 0x001a) },	/* S4 100 */
	{ USB_DEVICE(0x049f, 0x0021) },	/* S200 */
	/* Epson -- See Seiko/Epson below */
	/* Fujitsu */
	{ USB_DEVICE(0x04c5, 0x1041) }, /* fi-4220c USB/SCSI info:mza@mu-tec.de */
	{ USB_DEVICE(0x04c5, 0x1042) }, /* fi-4120c USB/SCSI info:mza@mu-tec.de */
	{ USB_DEVICE(0x04c5, 0x1029) }, /* fi-4010c USB AVision info:mza@mu-tec.de */
	/* Genius */
	{ USB_DEVICE(0x0458, 0x2001) },	/* ColorPage Vivid Pro */
	{ USB_DEVICE(0x0458, 0x2007) },	/* ColorPage HR6 V2 */
	{ USB_DEVICE(0x0458, 0x2008) }, /* ColorPage HR6 V2 */
	{ USB_DEVICE(0x0458, 0x2009) }, /* ColorPage HR6A */
	{ USB_DEVICE(0x0458, 0x2011) }, /* ColorPage Vivid3x */
	{ USB_DEVICE(0x0458, 0x2013) }, /* ColorPage HR7 */
	{ USB_DEVICE(0x0458, 0x2015) }, /* ColorPage HR7LE */
	{ USB_DEVICE(0x0458, 0x2016) }, /* ColorPage HR6X */
	{ USB_DEVICE(0x0458, 0x2018) },	/* ColorPage HR7X */
	{ USB_DEVICE(0x0458, 0x201b) },	/* Colorpage Vivid 4x */
	/* Hewlett Packard */
	/* IMPORTANT: Hewlett-Packard multi-function peripherals (OfficeJet, 
	   Printer/Scanner/Copier (PSC), LaserJet, or PhotoSmart printer)
	   should not be added to this table because they are accessed by a
	   userspace driver (hpoj) */
	{ USB_DEVICE(0x03f0, 0x0101) },	/* ScanJet 4100C */
	{ USB_DEVICE(0x03f0, 0x0102) },	/* PhotoSmart S20 */
	{ USB_DEVICE(0x03f0, 0x0105) },	/* ScanJet 4200C */
	{ USB_DEVICE(0x03f0, 0x0201) },	/* ScanJet 6200C */
	{ USB_DEVICE(0x03f0, 0x0205) },	/* ScanJet 3300C */
	{ USB_DEVICE(0x03f0, 0x0305) }, /* ScanJet 4300C */
	{ USB_DEVICE(0x03f0, 0x0401) },	/* ScanJet 5200C */
	{ USB_DEVICE(0x03f0, 0x0405) }, /* ScanJet 3400C */
	{ USB_DEVICE(0x03f0, 0x0505) }, /* ScanJet 2100C */
	{ USB_DEVICE(0x03f0, 0x0601) },	/* ScanJet 6300C */
	{ USB_DEVICE(0x03f0, 0x0605) },	/* ScanJet 2200C */
	//	{ USB_DEVICE(0x03f0, 0x0701) },	/* ScanJet 5300C - NOT SUPPORTED - use hpusbscsi driver */
	{ USB_DEVICE(0x03f0, 0x0705) }, /* ScanJet 4400C */
	//	{ USB_DEVICE(0x03f0, 0x0801) },	/* ScanJet 7400C - NOT SUPPORTED - use hpusbscsi driver */
	{ USB_DEVICE(0x03f0, 0x0805) },	/* ScanJet 4470c */
	{ USB_DEVICE(0x03f0, 0x0901) }, /* ScanJet 2300C */
	{ USB_DEVICE(0x03f0, 0x0a01) },	/* ScanJet 2400c */
	{ USB_DEVICE(0x03F0, 0x1005) },	/* ScanJet 5400C */
	{ USB_DEVICE(0x03F0, 0x1105) },	/* ScanJet 5470C */
	{ USB_DEVICE(0x03f0, 0x1205) }, /* ScanJet 5550C */
	{ USB_DEVICE(0x03f0, 0x1305) },	/* Scanjet 4570c */
	//	{ USB_DEVICE(0x03f0, 0x1411) }, /* PSC 750 - NOT SUPPORTED - use hpoj userspace driver */
	{ USB_DEVICE(0x03f0, 0x2005) },	/* ScanJet 3570c */
	{ USB_DEVICE(0x03f0, 0x2205) },	/* ScanJet 3500c */
	//	{ USB_DEVICE(0x03f0, 0x2f11) }, /* PSC 1210 - NOT SUPPORTED - use hpoj userspace driver */
	/* Lexmark */
	{ USB_DEVICE(0x043d, 0x002d) }, /* X70/X73 */
	{ USB_DEVICE(0x043d, 0x003d) }, /* X83 */
	/* LG Electronics */
	{ USB_DEVICE(0x0461, 0x0364) },	/* Scanworks 600U (repackaged Primax?) */
	/* Medion */
	{ USB_DEVICE(0x0461, 0x0377) },	/* MD 5345 - repackaged Primax? */
	/* Memorex */
	{ USB_DEVICE(0x0461, 0x0346) }, /* 6136u - repackaged Primax ? */
	/* Microtek */
	{ USB_DEVICE(0x05da, 0x20a7) },	/* ScanMaker 5600 */
	{ USB_DEVICE(0x05da, 0x20c9) }, /* ScanMaker 6700 */
	{ USB_DEVICE(0x05da, 0x30ce) },	/* ScanMaker 3800 */
	{ USB_DEVICE(0x05da, 0x30cf) },	/* ScanMaker 4800 */
	{ USB_DEVICE(0x05da, 0x30d4) },	/* ScanMaker 3830 + 3840 */
	{ USB_DEVICE(0x05da, 0x30d8) },	/* ScanMaker 5900 */
	{ USB_DEVICE(0x04a7, 0x0224) },	/* Scanport 3000 (actually Visioneer?)*/
	/* The following SCSI-over-USB Microtek devices are supported by the
	   microtek driver: Enable SCSI and USB Microtek in kernel config */
	//	{ USB_DEVICE(0x05da, 0x0099) },	/* ScanMaker X6 - X6U */
	//	{ USB_DEVICE(0x05da, 0x0094) },	/* Phantom 336CX - C3 */
	//	{ USB_DEVICE(0x05da, 0x00a0) },	/* Phantom 336CX - C3 #2 */
	//	{ USB_DEVICE(0x05da, 0x009a) },	/* Phantom C6 */
	//	{ USB_DEVICE(0x05da, 0x00a3) },	/* ScanMaker V6USL */
	//	{ USB_DEVICE(0x05da, 0x80a3) },	/* ScanMaker V6USL #2 */
	//	{ USB_DEVICE(0x05da, 0x80ac) },	/* ScanMaker V6UL - SpicyU */
	/* Minolta */
	{ USB_DEVICE(0x0686, 0x400d) }, /* Scan Dual III */
	/* The following SCSI-over-USB Minolta devices are supported by the
	   hpusbscsi driver: Enable SCSI and USB hpusbscsi in kernel config */
	//	{ USB_DEVICE(0x0638, 0x026a) }, /* Minolta Dimage Scan Dual II */
	//	{ USB_DEVICE(0x0686, 0x4004) }, /* Scan Elite II (need interrupt ep) */
	/* Mustek */
	{ USB_DEVICE(0x0400, 0x1000) },	/* BearPaw 1200 (National Semiconductor LM9831) */
	{ USB_DEVICE(0x0400, 0x1001) }, /* BearPaw 2400 (National Semiconductor LM9832) */
	{ USB_DEVICE(0x055f, 0x0001) },	/* ScanExpress 1200 CU */
	{ USB_DEVICE(0x055f, 0x0002) },	/* ScanExpress 600 CU */
	{ USB_DEVICE(0x055f, 0x0003) },	/* ScanExpress 1200 USB */
	{ USB_DEVICE(0x055f, 0x0006) },	/* ScanExpress 1200 UB */
	{ USB_DEVICE(0x055f, 0x0007) },	/* ScanExpress 1200 USB Plus */
	{ USB_DEVICE(0x055f, 0x0008) }, /* ScanExpress 1200 CU Plus */
	{ USB_DEVICE(0x055f, 0x0010) }, /* BearPaw 1200F */
	{ USB_DEVICE(0x055f, 0x0210) },	/* ScanExpress A3 USB */
	{ USB_DEVICE(0x055f, 0x0218) }, /* BearPaw 2400 TA */
	{ USB_DEVICE(0x055f, 0x0219) }, /* BearPaw 2400 TA Plus */
	{ USB_DEVICE(0x055f, 0x021c) }, /* BearPaw 1200 CU Plus */
	{ USB_DEVICE(0x055f, 0x021d) }, /* Bearpaw 2400 CU Plus */
	{ USB_DEVICE(0x055f, 0x021e) }, /* BearPaw 1200 TA/CS */
	{ USB_DEVICE(0x055f, 0x0400) }, /* BearPaw 2400 TA PRO */
	{ USB_DEVICE(0x055f, 0x0401) },	/* P 3600 A3 Pro */
	{ USB_DEVICE(0x055f, 0x0409) },	/* BearPaw 2448TA Pro */
	{ USB_DEVICE(0x055f, 0x0873) }, /* ScanExpress 600 USB */
	{ USB_DEVICE(0x055f, 0x1000) }, /* BearPaw 4800 TA PRO */
	//	{ USB_DEVICE(0x05d8, 0x4002) }, /* BearPaw 1200 CU and ScanExpress 1200 UB Plus (see Artec) */
	/* Nikon */
	{ USB_DEVICE(0x04b0, 0x4000) }, /* Coolscan LS 40 ED */
	/* Pacific Image Electronics */
	{ USB_DEVICE(0x05e3, 0x0120) },	/* PrimeFilm 1800u */
	/* Plustek */
	{ USB_DEVICE(0x07b3, 0x0001) },	/* 1212U */
	{ USB_DEVICE(0x07b3, 0x0005) }, /* Unknown */
	{ USB_DEVICE(0x07b3, 0x0007) }, /* Unknown */
	{ USB_DEVICE(0x07b3, 0x000F) }, /* Unknown */
	{ USB_DEVICE(0x07b3, 0x0010) }, /* OpticPro U12 */
	{ USB_DEVICE(0x07b3, 0x0011) }, /* OpticPro U24 */
	{ USB_DEVICE(0x07b3, 0x0012) }, /* Unknown */
	{ USB_DEVICE(0x07b3, 0x0013) }, /* UT12 */
	{ USB_DEVICE(0x07b3, 0x0014) }, /* Unknown */
	{ USB_DEVICE(0x07b3, 0x0015) }, /* OpticPro U24 */
	{ USB_DEVICE(0x07b3, 0x0016) }, /* Unknown */
	{ USB_DEVICE(0x07b3, 0x0017) }, /* OpticPro UT12/UT16/UT24 */
	{ USB_DEVICE(0x07b3, 0x0400) }, /* OpticPro 1248U */
	{ USB_DEVICE(0x07b3, 0x0401) }, /* OpticPro 1248U (another one) */
	{ USB_DEVICE(0x07b3, 0x0403) },	/* U16B */
	{ USB_DEVICE(0x07b3, 0x0413) },	/* OpticSlim 1200 */
	/* Primax/Colorado */
	{ USB_DEVICE(0x0461, 0x0300) },	/* G2-300 #1 */
	{ USB_DEVICE(0x0461, 0x0301) },	/* G2E-300 #1 */
	{ USB_DEVICE(0x0461, 0x0302) },	/* G2-300 #2 */
	{ USB_DEVICE(0x0461, 0x0303) },	/* G2E-300 #2 */
	{ USB_DEVICE(0x0461, 0x0340) },	/* Colorado USB 9600 */
	{ USB_DEVICE(0x0461, 0x0341) },	/* Colorado 600u */
	{ USB_DEVICE(0x0461, 0x0347) },	/* Primascan Colorado 2600u */
	{ USB_DEVICE(0x0461, 0x0360) },	/* Colorado USB 19200 */
	{ USB_DEVICE(0x0461, 0x0361) },	/* Colorado 1200u */
	{ USB_DEVICE(0x0461, 0x0380) },	/* G2-600 #1 */
	{ USB_DEVICE(0x0461, 0x0381) },	/* ReadyScan 636i */
	{ USB_DEVICE(0x0461, 0x0382) },	/* G2-600 #2 */
	{ USB_DEVICE(0x0461, 0x0383) },	/* G2E-600 */
	/* Prolink */
	{ USB_DEVICE(0x06dc, 0x0014) }, /* Winscan Pro 2448U */
	/* Reflecta  */
	{ USB_DEVICE(0x05e3, 0x0120) },	/* iScan 1800 */
	/* Relisis */
	// { USB_DEVICE(0x0475, 0x0103) },	/* Episode - undetected endpoint */
	{ USB_DEVICE(0x0475, 0x0210) }, /* Scorpio Ultra 3 */
	/* Seiko/Epson Corp. */
	{ USB_DEVICE(0x04b8, 0x0101) },	/* Perfection 636U and 636Photo */
	{ USB_DEVICE(0x04b8, 0x0102) }, /* GT-2200 */
	{ USB_DEVICE(0x04b8, 0x0103) },	/* Perfection 610 */
	{ USB_DEVICE(0x04b8, 0x0104) },	/* Perfection 1200U and 1200Photo*/
	{ USB_DEVICE(0x04b8, 0x0105) }, /* StylusScan 2000 */
	{ USB_DEVICE(0x04b8, 0x0106) },	/* Stylus Scan 2500 */
	{ USB_DEVICE(0x04b8, 0x0107) },	/* Expression 1600 */
	{ USB_DEVICE(0x04b8, 0x0109) }, /* Expression 1640XL */
	{ USB_DEVICE(0x04b8, 0x010a) }, /* Perfection 1640SU and 1640SU Photo */
	{ USB_DEVICE(0x04b8, 0x010b) }, /* Perfection 1240U */
	{ USB_DEVICE(0x04b8, 0x010c) }, /* Perfection 640U */
	{ USB_DEVICE(0x04b8, 0x010e) }, /* Expression 1680 */
	{ USB_DEVICE(0x04b8, 0x010f) }, /* Perfection 1250U */
	{ USB_DEVICE(0x04b8, 0x0110) }, /* Perfection 1650 */
	{ USB_DEVICE(0x04b8, 0x0112) }, /* Perfection 2450 - GT-9700 for the Japanese mkt */
	{ USB_DEVICE(0x04b8, 0x0114) }, /* Perfection 660 */
	{ USB_DEVICE(0x04b8, 0x011b) }, /* Perfection 2400 Photo */
	{ USB_DEVICE(0x04b8, 0x011c) }, /* Perfection 3200 */
	{ USB_DEVICE(0x04b8, 0x011d) }, /* Perfection 1260 */
	{ USB_DEVICE(0x04b8, 0x011e) }, /* Perfection 1660 Photo */
	{ USB_DEVICE(0x04b8, 0x011f) },	/* Perfection 1670 */
	{ USB_DEVICE(0x04b8, 0x0801) }, /* Stylus CX5200 */
	{ USB_DEVICE(0x04b8, 0x0802) }, /* Stylus CX3200 */
	/* Siemens */
	{ USB_DEVICE(0x0681, 0x0005) },	/* ID Mouse Professional */
	{ USB_DEVICE(0x0681, 0x0010) },	/* Cherry FingerTIP ID Board - Sensor */
	/* SYSCAN */
	{ USB_DEVICE(0x0a82, 0x4600) }, /* TravelScan 460/464 */
	/* Trust */
	{ USB_DEVICE(0x05cb, 0x1483) }, /* CombiScan 19200 */
	{ USB_DEVICE(0x05d8, 0x4006) }, /* Easy Webscan 19200 (repackaged Artec?) */
	/* Umax */
	{ USB_DEVICE(0x05d8, 0x4009) },	/* Astraslim (actually Artec?) */
	{ USB_DEVICE(0x1606, 0x0010) },	/* Astra 1220U */
	{ USB_DEVICE(0x1606, 0x0030) },	/* Astra 2000U */
	{ USB_DEVICE(0x1606, 0x0060) }, /* Astra 3400U/3450U */
	{ USB_DEVICE(0x1606, 0x0070) },	/* Astra 4400 */
	{ USB_DEVICE(0x1606, 0x0130) }, /* Astra 2100U */
	{ USB_DEVICE(0x1606, 0x0160) }, /* Astra 5400U */  
	{ USB_DEVICE(0x1606, 0x0230) },	/* Astra 2200U */
	/* Visioneer */
	{ USB_DEVICE(0x04a7, 0x0211) },	/* OneTouch 7600 USB */
	{ USB_DEVICE(0x04a7, 0x0221) },	/* OneTouch 5300 USB */
	{ USB_DEVICE(0x04a7, 0x0224) },	/* OneTouch 4800 USB */
	{ USB_DEVICE(0x04a7, 0x0226) },	/* OneTouch 5800 USB */
	{ USB_DEVICE(0x04a7, 0x0229) }, /* OneTouch 7100 USB */
	{ USB_DEVICE(0x04a7, 0x022c) },	/* OneTouch 9020 USB */
	{ USB_DEVICE(0x04a7, 0x0231) },	/* 6100 USB */
	{ USB_DEVICE(0x04a7, 0x0311) },	/* 6200 EPP/USB */
	{ USB_DEVICE(0x04a7, 0x0321) },	/* OneTouch 8100 EPP/USB */
	{ USB_DEVICE(0x04a7, 0x0331) }, /* OneTouch 8600 EPP/USB */
	{ USB_DEVICE(0x0461, 0x0345) }, /* 6200 (actually Primax?) */
	{ USB_DEVICE(0x0461, 0x0371) }, /* Onetouch 8920 USB (actually Primax?) */
	{ }				/* Terminating entry */
};

MODULE_DEVICE_TABLE (usb, scanner_device_ids);

#define IS_EP_BULK(ep)  ((ep).bmAttributes == USB_ENDPOINT_XFER_BULK ? 1 : 0)
#define IS_EP_BULK_IN(ep) (IS_EP_BULK(ep) && ((ep).bEndpointAddress & USB_ENDPOINT_DIR_MASK) == USB_DIR_IN)
#define IS_EP_BULK_OUT(ep) (IS_EP_BULK(ep) && ((ep).bEndpointAddress & USB_ENDPOINT_DIR_MASK) == USB_DIR_OUT)
#define IS_EP_INTR(ep) ((ep).bmAttributes == USB_ENDPOINT_XFER_INT ? 1 : 0)

#define USB_SCN_MINOR(X) MINOR((X)->i_rdev) - SCN_BASE_MNR

#ifdef DEBUG
#define SCN_DEBUG(X) X
#else
#define SCN_DEBUG(X)
#endif

#define IBUF_SIZE 32768
#define OBUF_SIZE 4096

/* read_scanner timeouts -- RD_NAK_TIMEOUT * RD_EXPIRE = Number of seconds */
#define RD_NAK_TIMEOUT (10*HZ)	/* Default number of X seconds to wait */
#define RD_EXPIRE 12		/* Number of attempts to wait X seconds */


/* USB bInterfaceClass used by Hewlett-Packard ScanJet 3300c and Genius HR6
   USB - Vivid III */
#define SCN_CLASS_SCANJET 16

#define SCN_MAX_MNR 16		/* We're allocated 16 minors */
#define SCN_BASE_MNR 48		/* USB Scanners start at minor 48 */

static DECLARE_MUTEX (scn_mutex); /* Initializes to unlocked */

struct scn_usb_data {
	struct usb_device *scn_dev;
	devfs_handle_t devfs;	/* devfs device */
	struct urb scn_irq;
	unsigned int ifnum;	/* Interface number of the USB device */
	kdev_t scn_minor;	/* Scanner minor - used in disconnect() */
	unsigned char button;	/* Front panel buffer */
	char isopen;		/* Not zero if the device is open */
	char present;		/* Not zero if device is present */
	char *obuf, *ibuf;	/* transfer buffers */
	char bulk_in_ep, bulk_out_ep, intr_ep; /* Endpoint assignments */
	wait_queue_head_t rd_wait_q; /* read timeouts */
	struct semaphore sem; /* lock to prevent concurrent reads or writes */
	unsigned int rd_nak_timeout; /* Seconds to wait before read() timeout. */
};

extern devfs_handle_t usb_devfs_handle;

static struct scn_usb_data *p_scn_table[SCN_MAX_MNR] = { NULL, /* ... */};

static struct usb_driver scanner_driver;
