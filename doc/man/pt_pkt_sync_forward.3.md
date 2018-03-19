% PT_PKT_SYNC_FORWARD(3)

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

pt_pkt_sync_forward, pt_pkt_sync_backward, pt_pkt_sync_set - synchronize an
Intel(R) Processor Trace packet decoder


# SYNOPSIS

| **\#include `<intel-pt.h>`**
|
| **int pt_pkt_sync_forward(struct pt_packet_decoder \**decoder*);**
| **int pt_pkt_sync_backward(struct pt_packet_decoder \**decoder*);**
| **int pt_pkt_sync_set(struct pt_packet_decoder \**decoder*,**
|                     **uint64_t *offset*);**

Link with *-lipt*.


# DESCRIPTION

**pt_pkt_sync_forward**() and **pt_pkt_sync_backward**() synchronize an Intel
Processor Trace (Intel PT) packet decoder pointed to by *decoder* onto the trace
stream in *decoder*'s trace buffer.  They search for a Packet Stream Boundary
(PSB) packet in the trace stream and, if successful, set *decoder*'s current
position to that packet.

**pt_pkt_sync_forward**() searches in forward direction from *decoder*'s current
position towards the end of the trace buffer.  If *decoder* has been newly
allocated and has not been synchronized yet, the search starts from the
beginning of the trace.

**pt_pkt_sync_backward**() searches in backward direction from *decoder*'s
current position towards the beginning of the trace buffer.  If *decoder* has
been newly allocated and has not been synchronized yet, the search starts from
the end of the trace.

**pt_pkt_sync_set**() sets *decoder*'s current position to *offset* bytes from
the beginning of its trace buffer.


# RETURN VALUE

All synchronization functions return zero or a positive value on success or a
negative *pt_error_code* enumeration constant in case of an error.


# ERRORS

pte_invalid
:   The *decoder* argument is NULL.

pte_eos
:   There is no (further) PSB packet in the trace stream
    (**pt_pkt_sync_forward**() and **pt_pkt_sync_backward**()) or the *offset*
    argument is too big and the resulting position would be outside of
    *decoder*'s trace buffer (**pt_pkt_sync_set**()).


# EXAMPLE

The following example re-synchronizes an Intel PT packet decoder after decode
errors:

~~~{.c}
int foo(struct pt_packet_decoder *decoder) {
	for (;;) {
		int errcode;

		errcode = pt_pkt_sync_forward(decoder);
		if (errcode < 0)
			return errcode;

		do {
			errcode = decode(decoder);
		} while (errcode >= 0);
	}
}
~~~


# SEE ALSO

**pt_pkt_alloc_decoder**(3), **pt_pkt_free_decoder**(3),
**pt_pkt_get_offset**(3), **pt_pkt_get_sync_offset**(3),
**pt_pkt_get_config**(3), **pt_pkt_next**(3)
