OpenCSD Library - Generic Trace Packet Descriptions   {#generic_pkts}	
===================================================

@brief Interpretation of the Generic Trace output packets.

Generic Trace Packets - Collection.
-----------------------------------

### Packet interface ###

The generic trace packets are the fully decoded output from the trace library. 

These are delivered to the client application in the form of a callback function. Packets from all trace sources 
will use the same single callback function, with the CoreSight Trace ID provided to identify the source.

The callback is in the form of an interface class ITrcGenElemIn, which has a single function:

~~~{.cpp}
virtual ocsd_datapath_resp_t TraceElemIn(    const ocsd_trc_index_t index_sop,
                                             const uint8_t trc_chan_id,
                                             const OcsdTraceElement &elem
										) = 0;
~~~

The client program will create derived class providing this interface to collect trace packets from the library.

The parameters describe the output packet and source channel:
|Parameter                        | Description                                                             | 
|:--------------------------------|:------------------------------------------------------------------------|
| `ocsd_trc_index_t index_sop`    | Index of the first byte of the trace packet that generated this output. |
| `uint8_t trc_chan_id`           | The source CoreSight Trace ID.                                          |
| `OcsdTraceElement &elem`        | The packet class - wraps the `ocsd_generic_trace_elem` structure.       | 

_Note_ : `index_sop` may be the same for multiple output packets. This is due to an one byte atom packet which
can represent multiple atoms and hence multiple ranges.

The C-API provides a similarly specified callback function definition, with an additional opaque `void *` pointer
that the client application may use.

~~~{.c}
/** function pointer type for decoder outputs. all protocols, generic data element input */
typedef ocsd_datapath_resp_t (* FnTraceElemIn)( const void *p_context, 
                                                const ocsd_trc_index_t index_sop, 
                                                const uint8_t trc_chan_id, 
                                                const ocsd_generic_trace_elem *elem); 
~~~

### The Packet Structure ###

~~~{.c}
typedef struct _ocsd_generic_trace_elem {
    ocsd_gen_trc_elem_t elem_type;   /* Element type - remaining data interpreted according to this value */
    ocsd_isa           isa;          /* instruction set for executed instructions */
    ocsd_vaddr_t       st_addr;      /* start address for instruction execution range / inaccessible code address / data address */
    ocsd_vaddr_t       en_addr;        /* end address (exclusive) for instruction execution range. */
    ocsd_pe_context    context;        /* PE Context */
    uint64_t           timestamp;      /* timestamp value for TS element type */
    uint32_t           cycle_count;    /* cycle count for explicit cycle count element, or count for element with associated cycle count */
    ocsd_instr_type    last_i_type;    /* Last instruction type if instruction execution range */
    ocsd_instr_subtype last_i_subtype; /* sub type for last instruction in range */
 
    //! per element flags
    union {
        struct {
            uint32_t last_instr_exec:1;     /* 1 if last instruction in range was executed; */
			uint32_t last_instr_sz:3;       /* size of last instruction in bytes (2/4) */
            uint32_t has_cc:1;              /* 1 if this packet has a valid cycle count included (e.g. cycle count included as part of instruction range packet, always 1 for pure cycle count packet.*/
            uint32_t cpu_freq_change:1;     /* 1 if this packet indicates a change in CPU frequency */
            uint32_t excep_ret_addr:1;      /* 1 if en_addr is the preferred exception return address on exception packet type */
            uint32_t excep_data_marker:1;   /* 1 if the exception entry packet is a data push marker only, with no address information (used typically in v7M trace for marking data pushed onto stack) */
            uint32_t extended_data:1;       /* 1 if the packet extended data pointer is valid. Allows packet extensions for custom decoders, or additional data payloads for data trace.  */
            uint32_t has_ts:1;              /* 1 if the packet has an associated timestamp - e.g. SW/STM trace TS+Payload as a single packet */
            uint32_t last_instr_cond:1;     /* 1 if the last instruction was conditional */
            uint32_t excep_ret_addr_br_tgt:1; /* 1 if exception return address (en_addr) is also the target of a taken branch addr from the previous range. */
        };
        uint32_t flag_bits;
    };

    //! packet specific payloads
    union {  
        uint32_t exception_number;          /* exception number for exception type packets */
        trace_event_t  trace_event;         /* Trace event - trigger etc      */
        trace_on_reason_t trace_on_reason;  /* reason for the trace on packet */
        ocsd_swt_info_t sw_trace_info;      /* software trace packet info    */
		uint32_t num_instr_range;	        /* number of instructions covered by range packet (for T32 this cannot be calculated from en-st/i_size) */

    };

    const void *ptr_extended_data;        /* pointer to extended data buffer (data trace, sw trace payload) / custom structure */

} ocsd_generic_trace_elem;
~~~

The packet structure contains multiple fields and flag bits. The validity of any of these fields or flags
is dependent on the `elem_type` member. The client program must not assume that field values will persist 
between packets, and must process all valid data during the callback function.

The packet reference guide below defines the fields valid for each packet type.

--------------------------------------------------------------------------------------------------

Generic Trace Packets - Packet Reference.
-----------------------------------------

This section contains reference descriptions of each of the generic trace packets types define as part of the 
`ocsd_gen_trc_elem_t` enum value that appears as the first `elem_type` field in the packet structure.

The descriptions will include information on which fields in the packets are always valid, optional and any protocol specific information. 

The tags used in the reference are:-
- __packet fields valid__ : fields that are always valid and filled for this packet type.
- __packet fields optional__ : fields that _may_ be filled for this packet type.
 The form `flag -> field` indicates a flag that may be set and the value that is valid if the flag is true
- __protocol specific__ : indicates type or fields may be source protocol specific.

_Note_: while most of the packets are not protocol specific, there are some protocol differences that mean
certain types and fields will differ slightly across protocols. These differences are highlighted in the 
reference.

### OCSD_GEN_TRC_ELEM_NO_SYNC ###
__packet fields valid__: None

Element output before the decoder has synchronised with the input stream, or synchronisation is lost.

### OCSD_GEN_TRC_ELEM_INSTR_RANGE ###
__packet fields valid__: `isa, st_addr, en_addr, last_i_type, last_i_subtype, last_instr_exec, last_instr_sz, num_instr_range, last_instr_cond`

__packet fields optional__: `has_cc -> cycle_count,`

__protocol specific__ : ETMv3, PTM 

This should be the most common packet output for full trace decode. Represents a range of instructions of 
a single `isa`, executed by the PE. Instruction byte range is from `st_addr` (inclusive) to `en_addr` (exclusive).
The total number of instructions executed for the range is given in `num_instr_range`.

Information on the last instruction in the range is provided. `last_i_type` shows if the last instruction
was a branch or otherwise - which combined with `last_instr_exec` determines if the branch was taken.
The last instruction size in bytes is given, to allow clients to quickly determine the address of the last 
instruction by subtraction from `en_addr`. This value can be 2 or 4 bytes in the T32 instruction set. 

__ETMv3, PTM__ : These protocols can output a cycle count directly as part of the trace packet that generates 
the trace range. In this case `has_cc` will be 1 and `cycle_count` will be valid.


### OCSD_GEN_TRC_ELEM_ADDR_NACC ###
__packet fields valid__: `st_addr`

Trace decoder found address in trace that cannot be accessed in the mapped memory images.
`st_addr` is the address that cannot be found.

Decoder will wait for new address to appear in trace before attempting to restart decoding. 
 

### OCSD_GEN_TRC_ELEM_UNKNOWN ###
__packet fields valid__: None

Decoder saw invalid packet for protocol being processed. Likely incorrect protocol settings, or corrupted 
trace data.
  
### OCSD_GEN_TRC_ELEM_TRACE_ON ###
__packet fields valid__: trace_on_reason

__packet fields optional__: `has_cc -> cycle_count,`

__protocol specific__ : ETMv3, PTM
 
Notification that trace has started / is synced after a discontinuity or at start of trace decode.

__ETMv3, PTM__ : These protocols can output a cycle count directly as part of the trace packet that generates 
the trace on indicator. In this case `has_cc`  will be 1 and `cycle_count` will be valid.


### OCSD_GEN_TRC_ELEM_EO_TRACE ###
__packet fields valid__: None

Marker for end of trace data. Sent once for each CoreSight ID channel.

### OCSD_GEN_TRC_ELEM_PE_CONTEXT ###
__packet fields valid__: context

__packet fields optional__: `has_cc -> cycle_count,`

__protocol specific__ : ETMv3, PTM

This packet indicates an update to the PE context - which may be the initial context in a trace stream, or a 
change since the trace started.

The context is contained in a `ocsd_pe_context` structure.

~~~{.c}
typedef struct _ocsd_pe_context {    
    ocsd_sec_level security_level;     /* security state */
    ocsd_ex_level  exception_level;    /* exception level */
    uint32_t        context_id;         /* context ID */
    uint32_t        vmid;               /* VMID */
    struct {
        uint32_t bits64:1;              /* 1 if 64 bit operation */
        uint32_t ctxt_id_valid:1;       /* 1 if context ID value valid */
        uint32_t vmid_valid:1;          /* 1 if VMID value is valid */
        uint32_t el_valid:1;            /* 1 if EL value is valid (ETMv4 traces current EL, other protocols do not) */
    };
} ocsd_pe_context;
~~~

__ETMv3, PTM__ : These protocols can output a cycle count directly as part of the trace packet that generates 
the PE context. In this case `has_cc`  will be 1 and `cycle_count` will be valid.

__ETMv3__ :  From ETM 3.5 onwards, exception_level can be set to `ocsd_EL2` when tracing through hypervisor code.
On all other occasions this will be set to `ocsd_EL_unknown`.


### OCSD_GEN_TRC_ELEM_ADDR_UNKNOWN ###
__packet fields optional__: `has_cc -> cycle_count,`

__protocol specific__: ETMv3

This packet will only be seen when decoding an ETMv3 protocol source. This indicates that the decoder
is waiting for a valid address in order to process trace correctly. 

The packet can have a cycle count associated with it which the client must account for when tracking cycles used.
The packet will be sent once when unknown address occurs. Further `OCSD_GEN_TRC_ELEM_CYCLE_COUNT` packets may follow
 before the decode receives a valid address to continue decode.


### OCSD_GEN_TRC_ELEM_EXCEPTION ###
__packet fields valid__: `exception_number`

__packet fields optional__: `has_cc -> cycle_count, excep_ret_addr -> en_addr, excep_data_marker, excep_ret_addr_br_tgt`

__protocol specific__: ETMv4, ETMv3, PTM

All protocols will include the exception number in the packet.

__ETMv4__ : This protocol may provide the preferred return address for the exception - this is the address of
the instruction that could be executed on exception return. This address appears in `en_addr` if `excep_ret_addr` = 1.

Additionally, this address could also represent the target address of a branch, if the exception occured at the branch target, before any further instructions were execute. If htis is the case then the excep_ret_addr_br_tgt flag will be set. This makes explicit what was previously only implied by teh packet ordered. This information could be used for clients such as perf that branch source/target address pairs.

__ETMv3__ : This can set the `excep_data_marker` flag. This indicates that the exception packet is a marker
to indicate exception entry in a 7M profile core, for the purposes of tracking data. This will __not__ provide
an exception number in this case.

__PTM__ : Can have an associated cycle count (`has_cc == 1`), and may provide preferred return address in `en_addr` 
if `excep_ret_addr` = 1.

### OCSD_GEN_TRC_ELEM_EXCEPTION_RET ###
__packet fields valid__: None

Marker that a preceding branch was an exception return.

### OCSD_GEN_TRC_ELEM_TIMESTAMP ###
__packet fields valid__: `timestamp`

__packet fields optional__: `has_cc -> cycle_count,`

__protocol specific__: ETMv4, PTM

The timestamp packet explicitly provides a timestamp value for the trace stream ID in the callback interface.

__PTM__ : This can have an associated cycle count (`has_cc == 1`). For this protocol, the cycle count __is__ part
of the cumulative cycle count for the trace session.

__ETMv4__ : This can have an associated cycle count (`has_cc == 1`). For this protocl, the cycle coun represents
the number of cycles between the previous cycle count packet and this timestamp packet, but __is not__ part of 
the cumulative cycle count for the trace session.


### OCSD_GEN_TRC_ELEM_CYCLE_COUNT ###
__packet fields valid__: `has_cc -> cycle_count`

Packet contains a cycle count value. A cycle count value represents the number of cycles passed since the 
last cycle count value seen. The cycle count value may be associated with a specific packet or instruction
range preceding the cycle count packet.

Cycle count packets may be added together to build a cumulative count for the trace session.

### OCSD_GEN_TRC_ELEM_EVENT ###
__packet fields valid__: `trace_event`

This is a hardware event injected into the trace by the ETM/PTM hardware resource programming. See the
relevent trace hardware reference manuals for the programming of these events. 

The `trace_event` is a `trace_event_t` structure that can have an event type - and an event number.

~~~{.c}
typedef struct _trace_event_t {
    uint16_t ev_type;          /* event type - unknown (0) trigger (1), numbered event (2)*/
    uint16_t ev_number;        /* event number if numbered event type */
} trace_event_t;
~~~

The event types depend on the trace hardware:-

__ETMv4__ : produces numbered events. The event number is a bitfield of up to four events that occurred.
Events 0-3 -> bits 0-3. The bitfield allows a single packet to represent multiple different events occurring.

_Note_: The ETMv4 specification has further information on timing of events and event packets.  Event 0 
is also considered a trigger event in ETMv4 hardware, but is not explicitly represented as such in the OCSD protocol.

__PTM__, __ETMv3__ : produce trigger events. Event number always set to 0.


### OCSD_GEN_TRC_ELEM_SWTRACE ###
__packet fields valid__: `sw_trace_info`

__packet fields optional__: `has_ts -> timestamp`, ` extended_data -> ptr_extended_data`

The Software trace packet always has a filled in `sw_trace_info` field to describe the current master and channel ID, 
plus the packet type and size of any payload data.

SW trace packets that have a payload will use the extended_data flag and pointer to deliver this data.

SW trace packets that include timestamp information will us the `has_ts` flag and fill in the timestamp value.


### OCSD_GEN_TRC_ELEM_CUSTOM ###
__packet fields optional__: `extended_data -> ptr_extended_data`,_any others_

Custom protocol decoders can use this packet type to provide protocol specific information. 

Standard fields may be used for similar purposes as defined above, or the extended data pointer can reference
other data.

--------------------------------------------------------------------------------------------------

Generic Trace Packets - Notes on interpretation.
------------------------------------------------

The interpretation of the trace output should always be done with reference to the underlying protocol 
specifications. 

While the output packets are in general protocol agnostic, there are some inevitable 
differences related to the underlying protocol that stem from the development of the trace hardware over time.

### OCSD ranges and Trace Atom Packets ###
The most common raw trace packet in all the protocols is the Atom packet, and this packet is the basis for most of
the `OCSD_GEN_TRC_ELEM_INSTR_RANGE` packets output from the library. A trace range will be output for each atom 
in the raw trace stream - the `last_instr_exec` flag taking the value of the Atom - 1 for E, 0 for N.

`OCSD_GEN_TRC_ELEM_INSTR_RANGE` packets can also be generated for non-atom packets, where flow changes - e.g.
exceptions.


### Multi feature OCSD output packets ###
Where a raw trace packet contains additional information on top of the basic packet data, then this additional
information will be added to the OCSD output packet and flagged accordingly (in the `flag_bits` union in the
packet structure).

Typically this will be atom+cycle count packets in ETMv3 and PTM protocols. For efficiency and to retain
the coupling between the information an `OCSD_GEN_TRC_ELEM_INSTR_RANGE` packet will be output in this case
with a `has_cc` flag set and the `cycle_count` value filled.

ETMv3 and PTM can add a cycle count to a number of packets, or explicitly emit a cycle count only packet. By
contrast ETMv4 only emits cycle count only packets.

Clients processing the library output must be aware of these optional additions to the base packet. The 
OCSD packet descriptions above outline where the additional information can occur.

### Cycle counts ###

Cycle counts are cumulative, and represent cycles since the last cycle count output.
Explicit cycle count packets are associated with the previous range event, otherwise where a 
packet includes a cycle count as additional information, then the count is associated with that
specific packet - which will often be a range packet.

The only exception to this is where the underlying protocol is ETMv4, and a cycle count is included
in a timestamp packet. Here the cycle count represents that number of cycles since the last cycle count
packet that occurred before the timestamp packet was emitted. This cycle count is not part of the cumulative 
count. See the ETMv4 specification for further details.


### Correlation - timestamps and cycle counts ###

Different trace streams can be correlated using either timestamps, or timestamps plus cycle counts. 

Both timestamps and cycle counts are enabled by programming ETM control registers, and it is also possible 
to control the frequency that timestamps appear, or the threshold at which cycle count packets are emitted by
additional programming. 

The output of timestamps and cycle counts increases the amount of trace generated, very significantly when cycle 
counts are present, so the choice of generating these elements needs to be balanced against the requirement
for their use.

Decent correlation can be gained by the use of timestamps alone - especially if the source is programmed to 
produce them more frequently than the default timestamp events. More precise correllation can be performed if
the 'gaps' between timestamps can be resolved using cycle counts. 

Correlation is performed by identifying the same/close timestamp values in two separate trace streams. Cycle counts 
if present can then be used to resolve the correlation with additional accuracy. 











