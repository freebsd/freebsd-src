% PT_BLK_NEXT(3)

<!---
 ! Copyright (c) 2016-2018, Intel Corporation
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

pt_blk_next, pt_block - iterate over blocks of traced instructions


# SYNOPSIS

| **\#include `<intel-pt.h>`**
|
| **struct pt_block;**
|
| **int pt_blk_next(struct pt_blk_decoder \**decoder*,**
|                  **struct pt_blk \**blk*, size_t *size*);**
|
| **int pt_blk_next(struct pt_block_decoder \**decoder*,**
|                 **struct pt_block \**block*, size_t *size*);**

Link with *-lipt*.


# DESCRIPTION

**pt_blk_next**() provides the next block of instructions in execution order,
which is described by the *pt_block* structure.

The *size* argument must be set to *sizeof(struct pt_block)*.  The function will
provide at most *size* bytes of the *pt_block* structure.  A newer decoder
library may truncate an extended *pt_block* object to *size* bytes.

An older decoder library may provide less *pt_block* fields.  Fields that are
not provided will be zero-initialized.  For fields where zero is a valid value
(e.g. for bit-fields), check the decoder library version to determine which
fields are valid.  See **pt_library_version**(3).

On success, the next block of instructions is provided in the *pt_block* object
pointed to by the *block* argument.  The *pt_block* structure is declared as:

~~~{.c}
/** A block of instructions.
 *
 * Instructions in this block are executed sequentially but are not necessarily
 * contiguous in memory.  Users are expected to follow direct branches.
 */
struct pt_block {
    /** The IP of the first instruction in this block. */
    uint64_t ip;

    /** The IP of the last instruction in this block.
     *
     * This can be used for error-detection.
     */
    uint64_t end_ip;

    /** The image section that contains the instructions in this block.
     *
     * A value of zero means that the section did not have an identifier.
     * The section was not added via an image section cache or the memory
     * was read via the read memory callback.
     */
    int isid;

    /** The execution mode for all instructions in this block. */
    enum pt_exec_mode mode;

    /** The instruction class for the last instruction in this block.
     *
     * This field may be set to ptic_error to indicate that the instruction
     * class is not available.  The block decoder may choose to not provide
     * the instruction class in some cases for performance reasons.
     */
    enum pt_insn_class iclass;

    /** The number of instructions in this block. */
    uint16_t ninsn;

    /** The raw bytes of the last instruction in this block in case the
     * instruction does not fit entirely into this block's section.
     *
     * This field is only valid if \@truncated is set.
     */
    uint8_t raw[pt_max_insn_size];

    /** The size of the last instruction in this block in bytes.
     *
     * This field is only valid if \@truncated is set.
     */
    uint8_t size;

    /** A collection of flags giving additional information about the
     * instructions in this block.
     *
     * - all instructions in this block were executed speculatively.
     */
    uint32_t speculative:1;

    /** - the last instruction in this block is truncated.
     *
     *    It starts in this block's section but continues in one or more
     *    other sections depending on how fragmented the memory image is.
     *
     *    The raw bytes for the last instruction are provided in \@raw and
     *    its size in \@size in this case.
     */
    uint32_t truncated:1;
};
~~~

The fields of the *pt_block* structure are described in more detail below:

ip
:   The virtual address of the first instruction in the block.  The address
    should be interpreted in the current address space context.

end_ip
:   The virtual address of the last instruction in the block.  The address
    should be interpreted in the current address space context.

    This can be used for error detection.  Reconstruction of the instructions in
    a block should end with the last instruction at *end_ip*.

isid
:   The image section identifier of the section from which the block of
    instructions originated.  This will be zero unless the instructions came
    from a section that was added via an image section cache.  See
    **pt_image_add_cached**(3).

    The image section identifier can be used for reading the memory containing
    an instruction in order to decode it and for tracing an instruction back to
    its binary file and from there to source code.

mode
:   The execution mode at which the instructions in the block were executed.
    The *pt_exec_mode* enumeration is declared as:

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
:   A coarse classification of the last instruction in the block.  This may be
    *ptic_error* to indicate that the classification is not available.

    The block decoder knows the instruction class of the instruction that ended
    the block most of the time.  If it does, it provides this information to
    save the caller the effort of decoding the instruction in some cases.

ninsn
:   The number of instructions contained in this block.

    The instructions are sequential in the sense that no trace is required for
    reconstructing them.  They are not necessarily contiguous in memory.

    The IP of the first instruction is given in the *ip* field and the IP of
    other instructions can be determined by decoding and examining the previous
    instruction.

raw
:   If the last instruction of this block can not be read entirely from this
    block's section, this field provides the instruction's raw bytes.

    It is only valid if the *truncated* flag is set.

size
:   If the last instruction of this block can not be read entirely from this
    block's section, this field provides the instruction's size in bytes.

    It is only valid if the *truncated* flag is set.

speculative
:   A flag giving the speculative execution status of all instructions in the
    block.  If set, the instructions were executed speculatively.  Otherwise,
    the instructions were executed normally.

truncated
:   A flag saying whether the last instruction in this block can not be read
    entirely from this block's section.  Some bytes need to be read from one or
    more other sections.  This can happen when an image section is partially
    overwritten by another image section.

    If set, the last instruction's memory is provided in *raw* and its size in
    *size*.


# RETURN VALUE

**pt_blk_next**() returns zero or a positive value on success or a negative
*pt_error_code* enumeration constant in case of an error.

On success, a bit-vector of *pt_status_flag* enumeration constants is returned.
The *pt_status_flag* enumeration is declared as:

~~~{.c}
/** Decoder status flags. */
enum pt_status_flag {
    /** There is an event pending. */
    pts_event_pending    = 1 << 0,

    /** The address has been suppressed. */
    pts_ip_suppressed    = 1 << 1,

    /** There is no more trace data available. */
    pts_eos              = 1 << 2
};
~~~

The *pts_event_pending* flag indicates that one or more events are pending.  Use
**pt_blk_event**(3) to process pending events before calling **pt_blk_next**()
again.

The *pt_eos* flag indicates that the information contained in the Intel PT
stream has been consumed.  Further calls to **pt_blk_next**() will continue to
provide blocks for instructions as long as the instruction's addresses can be
determined without further trace.


# ERRORS

pte_invalid
:   The *decoder* or *block* argument is NULL or the *size* argument is too
    small.

pte_eos
:   Decode reached the end of the trace stream.

pte_nosync
:   The decoder has not been synchronized onto the trace stream.  Use
    **pt_blk_sync_forward**(3), **pt_blk_sync_backward**(3), or
    **pt_blk_sync_set**(3) to synchronize *decoder*.

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

**pt_blk_alloc_decoder**(3), **pt_blk_free_decoder**(3),
**pt_blk_sync_forward**(3), **pt_blk_sync_backward**(3),
**pt_blk_sync_set**(3), **pt_blk_time**(3), **pt_blk_core_bus_ratio**(3),
**pt_blk_event**(3)
