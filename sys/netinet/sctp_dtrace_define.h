/*-
 * Copyright (c) 2008-2011, by Randall Stewart. All rights reserved.
 * Copyright (c) 2008-2011, by Michael Tuexen. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * a) Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 *
 * b) Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *   the documentation and/or other materials provided with the distribution.
 *
 * c) Neither the name of Cisco Systems, Inc. nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");
#ifndef __sctp_dtrace_define_h__
#include "opt_kdtrace.h"
#include <sys/kernel.h>
#include <sys/sdt.h>

SDT_PROVIDER_DEFINE(sctp);

/********************************************************/
/* Cwnd probe - tracks changes in the congestion window on a netp */
/********************************************************/
/* Initial */
SDT_PROBE_DEFINE(sctp, cwnd, net, init, init);
/* The Vtag for this end */
SDT_PROBE_ARGTYPE(sctp, cwnd, net, init, 0, "uint32_t");
/* The port number of the local side << 16 | port number of remote
 * in network byte order.
 */
SDT_PROBE_ARGTYPE(sctp, cwnd, net, init, 1, "uint32_t");
/* The pointer to the struct sctp_nets * changing */
SDT_PROBE_ARGTYPE(sctp, cwnd, net, init, 2, "uintptr_t");
/* The old value of the cwnd  */
SDT_PROBE_ARGTYPE(sctp, cwnd, net, init, 3, "int");
/* The new value of the cwnd */
SDT_PROBE_ARGTYPE(sctp, cwnd, net, init, 4, "int");


/* ACK-INCREASE */
SDT_PROBE_DEFINE(sctp, cwnd, net, ack, ack);
/* The Vtag for this end */
SDT_PROBE_ARGTYPE(sctp, cwnd, net, ack, 0, "uint32_t");
/* The port number of the local side << 16 | port number of remote
 * in network byte order.
 */
SDT_PROBE_ARGTYPE(sctp, cwnd, net, ack, 1, "uint32_t");
/* The pointer to the struct sctp_nets * changing */
SDT_PROBE_ARGTYPE(sctp, cwnd, net, ack, 2, "uintptr_t");
/* The old value of the cwnd  */
SDT_PROBE_ARGTYPE(sctp, cwnd, net, ack, 3, "int");
/* The new value of the cwnd */
SDT_PROBE_ARGTYPE(sctp, cwnd, net, ack, 4, "int");

/* FastRetransmit-DECREASE */
SDT_PROBE_DEFINE(sctp, cwnd, net, fr, fr);
/* The Vtag for this end */
SDT_PROBE_ARGTYPE(sctp, cwnd, net, fr, 0, "uint32_t");
/* The port number of the local side << 16 | port number of remote
 * in network byte order.
 */
SDT_PROBE_ARGTYPE(sctp, cwnd, net, fr, 1, "uint32_t");
/* The pointer to the struct sctp_nets * changing */
SDT_PROBE_ARGTYPE(sctp, cwnd, net, fr, 2, "uintptr_t");
/* The old value of the cwnd  */
SDT_PROBE_ARGTYPE(sctp, cwnd, net, fr, 3, "int");
/* The new value of the cwnd */
SDT_PROBE_ARGTYPE(sctp, cwnd, net, fr, 4, "int");


/* TimeOut-DECREASE */
SDT_PROBE_DEFINE(sctp, cwnd, net, to, to);
/* The Vtag for this end */
SDT_PROBE_ARGTYPE(sctp, cwnd, net, to, 0, "uint32_t");
/* The port number of the local side << 16 | port number of remote
 * in network byte order.
 */
SDT_PROBE_ARGTYPE(sctp, cwnd, net, to, 1, "uint32_t");
/* The pointer to the struct sctp_nets * changing */
SDT_PROBE_ARGTYPE(sctp, cwnd, net, to, 2, "uintptr_t");
/* The old value of the cwnd  */
SDT_PROBE_ARGTYPE(sctp, cwnd, net, to, 3, "int");
/* The new value of the cwnd */
SDT_PROBE_ARGTYPE(sctp, cwnd, net, to, 4, "int");


/* BurstLimit-DECREASE */
SDT_PROBE_DEFINE(sctp, cwnd, net, bl, bl);
/* The Vtag for this end */
SDT_PROBE_ARGTYPE(sctp, cwnd, net, bl, 0, "uint32_t");
/* The port number of the local side << 16 | port number of remote
 * in network byte order.
 */
