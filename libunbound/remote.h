/*
 * libunbound/remote.h - prototypes for remote control methods.
 *
 * Copyright (c) 2026, NLnet Labs. All rights reserved.
 *
 * This software is open source.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 * 
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * 
 * Neither the name of the NLNET LABS nor the names of its contributors may
 * be used to endorse or promote products derived from this software without
 * specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * \file
 *
 * This file declares the methods that must be implemented to use the
 * remote control service.
 */

#ifndef LIBUNBOUND_REMOTE_H
#define LIBUNBOUND_REMOTE_H

struct comm_reply;
struct comm_point;

/** fast reload thread commands to remote service thread event callback */
void fast_reload_service_cb(int fd, short bits, void* arg);

/** fast reload callback for the remote control client connection */
int fast_reload_client_callback(struct comm_point* c, void* arg, int err,
	struct comm_reply* rep);

/** handle remote control accept callbacks */
int remote_accept_callback(struct comm_point*, void*, int, struct comm_reply*);

/** handle remote control data callbacks */
int remote_control_callback(struct comm_point*, void*, int, struct comm_reply*);

/** routine to printout option values over SSL */
void  remote_get_opt_ssl(char* line, void* arg);

#endif /* LIBUNBOUND_REMOTE_H */
