/*
 * Copyright (C) 1995, HD Associates, Inc.
 * PO Box 276
 * Pepperell, MA 01463
 * 508 433 5266
 * dufault@hda.com
 *
 * This code is contributed to the University of California at Berkeley:
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
 * $Id: scsi_driver.h,v 1.6 1996/03/10 07:13:08 gibbs Exp $
 *
 */
#ifndef _SCSI__DRIVER_H_
#define _SCSI__DRIVER_H_

#ifdef KERNEL

#include <sys/conf.h>

struct kern_devconf;
struct scsi_link;
struct scsi_device;
struct buf;
struct proc;

int  scsi_goaway __P((int));
int  scsi_device_attach __P((struct scsi_link *));

int  scsi_open __P((dev_t, int, int, struct proc *, struct scsi_device *));
int  scsi_close __P((dev_t, int, int, struct proc *, struct scsi_device *));
int  scsi_ioctl __P((dev_t, u_int32_t, caddr_t, int, struct proc *,
      struct scsi_device *));
void scsi_strategy __P((struct buf *, struct scsi_device *));
void scsi_minphys __P((struct buf *, struct scsi_device *));

#endif /* KERNEL */

#endif /* _SCSI__DRIVER_H_ */
