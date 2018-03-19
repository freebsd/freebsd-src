% PT_CONFIG(3)

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

pt_config, pt_config_init, pt_cpu_errata - Intel(R) Processor Trace
encoder/decoder configuration


# SYNOPSIS

| **\#include `<intel-pt.h>`**
|
| **struct pt_config;**
|
| **void pt_config_init(struct pt_config \**config*);**
|
| **int pt_cpu_errata(struct pt_errata \**errata*, const struct pt_cpu \**cpu*);**

Link with *-lipt*.


# DESCRIPTION

The *pt_config* structure defines an Intel Processor Trace (Intel PT) encoder or
decoder configuration.  It is required for allocating a trace packet encoder
(see **pt_alloc_encoder**(3)), a trace packet decoder (see
**pt_pkt_alloc_decoder**(3)), a query decoder (see **pt_qry_alloc_decoder**(3)),
or an instruction flow decoder (see **pt_insn_alloc_decoder**(3)).

**pt_config_init**() zero-initializes its *config* argument and sets *config*'s
*size* field to *sizeof(struct pt_config)*.

**pt_cpu_errata**() enables workarounds for known errata in its *errata*
argument for the processor defined by its family/model/stepping in its *cpu*
argument.


The *pt_config* structure is declared as:

~~~{.c}
/** An Intel PT decoder configuration. */
struct pt_config {
	/** The size of the config structure in bytes. */
	size_t size;

	/** The trace buffer begin address. */
	uint8_t *begin;

	/** The trace buffer end address. */
	uint8_t *end;

	/** An optional callback for handling unknown packets.
	 *
	 * If \@callback is not NULL, it is called for any unknown
	 * opcode.
	 */
	struct {
		/** The callback function.
		 *
		 * It shall decode the packet at \@pos into \@unknown.
		 * It shall return the number of bytes read upon success.
		 * It shall return a negative pt_error_code otherwise.
		 * The below context is passed as \@context.
		 */
		int (*callback)(struct pt_packet_unknown *unknown,
				const struct pt_config *config,
				const uint8_t *pos, void *context);

		/** The user-defined context for this configuration. */
		void *context;
	} decode;

	/** The cpu on which Intel PT has been recorded. */
	struct pt_cpu cpu;

	/** The errata to apply when encoding or decoding Intel PT. */
	struct pt_errata errata;

	/** The CTC frequency.
	 *
	 * This is only required if MTC packets have been enabled in
	 * IA32_RTIT_CTRL.MTCEn.
	 */
	uint32_t cpuid_0x15_eax, cpuid_0x15_ebx;

	/** The MTC frequency as defined in IA32_RTIT_CTL.MTCFreq.
	 *
	 * This is only required if MTC packets have been enabled in
	 * IA32_RTIT_CTRL.MTCEn.
	 */
	uint8_t mtc_freq;

	/** The nominal frequency as defined in
	 * MSR_PLATFORM_INFO[15:8].
	 *
	 * This is only required if CYC packets have been enabled in
	 * IA32_RTIT_CTRL.CYCEn.
	 *
	 * If zero, timing calibration will only be able to use MTC
	 * and CYC packets.
	 *
	 * If not zero, timing calibration will also be able to use
	 * CBR packets.
	 */
	uint8_t nom_freq;

	/** A collection of decoder-specific flags. */
	struct pt_conf_flags flags;

	/** The address filter configuration. */
	struct pt_conf_addr_filter addr_filter;
};
~~~

The fields of the *pt_config* structure are described in more detail below:

size
:   The size of the *pt_config* structure for backward and forward
    compatibility.  Set it to *sizeof(struct pt_config)*.

begin, end
:   The begin and end of a user-allocated memory buffer; *begin* points to
    the first byte of the buffer, *end* points to one past the last byte in the
    buffer.

    The packet encoder will generate Intel PT packets into the memory buffer.

    The decoders expect the buffer to contain raw Intel PT packets.  They decode
    directly from the buffer and expect the buffer to remain valid until the
    decoder has been freed.

decode
:   An optional packet decode callback function.  If *decode.callback* is not
    NULL, it will be called for any unknown packet with the decoder
    configuration, the current decoder position and with a user-defined context
    provided in *callback.context* as arguments.

    If the callback function is able to decode the packet, it shall return the
    size of the decoded packet and provide details in a *pt_packet_unknown*
    object.

    If the packet cannot be decoded, the callback function shall return a
    negative *pt_error_code* enumeration constant.

    The *pt_packet_unknown* object can be used to provide user-defined
    information back to the user when using the packet decoder to iterate over
    Intel PT packets.  Other decoders ignore this information but will skip
    the packet if a non-zero size is returned by the callback function.

cpu
:   The processor on which the trace has been collected or for which the trace
    should be generated.  The processor is identified by its family, model, and
    stepping.

~~~{.c}
/** A cpu vendor. */
enum pt_cpu_vendor {
	pcv_unknown,
	pcv_intel
};

/** A cpu identifier. */
struct pt_cpu {
	/** The cpu vendor. */
	enum pt_cpu_vendor vendor;

