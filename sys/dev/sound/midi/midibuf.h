/*
 * Include file for midi buffer.
 * 
 * Copyright by Seigo Tanimura 1999.
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
 *
 */

/*      
 * descriptor of a midi buffer. See midibuf.c for documentation.
 * (rp,rl) and (fp,fl) identify the READY and FREE regions of the
 * buffer. dl contains the length used for dma transfer, dl>0 also
 * means that the channel is busy and there is a DMA transfer in progress.
 */     

typedef struct _midi_dbuf {
	char *buf;
	int     bufsize ;
	volatile int rp, fp;		/* pointers to the ready and free area */
	volatile int dl;		/* transfer size */
	volatile int rl, fl;		/* length of ready and free areas. */
	int int_count;
	int chan;			/* dma channel */
	int unit_size ;			/* unit size */
	struct selinfo sel;
	u_long total;			/* total bytes processed */
	u_long prev_total;		/* copy of the above when GETxPTR called */
	struct cv cv_in, cv_out;	/* condvars */
	int blocksize;			/* block size */
} midi_dbuf ;

/*
 * These are the midi buffer methods, used in midi interface devices.
 */
int midibuf_init(midi_dbuf *dbuf);
int midibuf_destroy(midi_dbuf *dbuf);
int midibuf_clear(midi_dbuf *dbuf);
int midibuf_seqwrite(midi_dbuf *dbuf, u_char* data, int len, int *lenw, midi_callback_t *cb, void *d, int reason, struct mtx *m);
int midibuf_output_intr(midi_dbuf *dbuf, u_char *data, int len, int *leno);
int midibuf_input_intr(midi_dbuf *dbuf, u_char *data, int len, int *leni);
int midibuf_seqread(midi_dbuf *dbuf, u_char* data, int len, int *lenr, midi_callback_t *cb, void *d, int reason, struct mtx *m);
int midibuf_seqcopy(midi_dbuf *dbuf, u_char* data, int len, int *lenc, midi_callback_t *cb, void *d, int reason, struct mtx *m);
int midibuf_seqdelete(midi_dbuf *dbuf, int len, int *lend, midi_callback_t *cb, void *d, int reason, struct mtx *m);
