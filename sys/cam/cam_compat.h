/*-
 * CAM ioctl compatibility shims
 *
 * Copyright (c) 2013 Scott Long
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification, immediately at the beginning of the file.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef _CAM_CAM_COMPAT_H
#define _CAM_CAM_COMPAT_H

int cam_compat_ioctl(struct cdev *dev, u_long *cmd, caddr_t *addr, int *flag, struct thread *td);

/* Version 0x16 compatibility */
#define CAM_VERSION_0x16	0x16

/* The size of the union ccb didn't change when going to 0x17 */
#define CAMIOCOMMAND_0x16	_IOWR(CAM_VERSION_0x16, 2, union ccb)
#define CAMGETPASSTHRU_0x16	_IOWR(CAM_VERSION_0x16, 3, union ccb)

#define CAM_SCATTER_VALID_0x16	0x00000010
#define CAM_SG_LIST_PHYS_0x16	0x00040000
#define CAM_DATA_PHYS_0x16	0x00200000

#endif

