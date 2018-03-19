% PT_ENC_GET_CONFIG(3)

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

pt_enc_get_config, pt_pkt_get_config, pt_qry_get_config, pt_insn_get_config,
pt_blk_get_config - get an Intel(R) Processor Trace encoder/decoder's
configuration


# SYNOPSIS

| **\#include `<intel-pt.h>`**
|
| **const struct pt_config \***
| **pt_enc_get_config(const struct pt_encoder \**encoder*);**
|
| **const struct pt_config \***
| **pt_pkt_get_config(const struct pt_packet_decoder \**decoder*);**
|
| **const struct pt_config \***
| **pt_qry_get_config(const struct pt_query_decoder \**decoder*);**
|
| **const struct pt_config \***
| **pt_insn_get_config(const struct pt_insn_decoder \**decoder*);**
|
| **const struct pt_config \***
| **pt_blk_get_config(const struct pt_block_decoder \**decoder*);**

Link with *-lipt*.


# DESCRIPTION

These functions return a pointer to their argument's configuration.  The
returned configuration object must not be freed.  It is valid as long as their
argument is not freed.


# RETURN VALUE

These functions returns a pointer to a *pt_config* object.  The returned pointer
is NULL if their argument is NULL.


# SEE ALSO

**pt_config**(3), **pt_alloc_encoder**(3), **pt_pkt_alloc_decoder**(3),
**pt_qry_alloc_decoder**(3), **pt_insn_alloc_decoder**(3),
**pt_blk_alloc_decoder**(3)
