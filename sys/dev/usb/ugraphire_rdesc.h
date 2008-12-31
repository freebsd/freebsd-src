/*	$NetBSD: usb/ugraphire_rdesc.h,v 1.1 2000/12/29 01:47:49 augustss Exp $	*/
/*	$FreeBSD: src/sys/dev/usb/ugraphire_rdesc.h,v 1.3.6.1 2008/11/25 02:59:29 kensmith Exp $ */
/*-
 * Copyright (c) 2000 Nick Hibma <n_hibma@freebsd.org>
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
 */

static const uByte uhid_graphire_report_descr[] = {
    0x05, 0x0d,                    /*  USAGE_PAGE (Digitizers)		*/
    0x09, 0x01,                    /*  USAGE (Digitizer)		*/
    0xa1, 0x01,                    /*  COLLECTION (Application)		*/
    0x85, 0x02,                    /*    REPORT_ID (2)			*/
    0x05, 0x0d,                    /*    USAGE_PAGE (Digitizers)	*/
    0x09, 0x01,                    /*    USAGE (Digitizer)		*/
    0xa1, 0x00,                    /*    COLLECTION (Physical)		*/
    0x15, 0x00,                    /*      LOGICAL_MINIMUM (0)		*/
    0x25, 0x01,                    /*      LOGICAL_MAXIMUM (1)		*/
    0x09, 0x33,                    /*      USAGE (Touch)		*/
    0x95, 0x01,                    /*      REPORT_COUNT (1)		*/
    0x75, 0x01,                    /*      REPORT_SIZE (1)		*/
    0x81, 0x02,                    /*      INPUT (Data,Var,Abs)		*/
    0x09, 0x44,                    /*      USAGE (Barrel Switch)	*/
    0x95, 0x02,                    /*      REPORT_COUNT (2)		*/
    0x75, 0x01,                    /*      REPORT_SIZE (1)		*/
    0x81, 0x02,                    /*      INPUT (Data,Var,Abs)		*/
    0x09, 0x00,                    /*      USAGE (Undefined)		*/
    0x95, 0x02,                    /*      REPORT_COUNT (2)		*/
    0x75, 0x01,                    /*      REPORT_SIZE (1)		*/
    0x81, 0x03,                    /*      INPUT (Cnst,Var,Abs)		*/
    0x09, 0x3c,                    /*      USAGE (Invert)		*/
    0x95, 0x01,                    /*      REPORT_COUNT (1)		*/
    0x75, 0x01,                    /*      REPORT_SIZE (1)		*/
    0x81, 0x02,                    /*      INPUT (Data,Var,Abs)		*/
    0x09, 0x38,                    /*      USAGE (Transducer Index)	*/
    0x95, 0x01,                    /*      REPORT_COUNT (1)		*/
    0x75, 0x01,                    /*      REPORT_SIZE (1)		*/
    0x81, 0x02,                    /*      INPUT (Data,Var,Abs)		*/
    0x09, 0x32,                    /*      USAGE (In Range)		*/
    0x95, 0x01,                    /*      REPORT_COUNT (1)		*/
    0x75, 0x01,                    /*      REPORT_SIZE (1)		*/
    0x81, 0x02,                    /*      INPUT (Data,Var,Abs)		*/
    0x05, 0x01,                    /*      USAGE_PAGE (Generic Desktop)	*/
    0x09, 0x30,                    /*      USAGE (X)			*/
    0x15, 0x00,                    /*      LOGICAL_MINIMUM (0)		*/
    0x26, 0xde, 0x27,              /*      LOGICAL_MAXIMUM (10206)	*/
    0x95, 0x01,                    /*      REPORT_COUNT (1)		*/
    0x75, 0x10,                    /*      REPORT_SIZE (16)		*/
    0x81, 0x02,                    /*      INPUT (Data,Var,Abs)		*/
    0x09, 0x31,                    /*      USAGE (Y)			*/
    0x26, 0xfe, 0x1c,              /*      LOGICAL_MAXIMUM (7422)	*/
    0x95, 0x01,                    /*      REPORT_COUNT (1)		*/
    0x75, 0x10,                    /*      REPORT_SIZE (16)		*/
    0x81, 0x02,                    /*      INPUT (Data,Var,Abs)		*/
    0x05, 0x0d,                    /*      USAGE_PAGE (Digitizers)	*/
    0x09, 0x30,                    /*      USAGE (Tip Pressure)		*/
    0x26, 0xff, 0x01,              /*      LOGICAL_MAXIMUM (511)	*/
    0x95, 0x01,                    /*      REPORT_COUNT (1)		*/
    0x75, 0x10,                    /*      REPORT_SIZE (16)		*/
    0x81, 0x02,                    /*      INPUT (Data,Var,Abs)		*/
    0xc0,                          /*    END_COLLECTION			*/
    0x05, 0x0d,                    /*    USAGE_PAGE (Digitizers)	*/
    0x09, 0x00,                    /*    USAGE (Undefined)		*/
    0x85, 0x02,                    /*    REPORT_ID (2)			*/
    0x95, 0x01,                    /*    REPORT_COUNT (1)		*/
    0xb1, 0x02,                    /*    FEATURE (Data,Var,Abs)		*/
    0x09, 0x00,                    /*    USAGE (Undefined)		*/
    0x85, 0x03,                    /*    REPORT_ID (3)			*/
    0x95, 0x01,                    /*    REPORT_COUNT (1)		*/
    0xb1, 0x02,                    /*    FEATURE (Data,Var,Abs)		*/
    0xc0,                          /*  END_COLLECTION			*/
};

