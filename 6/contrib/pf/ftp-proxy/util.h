/*	$OpenBSD: util.h,v 1.5 2005/02/24 15:49:08 dhartmei Exp $ */

/*
 * Copyright (c) 1996-2001
 *	Obtuse Systems Corporation.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the Obtuse Systems nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL OBTUSE SYSTEMS CORPORATION OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

struct proxy_channel {
	int pc_to_fd, pc_from_fd;
	int pc_alive;
	int pc_nextbyte;
	int pc_flags;
	int pc_length;
	int pc_size;
	struct sockaddr_in pc_from_sa, pc_to_sa;
	int (*pc_filter)( void ** databuf, int datalen);
	char *pc_buffer;
};

struct csiob {
	int fd;
	int line_buffer_size, io_buffer_size, io_buffer_len, next_byte;
	unsigned char *io_buffer, *line_buffer;
	struct sockaddr_in sa, real_sa;
	const char *who;
	char alive, got_eof, data_available;
	int send_oob_flags;
};

extern int telnet_getline(struct csiob *iobp,
    struct csiob *telnet_passthrough);

extern int get_proxy_env(int fd, struct sockaddr_in *server_sa_ptr,
    struct sockaddr_in *client_sa_ptr, struct sockaddr_in *proxy_sa_ptr);

extern int get_backchannel_socket(int type, int min_port, int max_port,
    int start_port, int direction, struct sockaddr_in *sap);

extern int xfer_data(const char *what_read, int from_fd, int to_fd,
    struct in_addr from, struct in_addr to);

extern char *ProgName;