SDT_PROBE_ARGTYPE(sctp, cwnd, net, bl, 1, "uint32_t");
/* The pointer to the struct sctp_nets * changing */
SDT_PROBE_ARGTYPE(sctp, cwnd, net, bl, 2, "uintptr_t");
/* The old value of the cwnd  */
SDT_PROBE_ARGTYPE(sctp, cwnd, net, bl, 3, "int");
/* The new value of the cwnd */
SDT_PROBE_ARGTYPE(sctp, cwnd, net, bl, 4, "int");


/* ECN-DECREASE */
SDT_PROBE_DEFINE(sctp, cwnd, net, ecn, ecn);
/* The Vtag for this end */
SDT_PROBE_ARGTYPE(sctp, cwnd, net, ecn, 0, "uint32_t");
/* The port number of the local side << 16 | port number of remote
 * in network byte order.
 */
SDT_PROBE_ARGTYPE(sctp, cwnd, net, ecn, 1, "uint32_t");
/* The pointer to the struct sctp_nets * changing */
SDT_PROBE_ARGTYPE(sctp, cwnd, net, ecn, 2, "uintptr_t");
/* The old value of the cwnd  */
SDT_PROBE_ARGTYPE(sctp, cwnd, net, ecn, 3, "int");
/* The new value of the cwnd */
SDT_PROBE_ARGTYPE(sctp, cwnd, net, ecn, 4, "int");


/* PacketDrop-DECREASE */
SDT_PROBE_DEFINE(sctp, cwnd, net, pd, pd);
/* The Vtag for this end */
SDT_PROBE_ARGTYPE(sctp, cwnd, net, pd, 0, "uint32_t");
/* The port number of the local side << 16 | port number of remote
 * in network byte order.
 */
SDT_PROBE_ARGTYPE(sctp, cwnd, net, pd, 1, "uint32_t");
/* The pointer to the struct sctp_nets * changing */
SDT_PROBE_ARGTYPE(sctp, cwnd, net, pd, 2, "uintptr_t");
/* The old value of the cwnd  */
SDT_PROBE_ARGTYPE(sctp, cwnd, net, pd, 3, "int");
/* The new value of the cwnd */
SDT_PROBE_ARGTYPE(sctp, cwnd, net, pd, 4, "int");



/********************************************************/
/* Rwnd probe - tracks changes in the receiver window for an assoc */
/********************************************************/
SDT_PROBE_DEFINE(sctp, rwnd, assoc, val, val);
/* The Vtag for this end */
SDT_PROBE_ARGTYPE(sctp, rwnd, assoc, val, 0, "uint32_t");
/* The port number of the local side << 16 | port number of remote
 * in network byte order.
 */
SDT_PROBE_ARGTYPE(sctp, rwnd, assoc, val, 1, "uint32_t");
/* The up/down amount */
SDT_PROBE_ARGTYPE(sctp, rwnd, assoc, val, 2, "int");
/* The new value of the cwnd */
SDT_PROBE_ARGTYPE(sctp, rwnd, assoc, val, 3, "int");

/********************************************************/
/* flight probe - tracks changes in the flight size on a net or assoc */
/********************************************************/
SDT_PROBE_DEFINE(sctp, flightsize, net, val, val);
/* The Vtag for this end */
SDT_PROBE_ARGTYPE(sctp, flightsize, net, val, 0, "uint32_t");
/* The port number of the local side << 16 | port number of remote
 * in network byte order.
 */
SDT_PROBE_ARGTYPE(sctp, flightsize, net, val, 1, "uint32_t");
/* The pointer to the struct sctp_nets * changing */
SDT_PROBE_ARGTYPE(sctp, flightsize, net, val, 2, "uintptr_t");
/* The up/down amount */
SDT_PROBE_ARGTYPE(sctp, flightsize, net, val, 3, "int");
/* The new value of the cwnd */
SDT_PROBE_ARGTYPE(sctp, flightsize, net, val, 4, "int");
/********************************************************/
/* The total flight version */
/********************************************************/
SDT_PROBE_DEFINE(sctp, flightsize, assoc, val, val);
/* The Vtag for this end */
SDT_PROBE_ARGTYPE(sctp, flightsize, assoc, val, 0, "uint32_t");
/* The port number of the local side << 16 | port number of remote
 * in network byte order.
 */
SDT_PROBE_ARGTYPE(sctp, flightsize, assoc, val, 1, "uint32_t");
/* The up/down amount */
SDT_PROBE_ARGTYPE(sctp, flightsize, assoc, val, 2, "int");
/* The new value of the cwnd */
SDT_PROBE_ARGTYPE(sctp, flightsize, assoc, val, 3, "int");

#endif
