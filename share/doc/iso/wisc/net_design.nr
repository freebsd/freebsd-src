.NC "The Design of the ARGO Network Layer"
.sh 1 "Connectionless Network Layer
.pp
The following sections describe the design of the ARGO
connectionless network layer (CLNL).
The connectionless network service is provided by several
network-layer protocols: ES-IS (ISO 9542), 
CLNP (ISO 8348), and (ISO 8208) X.25.
The protocol CLNP is the primary connectionless network layer
protocol.
It is supported by X.25 when X.25 is used as a subnetwork layer.
X.25 can also be viewed as a link layer protocol in this context.
The ES-IS protocol supports CLNP by providing the following functions:
.ip \(bu 5
automatic mapping of NSAP-addresses to SNPA addresses,
.ip \(bu 5
automatic configuration of networks of end systems and intermediate
systems,  and
.ip \(bu 5
redirection of network-layer traffic in response to 
configuration changes.
.pp
The rest of this chapter describes the design of 
CLNP, the design of  ES-IS,
and the design of the connection-oriented
network layer, including the connection-oriented subnetwork service (X.25).
.pp
CLNP has two subsets defined: the Inactive Network Layer 
protocol subset and the Non-Segmenting protocol subset. 
The Inactive Network Layer subset is a null-function subset 
in which the CLNP is not needed, and the 
protocol consists of sending
a 1-byte header containing the value zero. 
This "subset" is not supported in ARGO.
.pp
The Non-Segmenting protocol subset permits simplification of the DT NPDU
header when it is known that segmentation of the DT NPDU is not required.
ARGO supports this subset.
When this subset is used, 
the segmentation part of the DT NPDU (data packet) header is not present, 
and the \fIdon't segment\fR bit is set in the 
fixed part of the header. 
This subset is chosen by setting the bit
\fICLNP_NO_SEG\fR in the \fIflags\fR argument to \fIclnp_output()\fR.
.pp
Throughout the remainder of this
document,
following definitions apply:
.(b
\(bu DT NPDU: data transfer NPDU.
\(bu ER NPDU: error report NPDU.
\(bu NPDU: either an ER or DT NPDU.
.)b
.sh 2 "DT NPDU Output"
.pp
A CLNP DT NPDU is transmitted by calling \fIclnp_output()\fR. 
.so figs/clnp_output.nr
.\" FIGURE
.CF
outlines the sequence of steps taken by \fIclnp_output()\fR when
transmitting an NPDU. 
The solid lines indicate normal flow of control. The
dashed lines indicate possible error returns (with associated
error code).
.pp
\fIClnp_output()\fR will automatically cache (in the \fIisopcb\fR)
the header of each packet it sends. This cached copy of the header
is used on subsequent sends reducing the amount of time spent generating
the header. Therefore, the first action \fIclnp_output()\fR takes is to
examine the cached header (if any). If the header is still valid (see below)
then it is used. Otherwise, a new header is built.
.sh 3 "When The Cached Header Is Invalid"
.pp
Before any resources are allocated, the options to be sent with the packet
are examined. If any unsupported options are present, the error \fIEINVAL\fR
is returned.
Next, the length of the source and destination 
NSAP addresses (taken from the \fIisopcb\fR)
are checked. The source address length may be zero. This
indicates that \fIclnp_output()\fR should compute the source address based upon
the route taken, in which case CLNP calls
the function \fIclnp_srcroute()\fR.
Source routing 
will be discussed in detail later in this section.
If, in the process of checking
the address lengths, an invalid length is detected, the error
\fIENAMETOOLONG\fR is returned.
.pp
After checking the lengths of the addresses, 
CLNP allocates an \fImbuf\fR in which the DT NPDU header will be constructed. 
If an \fImbuf\fR cannot be found, the error
\fIENOBUFS\fR is returned. Once the \fImbuf\fR is allocated, 
the fixed part of the DT NPDU header is copied into the \fImbuf\fR.
.pp
The next step is to route the DT NPDU. This is accomplished by the
\fIclnp_route()\fR function. 
It is necessary to route the datagram early in the output process because
in many cases, the source address will not be known until the route
has been created.
When a system is multi-homed it has several source addresses. 
The source address to choose depends on the
network interface (thus, the route) used.
.pp
The address part of the DT NPDU follows the fixed part. 
Since appending the address part is the next task, 
the source address must be determined.
Therefore the route must be determined.
.pp
After appending the address part to the fixed part of the
NPDU header, CLNP
appends any options given in the arguments to 
\fIclnp_output()\fR.
The options are specified in a
separate \fImbuf\fR stored in the \fIiso_pcb\fR.
If this \fImbuf\fR
pointer is not null, a copy of the \fImbuf\fR is made, and this copy is 
chained (appended) to the
\fImbuf\fR in which the
NPDU header resides. The options \fImbuf\fR linked in with the DT packet
must be a copy of the options \fImbuf\fR passed to \fIclnp_output()\fR. If
this was not done, then
the options \fImbuf\fR passed would be freed by the interface
driver after the NPDU had been transmitted.
Since a copy must be made, it is possible for \fIclnp_output()\fR to
return \fIENOBUFS\fR at this time.
A later section of this chapter describes
the handling of options in greater detail.
.pp
User data for the packet are passed to 
\fIclnp_output()\fR as an \fImbuf\fR chain.
This \fImbuf\fR chain is appended to the DT NPDU header chain. 
At this point, the DT NPDU is ready for transmission. 
If header caching has not been disabled, a cache entry is made in the 
\fIisopcb\fR.
If the size of the entire packet 
is less than the maximum transmission unit (MTU) of the 
network interface to be used,
the packet is placed on the queue for that network interface,
otherwise \fIclnp_fragment()\fR is invoked to
break up the packet into smaller packets, called
"derived NPDUs", and transmit the derived NPDUs.
.sh 3 "When A Cached Header Exists"
.pp
In this case, \fIclnp_output()\fR updates the segmentation part of the
header (if segmenting is permitted), computes the checksum, and transmits
(or fragments) the packet.
.pp
The cached CLNP header is stored in the \fIstruct isopcb\fR. The field
\fIisop_clnpcache\fR within the \fIisopcb\fR points to an \fImbuf\fR
which contains a \fIstruct clnp_cache\fR:
.(b
\fC
.TS
tab(+);
l s s.
struct clnp_cache {
.T&
l l l l.
+u_short+cni_securep;+/* ptr to security option */
+struct iso_addr+clc_dst;+/* destination of packet */
+struct mbuf+*clc_options;+/* ptr to options mbuf */
+int+clc_flags;+/* flags passed to clnp_output */
+int+clc_segoff;+/* offset of seg part of header */
+struct sockaddr+*clc_firsthop;+/* first hop of packet */
+struct ifnet+*clc_ifp;+/* ptr to interface */
+struct mbuf+*clc_hdr;+/* cached pkt hdr (finally)! */
};
.TE
\fR
.)b
The first three fields \fIclc_dst, clc_options\fR and \fIclc_flags\fR
are used to check the validity of the cache entry. The cache is considered
valid if:
.ip \(bu 5
The options mbuf has not changed.
.ip \(bu 5
The destination of the packet has not changed.
.ip \(bu 5
The route still exists and is up.
.ip \(bu 5
The flags have not changed.
.pp
If all these conditions are met, then the bulk of the \fIclnp_output()\fR
processing is avoided. The fields \fIclc_segoff, clc_firsthop,\fR 
and \fIclc_ifp\fR are used by \fIclnp_output()\fR to transmit the packet.
The field \fIclc_ifp\fR contains the actual cached header which is copied
and then enqueued on the outgoing interface.
.sh 2 "NPDU Input"
.pp
.\" FIGURE
.so figs/clnp_input.nr
All CLNP NPDUs are processed by \fIclnp_input()\fR. 
.CF
outlines
the flow of control within \fIclnlintr()\fR and \fIclnp_input()\fR. 
The solid lines
indicate normal flow of control. The dashed lines indicate 
possible error returns.
.pp
\fIClnlintr()\fR is invoked by a software interrupt. 
This interrupt is posted by a device driver whenever a 
packet is placed in CLNL's input queue 
\fIclnlintrq()\fR, and the queue is empty. 
It is the responsibility of \fIclnlintr()\fR, when invoked, 
to process all packets present on the input queue. 
Thus, to begin the task of processing a packet, \fIclnlintr()\fR
removes the next packet from the queue. 
When an error is discovered during processing, the packet is discarded and
\fIclnlintr()\fR begins afresh.
.pp
Once removed, the type of the NPDU is checked. If the NPDU is an
ES-IS packet, then \fIesis_input()\fR is called. If the NPDU is a CLNP
packet, then \fIclnp_input()\fR is called. Other packets are silently
discarded.
The function \fIclnp_hdr_ck()\fR checks the NPDU for consistency. 
Before checking consistency, \fIclnp_hdr_ck()\fR insures
that the entire NPDU header is located
contigiously in a single \fImbuf\fR (\fIm_pullup()\fR\** performs this task).
.(f
\** If the NPDU header is larger than \fIMLEN\fR (currently 256), then
\fIm_pullup()\fR will allocate a cluster \fImbuf\fR.
.)f
After "pulling" the header into a single \fImbuf\fR, \fIclnp_hdr_ck()\fR
checks for the proper CLNP version and protocol identification. 
It also checks that the lifetime field is greater than zero.
After checking header consistency, the NPDU checksum is computed.\**
.(f
\** If the checksum value is zero, the checksum is not computed. 
The value zero is reserved to mean \*(lqdo not use checksum\*(rq.
.)f
If the checksum is valid, \fIclnp_data_ck()\fR is called to insure
that the amount of data in the \fImbuf\fR chain corresponds to the
amount indicated in the NPDU header.
.pp
Once the consistency of the NPDU has been assured, the various parts of the
packet are extracted. 
Care is taken with each extraction to insure that an attempt is not made
to address data that does not really exist. (Such an attempt could
result in a kernel trap).
.pp
Next, the options part of the NPDU, if present, is checked for validity.
If unsupported options are found, the packet is discarded. 
See the section \*(lqNPDU options\*(rq for details of options processing.
.pp
Finally, after the preceding checks and extractions have been made, the
destination address is examined. 
If the address indicates that the packet's destination is not this
system, the packet is forwarded by calling \fIclnp_forward()\fR. 
See the section \*(lqDT NPDU Forwarding\*(rl for details of packet forwarding.
If this end system is the
packet's destination, processing continues.
.pp
If the packet is not complete, it is passed to \fIclnp_reass()\fR for
reassembly. 
See the section \*(lqDT NPDU Reassembly\*(rq
for details of packet reassembly.
.pp
At this point, a complete NPDU is in hand. 
If the NPDU is a DT NPDU, it is given to the transport layer
by calling the TP input routine. 
Otherwise, it is give to the ER NPDU processing function, 
\fIclnp_er_input()\fR.
.sh 3 "DT NPDU Forwarding"
.pp
Packet forwarding is accomplished by \fIclnp_forward()\fR. 
This is performed regardless of the system's type (end or intermediate).
The task of
forwarding a packet is fairly straight-forward. First, the lifetime
field of the datagram is decremented. 
If this operation changes the value to zero, the packet is discarded.
.pp
If the source route option is present, and the address at the top of the list
matches an address of one of the system's network interfaces, then
the next-source-route-to-be-used offset is adjusted in the option.
Next, the packet is routed by \fIclnp_route()\fR
or \fIclnp_srcroute()\fR. 
If the record route option is present, the address of the outgoing 
network interface is recorded by \fIclnp_dooptions()\fR.
.pp
Finally the packet is dispatched. 
If the size of the entire packet is less than the MTU of the output 
network interface, the packet is enqueued for that interface, 
otherwise \fIclnp_fragment()\fR is invoked to
fragment the packet and enqueue the derived NPDUs.
.sh 2 "NPDU Options"
.pp
The options section of an NPDU consists of a series of triplets:
\fIoption identification\fR, \fIoption length\fR, 
and \fIoption value\fR. 
These triplets are checked each time the options are examined or changed. 
To avoid repeated parsing of the options, the ARGO CLNP
maintains an index. 
This index is organized as a \fIclnp_optidx\fR structure. 
This structure is shown below.
.(b
\fC
.TS
tab(+);
l s s.
struct clnp_optidx {
.T&
l l l l.
+u_short+cni_securep;+/* ptr to security option */
+char+cni_secure_len;+/* length of security option */
+u_short+cni_srcrt_s;+/* offset of src rt option */
+u_short+cni_srcrt_len;+/* length of src rt option */
+u_short+cni_recrtp;+/* ptr to head of recrt option */
+char+cni_recrt_len;+/* length of recrt option */
+char+cni_priorp;+/* ptr to priority option */
+u_short+cni_qos_formatp;+/* ptr to format of qos option */
+char+cni_qos_len;+/* length of qos option */
+char+cni_er_reason;+/* reason from ER pdu option */
};
.TE
.)b
This index allows CLNP quickly to discover the existence 
and value of an option. 
For example, if a security option is present, the \fIcni_securep\fR
field of the option index is non-zero and the value of
\fIcni_securep\fR is an offset to the beginning of the 
security option. 
The function \fIclnp_opt_sanity()\fR 
parses the options and computes the index.
While parsing, it also verifies that the 
options are valid and correctly structured.
If an error occurs while parsing an option, 
\fIclnp_opt_sanity()\fR returns an error code. 
The following sections describe how options are processed
during the send, forward and receive operations.
.sh 3 "Sending Options"
.pp
Options to be sent with a datagram are passed to \fIclnp_output()\fR as
two arguments. An option index is passed along with an \fImbuf\fR
containing the options. 
The options in the \fImbuf\fR must be formatted
exactly as specified by CLNP. 
If the security, quality of service, or
priority options are specified, \fIclnp_output()\fR will not transmit the
datagram and \fIEINVAL\fR is returned.
The system call \fIsetsockopt()\fR is used to set the CLNP options 
to be sent on a datagram. 
See \fIclnp(4)\fR for more information about setting CLNP options.
.pp
If a source route is specified, 
the normal CLNP routing function \fIclnp_route()\fR is not used, and 
\fIclnp_srcroute()\fR is invoked.
.pp
When the DECBIT config option is specified, \fIclnp_output\fR will
automatically add the globally unique quality of service option to the packet.
The sequencing preferred and low delay bits in this option are set.
.sh 3 "Forwarding Options"
.pp
During packet forwarding, the padding, security,
and priority options are ignored. If record route is selected, the
function \fIclnp_dooptions()\fR logs the current network
interface address in the record route list.
.pp
If a source route is specified, 
the normal CLNP routing function \fIclnp_route()\fR is not used, and 
\fIclnp_srcroute()\fR is invoked.
.sh 4 "The Congestion Experienced Bit"
.pp
If a packet is forwarded containing the globally unique quality of
service option, and the interface through which the packet will be 
transmitted has a queue length greater than \fIcongest_threshold\fR,
then the congestion experienced bit is set in the quality of service option.
.pp
The threshold value stored in \fIcongest_threshold\fR may be changed
with the \fIclnlutil\fR utility.
.sh 3 "Receiving Options"
.pp
On receipt, all CLNP options are ignored except the security 
and globally unique quality of service option.
If the security option is found, the packet is discarded.
If the globally unique quality of service option is present, and the
congestion experienced bit is set, then the transport congestion
control function \fItpclnp_ctlinput(PRC_QUENCH2, addr)\fR is called.
The following table summarizes the CLNP option processing.
.(b
.TS
allbox, tab(+);
l l l l.
Option+Send+Forward+Receive
=
Padding+may be set+-+-
Security+reject+ignore+discard
Source Route+\fIclnp_srcroute()\fR+\fIclnp_srcroute()\fR+-
Record Route+-+\fIclnp_dooptions()\fR+-
QOS+added+congestion bit set+tpclnp_ctlinput()
Priority+reject+ignore+-
.TE
.)b
.sh 2 "DT NPDU Segmentation"
.pp
Segmentation is the process by which initial NPDUs are segmented into 
smaller derived NPDUs when the initial NPDU is too large for transmission
on a network interface.
Segmentation is accomplished by \fIclnp_fragment()\fR. 
This function chops the NPDU into pieces and individually places the pieces
in the appropriate network interface's output queue. 
Each piece is made as large as possible. 
Note: The phrase "fragmentation" is used synonymously with "segmentation"
throughout this prose and the CLNP fragmentation code. This is due to 
this author's familiarity with the DoD Internet Protocol which uses
the term "fragment."
.sh 2 "DT NPDU Reassembly"
.pp
Derived NPDUs are put back together by the process called 
reassembly. 
Reassembly is performed only at the destination end system.
When a derived NPDU arrives, it is passed to \fIclnp_reass()\fR. 
This function scans a linked list of NPDUs awaiting reassembly. 
Each packet in the list is represented by a fragment list
descriptor, which is stored in an \fImbuf\fR:
.(b
\fC
.TS
tab(+);
l s s s.
struct clnp_fragl {
.T&
l l l l.
+struct iso_addr+cfl_src;+/* source */
+struct iso_addr+cfl_dst;+/* destination */
+u_short+cfl_id;+/* id of the pkt */
+u_char+cfl_ttl;+/* time to live */
+u_short+cfl_last;+/* offset of last 
+++byte of packet */
+struct mbuf +*cfl_orighdr;+/* ptr to 
+++original header */
+struct clnp_frag+*cfl_frags;+/* linked list 
+++of fragments */
+struct clnp_fragl+*cfl_next;+/* next pkt be-
+++ing reassembled */
};
.TE
\fR
.)b
The fields \fIcfl_src\fR, \fIcfl_dst\fR, and \fIcfl_id\fR are used to
match an incoming derived NPDU with a fragment list. 
\fICfl_orighdr\fR contains a copy of the NPDU header of the first fragment received. 
The linked list of fragments pertaining to the packet is stored in the
\fIcfl_frags\fR field. 
Each NPDU fragment represented by a \fIclnp_frag\fR structure, 
stored in an \fImbuf\fR:
.(b
\fC
.TS
tab(+);
l s s s.
struct clnp_frag {
.T&
l l l l.
+u_int+cfr_first;+/* offset of 
+++first byte of this frag */
+u_int+cfr_last;+/* offset of last 
+++byte of this frag */
+u_int+cfr_bytes;+/* bytes to shave */
+struct mbuf+*cfr_data;+/* ptr to data */
+struct clnp_frag+*cfr_next;+/* next frag */
};
.TE
\fR
.)b
The fields \fIcfr_first\fR and \fIcfr_last\fR indicate the first and
last octet of the fragment. 
\fICfr_data\fR points to an mbuf chain
which contains the data for the fragment.
.pp
If \fIclnp_reass()\fR finds a \fIclnp_fragl\fR structure matching the
incoming derived NPDU, \fIclnp_insert_frag()\fR is called to create
a \fIclnp_frag\fR structure and insert it in the linked list of
packet fragments. 
If no \fIclnp_fragl\fR structure is found, 
\fIclnp_newpkt()\fR is invoked to create a new fragment list structure.
.pp
The last task \fIclnp_reass()\fR performs is to check if the fragment
that just arrived completes the reassembly of the initial NPDU. 
If it does, the reassembled NPDU is rearranged to 
look like it just arrived intact.
It accomplishes this by linking the \fImbuf\fRs holding
the fragments into one \fImbuf\fR chain that represents the initial
NPDU.
A pointer to this \fImbuf\fR chain is returned by \fIclnp_reass()\fR.
.pp
If the newly arrived fragment does not complete an initial NPDU, 
\fIclnp_reass()\fR returns NULL.
.sh 3 "Reassembly Lifetime Control"
.pp
One function of the CLNP is to prevent
a proliferation of fragments awaiting reassembly from
consuming buffers in an end system for indefinite periods of time.
This function is called reassembly lifetime control.
It is accomplished by 
periodic traversal of
the list of \fIclnp_fragl\fR structures, decrementing the 
\fIcfl_ttl\fR field. 
This field is a copy of the NPDU time-to-live
field. If \fIcfl_ttl\fR reaches zero, all resources associated with the
fragment are released.
The procedure
\fIclnp_slowtimo()\fR, which is called by the system
clock every 500 milliseconds (every half-second),
performs the CLNP reassembly lifetime control.
.sh 2 "ER NPDU"
.pp
An ER NPDU is sent to the originator of a packet when a DT NPDU is
discarded and the error report function is not suppressed. Suppression
of the error report function is accomplished by setting the "no ER"
bit in the CLNP header.
A packet is discarded by \fIclnp_discard()\fR. 
Before it
returns the \fImbufs\fR used to store the 
the discarded packet to the \fImbuf\fR free list,
\fIclnp_discard()\fR 
determines if the error report function is suppressed. 
If not, 
an ER NPDU will be sent to the originator of the discarded packet by
calling \fIclnp_emit_er()\fR.
.pp
\fIClnp_emit_er()\fR will create an ER NPDU, address it to the 
originator of the discarded packet, route the NPDU, 
and transmit it, sending the header of the discarded NPDU as data. 
ER NPDUs may not be segmented. 
If the ER NPDU is too large for the outgoing network interface, 
the packet is truncated.
.sh 2 "Raw CLNP"
.pp
In order to test CLNP in isolation from higher layer
protocols, ARGO provides a \*(lqraw\*(rq interface to CLNP.
This raw interface is selected with the \fISOCK_RAW\fR parameter to
the
\fIsocket()\fR
system call.
When a \*(rqraw\*(rq socket is open,
and CLNP receives an NPDU,
CLNP must determine whether the incoming NPDU is destined for
the 
\*(rqraw\*(rq interface or for the interface to the 
OSI transport protocol entity.
ARGO addresses this problem by using non-standard NPDU types
for packets sent on \*(rqraw\*(rq sockets.
The type field in the CLNP NPDU header
is set to \fICLNP_RAW\fR (hex 1d) rather than \fICLNP_DT\fR
in NPDUs that originate from 
\*(rqraw\*(rq sockets.
This non-standard type value is used by \fIclnp_input()\fR
to decide which upper layer protocol should receive the packet.
See \fIclnptest(8)\fR for more information about the.
\*(rqraw\*(rq CLNP interface.
.sh 2 "CLNP Echo"
.pp
In the DoD world, ICMP supports an \fIecho\fR service. 
This allows one to \*(lqping\*(rq a distant gateway and 
to receive an echo response (a packet in return) if the gateway is working.
There is no counterpart to \*(lqecho\*(rq in ISO 8473 (CLNP). 
ARGO provides this non-standard feature in its connectionless
network layer.
.pp
Like raw CLNP, implementing an echo function requires a non-standard
NPDU type value to allow
\fIclnp_input()\fR to differentiate between a DT NPDU to be forwarded
or passed to a higher layer protocol, and an NPDU that is to be echoed.
When requesting an echo, 
the CLNP type field is set to \fICLNP_EC\fR (hex 1E) rather
than CLNP_DT. 
When \fIclnp_input()\fR receives a packet with type
\fICLNP_EC\fR, 
it swaps the source and destination addresses, sets the
type field to \fICLNP_ECR\fR (hex 1F) and forwards
the packet back to the sender. 
See also \fIclnpping(8)\fR.
.sh 2 "Timers"
.pp
The only timer used by CLNP is the 
500 millisecond timer, which is 
user for reassembly lifetime control.
See the section \*(lqReassembly Lifetime Control.\*(rq
.sh 1 "End System to Intermediate System Routing Protocol (ES-IS)"
.\" ROB
.sh 2 "Overview"
.pp
This section describes the implementation of the ES-IS routing protocol.
This protocol is used primarily to resolve NSAP address to SNPA address 
translations. It is also used to identify end systems
and intermediate systems on
the local subnetwork. 
All of this work is accomplished by transmitting
packets of the type End System Hello (ESH), Intermediate System Hello (ISH)
and Request Redirect (RD).
.pp
For the purpose of this section, the following definitions of end system (ES)
and intermediate system (IS) apply.
.ip \(bu 5
An \fIend system\fR is an open system that
is an OSI end system in the standard OSI sense
(that it supports a full OSI protocol suite in addition to the network layer)
and that
implements the functions of the
the ES-IS protocol that are mandatory for end systems,
such as the Query Configuration function and the Record Redirect
function,
but that does not implement
the functions of the ES-IS protocol that are for intermediate systems.
.ip \(bu 5
An \fIintermediate system\fR is an open system that
is an OSI intermediate system in the standard OSI sense
(that it performs packet routing in the network layer)
and that
implements the functions of the
the ES-IS protocol that are mandatory for intermediate systems,
such as the Request Redirect function, 
but not the functions of the ES-IS protocol that are for end systems.
.pp
While system may be an ES or an IS or both according to the
standard OSI definitions, this is not the case in the context of
the ES-IS protocol.
.pp
An ARGO system is by default an end system, by the definitions given above.
An ARGO system can be made to function as an intermediate system
instead of an end system with the \fIclnlutil\fR program. 
See \fIclnlutil(8)\fR for more information.
.sh 2 "Report Configuration Function"
.pp
The report configuration function is used by end systems and intermediate
systems to inform each other of their reachability and current subnetwork
addresses. 
This function is invoked whenever the configuration timer
expires. 
This timer fires at a frequency of once every
\fIesis_config_time\fR seconds. 
By default, this value is 60 (seconds), 
but it may be changed with the \fIclnlutil\fR program.
.pp
The report configuration function is contained in the C function 
\fIesis_config()\fR. Called every \fIesis_config_time\fR seconds, 
\fIesis_config()\fR searches the list of active network interfaces
calling \fIesis_shoutput\fR for each interface that is up, has
broadcast ability and has an ISO address configured.
.pp
The function \fIesis_shoutput()\fR has the responsibility of building and 
transmitting ESH and ISH packets.
It takes several arguments, including  a pointer to a network interface
and
a packet type (ESH or ISH).
If the packet type is ESH, then
each NSAP address configured on the specified interface is added to
the ESH NPDU. ISH NPDUs may only contain a single NSAP address\**.
.(f
\** Actually, ISH packets contain Network Entity Titles (NETs). ARGO
does not make a distinction between NETs and NSAPs.
.)f
After the packet is built, it is transmitted on the subnetwork. ESH packets
are sent to the multicast address \fIall intermediate systems\fR, whereas
ISH packets are sent to the multicast address \fIall end systems\fR.
.pp
Each ISH and ESH NPDU contains 
a holding timer setting. This setting (specified 
in seconds) is used by the receiver of the NPDU to set its
holding timer. When its holding timer expires, the information from
the NPDU is erased. The holding timer value sent on each ISH and ESH NPDU
is contained in the variable \fIesis_holding_time\fR. By default, this
timer setting is 120 seconds. This value may be changed with the 
\fIclnlutil\fR utility program.
.sh 2 "Record Configuration Function"
.pp
The Record Configuration function receives ESH or ISH NPDUs, extracts the
configuration information, and updates kernel-resident tables. 
The two functions \fIesis_eshinput()\fR and \fIesis_ishinput()\fR 
process incoming ESH and ISH NPDUs, respectively.
.pp
The ES-IS entity maintains a table that
associates a SNPA-addresses with NSAP-addresses.
This table is called the \fISNPA cache\fR.
.pp
Whenever an ESH or ISH NPDU is received, 
an entry is made in the SNPA cache
via the \fIsnpac_add()\fR function. 
This entry is kept in the cache until the holding timer expires. 
In addition to adding an entry to the SNPA cache, \fIsnpac_add()\fR creates
a default ISO route toward the sender of the ISH.
One such route is kept so that the ES-IS entity has at most one
route to an IS at any time.
Note that ISHs from different sources will 
cause the route to the source of the earlier ISH to be 
overwritten.
The default route
will be removed when the ISH holding timer expires.
.pp
If, at the time an ESH or ISH NPDU is received, the SNPA cache
contains no entry for the NSAP address in the NPDU just received, 
an ESH or ISH (depending on the system type) NPDU is
transmitted to the sender of the NPDU just received.
.sh 2 "Resolving NSAP addresses to SNPA addresses: Query Configuration Function"
.pp
Whenever a device driver needs to resolve an NSAP address to 
an SNPA address, it calls \fIiso_snparesolve()\fR. This function first looks
up the NSAP address in the SNPA cache. If a match is found, the
corresponding SNPA address is returned. If a match is not found and the
system is an end system, and there is a known intermediate system, then
the SNPA address of the intermediate system is returned. It is assumed that
the intermediate system will forward the packet and transmit a redirect back
(see "Redirection Generation", below).
If a match is not found and the system is an end system, but there is no
known intermediate system, then \fIiso_snparesolve()\fR will return 
the multicast address \fIall end systems\fR. 
In all other cases, \fIiso_snparesolve()\fR will return an error.
This is known as the query configuration function. 
.sh 3 "Configuration Response Function"
.pp
In order for the query configuration function to be effective, the network
entity that receives a CLNP DT sent to the \fIall end system\fR
multicast address must transmit an ESH back to the sender of the DT.
This is called the configuration response function and is accomplished by
calling \fIsh_output()\fR from within \fIclnp_input()\fR.
.sh 2 "Redirection Generation"
.pp
When an intermediate system forwards a packet onto the same interface 
upon which 
the packet arrived, a redirect (RD) NPDU is generated. This NPDU is
transmitted by calling \fIesis_rdoutput()\fR from within \fIclnp_forward()\fR.
Note that end systems may forward packets but they do not generate RD PDUs.
.sh 2 "Redirection Receipt"
.pp
RD NPDUs direct an end system to create an SNPA cache entry 
for an NSAP address, or, if such an entry exists, to change
the SNPA address associated with the NSAP address.
The receipt of RD NPDUs is handled by \fIesis_rdinput()\fR. 
This function
parses the RD NPDU and adds an entry to the SNPA cache for the corresponding
destination NSAP address.
If the redirect is toward an intermediate system,
meaning that the RD NPDU contains an SNPA address
of an intermediate system (gateway),
a route is created for the destination NSAP with the intermediate system as
the first hop, or gateway, in the route.
.sh 2 "Multicast Addresses"
.pp
As specified by the December 1987 NBS agreements, the address
\fIall end systems\fR is {0x09, 0x00, 0x2B, 0x00, 0x00, x04} and the address
\fIall intermediate systems\fR is {0x09, 0x00, x02B, 0x00, 0x00, 0x05}. 
These multicast addresses are only used on the 802.3 subnetwork (baseband).
Broadcast addresses are used on the 802.5 subnetwork (token ring). See
the comment in \fC/sys/netargo/iso_snpac.c\fR for more information on 
multicast addresses.
.sh 1 "Connection Oriented Network Service and Subnetwork Service"
.pp
The following sections describe the design of the Connection Oriented 
Network Service (CONS) and the Connection Oriented Subnetwork Service
(COSNS).
The CONS and COSNS are provided by two functionally separate but related
modules, a connection manager and the ISO 8208 (X.25) protocols.
The connection manager is also known in OSI terminology as a 
subnetwork dependent convergence function, or SNDCF.
In ARGO it is used for more than an SNDCF, and it is a sort of 
"glue" that binds a transport service, a network service, a
subnetwork service, and a device driver together, so 
hereinafter it is called "the glue".
This code performs the some of the functions of ISO 8878,
which specifies how ISO 8208 (X.25) can be used to provide the OSI 
connection oriented network service.
The X.25 protocols are implemented in a coprocessor
made by Eicon Technology, Inc.
The device driver \fBecn\fR is the Unix kernel interface to this
coprocessor.
The sections that follow describe the glue and the \fBecn\fR device
driver.
.sh 2 "The Glue"
.pp
The glue provides 
services to several modules in the kernel:
.ip "Subnetwork service" 5
is provided to other network layer protocols, such as CLNP (ISO 8473).
The ARGO CLNP uses this service.  
The Internet IP could be made to use this service with 
minimal effort, because this service interface is made to look
like a standard Unix BSD link layer service (it has
a device driver interface).
.ip "Network service" 5
is provided to transport layer protocols, such as TP (ISO 8073).
This service interface looks like a standard Unix BSD 
network service (a procedure call interface).
.ip "Transport service" 5
could be provided to the socket module. 
While this is not provided with the ARGO software, the glue 
is designed to permit
such a service to be provided with little additional programming effort.
.pp
Higher layer protocols 
that use a connection-oriented
network or subnetwork service need to manage virtual
circuits in a similar fashion. 
Rather than put connection management functions into each higher
layer protocol (HLP) entity
that uses the CONS or COSNS,
in ARGO the connection management is in one module, the glue.
Other alternatives exist, for example in the OSI world,
one may place in the TP entity the function of connection management for TP,
and implement a network connection management subprotocol
of the transport layer (ISO 8073 DAD1, NCMS). 
In addition, connection management for CLNP may be implemented as part of 
the CLNP entity.
A subnetwork dependent convergence protocol (ISO 8878/A) may
be implemented to support connection management for CLNP.
The approach taken in ARGO is different from those suggested in ISO
for two reasons.
First, ARGO aims to minimize the amount of code written to perform a given
task.
Second, ARGO has several coexisting paths through the network layer,
which the ISO approach does not address.
For example, in both ISO 8878/A and in NCMS it is assumed that if
an incoming call arrives from NSAP \(*b 
while a call to NSAP \(*b is being placed,
the two calls are resolved to one virtual circuit.
This is not feasible in the ARGO scenario, since it may not be known
until after
the calls are established and higher level packets are exchanged 
whether the two calls are to be used
for the same path and for the same higher layer protocols.
A possible alternative approach is to use an NSAP-address for each path
through the network layer
(or protocol suite).
This was rejected in the ARGO design because it puts the burden
on the calling application entity or network entity to 
determine the proper NSAP-address to use to determine the protocol
suite to be used to reach the destination end system.
For this reason, none of the approaches suggested in ISO is adopted
here.
.pp
The glue provided in the ARGO
kernel does not provide the full OSI network service.
It provides that subset of the network service that is used
by ARGO TP and by ARGO CLNP.
The OSI connection-oriented network service elements that are
are provided are described in Chapter Four,
in the section titled "Connection Oriented Network Service".
.pp
Each module using the glue has its own service
interface to the glue.
.\" When X.25 is used as a 
.\"transport service, the standard protocol switch table is used, and the procedure
.\"\fIcons_usrreq()\fR is the protosw entry for a
.\"service in the iso protosw table that provides the 
.\"SOCK_STREAM abstraction in the AF_ISO address family,
.\"with protocol ISOPROTO_X25.
.\"This service is called XTS in the glue code and hereafter
.\"in this document.
.\".pp
When the transport layer uses the glue as a network service,
the interface is the procedure
.(b
\fC
.TS
tab(+);
l s s s.
error = cons_output( isop, m, len, isdgm )
.T&
l l l.
 +struct isopcb +*isop;
 +struct mbuf +*m;
 +int+error, len, isdgm;
.TE
\fR
.)b
.pp
When the network layer uses the glue as a subnetwork service
the interface is the device driver-like procedure
.(b
\fC
.TS
tab(+);
l s s s.
error = cosns_output( ifp, m, dst )
.T&
l l l.
 +struct ifnet +*ifp;
 +struct mbuf +*m;
 +struct sockaddr_iso +*dst;
 +int+error;
.TE
\fR
.)b
.pp
When the glue is used as a connection-oriented service 
(i.e., by TP 0, and by TP 4 during the transport 
connection establishment phase, during which
it is not yet known whether class 0 or class 4 will be used)
the following procedures are used:
.(b
\fC
.TS
tab(+);
l s s s.
error = cons_openvc( copcb, dstaddr, so )
.T&
l l l.
 +struct cons_pcb +*copcb;
 +struct sockaddr_iso +*dstaddr;
 +struct socket+*so;
.T&
l s s s.
 +++
error = cons_netcmd( cmd, isop, vc, isdgm )
.T&
l l l.
 +int+cmd;
 +struct isopcb +*isop;
 +int+channel, isdgm;
.TE
\fR
.)b
.pp
The procedure \fIcons_openvc()\fR places a call.
The procedure \fIcons_netcmd()\fR accepts, rejects, or clears
a call. 
There is no incoming call indication, because
the glue uses the passive open model for accepting calls.
The HLP simply sees a new incoming packet, and is given
a virtual circuit number (channel) along with the incoming packet.
If the HLP chooses to reject the call
it may do so, which will cause the virtual circuit (VC) to be cleared.
.pp
The glue may reject (clear) an incoming call for its own reasons.
The following table lists the reasons that the glue may
clear a call and the ISO 8208 diagnostic code used on the X.25 clear packet
in each case.
For a complete list of the permissible diagnostic codes, see
Figure 14-B of ISO 8208.
.in -5
.(b
.TS
center expand box tab(+);
l l.
Reason+Diagnosic code
=
The VC was opened for use with CLNP +Higher level initiated reset
or TP 4 and has been idle for the   +user resynchronization
maximum inactivity time.            +(0xfa)
_
The HLP closed                      +Higher level initiated disconnection
this network connection.            +- normal (0xf1)
_
The HLP rejected                    +Higher level initiated connection
this network connection.            +rejection - transient condition (0xf4)
_
The X.25 call packet contained      +Higher level initiated connection
facilities that are not supported   +rejection - incompatible
by the glue, or did not contain     +information in user data (0xf8)
necessary information, e.g. calling +     
or called DTE address.              +
_
The X.25 call packet contained      +Higher level initiated connection
call user data that does not        +rejection - unrecognizable protocol
indicate any HLP supported by ARGO  +identifier in user data
HLP supported by ARGO               +(0xf9)
_
The given destination               +OSI Network service problem: NSAP
NSAP-address is not supported       +address unknown (permanent
						            +condition) (0xeb)
_
The X.25 packet or a facility       +Packet not allowed-
therein was too long                +packet too long. (0x27)
.TE
.)b
.in +5
.pp
The glue provides several functions common to all 
modules (HLPs) that use the glue.
Regardless of the HLP,
the DTE addresses and NSAP addresses are associated in the same 
manner.
One same network layer protocol identification scheme
(ISO PDTR 9577) for all HLPs.
Several different HLPs need to close inactive X.25
virtual circuits after a timer expires.
The glue insulates the 
device driver interface to the X.25 coprocessor 
from the HLP.
.pp
TP class 0 connections
.\" and the X.25 "transport service" 
do not share X.25 VCs
.\" with each other or among transport service-level circuits (sockets), 
so
.\" these two modules need to keep X.25
the glue needs to maintain 
a 1-1 correspondence between VCs
and sockets.
.\" For use by TP 0 and XTS,
For use by TP 0, 
one network-level pcb is needed for each socket, and that is a
\fIcons_pcb\fR, described below.
.pp
TP class 4 connections may share VCs, 
and TP 4 makes no correspondence between sockets and VCs.
CLNP regards VCs similarly to TP 4.
A given VC may be used simultaneously for many higher level connections,
but all higher level connections using a given VC must use the same
path or protocol suite.
In other words, a TP4 connection running over CONS may not share a
VC with a TP4 connection running over CLNS/COSNS.
.pp
To manage VCs and to maintain the separation of sharable and non-sharable
VCs, the glue uses the following protocol control block:
.(b
\fC
.TS
tab(+);
l s s s.
struct cons_pcb {
.T&
l l l.
 +struct isopcb+_co_isopcb;
+u_short+co_state; 
+u_char+co_flags; 
+u_short+co_ttl;
+u_short+co_init_ttl;
+int+co_channel;
+struct ifnet+*co_ifp;
+struct protosw+*co_proto; 
+struct dte_addr+co_peer_dte;
+struct ifqueue+co_pending;
};
.T&
l l s.
#define co_next+_co_isopcb.isop_next
#define co_prev+_co_isopcb.isop_prev
#define co_head+_co_isopcb.isop_head
#define co_laddr+_co_isopcb.isop_laddr
#define co_faddr+_co_isopcb.isop_faddr
#define co_lport+_co_isopcb.isop_laddr.siso_tsuffix
#define co_fport+_co_isopcb.isop_faddr.siso_tsuffix
#define co_route+_co_isopcb.isop_route
#define co_socket+_co_isopcb.isop_socket
}+
.TE
\fR
.)b
.pp
The \fIcons_pcb\fR contains
an \fIisopcb\fR so that TP 0 
.\" and XTS 
may use the routines that manipulate \fIisopcb\fR structures for allocating
and 
deallocating PCBs, binding addresses to PCBs,
and finding routes.
.pp
A CONS PCB has states CLOSED, LISTENING, CLOSING, 
CONNECTING, ACKWAIT, and  OPEN.
This represents the state of the VC to the degree necessary to the glue.
The glue uses the passive open model for opening VCs.
The coprocessor device driver always accepts
incoming calls and passes an indication to the glue when
a call is accepted by the coprocessor.
If the user of the glue (the HLP) or the glue itself decides
that the VC is not desired, the VC is cleared.
.pp
The \fIcons_pcb\fR contains a bit mask, \fIco_flags\fR, with values:
.(b
\fC
.TS
tab(+);
l l l l.
#define+CONSF_OCRE+0x40+/* created on OUTPUT */
#define+CONSF_ICRE+0x20+/* created on INPUT */
#define+CONSF_DGM+0x04+/* for datagram use only */
.TE
\fR
.)b
.pp
The flag 
CONSF_DGM means that the VC is being used to provide a
datagram (connectionless, unreliable, unsequenced) 
service to the higher layer, and that requests for additional VCs
from the same higher layer entity
may be served by this VC, effectively 
multiplexing higher layer connections on this VC.
When this flag is set in a \fIcons_pcb\fR, there is no associated
\fIco_socket\fR pointer.
When CONSF_DGM is not set, there is an associated
\fIco_socket\fR pointer, and the VC is being used for
TP 0.
.pp
The flag 
CONSF_ICRE means that the VC was created by 
and incoming call indication.
The flag 
CONSF_OCRE means that the VC was created 
on behalf of an outgoing call request.
.pp
The \fIstruct dte_addr\fR field, \fIco_peer_dte\fR,
contains the peer's DTE address.
The glue locates VCs by searching the list of protocol control
blocks for a PCB with a DTE matching that desired.
.pp
The glue is given an NSAP-address by the HLP entity.
The glue finds the desired DTE address by searching the
ES-IS SNPA cache for an SNPA-address (DTE address) associated
with the NSAP-address given by the HLP entity.
This means that to use the CONS, an entry for each desired
peer must appear in the SNPA cache.
ARGO does not provide the ES-IS protocol for use with ISO 8208, so
"permanent" or static entries must be placed in this cache by hand,
using the utility program \fIclnlutil\fR.
.pp
When an incoming call is accepted, the peer's DTE address is
placed in the SNPA cache along with
an NSAP address generated as follows:
.np
If the incoming call contained the peer's NSAP-address
in an Address Extension Facility (AEF, available with 1984 X.25),
this NSAP-address is used, otherwise
.np
the glue creates a "type-37" address (the format defined by AFI 37
in ISO 8348/AD 2).
.pp
TP 4 can have its outgoing packets sent on more than one VC.
The glue presently contains no mechanism for fanning outgoing
packets onto several VCs, however,
it does not prohibit packets arriving for TP 4 on any VC that 
opened with the protocol identifier for TP.
.pp
The glue has the ability to generate AEFs on outgoing calls, but
this ability is turned off,
since the public data network on which ARGO runs at Wisconsin
does not support 1984 X.25, and so it rejects packets containing
AEFs.
The use of AEFs can be reinstated by making a kernel with the 
option \fBX25_1984\fR or by adding the line
.nf
.in +5
\fC
#define X25_1984
\fR
.in -5
.fi
at the top of the file
\fC/sys/netargo/if_cons.c\fR
and rebuilding the kernel.
