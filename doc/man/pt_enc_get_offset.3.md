% PT_ENC_GET_OFFSET(3)

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

pt_enc_get_offset, pt_enc_sync_set - get/set an Intel(R) Processor Trace packet
encoder's current trace buffer offset


# SYNOPSIS

| **\#include `<intel-pt.h>`**
|
| **int pt_enc_get_offset(const struct pt_packet_encoder \**encoder*,**
|                       **uint64_t \**offset*);**
| **int pt_enc_sync_set(const struct pt_packet_encoder \**encoder*,**
|                     **uint64_t *offset*);**

Link with *-lipt*.


# DESCRIPTION

**pt_enc_get_offset**() provides *encoder*'s current position as offset in bytes
from the beginning of *encoder*'s trace buffer in the unsigned integer variable
pointed to by *offset*.

**pt_enc_sync_set**() sets *encoder*'s current position to *offset* bytes from
the beginning of its trace buffer.


# RETURN VALUE

Both functions return zero on success or a negative *pt_error_code* enumeration
constant in case of an error.


# ERRORS

pte_invalid
:   The *encoder* or *offset* (for **pt_enc_sync_set**()) argument is NULL.

pte_eos
:   The *offset* argument is too big and the resulting position would be outside
    of *encoder*'s trace buffer (**pt_enc_sync_set**() only).


# SEE ALSO

**pt_enc_alloc_encoder**(3), **pt_enc_free_encoder**(3), **pt_enc_next**(3)
