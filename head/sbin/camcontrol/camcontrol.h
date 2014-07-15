/*
 * Copyright (c) 1998 Kenneth D. Merry.
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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

#ifndef _CAMCONTROL_H
#define _CAMCONTROL_H

typedef enum {
	CC_OR_NOT_FOUND,
	CC_OR_AMBIGUOUS,
	CC_OR_FOUND
} camcontrol_optret;

/*
 * get_hook: Structure for evaluating args in a callback.
 */
struct get_hook
{
	int argc;
	char **argv;
	int got;
};

extern int verbose;

int fwdownload(struct cam_device *device, int argc, char **argv,
	       char *combinedopt, int printerrors, int retry_count, int timeout,
	       const char */*type*/);
void mode_sense(struct cam_device *device, int mode_page, int page_control,
		int dbd, int retry_count, int timeout, u_int8_t *data,
		int datalen);
void mode_select(struct cam_device *device, int save_pages, int retry_count,
		 int timeout, u_int8_t *data, int datalen);
void mode_edit(struct cam_device *device, int page, int page_control, int dbd,
	       int edit, int binary, int retry_count, int timeout);
void mode_list(struct cam_device *device, int page_control, int dbd,
	       int retry_count, int timeout);
int scsidoinquiry(struct cam_device *device, int argc, char **argv,
		  char *combinedopt, int retry_count, int timeout);
int scsipersist(struct cam_device *device, int argc, char **argv,
		char *combinedopt, int retry_count, int timeout, int verbose,
		int err_recover);
char *cget(void *hook, char *name);
int iget(void *hook, char *name);
void arg_put(void *hook, int letter, void *arg, int count, char *name);
int get_confirmation(void);
void usage(int printlong);
#endif /* _CAMCONTROL_H */
