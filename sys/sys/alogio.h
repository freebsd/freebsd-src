/*
 * Copyright (c) 1998 Scottibox
 * All rights reserved.
 *
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Industrial Computer Source model AIO8-P
 * 128 channel MUX capability via daisy chained AT-16P units
 * alogio.h, definitions for alog ioctl(), last revised January 6 1998
 * See http://www.scottibox.com
 *     http://www.indcompsrc.com/products/data/html/aio8g-p.html
 *     http://www.indcompsrc.com/products/data/html/at16-p.html
 *
 * Written by: Jamil J. Weatherbee <jamil@scottibox.com>
 *
 */

#ifndef _SYS_ALOGIO_H_
#define _SYS_ALOGIO_H_

#ifndef KERNEL
#include <sys/types.h>
#endif
#include <sys/ioccom.h>

/* Note: By default A/D conversions are started when a channel is open */

/* Halt clocked A/D conversion on an open channel */
#define AD_STOP _IO('A', 100) 
/* Restart clocked A/D conversion on an open channel */
#define AD_START _IO('A', 101)
/* Get the number of entries the fifo for this channel will hold */
#define AD_FIFOSIZE_GET _IOR('A', 102, int)
/* Set the minimum number of entries a fifo must contain before it
 * notifies a poll() or read() that is waiting for it to fill */
#define AD_FIFO_TRIGGER_SET _IOW('A', 103, int)
/* This gets the the fifo trigger setting */
#define AD_FIFO_TRIGGER_GET _IOR('A', 104, int)

#endif