	/** The cpu family. */
	uint16_t family;

	/** The cpu model. */
	uint8_t model;

	/** The stepping. */
	uint8_t stepping;
};
~~~

errata
:   The errata workarounds to be applied by the trace encoder or decoder that
    is created using this configuration.

    The *pt_errata* structure is a collection of one-bit-fields, one for each
    supported erratum.  Duplicate errata are indicated by comments for the
    erratum for which the workaround was first implemented.  Set the field of an
    erratum to enable the correspondig workaround.

    The *pt_errata* structure is declared as:

~~~{.c}
/** A collection of Intel PT errata. */
struct pt_errata {
	/** BDM70: Intel(R) Processor Trace PSB+ Packets May Contain
	 *         Unexpected Packets.
	 *
	 * Same as: SKD024.
	 *
	 * Some Intel Processor Trace packets should be issued only
	 * between TIP.PGE and TIP.PGD packets.  Due to this erratum,
	 * when a TIP.PGE packet is generated it may be preceded by a
	 * PSB+ that incorrectly includes FUP and MODE.Exec packets.
	 */
	uint32_t bdm70:1;

	/** BDM64: An Incorrect LBR or Intel(R) Processor Trace Packet
	 *         May Be Recorded Following a Transactional Abort.
	 *
	 * Use of Intel(R) Transactional Synchronization Extensions
	 * (Intel(R) TSX) may result in a transactional abort.  If an
	 * abort occurs immediately following a branch instruction,
	 * an incorrect branch target may be logged in an LBR (Last
	 * Branch Record) or in an Intel(R) Processor Trace (Intel(R)
	 * PT) packet before the LBR or Intel PT packet produced by
	 * the abort.
	 */
	uint32_t bdm64:1;

	[...]
};
~~~

cpuid_0x15_eax, cpuid_0x15_ebx
:   The values of *eax* and *ebx* on a *cpuid* call for leaf *0x15*.

    The value *ebx/eax* gives the ratio of the Core Crystal Clock (CTC) to
    Timestamp Counter (TSC) frequency.

    This field is ignored by the packet encoder and packet decoder.  It is
    required for other decoders if Mini Time Counter (MTC) packets are enabled
    in the collected trace.

mtc_freq
:   The Mini Time Counter (MTC) frequency as defined in *IA32_RTIT_CTL.MTCFreq*.

    This field is ignored by the packet encoder and packet decoder.  It is
    required for other decoders if Mini Time Counter (MTC) packets are enabled
    in the collected trace.

nom_freq
:   The nominal or max non-turbo frequency.

    This field is ignored by the packet encoder and packet decoder.  It is
    used by other decoders if Cycle Count (CYC) packets are enabled to improve
    timing calibration for cycle-accurate tracing.

    If the field is zero, the time tracking algorithm will use Mini Time
    Counter (MTC) and Cycle Count (CYC) packets for calibration.

    If the field is non-zero, the time tracking algorithm will additionally be
    able to calibrate at Core:Bus Ratio (CBR) packets.

flags
:   A collection of decoder-specific configuration flags.

addr_filter
:   The address filter configuration.  It is declared as:

~~~{.c}
/** The address filter configuration. */
struct pt_conf_addr_filter {
	/** The address filter configuration.
	 *
	 * This corresponds to the respective fields in IA32_RTIT_CTL MSR.
	 */
	union {
		uint64_t addr_cfg;

		struct {
			uint32_t addr0_cfg:4;
			uint32_t addr1_cfg:4;
			uint32_t addr2_cfg:4;
			uint32_t addr3_cfg:4;
		} ctl;
	} config;

	/** The address ranges configuration.
	 *
	 * This corresponds to the IA32_RTIT_ADDRn_A/B MSRs.
	 */
	uint64_t addr0_a;
	uint64_t addr0_b;
	uint64_t addr1_a;
	uint64_t addr1_b;
	uint64_t addr2_a;
	uint64_t addr2_b;
	uint64_t addr3_a;
	uint64_t addr3_b;

	/* Reserve some space. */
	uint64_t reserved[8];
};
~~~

# RETURN VALUE

**pt_cpu_errata**() returns zero on success or a negative *pt_error_code*
enumeration constant otherwise.


# ERRORS

**pt_cpu_errata**() may return the following errors:

pte_invalid
:	The *errata* or *cpu* argument is NULL.


# EXAMPLE

~~~{.c}
int foo(uint8_t *trace_buffer, size_t size, struct pt_cpu cpu) {
	struct pt_config config;
	int errcode;

	pt_config_init(&config);
	config.begin = trace_buffer;
	config.end = trace_buffer + size;
	config.cpu = cpu;

	errcode = pt_cpu_errata(&config.errata, &config.cpu);
	if (errcode < 0)
		return errcode;

	[...]
}
~~~


# SEE ALSO

**pt_alloc_encoder**(3), **pt_pkt_alloc_decoder**(3),
**pt_qry_alloc_decoder**(3), **pt_insn_alloc_decoder**(3)
