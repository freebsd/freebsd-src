/*****************************************************************************/
/*
 *      auerchar.h  --  Auerswald PBX/System Telephone character interface.
 *
 *      Copyright (C) 2002  Wolfgang Mües (wolfgang@iksw-muees.de)
 *
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
 /*****************************************************************************/

#ifndef AUERCHAR_H
#define AUERCHAR_H

#include "auerchain.h"
#include "auerbuf.h"
#include "auerserv.h"

/* External data structures / Interface                              */
struct audevinfo {
	char *buf;		/* return buffer for string contents */
	unsigned int bsize;	/* size of return buffer */
};

/* IO controls */
#define IOCTL_AU_SLEN	  _IOR( 'U', 0xF0, int)	/* return the max. string descriptor length */
#define IOCTL_AU_DEVINFO  _IOWR('U', 0xF1, struct audevinfo)	/* get name of a specific device */
#define IOCTL_AU_SERVREQ  _IOW( 'U', 0xF2, int)	/* request a service channel */
#define IOCTL_AU_BUFLEN	  _IOR( 'U', 0xF3, int)	/* return the max. buffer length for the device */
#define IOCTL_AU_RXAVAIL  _IOR( 'U', 0xF4, int)	/* return != 0 if Receive Data available */
#define IOCTL_AU_CONNECT  _IOR( 'U', 0xF5, int)	/* return != 0 if connected to a service channel */
#define IOCTL_AU_TXREADY  _IOR( 'U', 0xF6, int)	/* return != 0 if Transmitt channel ready to send */
/*                              'U'  0xF7..0xFF reserved */

/* character device context */
struct auerswald;
struct auerchar {
	struct semaphore mutex;		/* protection in user context */
	struct auerswald *auerdev;	/* context pointer of assigned device */
	struct auerbufctl bufctl;	/* controls the buffer chain */
	struct auerscon scontext;	/* service context */
	wait_queue_head_t readwait;	/* for synchronous reading */
	struct semaphore readmutex;	/* protection against multiple reads */
	struct auerbuf *readbuf;	/* buffer held for partial reading */
	unsigned int readoffset;	/* current offset in readbuf */
	unsigned int removed;		/* is != 0 if device is removed */
};

/* Function prototypes */
void auerchar_delete(struct auerchar *ccp);

int auerchar_open(struct inode *inode, struct file *file);

int auerchar_ioctl(struct inode *inode, struct file *file,
		   unsigned int cmd, unsigned long arg);

loff_t auerchar_llseek(struct file *file, loff_t offset, int origin);

ssize_t auerchar_read(struct file *file, char *buf, size_t count,
		      loff_t * ppos);

ssize_t auerchar_write(struct file *file, const char *buf, size_t len,
		       loff_t * ppos);

int auerchar_release(struct inode *inode, struct file *file);


#endif	/* AUERCHAR_H */
