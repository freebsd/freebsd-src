/* 
 * Contributed by HD Associates (hd@world.std.com).
 * Copyright (c) 1992, 1993 HD Associates
 *
 * Berkeley style copyright.  I've just snarfed it out of stdio.h:
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from: @(#)stdio.h	5.17 (Berkeley) 6/3/91
 *	$Id: scsi_generic.h,v 1.2 1993/10/16 17:21:05 rgrimes Exp $
 */

/* generic SCSI header file.  We use the same minor number format
 * as on SGI except that the flag bits aren't available because they
 * are used as the board index.
 *
 * The minor number format is:
 *  FF UUU III (FFUU UIII)
 *
 * Where:
 *  FF is the board index
 * UUU are the LUN
 * III is the SCSI ID (controller)
 */

#ifndef _SCSI_GENERIC_H_
#define _SCSI_GENERIC_H_

#define G_SCSI_FLAG(DEV) (((DEV) & 0xC0) >> 6)
#define G_SCSI_UNIT(DEV) G_SCSI_FLAG(DEV)
#define G_SCSI_LUN(DEV) (((DEV) & 0x38) >> 3)
#define G_SCSI_ID(DEV) ((DEV) & 0x7)

#define G_SCSI_MINOR(FLAG, LUN, ID)  \
 (((FLAG) << 6) | ((LUN) << 3) | (ID))

#endif /* _SCSI_GENERIC_H_ */
