/*-
 * Copyright (c) 1998 - 2004 Søren Schmidt <sos@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    without modification, immediately at the beginning of the file.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
 * $FreeBSD$
 */

/* ATA commands */
#define ATA_NOP				0x00	/* NOP command */
#define		ATA_NF_FLUSHQUEUE	0x00	/* flush queued cmd's */
#define		ATA_NF_AUTOPOLL		0x01	/* start autopoll function */
#define ATA_ATAPI_RESET			0x08	/* reset ATAPI device */
#define ATA_READ			0x20	/* read command */
#define ATA_READ48			0x24	/* read command */
#define ATA_READ_DMA48			0x25	/* read w/DMA command */
#define ATA_READ_DMA_QUEUED48		0x26	/* read w/DMA QUEUED command */
#define ATA_READ_MUL48			0x29	/* read multi command */
#define ATA_WRITE			0x30	/* write command */
#define ATA_WRITE48			0x34	/* write command */
#define ATA_WRITE_DMA48			0x35	/* write w/DMA command */
#define ATA_WRITE_DMA_QUEUED48		0x36	/* write w/DMA QUEUED command */
#define ATA_WRITE_MUL48			0x39	/* write multi command */
#define ATA_READ_FPDMA_QUEUED		0x60	/* read w/DMA NCQ */
#define ATA_WRITE_FPDMA_QUEUED		0x61	/* write w/DMA NCQ */
#define ATA_PACKET_CMD			0xa0	/* packet command */
#define ATA_ATAPI_IDENTIFY		0xa1	/* get ATAPI params*/
#define ATA_SERVICE			0xa2	/* service command */
#define ATA_READ_MUL			0xc4	/* read multi command */
#define ATA_WRITE_MUL			0xc5	/* write multi command */
#define ATA_SET_MULTI			0xc6	/* set multi size command */
#define ATA_READ_DMA_QUEUED		0xc7	/* read w/DMA QUEUED command */
#define ATA_READ_DMA			0xc8	/* read w/DMA command */
#define ATA_WRITE_DMA			0xca	/* write w/DMA command */
#define ATA_WRITE_DMA_QUEUED		0xcc	/* write w/DMA QUEUED command */
#define ATA_SLEEP			0xe6	/* sleep command */
#define ATA_FLUSHCACHE			0xe7	/* flush cache to disk */
#define ATA_FLUSHCACHE48		0xea	/* flush cache to disk */
#define ATA_ATA_IDENTIFY		0xec	/* get ATA params */
#define ATA_SETFEATURES			0xef	/* features command */
#define		ATA_SF_SETXFER		0x03	/* set transfer mode */
#define		ATA_SF_ENAB_WCACHE	0x02	/* enable write cache */
#define		ATA_SF_DIS_WCACHE	0x82	/* disable write cache */
#define		ATA_SF_ENAB_RCACHE	0xaa	/* enable readahead cache */
#define		ATA_SF_DIS_RCACHE	0x55	/* disable readahead cache */
#define		ATA_SF_ENAB_RELIRQ	0x5d	/* enable release interrupt */
#define		ATA_SF_DIS_RELIRQ	0xdd	/* disable release interrupt */
#define		ATA_SF_ENAB_SRVIRQ	0x5e	/* enable service interrupt */
#define		ATA_SF_DIS_SRVIRQ	0xde	/* disable service interrupt */

/* ATAPI commands */
#define ATAPI_TEST_UNIT_READY		0x00	/* check if device is ready */
#define ATAPI_REZERO			0x01	/* rewind */
#define ATAPI_REQUEST_SENSE		0x03	/* get sense data */
#define ATAPI_FORMAT			0x04	/* format unit */
#define ATAPI_READ			0x08	/* read data */
#define ATAPI_WRITE			0x0a	/* write data */
#define ATAPI_WEOF			0x10	/* write filemark */
#define		ATAPI_WF_WRITE		0x01
#define ATAPI_SPACE			0x11	/* space command */
#define		ATAPI_SP_FM		0x01
#define		ATAPI_SP_EOD		0x03
#define ATAPI_MODE_SELECT		0x15	/* mode select */
#define ATAPI_ERASE			0x19	/* erase */
#define ATAPI_MODE_SENSE		0x1a	/* mode sense */
#define ATAPI_START_STOP		0x1b	/* start/stop unit */
#define		ATAPI_SS_LOAD		0x01
#define		ATAPI_SS_RETENSION	0x02
#define		ATAPI_SS_EJECT		0x04
#define ATAPI_PREVENT_ALLOW		0x1e	/* media removal */
#define ATAPI_READ_FORMAT_CAPACITIES	0x23	/* get format capacities */
#define ATAPI_READ_CAPACITY		0x25	/* get volume capacity */
#define ATAPI_READ_BIG			0x28	/* read data */
#define ATAPI_WRITE_BIG			0x2a	/* write data */
#define ATAPI_LOCATE			0x2b	/* locate to position */
#define ATAPI_READ_POSITION		0x34	/* read position */
#define ATAPI_SYNCHRONIZE_CACHE		0x35	/* flush buf, close channel */
#define ATAPI_WRITE_BUFFER		0x3b	/* write device buffer */
#define ATAPI_READ_BUFFER		0x3c	/* read device buffer */
#define ATAPI_READ_SUBCHANNEL		0x42	/* get subchannel info */
#define ATAPI_READ_TOC			0x43	/* get table of contents */
#define ATAPI_PLAY_10			0x45	/* play by lba */
#define ATAPI_PLAY_MSF			0x47	/* play by MSF address */
#define ATAPI_PLAY_TRACK		0x48	/* play by track number */
#define ATAPI_PAUSE			0x4b	/* pause audio operation */
#define ATAPI_READ_DISK_INFO		0x51	/* get disk info structure */
#define ATAPI_READ_TRACK_INFO		0x52	/* get track info structure */
#define ATAPI_RESERVE_TRACK		0x53	/* reserve track */
#define ATAPI_SEND_OPC_INFO		0x54	/* send OPC structurek */
#define ATAPI_MODE_SELECT_BIG		0x55	/* set device parameters */
#define ATAPI_REPAIR_TRACK		0x58	/* repair track */
#define ATAPI_READ_MASTER_CUE		0x59	/* read master CUE info */
#define ATAPI_MODE_SENSE_BIG		0x5a	/* get device parameters */
#define ATAPI_CLOSE_TRACK		0x5b	/* close track/session */
#define ATAPI_READ_BUFFER_CAPACITY	0x5c	/* get buffer capicity */
#define ATAPI_SEND_CUE_SHEET		0x5d	/* send CUE sheet */
#define ATAPI_BLANK			0xa1	/* blank the media */
#define ATAPI_SEND_KEY			0xa3	/* send DVD key structure */
#define ATAPI_REPORT_KEY		0xa4	/* get DVD key structure */
#define ATAPI_PLAY_12			0xa5	/* play by lba */
#define ATAPI_LOAD_UNLOAD		0xa6	/* changer control command */
#define ATAPI_READ_STRUCTURE		0xad	/* get DVD structure */
#define ATAPI_PLAY_CD			0xb4	/* universal play command */
#define ATAPI_SET_SPEED			0xbb	/* set drive speed */
#define ATAPI_MECH_STATUS		0xbd	/* get changer status */
#define ATAPI_READ_CD			0xbe	/* read data */
#define ATAPI_POLL_DSC			0xff	/* poll DSC status bit */