static const uByte uhid_graphire3_4x5_report_descr[] = {
    0x05, 0x01,                    /* USAGE_PAGE (Generic Desktop)	*/
    0x09, 0x02,                    /* USAGE (Mouse)			*/
    0xa1, 0x01,                    /* COLLECTION (Application)		*/
    0x85, 0x01,                    /*   REPORT_ID (1)			*/
    0x09, 0x01,                    /*   USAGE (Pointer)			*/
    0xa1, 0x00,                    /*   COLLECTION (Physical)		*/
    0x05, 0x09,                    /*     USAGE_PAGE (Button)		*/
    0x19, 0x01,                    /*     USAGE_MINIMUM (Button 1)	*/
    0x29, 0x03,                    /*     USAGE_MAXIMUM (Button 3)	*/
    0x15, 0x00,                    /*     LOGICAL_MINIMUM (0)		*/
    0x25, 0x01,                    /*     LOGICAL_MAXIMUM (1)		*/
    0x95, 0x03,                    /*     REPORT_COUNT (3)		*/
    0x75, 0x01,                    /*     REPORT_SIZE (1)		*/
    0x81, 0x02,                    /*     INPUT (Data,Var,Abs)		*/
    0x95, 0x01,                    /*     REPORT_COUNT (1)		*/
    0x75, 0x05,                    /*     REPORT_SIZE (5)		*/
    0x81, 0x01,                    /*     INPUT (Cnst,Ary,Abs)		*/
    0x05, 0x01,                    /*     USAGE_PAGE (Generic Desktop)	*/
    0x09, 0x30,                    /*     USAGE (X)			*/
    0x09, 0x31,                    /*     USAGE (Y)			*/
    0x09, 0x38,                    /*     USAGE (Wheel)			*/
    0x15, 0x81,                    /*     LOGICAL_MINIMUM (-127)	*/
    0x25, 0x7f,                    /*     LOGICAL_MAXIMUM (127)		*/
    0x75, 0x08,                    /*     REPORT_SIZE (8)		*/
    0x95, 0x03,                    /*     REPORT_COUNT (3)		*/
    0x81, 0x06,                    /*     INPUT (Data,Var,Rel)		*/
    0xc0,                          /*   END_COLLECTION			*/
    0xc0,                          /* END_COLLECTION			*/
    0x05, 0x0d,                    /* USAGE_PAGE (Digitizers)		*/
    0x09, 0x01,                    /* USAGE (Pointer)			*/
    0xa1, 0x01,                    /* COLLECTION (Applicaption)		*/
    0x85, 0x02,                    /*   REPORT_ID (2)			*/
    0x05, 0x0d,                    /*   USAGE_PAGE (Digitizers)		*/
    0x09, 0x01,                    /*   USAGE (Digitizer)		*/
    0xa1, 0x00,                    /*   COLLECTION (Physical)		*/
    0x09, 0x33,                    /*     USAGE (Touch)			*/
    0x09, 0x44,                    /*     USAGE (Barrel Switch)		*/
    0x09, 0x44,                    /*     USAGE (Barrel Switch)		*/
    0x15, 0x00,                    /*     LOGICAL_MINIMUM (0)		*/
    0x25, 0x01,                    /*     LOGICAL_MAXIMUM (1)		*/
    0x75, 0x01,                    /*     REPORT_SIZE (1)		*/
    0x95, 0x03,                    /*     REPORT_COUNT (3)		*/
    0x81, 0x02,                    /*     INPUT (Data,Var,Abs)		*/
    0x75, 0x01,                    /*     REPORT_SIZE (1)		*/
    0x95, 0x02,                    /*     REPORT_COUNT (2)		*/
    0x81, 0x01,                    /*     INPUT (Cnst,Ary,Abs)		*/
    0x09, 0x3c,                    /*     USAGE (Invert)		*/
    0x09, 0x38,                    /*     USAGE (Transducer Index)	*/
    0x09, 0x32,                    /*     USAGE (In Range)		*/
    0x75, 0x01,                    /*     REPORT_SIZE (1)		*/
    0x95, 0x03,                    /*     REPORT_COUNT (3)		*/
    0x81, 0x02,                    /*     INPUT (Data,Var,Abs)		*/
    0x05, 0x01,                    /*     USAGE_PAGE (Generic Desktop)	*/
    0x09, 0x30,                    /*     USAGE (X)			*/
    0x15, 0x00,                    /*     LOGICAL_MINIMUM (0)		*/
    0x26, 0xde, 0x27,              /*     LOGICAL_MAXIMUM (10206)	*/
    0x75, 0x10,                    /*     REPORT_SIZE (16)		*/
    0x95, 0x01,                    /*     REPORT_COUNT (1)		*/
    0x81, 0x02,                    /*     INPUT (Data,Var,Abs)		*/
    0x09, 0x31,                    /*     USAGE (Y)			*/
    0x26, 0xfe, 0x1c,              /*     LOGICAL_MAXIMUM (7422)	*/
    0x75, 0x10,                    /*     REPORT_SIZE (16)		*/
    0x95, 0x01,                    /*     REPORT_COUNT (1)		*/
    0x81, 0x02,                    /*     INPUT (Data,Var,Abs)		*/
    0x05, 0x0d,                    /*     USAGE_PAGE (Digitizers)	*/
    0x09, 0x30,                    /*     USAGE (Tip Pressure)		*/
    0x26, 0xff, 0x01,              /*     LOGICAL_MAXIMUM (511)		*/
    0x75, 0x10,                    /*     REPORT_SIZE (16)		*/
    0x95, 0x01,                    /*     REPORT_COUNT (1)		*/
    0x81, 0x02,                    /*     INPUT (Data,Var,Abs)		*/
    0xc0,                          /*   END_COLLECTION			*/
    0x05, 0x0d,                    /*   USAGE_PAGE (Digitizers)		*/
    0x09, 0x00,                    /*   USAGE (Undefined)		*/
    0x85, 0x02,                    /*   REPORT_ID (2)			*/
    0x95, 0x01,                    /*   REPORT_COUNT (1)		*/
    0xb1, 0x02,                    /*   FEATURE (Data,Var,Abs)		*/
    0x09, 0x00,                    /*   USAGE (Undefined)		*/
    0x85, 0x03,                    /*   REPORT_ID (3)			*/
    0x95, 0x01,                    /*   REPORT_COUNT (1)		*/
    0xb1, 0x02,                    /*   FEATURE (Data,Var,Abs)		*/
    0xc0                           /* END_COLLECTION			*/
};
