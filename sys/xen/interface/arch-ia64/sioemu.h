/******************************************************************************
 * sioemu.h
 *
 * Copyright (c) 2008 Tristan Gingold <tgingold@free.fr>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifndef __XEN_PUBLIC_IA64_SIOEMU_H__
#define __XEN_PUBLIC_IA64_SIOEMU_H__

/* SIOEMU specific hypercalls.
   The numbers are the minor part of FW_HYPERCALL_SIOEMU.  */

/* Defines the callback entry point.  r8=ip, r9=data.
   Must be called per-vcpu.  */
#define SIOEMU_HYPERCALL_SET_CALLBACK 0x01

/* Finish sioemu fw initialization and start firmware.  r8=ip.  */
#define SIOEMU_HYPERCALL_START_FW 0x02

/* Add IO pages in physmap.  */
#define SIOEMU_HYPERCALL_ADD_IO_PHYSMAP 0x03

/* Get wallclock time.  */
#define SIOEMU_HYPERCALL_GET_TIME 0x04

/* Flush cache.  */
#define SIOEMU_HYPERCALL_FLUSH_CACHE 0x07

/* Get freq base.  */
#define SIOEMU_HYPERCALL_FREQ_BASE 0x08

/* Return from callback.  */
#define SIOEMU_HYPERCALL_CALLBACK_RETURN 0x09

/* Deliver an interrupt.  */
#define SIOEMU_HYPERCALL_DELIVER_INT 0x0a

/* SIOEMU callback reason.  */

/* An event (from event channel) has to be delivered.  */
#define SIOEMU_CB_EVENT       0x00

/* Emulate an IO access.  */
#define SIOEMU_CB_IO_EMULATE  0x01

/* An IPI is sent to a dead vcpu.  */
#define SIOEMU_CB_WAKEUP_VCPU 0x02

/* A SAL hypercall is executed.  */
#define SIOEMU_CB_SAL_ASSIST  0x03

#ifndef __ASSEMBLY__
struct sioemu_callback_info {
    /* Saved registers.  */
    unsigned long ip;
    unsigned long psr;
    unsigned long ifs;
    unsigned long nats;
    unsigned long r8;
    unsigned long r9;
    unsigned long r10;
    unsigned long r11;

    /* Callback parameters.  */
    unsigned long cause;
    unsigned long arg0;
    unsigned long arg1;
    unsigned long arg2;
    unsigned long arg3;
    unsigned long _pad2[2];
    unsigned long r2;
};
#endif /* __ASSEMBLY__ */
#endif /* __XEN_PUBLIC_IA64_SIOEMU_H__ */
