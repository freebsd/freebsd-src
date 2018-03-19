% PT_ALLOC_ENCODER(3)

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

pt_alloc_encoder, pt_free_encoder - allocate/free an Intel(R) Processor Trace
packet encoder


# SYNOPSIS

| **\#include `<intel-pt.h>`**
|
| **struct pt_packet_encoder \***
| **pt_alloc_encoder(const struct pt_config \**config*);**
|
| **void pt_free_encoder(struct pt_packet_encoder \**encoder*);**

Link with *-lipt*.


# DESCRIPTION

**pt_alloc_encoder**() allocates a new Intel Processor Trace (Intel PT) packet
encoder and returns a pointer to it.  The packet encoder generates Intel PT
trace from *pt_packet* objects.  See **pt_enc_next**(3).

The *config* argument points to a *pt_config* object.  See **pt_config**(3).
The *config* argument will not be referenced by the returned encoder but the
trace buffer defined by the *config* argument's *begin* and *end* fields will.

The returned packet encoder is initially synchronized onto the beginning of the
trace buffer specified in its *config* argument.  Use **pt_enc_sync_set**(3) to
move it to any other position inside the trace buffer.

**pt_free_encoder**() frees the Intel PT packet encoder pointed to by encoder*.
*The *encoder* argument must be NULL or point to an encoder that has been
*allocated by a call to **pt_alloc_encoder**().


# RETURN VALUE

**pt_alloc_encoder**() returns a pointer to a *pt_packet_encoder* object on
success or NULL in case of an error.


# EXAMPLE

~~~{.c}
int foo(const struct pt_config *config) {
	struct pt_packet_encoder *encoder;
	errcode;

	encoder = pt_alloc_encoder(config);
	if (!encoder)
		return pte_nomem;

	errcode = bar(encoder);

	pt_free_encoder(encoder);
	return errcode;
}
~~~


# SEE ALSO

**pt_config**(3), **pt_enc_sync_set**(3), **pt_enc_get_offset**(3),
**pt_enc_get_config**(3), **pt_enc_next**(3)
