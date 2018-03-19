% PT_QRY_COND_BRANCH(3)

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

pt_qry_cond_branch, pt_qry_indirect_branch - query an Intel(R) Processor Trace
query decoder


# SYNOPSIS

| **\#include `<intel-pt.h>`**
|
| **int pt_qry_cond_branch(struct pt_query_decoder \**decoder*,**
|                        **int \**taken*);**
| **int pt_qry_indirect_branch(struct pt_query_decoder \**decoder*,**
|                            **uint64_t \**ip*);

Link with *-lipt*.


# DESCRIPTION

**pt_qry_cond_branch**() uses Intel Processor Trace (Intel PT) to determine
whether the next conditional branch in the traced code was taken or was not
taken.  The *decoder* argument must point to an Intel PT query decoder.

On success, sets the variable the *taken* argument points to a non-zero value
if the next condition branch is taken and to zero if it is not taken.

**pt_qry_indirect_branch**() uses Intel Processor Trace (Intel PT) to determine
the destination virtual address of the next indirect branch in the traced code.

On success, provides the destination address in the integer variable pointed to
be the *ip* argument.  If the destination address has been suppressed in the
Intel PT trace, the lack of an IP is indicated in the return value by setting
the *pts_ip_suppressed* bit.


# RETURN VALUE

Both functions return zero or a positive value on success or a negative
*pt_error_code* enumeration constant in case of an error.

On success, a bit-vector of *pt_status_flag* enumeration constants is returned.
The *pt_status_flag* enumeration is declared as:

~~~{.c}
/** Decoder status flags. */
enum pt_status_flag {
	/** There is an event pending. */
	pts_event_pending	= 1 << 0,

	/** The address has been suppressed. */
	pts_ip_suppressed	= 1 << 1,

	/** There is no more trace data available. */
	pts_eos				= 1 << 2
};
~~~


# ERRORS

pte_invalid
:   The *decoder* argument or the *taken* (**pt_qry_cond_branch**()) or *ip*
    (**pt_qry_indirect_branch**()) argument is NULL.

pte_eos
:   Decode reached the end of the trace stream.

pte_nosync
:   The decoder has not been synchronized onto the trace stream.  Use
    **pt_qry_sync_forward**(3), **pt_qry_sync_backward**(3), or
    **pt_qry_sync_set**(3) to synchronize *decoder*.

pte_bad_opc
:   The decoder encountered an unsupported Intel PT packet opcode.

pte_bad_packet
:   The decoder encountered an unsupported Intel PT packet payload.

pte_bad_query
:   The query does not match the data provided in the Intel PT stream.  Based on
    the trace, the decoder expected a call to the other query function or a call
    to **pt_qry_event**(3).  This usually means that execution flow
    reconstruction and trace got out of sync.


# EXAMPLE

The following example sketches an execution flow reconstruction loop.
Asynchronous events have been omitted.

~~~{.c}
int foo(struct pt_query_decoder *decoder, uint64_t ip) {
	for (;;) {
		if (insn_is_cond_branch(ip)) {
			int errcode, taken;

			errcode = pt_qry_cond_branch(decoder, &taken);
			if (errcode < 0)
				return errcode;

			if (taken)
				ip = insn_destination(ip);
			else
				ip = insn_next_ip(ip);
		} else if (insn_is_indirect_branch(ip)) {
			int errcode;

			errcode = pt_qry_indirect_branch(decoder, &ip);
			if (errcode < 0)
				return errcode;
		} else
			ip = insn_next_ip(ip);
	}
}
~~~


# SEE ALSO

**pt_qry_alloc_decoder**(3), **pt_qry_free_decoder**(3),
**pt_qry_event**(3), **pt_qry_time**(3), **pt_qry_core_bus_ratio**(3)
