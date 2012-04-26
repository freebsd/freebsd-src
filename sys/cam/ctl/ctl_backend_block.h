/*-
 * Copyright (c) 2003 Silicon Graphics International Corp.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    substantially similar to the "NO WARRANTY" disclaimer below
 *    ("Disclaimer") and any redistribution must be conditioned upon
 *    including a substantially similar Disclaimer requirement for further
 *    binary redistribution.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGES.
 *
 * $Id: //depot/users/kenm/FreeBSD-test2/sys/cam/ctl/ctl_backend_block.h#1 $
 * $FreeBSD$
 */
/*
 * CAM Target Layer driver backend interface for block devices.
 *
 * Author: Ken Merry <ken@FreeBSD.org>
 */

#ifndef	_CTL_BACKEND_BLOCK_H_
#define	_CTL_BACKEND_BLOCK_H_

struct ctl_block_disk {
	uint32_t   version;	/* interface version */
	uint32_t   disknum;	/* returned device number */
	STAILQ_ENTRY(ctl_block_disk) links;  /* linked list pointer */
	char	   disk_name[MAXPATHLEN]; /* name of this device */
	int        allocated;	/* disk is allocated to a LUN */
	uint64_t   size_blocks; /* disk size in blocks */
	uint64_t   size_bytes;  /* disk size in bytes */
};

typedef enum {
	CTL_BLOCK_DEVLIST_MORE,
	CTL_BLOCK_DEVLIST_DONE
} ctl_block_devlist_status;

struct ctl_block_devlist {
	uint32_t		version;	/* interface version */
	uint32_t		buf_len;	/* passed in, buffer length */
	uint32_t		ctl_disk_size;	/* size of adddev, passed in */
	struct ctl_block_disk	*devbuf;	/* buffer passed in/filled out*/
	uint32_t		num_bufs;	/* number passed out */
	uint32_t		buf_used;	/* bytes passed out */
	uint32_t		total_disks;	/* number of disks in system */
	ctl_block_devlist_status status;	/* did we get the whole list? */
};

#define	CTL_BLOCK_ADDDEV	_IOWR(COPAN_ARRAY_BE_BLOCK, 0x00, struct ctl_block_disk)
#define	CTL_BLOCK_DEVLIST	_IOWR(COPAN_ARRAY_BE_BLOCK, 0x01, struct ctl_block_devlist)
#define	CTL_BLOCK_RMDEV		_IOW(COPAN_ARRAY_BE_BLOCK, 0x02, struct ctl_block_disk)

#endif	/* _CTL_BACKEND_BLOCK_H_ */
