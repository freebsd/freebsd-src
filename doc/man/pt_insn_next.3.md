% PT_INSN_NEXT(3)

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
 ! ARE DISCLAIMED. IN NO NEXT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 ! LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 ! CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 ! SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 ! INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 ! CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 ! ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 ! POSSIBILITY OF SUCH DAMAGE.
 !-->

# NAME

pt_insn_next, pt_insn - iterate over traced instructions


# SYNOPSIS

| **\#include `<intel-pt.h>`**
|
| **struct pt_insn;**
|
| **int pt_insn_next(struct pt_insn_decoder \**decoder*,**
|                  **struct pt_insn \**insn*, size_t *size*);**

Link with *-lipt*.


# DESCRIPTION

**pt_insn_next**() provides the next instruction in execution order, which is
described by the *pt_insn* structure.

The *size* argument must be set to *sizeof(struct pt_insn)*.  The function will
provide at most *size* bytes of the *pt_insn* structure.  A newer decoder
library may truncate an extended *pt_insn* object to *size* bytes.

An older decoder library may provide less *pt_insn* fields.  Fields that are not
provided will be zero-initialized.  For fields where zero is a valid value
(e.g. for bit-fields), check the decoder library version to determine which
fields are valid.  See **pt_library_version**(3).

On success, the next instruction is provided in the *pt_insn* object pointed to
by the *insn* argument.  The *pt_insn* structure is declared as:

~~~{.c}
/** A single traced instruction. */
struct pt_insn {
	/** The virtual address in its process. */
	uint64_t ip;

	/** The image section identifier for the section containing this
	 * instruction.
	 *
	 * A value of zero means that the section did not have an identifier.
	 * The section was not added via an image section cache or the memory
	 * was read via the read memory callback.
	 */
	int isid;

	/** The execution mode. */
	enum pt_exec_mode mode;

	/** A coarse classification. */
	enum pt_insn_class iclass;

	/** The raw bytes. */
	uint8_t raw[pt_max_insn_size];

	/** The size in bytes. */
	uint8_t size;

	/** A collection of flags giving additional information:
	 *
	 * - the instruction was executed speculatively.
	 */
	uint32_t speculative:1;

	/** - this instruction is truncated in its image section.
	 *
	 *    It starts in the image section identified by \@isid and continues
	 *    in one or more other sections.
	 */
	uint32_t truncated:1;
};
~~~

The fields of the *pt_insn* structure are described in more detail below:

ip
:   The virtual address of the instruction.  The address should be interpreted
    in the current address space context.

isid
:   The image section identifier of the section from which the instruction
    originated.  This will be zero unless the instruction came from a section
    that was added via an image section cache.  See **pt_image_add_cached**(3).

    The image section identifier can be used to trace an instruction back to
    its binary file and from there to source code.

mode
:   The execution mode at which the instruction was executed.  The
    *pt_exec_mode* enumeration is declared as:

~~~{.c}
/** An execution mode. */
enum pt_exec_mode {
	ptem_unknown,
	ptem_16bit,
	ptem_32bit,
	ptem_64bit
};
~~~

iclass
:   A coarse classification of the instruction suitable for constructing a call
    back trace.  The *pt_insn_class* enumeration is declared as:

~~~{.c}
/** The instruction class.
 *
 * We provide only a very coarse classification suitable for
 * reconstructing the execution flow.
 */
enum pt_insn_class {
	/* The instruction could not be classified. */
	ptic_error,

	/* The instruction is something not listed below. */
	ptic_other,

	/* The instruction is a near (function) call. */
	ptic_call,

	/* The instruction is a near (function) return. */
	ptic_return,

	/* The instruction is a near unconditional jump. */
	ptic_jump,

	/* The instruction is a near conditional jump. */
	ptic_cond_jump,

	/* The instruction is a call-like far transfer.
	 * E.g. SYSCALL, SYSENTER, or FAR CALL.
	 */
	ptic_far_call,

	/* The instruction is a return-like far transfer.
	 * E.g. SYSRET, SYSEXIT, IRET, or FAR RET.
	 */
	ptic_far_return,

	/* The instruction is a jump-like far transfer.
	 * E.g. FAR JMP.
	 */
	ptic_far_jump
};
~~~

raw
:   The memory containing the instruction.

size
:   The size of the instruction in bytes.

speculative
:   A flag giving the speculative execution status of the instruction.  If set,
    the instruction was executed speculatively.  Otherwise, the instruction was
    executed normally.

truncated
:   A flag saying whether this instruction spans more than one image section.
    If clear, this instruction originates from a single section identified by
    *isid*.  If set, the instruction overlaps two or more image sections.  In
    this case, *isid* identifies the section that contains the first byte.


# RETURN VALUE

**pt_insn_next**() returns zero or a positive value on success or a negative
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

The *pts_event_pending* flag indicates that one or more events are pending.  Use
**pt_insn_event**(3) to process pending events before calling **pt_insn_next**()
again.

The *pt_eos* flag indicates that the information contained in the Intel PT
stream has been consumed.  Further calls to **pt_insn_next**() will continue to
provide instructions as long as the instruction's address can be determined
without further trace.


# ERRORS

pte_invalid
:   The *decoder* or *insn* argument is NULL or the *size* argument is too
    small.

pte_eos
:   Decode reached the end of the trace stream.

pte_nosync
:   The decoder has not been synchronized onto the trace stream.  Use
    **pt_insn_sync_forward**(3), **pt_insn_sync_backward**(3), or
    **pt_insn_sync_set**(3) to synchronize *decoder*.

pte_bad_opc
:   The decoder encountered an unsupported Intel PT packet opcode.

pte_bad_packet
:   The decoder encountered an unsupported Intel PT packet payload.

pte_bad_query
:   Execution flow reconstruction and trace got out of sync.

    This typically means that, on its way to the virtual address of the next
    event, the decoder encountered a conditional or indirect branch for which it
    did not find guidance in the trace.


# SEE ALSO

**pt_insn_alloc_decoder**(3), **pt_insn_free_decoder**(3),
**pt_insn_sync_forward**(3), **pt_insn_sync_backward**(3),
**pt_insn_sync_set**(3), **pt_insn_time**(3), **pt_insn_core_bus_ratio**(3),
**pt_insn_event**(3)
