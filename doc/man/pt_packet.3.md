% PT_PACKET(3)

<!---
 ! Copyright (c) 2015-2018, Intel Corporation
 !
 ! Redistribution and use in source and binary forms, with or without
 ! modification, are permitted provided that the following conditions are met:
 !
 !  * Redistributions of source code must retain the above copyright notice,
 !    this list of conditions and the following disclaimer.
 !  * Redistributions in binary form must reproduce the above copyright notice,
 !    this list of conditions and the following disclaimer in the documentation
 !    and/or other materials provided with the distribution.
 !  * Neither the name of Intel Corporation nor the names of its contributors
 !    may be used to endorse or promote products derived from this software
 !    without specific prior written permission.
 !
 ! THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 ! AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 ! IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 ! ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 ! LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 ! CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 ! SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 ! INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 ! CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 ! ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 ! POSSIBILITY OF SUCH DAMAGE.
 !-->

# NAME

pt_packet, pt_enc_next, pt_pkt_next - encode/decode an Intel(R) Processor Trace
packet


# SYNOPSIS

| **\#include `<intel-pt.h>`**
|
| **struct pt_packet;**
|
| **int pt_enc_next(struct pt_packet_encoder \**encoder*,**
|				  **const struct pt_packet \**packet*);**
|
| **int pt_pkt_next(struct pt_packet_decoder \**decoder*,**
|				  **struct pt_packet \**packet*, size_t *size*);**

Link with *-lipt*.


# DESCRIPTION

**pt_enc_next**() encodes its *packet* argument as Intel Processor Trace (Intel
PT) packet at *encoder*'s current position.  On success, sets *encoder*'s
current position to point to the first byte after the encoded packet.


**pt_pkt_next**() decodes the Intel PT packet at decoder*'s current position
into *packet*.  On success, sets *decoder*'s current position to point to the
first byte after the decoded packet.

The caller is responsible for allocating and freeing the *pt_packet* object
pointed to be the *packet* argument.

The *size* argument of **pt_pkt_next**() must be set to *sizeof(struct
pt_packet)*.  The function will provide at most *size* bytes of packet data.  A
newer decoder library may provide packet types that are not yet defined.  Those
packets may be truncated.  Unknown packet types should be ignored.

If the packet decoder does not know the packet opcode at *decoder*'s current
position and if *decoder*'s configuration contains a packet decode callback
function, **pt_pkt_next**() will call that callback function to decode the
unknown packet.  On success, a *ppt_unknown* packet type is provided with the
information provided by the decode callback function.

An Intel PT packet is described by the *pt_packet* structure, which is declared
as:

~~~{.c}
/** An Intel PT packet. */
struct pt_packet {
	/** The type of the packet.
	 *
	 * This also determines the \@variant field.
	 */
	enum pt_packet_type type;

	/** The size of the packet including opcode and payload. */
	uint8_t size;

	/** Packet specific data. */
	union {
		/** Packets: pad, ovf, psb, psbend, stop - no payload. */

		/** Packet: tnt-8, tnt-64. */
		struct pt_packet_tnt tnt;

		/** Packet: tip, fup, tip.pge, tip.pgd. */
		struct pt_packet_ip ip;

		/** Packet: mode. */
		struct pt_packet_mode mode;

		/** Packet: pip. */
		struct pt_packet_pip pip;

		/** Packet: tsc. */
		struct pt_packet_tsc tsc;

		/** Packet: cbr. */
		struct pt_packet_cbr cbr;

		/** Packet: tma. */
		struct pt_packet_tma tma;

		/** Packet: mtc. */
		struct pt_packet_mtc mtc;

		/** Packet: cyc. */
		struct pt_packet_cyc cyc;

		/** Packet: vmcs. */
		struct pt_packet_vmcs vmcs;

		/** Packet: mnt. */
		struct pt_packet_mnt mnt;

		/** Packet: unknown. */
		struct pt_packet_unknown unknown;
	} payload;
};
~~~

See the *intel-pt.h* header file for more detail.


# RETURN VALUE

**pt_enc_next**() returns the number of bytes written on success or a negative
*pt_error_code* enumeration constant in case of an error.

**pt_pkt_next**() returns the number of bytes consumed on success or a negative
*pt_error_code* enumeration constant in case of an error.


# ERRORS

pte_invalid
:   The *encoder*/*decoder* or *packet* argument is NULL or the *size* argument
    is zero (**pt_pkt_next**() only).

pte_nosync
:   *decoder* has not been synchronized onto the trace stream (**pt_pkt_next**()
    only).  Use **pt_pkt_sync_forward**(3), **pt_pkt_sync_backward**(3), or
    **pt_pkt_sync_set**(3) to synchronize *decoder*.

pte_eos
:   Encode/decode has reached the end of the trace buffer.  There is not enough
    space in the trace buffer to generate *packet* (**pt_enc_next**()) or the
    trace buffer does not contain a full Intel PT packet (**pt_pkt_next**()).

pte_bad_opc
:   The type of the *packet* argument is not supported (**pt_enc_next**()) or
    the packet at *decoder*'s current position is not supported
    (**pt_pkt_next**()).

pte_bad_packet
:   The payload or parts of the payload of the *packet* argument is not
    supported (**pt_enc_next**()) or the packet at *decoder*'s current position
    contains unsupported payload (**pt_pkt_next**()).


# EXAMPLE

The example shows a typical Intel PT packet decode loop.

~~~{.c}
int foo(struct pt_packet_decoder *decoder) {
	for (;;) {
		struct pt_packet packet;
		int errcode;

		errcode = pt_pkt_next(decoder, &packet, sizeof(packet));
		if (errcode < 0)
			return errcode;

		[...]
	}
}
~~~


# SEE ALSO

**pt_alloc_encoder**(3), **pt_pkt_alloc_decoder**(3),
**pt_pkt_sync_forward**(3), **pt_pkt_sync_backward**(3), **pt_pkt_sync_set**(3)
