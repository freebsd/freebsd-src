% PT_QRY_TIME(3)

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

pt_qry_time, pt_qry_core_bus_ratio, pt_insn_time, pt_insn_core_bus_ratio,
pt_blk_time, pt_blk_core_bus_ratio - query an Intel(R) Processor Trace decoder
for timing information


# SYNOPSIS

| **\#include `<intel-pt.h>`**
|
| **int pt_qry_time(struct pt_query_decoder \**decoder*, uint64_t \**time*,**
|                 **uint32_t \**lost_mtc*, uint32_t \**lost_cyc*);**
| **int pt_qry_core_bus_ratio(struct pt_query_decoder \**decoder*,**
|                           **uint32_t \**cbr*);**
|
| **int pt_insn_time(struct pt_insn_decoder \**decoder*, uint64_t \**time*,**
|                  **uint32_t \**lost_mtc*, uint32_t \**lost_cyc*);**
| **int pt_insn_core_bus_ratio(struct pt_insn_decoder \**decoder*,**
|                            **uint32_t \**cbr*);**
|
| **int pt_blk_time(struct pt_block_decoder \**decoder*, uint64_t \**time*,**
|                 **uint32_t \**lost_mtc*, uint32_t \**lost_cyc*);**
| **int pt_blk_core_bus_ratio(struct pt_block_decoder \**decoder*,**
|                           **uint32_t \**cbr*);**

Link with *-lipt*.


# DESCRIPTION

**pt_qry_time**(), **pt_insn_time**(), and **pt_blk_time**() provide the current
estimated timestamp count (TSC) value in the unsigned integer variable pointed
to by the *time* argument.  The returned value corresponds to what an **rdtsc**
instruction would have returned.

At configurable intervals, Intel PT contains the full, accurate TSC value.
Between those intervals, the timestamp count is estimated using a collection of
lower-bandwidth packets, the Mini Time Counter (MTC) packet and the Cycle Count
Packet (CYC).  Depending on the Intel PT configuration, timing can be very
precise at the cost of increased bandwidth or less precise but requiring lower
bandwidth.

The decoder needs to be calibrated in order to translate Cycle Counter ticks
into Core Crystal Clock ticks.  Without calibration, CYC packets need to be
dropped.  The decoder calibrates itself using MTC, CYC, and CBR packets.

To interpret MTC and CYC packets, the decoder needs additional information
provided in respective fields in the *pt_config* structure.  Lacking this
information, MTC packets may need to be dropped.  This will impact the precision
of the estimated timestamp count by losing periodic updates and it will impact
calibration, which may result in reduced precision for cycle-accurate timing.

The number of dropped MTC and CYC packets gives a rough idea about the quality
of the estimated timestamp count.  The value of dropped MTC and CYC packets is
given in the unsigned integer variables pointed to by the *lost_mtc* and
*lost_cyc* arguments respectively.  If one or both of the arguments is NULL, no
information on lost packets is provided for the respective packet type.

**pt_qry_core_bus_ratio**(), **pt_insn_core_bus_ratio**(), and
**pt_blk_core_bus_ratio**() give the last known core:bus ratio as provided by
the Core Bus Ratio (CBR) Intel PT packet.


# RETURN VALUE

All functions return zero on success or a negative *pt_error_code* enumeration
constant in case of an error.


# ERRORS

pte_invalid
:   The *decoder* or *time* (**pt_qry_time**(), **pt_insn_time**(), and
    **pt_blk_time**()) or *cbr* (**pt_qry_core_bus_ratio**(),
    **pt_insn_core_bus_ratio**(), and **pt_blk_core_bus_ratio**()) argument is
    NULL.

pte_no_time
:   There has not been a TSC packet to provide the full, accurate Time Stamp
    Count.  There may have been MTC or CYC packets, so the provided *time* may
    be non-zero.  It is zero if there has not been any timing packet yet.

    Depending on the Intel PT configuration, TSC packets may not have been
    enabled.  In this case, the *time* value provides the relative time based on
    other timing packets.

pte_no_cbr
:   There has not been a CBR packet to provide the core:bus ratio.  The *cbr*
    value is undefined in this case.


# SEE ALSO

**pt_qry_alloc_decoder**(3), **pt_qry_free_decoder**(3),
**pt_qry_cond_branch**(3), **pt_qry_indirect_branch**(3), **pt_qry_event**(3),
**pt_insn_alloc_decoder**(3), **pt_insn_free_decoder**(3), **pt_insn_next**(3),
**pt_blk_alloc_decoder**(3), **pt_blk_free_decoder**(3), **pt_blk_next**(3)
