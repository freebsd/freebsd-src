% PT_QRY_EVENT(3)

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

pt_qry_event, pt_insn_event, pt_blk_event - query an Intel(R) Processor Trace
decoder for an asynchronous event


# SYNOPSIS

| **\#include `<intel-pt.h>`**
|
| **int pt_qry_event(struct pt_query_decoder \**decoder*,**
|                  **struct pt_event \**event*, size_t *size*);**
|
| **int pt_insn_event(struct pt_insn_decoder \**decoder*,**
|                   **struct pt_event \**event*, size_t *size*);**
|
| **int pt_blk_event(struct pt_block_decoder \**decoder*,**
|                  **struct pt_event \**event*, size_t *size*);**

Link with *-lipt*.


# DESCRIPTION

**pt_qry_event**(), **pt_insn_event**(), and **pt_blk_event**() provide the next
pending asynchronous event in *decoder*'s Intel Processor Trace (Intel PT)
decode in the *pt_event* object pointed to by the *event* argument.

The *size* argument must be set to *sizeof(struct pt_event)*.  The function will
provide at most *size* bytes of the *pt_event* structure.  A newer decoder
library may provide event types that are not yet defined.  Those events may be
truncated.

On success, detailed information about the event is provided in the *pt_event*
object pointed to by the *event* argument.  The *pt_event* structure is declared
as:

~~~{.c}
/** An event. */
struct pt_event {
	/** The type of the event. */
	enum pt_event_type type;

	/** A flag indicating that the event IP has been
	 * suppressed.
	 */
	uint32_t ip_suppressed:1;

	/** A flag indicating that the event is for status update. */
	uint32_t status_update:1;

	/** A flag indicating that the event has timing
	 * information.
	 */
	uint32_t has_tsc:1;

	/** The time stamp count of the event.
	 *
	 * This field is only valid if \@has_tsc is set.
	 */
	uint64_t tsc;

	/** The number of lost mtc and cyc packets.
	 *
	 * This gives an idea about the quality of the \@tsc.  The
	 * more packets were dropped, the less precise timing is.
	 */
	uint32_t lost_mtc;
	uint32_t lost_cyc;

	/* Reserved space for future extensions. */
	uint64_t reserved[2];

	/** Event specific data. */
	union {
		/** Event: enabled. */
		struct {
			/** The address at which tracing resumes. */
			uint64_t ip;

			/** A flag indicating that tracing resumes from the IP
			 * at which tracing had been disabled before.
			 */
			uint32_t resumed:1;
		} enabled;

		/** Event: disabled. */
		struct {
			/** The destination of the first branch inside a
			 * filtered area.
			 *
			 * This field is not valid if \@ip_suppressed is set.
			 */
			uint64_t ip;

			/* The exact source ip needs to be determined using
			 * disassembly and the filter configuration.
			 */
		} disabled;

		[...]
	} variant;
};
~~~

See the *intel-pt.h* header file for more detail.  The common fields of the
*pt_event* structure are described in more detail below:

type
:   The type of the event as a *pt_event_type* enumeration, which is declared
    as:

~~~{.c}
/** Event types. */
enum pt_event_type {
	/* Tracing has been enabled/disabled. */
	ptev_enabled,
	ptev_disabled,

	/* Tracing has been disabled asynchronously. */
	ptev_async_disabled,

	/* An asynchronous branch, e.g. interrupt. */
	ptev_async_branch,

	/* A synchronous paging event. */
	ptev_paging,

	/* An asynchronous paging event. */
	ptev_async_paging,

	/* Trace overflow. */
	ptev_overflow,

	/* An execution mode change. */
	ptev_exec_mode,

	/* A transactional execution state change. */
	ptev_tsx,

	/* Trace Stop. */
	ptev_stop,

	/* A synchronous vmcs event. */
	ptev_vmcs,

	/* An asynchronous vmcs event. */
	ptev_async_vmcs,

	/* Execution has stopped. */
	ptev_exstop,

	/* An MWAIT operation completed. */
	ptev_mwait,

	/* A power state was entered. */
	ptev_pwre,

	/* A power state was exited. */
	ptev_pwrx,

	/* A PTWRITE event. */
	ptev_ptwrite,

	/* A timing event. */
	ptev_tick,

	/* A core:bus ratio event. */
	ptev_cbr,

	/* A maintenance event. */
	ptev_mnt
};
~~~

ip_suppressed
:   A flag indicating whether the *ip* field in the event-dependent part is not
    valid because the value has been suppressed in the trace.

status_update
:   A flag indicating whether the event is for updating the decoder's status.
    Status update events originate from Intel PT packets in PSB+.

has_tsc
:   A flag indicating that the event's timing-related fields *tsc*, *lost_mtc*,
    and *lost_cyc* are valid.

tsc
:   The last time stamp count before the event.  Depending on the timing
    configuration, the timestamp can be more or less precise.  For
    cycle-accurate tracing, event packets are typically CYC-eligible so the
    timestamp should be cycle-accurate.

lost_mtc, lost_cyc
:   The number of lost MTC and CYC updates.  An update is lost if the decoder
    was not able to process an MTC or CYC packet due to missing information.
    This can be either missing calibration or missing configuration information.
    The number of lost MTC and CYC updates gives a rough idea about the quality
    of the *tsc* field.

variant
:   This field contains event-specific information.  See the *intel-pt.h* header
    file for details.


# RETURN VALUE

**pt_qry_event**(), **pt_insn_event**(), and **pt_blk_event**() return zero or a
*positive value on success or a negative pt_error_code* enumeration constant in
*case of an error.

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
:   The *decoder* or *event* argument is NULL or the *size* argument is too
    small.

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
    the trace, the decoder expected a call to **pt_qry_cond_branch**(3) or
    **pt_qry_indirect_branch**(3).  This usually means that execution flow
    reconstruction and trace got out of sync.


# SEE ALSO

**pt_qry_alloc_decoder**(3), **pt_qry_free_decoder**(3),
**pt_qry_cond_branch**(3), **pt_qry_indirect_branch**(3), **pt_qry_time**(3),
**pt_qry_core_bus_ratio**(3), **pt_insn_next**(3), **pt_blk_next**(3)
