/*
 * Copyright (c) 1999 Cameron Grant <gandalf@vilnya.demon.co.uk>
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
 *
 * $FreeBSD$
 */

int chn_reinit(pcm_channel *c);
int chn_write(pcm_channel *c, struct uio *buf);
int chn_read(pcm_channel *c, struct uio *buf);
u_int32_t chn_start(pcm_channel *c, int force);
int chn_sync(pcm_channel *c, int threshold);
int chn_flush(pcm_channel *c);
int chn_poll(pcm_channel *c, int ev, struct proc *p);

int chn_init(pcm_channel *c, void *devinfo, int dir);
int chn_kill(pcm_channel *c);
int chn_setdir(pcm_channel *c, int dir);
int chn_reset(pcm_channel *c, u_int32_t fmt);
int chn_setvolume(pcm_channel *c, int left, int right);
int chn_setspeed(pcm_channel *c, int speed);
int chn_setformat(pcm_channel *c, u_int32_t fmt);
int chn_setblocksize(pcm_channel *c, int blkcnt, int blksz);
int chn_trigger(pcm_channel *c, int go);
int chn_getptr(pcm_channel *c);
pcmchan_caps *chn_getcaps(pcm_channel *c);
u_int32_t chn_getformats(pcm_channel *c);

int chn_allocbuf(snd_dbuf *b, bus_dma_tag_t parent_dmat);
void chn_freebuf(snd_dbuf *b);
void chn_resetbuf(pcm_channel *c);
void chn_intr(pcm_channel *c);
void chn_checkunderflow(pcm_channel *c);
int chn_wrfeed(pcm_channel *c);
int chn_rdfeed(pcm_channel *c);
int chn_abort(pcm_channel *c);

int fmtvalid(u_int32_t fmt, u_int32_t *fmtlist);

void buf_isadma(snd_dbuf *b, int go);
int buf_isadmaptr(snd_dbuf *b);

#define PCMDIR_PLAY 1
#define PCMDIR_REC -1

#define PCMTRIG_START 1
#define PCMTRIG_EMLDMAWR 2
#define PCMTRIG_EMLDMARD 3
#define PCMTRIG_STOP 0
#define PCMTRIG_ABORT -1

#define CHN_F_READING           0x00000001  /* have a pending read */
#define CHN_F_WRITING           0x00000002  /* have a pending write */
#define CHN_F_CLOSING           0x00000004  /* a pending close */
#define CHN_F_ABORTING          0x00000008  /* a pending abort */
#define	CHN_F_PENDING_IO	(CHN_F_READING | CHN_F_WRITING)
#define CHN_F_RUNNING		0x00000010  /* dma is running */
#define CHN_F_TRIGGERED		0x00000020
#define CHN_F_NOTRIGGER		0x00000040

#define CHN_F_BUSY              0x00001000  /* has been opened 	*/
#define	CHN_F_HAS_SIZE		0x00002000  /* user set block size */
#define CHN_F_NBIO              0x00004000  /* do non-blocking i/o */
#define CHN_F_INIT              0x00008000  /* changed parameters. need init */
#define CHN_F_MAPPED		0x00010000  /* has been mmap()ed */
#define CHN_F_DEAD		0x00020000


#define CHN_F_RESET		(CHN_F_BUSY | CHN_F_DEAD)

/*
 * This should be large enough to hold all pcm data between
 * tsleeps in chn_{read,write} at the highest sample rate.
 * (which is usually 48kHz * 16bit * stereo = 192000 bytes/sec)
 */
#define CHN_2NDBUFBLKSIZE	(2 * 1024)
/* The total number of blocks per secondary buffer. */
#define CHN_2NDBUFBLKNUM	(32)
/* The size of a whole secondary buffer. */
#define CHN_2NDBUFMAXSIZE	(131072)
