/* trace.h

   Definitions for dhcp tracing facility... */

/*
 * Copyright (c) 2001 Internet Software Consortium.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of The Internet Software Consortium nor the names
 *    of its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INTERNET SOFTWARE CONSORTIUM AND
 * CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE INTERNET SOFTWARE CONSORTIUM OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This software has been written for the Internet Software Consortium
 * by Ted Lemon, as part of a project for Nominum, Inc.   To learn more
 * about the Internet Software Consortium, see http://www.isc.org/.  To
 * learn more about Nominum, Inc., see ``http://www.nominum.com''.
 */

typedef struct {
	struct in_addr primary_address;
	u_int32_t index;
	struct hardware hw_address;
	char name [IFNAMSIZ];
} trace_interface_packet_t;

typedef struct {
	u_int32_t index;
	struct iaddr from;
	u_int16_t from_port;
	struct hardware hfrom;
	u_int8_t havehfrom;
} trace_inpacket_t;

typedef struct {
	u_int32_t index;
	struct iaddr from;
	struct iaddr to;
	u_int16_t to_port;
	struct hardware hto;
	u_int8_t havehto;
} trace_outpacket_t;

void trace_interface_register (trace_type_t *, struct interface_info *);
void trace_interface_input (trace_type_t *, unsigned, char *);
void trace_interface_stop (trace_type_t *);
void trace_inpacket_stash (struct interface_info *,
			   struct dhcp_packet *, unsigned, unsigned int,
			   struct iaddr, struct hardware *);
void trace_inpacket_input (trace_type_t *, unsigned, char *);
void trace_inpacket_stop (trace_type_t *);
void trace_outpacket_input (trace_type_t *, unsigned, char *);
void trace_outpacket_stop (trace_type_t *);
ssize_t trace_packet_send (struct interface_info *,
			   struct packet *, struct dhcp_packet *, size_t, 
			   struct in_addr,
			   struct sockaddr_in *, struct hardware *);
void trace_icmp_input_input (trace_type_t *, unsigned, char *);
void trace_icmp_input_stop (trace_type_t *);
void trace_icmp_output_input (trace_type_t *, unsigned, char *);
void trace_icmp_output_stop (trace_type_t *);
void trace_seed_stash (trace_type_t *, unsigned);
void trace_seed_input (trace_type_t *, unsigned, char *);
void trace_seed_stop (trace_type_t *);
