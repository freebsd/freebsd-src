.\"	$FreeBSD$
.\"
.NC "The Design of the ARGO Transport Entity"
.sh 1 "Protocol Hooks"
.pp
The design of the AOS kernel IPC support to some
extent mandates the
design of protocols. 
Each protocol must provide the following 
protocol hooks, which are procedures called through a
protocol switch table
(an array of type \fIprotosw\fR as described in
Chapter Five.
.ip "pr_input()" 5
Called when data are to be passed up from a lower layer.
.ip "pr_output()" 5
Called when data are to be passed down from a higher layer.
.ip "pr_init()" 5
Called when the system is brought up.
.ip "pr_fasttimo()" 5
Called every 200 milliseconds by the clock functional unit.
.ip "pr_slowtimo()" 5
Called every 500 milliseconds by the clock functional unit.
.ip "pr_drain()" 5
This is meant to be called when buffer space is low.
Each protocol is expected to provide this routine to free
non-critical buffer space.
This is not yet called anywhere.
.ip "pr_ctlinput()" 5
Used for exchanging information between
protocols, such as notifying a transport protocol of changes
in routing or configuration information.
.ip "pr_ctloutput()" 5
Supports the protocol-dependent 
\fIgetsockopt()\fR
and 
\fIsetsockopt()\fR
options.
.ip "pr_usrreq()" 5
Called by the socket code to pass along a \*(lquser request\*(rq -
in other words a service primitive.
This call is also used for other protocol functions.
The functions served by the \fIpr_usrreq()\fR routine are:
.ip "     PRU_ATTACH" 10
Creates a protocol control block and attaches it to a given socket.
Called as a result of a \fIsocket()\fR system call.
.ip "     PRU_DISCONNECT" 10
Called as a result of a 
\fIclose()\fR system call.
Initiates disconnection.
.ip "     PRU_DETACH" 10
Disassociates a protocol control block from a socket and recycles
the buffer space used for the protocol control block.
Called after PRU_DISCONNECT.
.ip "     PRU_SHUTDOWN" 10
Called as a result of a 
\fIshutdown()\fR system call.
If the protocol supports the notion of half-open connections,
this closes the connection in one direction or both directions,
depending on the arguments passed to
\fIshutdown\fR.
.ip "     PRU_BIND" 10
Gives an address to a socket.
Called as a result of a 
\fIbind()\fR system call, also
when 
socket without a bound address is used.
In the latter case, an unused transport suffix is located and
bound to the socket.
.ip "     PRU_LISTEN" 10
Called as a result of a 
\fIlisten()\fR system call.
Marks the socket as willing to queue incoming connection
requests.
.ip "     PRU_CONNECT" 10
Called as a result of a 
\fIconnect()\fR system call.
Initiates a connection request.
.ip "     PRU_ACCEPT" 10
Called as a result of an 
\fIaccept()\fR system call.
Dequeues a pending connection request, or blocks waiting for
a connection request to arrive.
In the latter case, it marks the socket as willing to accept
connections.
.ip "     PRU_RCVD" 10
The protocol module is expected to have put incoming data
into the socket's receive buffer, \fIso_rcv\fR.
When a receive primitive is used
(\fIrecv(), recvmsg(), recvfrom(),
read(), readv(), \fRand 
\fIrecvv()\fR system calls)
the socket code module copies data from the
\fIso_rcv\fR to the user's
address space.
The protocol module may arrange to be informed each time the socket code
does this, in which case the socket code calls \fIpr_usrreq\fR(PRU_RCVD)
after the data were copied to the user.
.ip "     PRU_SEND" 10
This performs the protocol-dependent part of a send primitive
(\fIsend(), sendmsg(), sendto(), write(), writev(), 
\fRand \fIsendv()\fR system calls).
The socket code 
(procedures \fIsendit() and \fIsosend()\fR)
moves outgoing data from the user's
address space into a chain of \fImbufs\fR.
The socket code takes as much data from the user as it
determines will fit into the outgoing socket buffer, so_snd. 
It passes this much data in the form of an mbuf chain to the protocol
via \fIpr_usrreq\fR(PRU_SEND).
If there are more data than 
the so_snd can accommodate,
the socket code, which is running on behalf of a user process,
puts the user process to sleep.
The protocol module is expected to wake up the user process when
more room appears in so_snd.
.ip "     PRU_ABORT" 10
Called when a socket is closed and that socket
is accepting connections and has
queued pending
connection requests or
partially open connections.
.ip "     PRU_CONTROL" 10
Called as a result of an 
\fIioctl()\fR system call.
.ip "     PRU_SENSE" 10
Called as a result of an 
\fIfstat()\fR system call.
.ip "     PRU_RCVOOB" 10
Performs the work of receiving \*(lqout-of-band\*(rq data.
The socket module has already allocated an mbuf into which
the protocol module is expected to put the incoming 
\*(lqout-of-band\*(rq data.
The socket code will then move the data from this mbuf
to the user's address space.
.ip "     PRU_SENDOOB" 10
Performs the work of sending \*(lqout-of-band\*(rq data.
The socket module has already moved the data
from the user's address space into a chain of mbufs,
which it now passes to the protocol module.
.ip "     PRU_SOCKADDR" 10
Supports the system call
\fIgetsockname()\fR.
Puts the socket's bound address into an mbuf.
.ip "     PRU_PEERADDR" 10
Supports the system call
\fIgetpeername\fR().
Puts the peer's address into an mbuf.
.ip "     PRU_CONNECT2" 10
This is used in the Unix domain to support pipes.
It is not generally supported by transport protocols.
.ip "     PRU_FASTTIMO, PRU_SLOWTIMO" 10
These are superfluous.
None of the transport protocols uses them.
.ip "     PRU_PROTORCV, PRU_PROTOSEND" 10
None of the transport protocols uses these.
.ip "     PRU_SENDEOT" 10
This was added to support TP.
This indicates that the end of the data sent in this
send primitive should
be marked by the protocol as the end of the TSDU.
.sh 1 "The Interface Between the Transport Entity and Lower Layers"
.pp
The transport layer may run over a network layer such as IP
or the ISO connectionless network layer,
or it may run over a multi-purpose layer such as the service
provided by X.25.
X.25 is viewed as a network layer when
TP runs over X.25, and as a 
subnetwork layer 
when IP is running over X.25.
The software interface between data link and network layers differs
considerably from the software interface between transport and network
layers in AOS.
For this reason some modification of the transport-to-lower-layer
interface is necessary to support the suite of protocols included in 
ARGO.
.pp
In AOS it is assumed that the transport layer will run over one
and only one network layer, and therefore it may call the
network layer output procedure directly.
In order to allow TP to run over a set of lower layers,
all domain-specific functions have been put into a set of routines
that are called indirectly through a domain-specific switch table.
The primary reason for this is that the transport and network
layers share information, mostly information pertaining to addresses.
The protocol control blocks for different network layers
differ, so the transport layer cannot just directly
access the network layer's pcb.
Similarly, a network layer may not directly access the transport
pcb because a multitude of transport protocols can run over each
of the network protocols.
.pp
To permit different network-layer protocol control blocks to coexist
under one transport layer, all transport-dependent control
information was put into a transport-specific protocol control block.
A new field, \fIso_tpcb\fR,
was added to the \fIsocket\fR structure to hold a pointer to
the transport-layer protocol control block. 
The existing
field \fCso_pcb\fR is used for the network layer pcb.
.pp
The following structure was added to allow domain-specific
functions to be called indirectly.
All these functions operate on a network-layer pcb.
.pp
.(b
\fC
.TS
tab(+);
l s s s.
struct nl_protosw {
.T&
l l l l.
+int+nlp_afamily;+/* address family */
+int+(*nlp_putnetaddr)();+/* puts addrs in pcb */
+int+(*nlp_getnetaddr)();+/* gets addrs from pcb */
+int+(*nlp_putsufx)();+/* transp suffix -> pcb */
+int+(*nlp_getsufx)();+/* gets t-suffix */
+int+(*nlp_recycle_suffix)();+/* zeroes suffix */
+int+(*nlp_mtu)();+/* get maximum
+++transmission unit size */
+int+(*nlp_pcbbind)();+/* bind to pcb */
+int+(*nlp_pcbconn)();+/* connect */
+int+(*nlp_pcbdisc)();+/* disconnect */
+int+(*nlp_pcbdetach)();+/* detach pcb */
+int+(*nlp_pcballoc)();+/* allocate a pcb */
+int+(*nlp_output)();+/* emit packet */
+int+(*nlp_dgoutput)();+/* emit datagram */
+caddr_t+nlp_pcblist;+/* list of pcbs 
+++for management 
+++of connections */
};
.TE
\fR
.)b
.lp
The switch is based on the address family chosen when the
\fIsocket()\fR system call is made prior to connection establishment.
This unfortunately ties the address family to the domain,
but the only alternative is to add an argument to the \fIsocket()\fR
system call to let the user specify the desired network layer.
In the case of a connection oriented environment with no multi-homing,
it would be possible to determine which network layer is to be
used
from routing
information, but to do this requires unrealistic assumptions
about the environment.
For these reasons, linking the address family to the network
layer protocol is seen as the least of the evils.
The transport suffixes are kept in the network layer's pcb
as well as in the transport layer because 
full transport address pairs are used to identify a connection
in the Internet domain.
.sh 1 "The Architecture of the Transport Protocol Entity"
.pp
A set of protocol hooks is required
by the AOS IPC architecture.
These hooks are used by the protocol-independent parts of the kernel
to gain entry to protocol-specific code.
The protocol code can be entered in one of the following ways:
.ip "1) " 5
at boot time, when autoconfiguration
initializes each protocol through
the 
\fIpr_init()\fR
hook,
.ip "2) " 5
from above, either
a user program making a system call, through
the \fIpr_usrreq()\fR or \fIpr_ctloutput()\fR hooks, or
from a higher layer protocol using the
\fIpr_output()\fR hook,
.ip "3) " 5
from below, a device interrupt servicing an incoming packet
through the \fIpr_input()\fR  and \fIpr_ctlinput()\fR hooks, and
.ip "4) " 5
from a clock interrupt through the \fIpr_slowtimo()\fR
or the
\fIpr_fasttimo()\fR hook.
.\" FIGURE
.so figs/trans_flow.nr
.\".so figs/trans_flow.grn
.pp
The protocol code can be divided into
the following modules, which are described in more detail below.
.CF
shows the flow of data and control 
among these modules.
.in +5
.ip "Timers and References:" 5
The code executed on behalf of \fIpr_slowtimo()\fR.
The fast timeout is not used by TP.
.ip "Driver:" 5
This is the finite state machine for TP.
.ip "Input:     " 5
This is the module that decodes incoming packets,
identifies or creates the pcb for which 
the packet is destined, and creates an "event" to
pass to the driver.
.ip "Output:" 5
This is the module that creates a packet header of a given type
with fields containing 
values that are appropriate to the connection
on which the packet is being sent, appends data if necessary,
and hands a packet
to the lower layer, according to the transport-to-lower-layer
interface.
.ip "Send:      " 5
This module packetizes data from the outbound
socket buffer, \fIso_snd\fR,
handles retransmissions of packetized data, and
drops packetized data from the retransmission queue.
.ip "Receive:" 5
This module reorders packets if necessary,
depacketizes data, passes it to the socket code module,
and determines when acknowledgments should be sent.
.in -5
.sh 1 "Timers and References"
.pp
TP identifies sockets by \fIreference numbers\fR, or
\fIreferences\fR,
which are \*(lqfrozen\*(rq (may not be reassigned)
until some locally defined time after
a connection is broken and its protocol control block
is discarded.
An array of \fIreference blocks\fR is maintained by TP.
The reference number of a reference block is its
offset in the array.
When a reference block is in use it contains 
a pointer to the pcb for the socket to which the
reference applies.
.pp
The system clock calls the \fIpr_slowtimo()\fR and 
\fIpr_fasttimo()\fR hooks for each protocol in the protocol switch table
every 500 and 200 microseconds, respectively.
Each protocol handles its own timers its own way.
The timers in TP take two forms
- those that typically are cancelled and
those that usually expire.
The latter form may have more than one instantiation at any given
time.
The former may not.
The two are implemented slightly
differently for the sake of performance.
.pp
The timers that normally expire 
are kept in a queue, their values all relative
to the value of preceding timer.
Thus all timer values are decremented by a single
operation on the value of the first timer.
The timer is represented by the Ecallout structure:
.(b
\fC
.TS
tab(+);
l s s s.
struct Ecallout {
.T&
l l l l.
+int+c_time;+/* incremental time */
+int+c_func;+/* function to call */
+u_int+c_arg1;+/* argument to routine */
+u_int+c_arg2;+/* argument to routine */
+int+c_arg3;+/* argument to routine */
+struct Ecallout+*c_next;
};
.TE
\fR
.)b
.lp
When an Ecallout structure migrates to the head
of the E timer list, and its \fIc_time\fR
field is decremented to zero, 
the function stored in \fIc_func\fR is
called, with \fIc_arg1, c_arg2\fR, and \fIc_arg3\fR
as arguments.
Setting and cancelling these timers
are accomplished by a linear search and one
insertion or deletion from the timer queue.
This queue is linked to the 
reference block associated with a communication endpoint.
This form used for the reference timer
and for the retransmission timers for data TPDUs.
.pp
The second form of timer, the type that
typically is cancelled, is used for several
timers - the inactivity timer, the sendack timer,
and the retransmission
timer for all types of TPDUs except data TPDUs.
.(b
\fC
.TS
tab(+);
l s s s.
struct Ccallout {
.T&
l l l l.
+int+c_time;+/* incremental time */
+int+c_active;+/* this timer is active? */
};
.TE
\fR
.)b
.lp
All of these timers are stored
directly
in the reference block.
These timers are decremented in one linear scan of
the reference blocks.
Cancelling, setting, and both
cancelling and resetting one of these timers is accomplished by a
single assignment to an array element.
.sh 1 "Driver"
.pp
This is the finite state machine for TP.
A connection is managed by the finite state machine (fsm).
All events that pertain to a connection cause the
finite state machine driver to be called.
The driver takes two arguments - the pcb for the connection
and an event structure.
The event structure contains a field that discriminates
the different types of events, and a union of 
structures that are specific to the event types.
The driver evaluates a set of predicates based on the current
state of the finite state machine (which is kept in the pcb) and the event type.
The result of the predicate evaluation determines
a set of actions to take and a state transition.
The driver takes the actions and if they complete
without errors, the driver makes the state transition.
.pp
The states, event types, predicates, actions, and state transitions are all
specified as a \fIxebec transition file\fR.
\fIXebec\fR is a utility that takes a human-readable description
of a finite state machine
and produces a set of tables and C source code for the driver.
The driver procedure is called \fItp_driver()\fR.
It is located in a file generated by xebec, 
\fCtp_driver.c\fR.
For more details about xebec, see the manual page \fIxebec(1)\fR.
.pp
The transition file for TP is \fCtp.trans\fR,
and it is a good place to begin a perusal of the TP
source code.
.sh 1 "Input"
.pp
This is the module that decodes an incoming packet,
locates or creates the pcb for which 
the packet is destined, and creates an event to
pass to the driver.
The network layer passes a packet up to the appropriate
transport layer by indirectly calling a transport input
routine through the protocol switch table for the network
domain.
There is one protocol switch entry for TP for each domain in which
TP will run (Internet, ISO).
In the Internet domain, the protocol switch field \fIpr_input()\fR
takes the value \fItpip_input()\fR.
This procedure accepts a packet from IP, with the IP header
still intact.
It extracts the network addresses from the IP header,
strips the IP header, and calls the domain-independent
input procedure for TP,
\fItp_input()\fR.
\fITp_input()\fR
decodes a TPDU.
The multitude of options, the variable-length
nature of the options, the semantics of the
options, and the possible combinations of concatenated
TPDUs make this a 
complex procedure.
It is sensitive to changes, and from 
the point of view of a software maintenance, it is a
potential hazard.
Because it is in the 
critical path of TP however, some compromise
was made between maintainability and efficiency.
Multiple copies of sections of code were avoided as much as
possible,
not for the sake of saving space, but rather for the sake
of maintainability.
Ironically,
this detracts somewhat from the readability of the code.
.pp
Once a TPDU has been decoded and a pcb has been
identified for the TPDU,
the appropriate fields of the TPDU
are extracted and their values are placed in
an event structure.
Finally, \fItp_driver()\fR is called with
the event structure and the pcb as parameters.
.sh 1 "Output"
.pp
This module creates a TPDU header of a given type
with field values that are appropriate to the connection
on which the TPDU is being sent, appends data if necessary,
and hands a TPDU
to the lower layer according to the transport-to-lower-layer
interface.
Whenever a TPDU is to be sent to the peer or prospective peer,
the function \fItp_emit()\fR
is called, passing as arguments the pcb a TPDU type and several miscellaneous
other type-specific arguments, possibly including some data.
The data are in the form of an mbuf chain.
\fITp_emit()\fR prepends to the data an mbuf containing a TP header,
fills in the fields of the header according to the parameters
given, performs the checksum if appropriate, and
calls a domain-specific output routine.
For the Internet domain, this output routine is
\fItpip_output()\fR, which takes
as arguments the mbuf chain representing the TPDU,
and a network level pcb.
Some protocol errors cannot be associated with 
a connection 
but require that TP issue
an ER TPDU or a DR TPDU. 
When these errors occur the routine
\fItp_error_emit()\fR is called.
This procedure creates the appropriate type of TPDU
and passes it to a domain-dependent routine for transmitting datagrams.
In the Internet domain,
\fItpip_output_dg()\fR is called.
This takes as arguments an mbuf chain representing the TPDU,
a source network address, and a destination network address.
.sh 1 "Send"
.\" FIGURE
.so figs/mbufsnd.nr
.\".so figs/mbufsnd.grn
.pp
This module packetizes data from the outbound
socket buffer, \fIso_snd\fR,
handles retransmissions of packetized data, and
drops packetized data from the retransmission queue.
The major routine in this module is \fItp_send()\fR, which
takes a range of sequence numbers as arguments.
For each sequence number in the range,
it packetizes the an appropriate amount
of outbound data, and places the resulting TPDU on 
a retransmission control queue subject to the
constraints imposed by the rules of expedited data,
maximum packet sizes, and end-of-TSDU markers.
.pp
The most complicating factor is that of managing
expedited data.
A normal datum may not be sent (for its first time) before the
acknowledgment of any expedited datum
that was received from the user after the 
normal datum was received. 
In order to enforce this rule,
each TPDU must be marked in some way
so that it will be known which expedited datum
must be delivered and acknowledged by the peer before this TPDU may be transmitted
for the first time.
Markers are placed in \fIso_snd\fR 
when an
outgoing expedited datum arrives from the user. 
A marker is an mbuf structure with an \fIm_len\fR
of zero, but with the data area nevertheless containing
the sequence number of an expedited data TPDU.
The \fIm_type\fR of a marker is a new type, MT_XPD.
.pp
\fITp_send()\fR stops packetizing data when it encounters a marker
for an unacknowledged expedited datum.
If it encounters a marker for an expedited TPDU that has already
been acknowledged, the marker is jettisoned.
.CF
illustrates the structure of the sending socket buffer used
for normal data.
.pp
When \fItp_send()\fR moves data from mbufs on \fIso_snd\fR to the retransmission
control queue, it needs to know
how many octets of data can be placed in each TPDU.
The appropriate amount depends on, among other things,
the maximum transmission unit of the network layer
on the route the packet will take.
To determine the maximum transmission unit,
TP queries the network layer through
the domain-dependent switch table's field, \fInl_mtu\fR.
In the Internet domain, this resolves to \fItp_inmtu()\fR.
The header sizes for the network and transport layers
also affect the amount of data that can go into a packet,
and these sizes depend on the connection's characteristics.
.pp
Once the maximum amount of data per TPDU is determined,
\fItp_send()\fR can pull this amount off the \fIso_snd\fR queue to form
a TPDU,
assign a TPDU sequence number,
and place the new TPDU on the 
retransmission control queue.
The retransmission control queue is a list of mbuf chains.
Each mbuf chain represents one TPDU, preceded by an
\fIrtc structure\fR:
.(b
\fC
.TS
tab(+);
l s s s.
struct tp_rtc {
.T&
l l l l.
+struct tp_rtc+*tprt_next;+/* next rtc struct in list */
+SeqNum+tprt_seq;+/* seq # of this TPDU */
+int+tprt_eot;+/* end of TSDU? */
+int+tprt_octets;+/* # octets in this TPDU */
+struct mbuf+*tprt_data;+/* ptr to the octets of data */
.\"/* Performance measurment info: */
.\"int	tprt_window;	/* in which call to tp_send() was
.\"			  * this TPDU formed? 
.\"			  */
.\"struct timeval	tprt_sess_time;	/* time session received the 
.\"			* majority of the data for this packet on send;
.\"			* on recv, this is the time it's given to session 
.\"			*/
.\"struct timeval	tprt_net_time;	/* time first copy was given to net layer
.\"			* on send; on receive it's the time received from
.\"			* the network 
.\"			*/
};
.TE
\fR
.)b
.lp
Once TPDUs are on the retransmission control queue,
they are retransmitted or dropped by the actions
of timers.
The procedure \fItp_sbdrop()\fR
removes the TPDUs from the retransmission queue.
It takes a sequence number as an argument and drops
all TPDUs up to and including the TPDU with that sequence number.
.pp
When an AK TPDU arrives, the values from
its credit and sequence number fields
are passed to \fItp_goodack()\fR, which
determines whether or not the AK brought any news with it,
and therefore whether TP can send more data
or expedited data.
If this AK acknowledges something heretofore unacknowledged,
\fItp_goodack()\fR drops the appropriate TPDU(s) from the retransmission
control list, computes the smoothed average round trip time
and standard deviation of the round trip time, 
and updates
the retransmission timer based on these statistics.
It sets a flag in the pcb if the TP entity is obliged to
send the flow control confirmation parameter on its next
AK TPDU.
\fITp_goodack()\fR returns true if the AK brought some news with it,
either with respect to a change in credit or with respect to
new acknowledgments.
.pp
The function \fItp_goodXack()\fR is called when an XAK TPDU
arrives.
It takes the XAK sequence number as an argument and
determines if the XAK acknowledges the last XPD TPDU sent.
If so, it drops the expedited data from the outgoing
expedited data buffer.
By its definition in the TP specification,
the expedited data stream has a window
of size 1,
that is, 
only one expedited datum (packet) can be buffered
at a time.
\fITp_goodXack()\fR returns true if the XAK acknowledged
the last XPD TPDU sent and the data were dropped,
and it returns false if the acknowledgment caused no action to be taken.
.\" NEXT FIGURE
.so figs/mbufrcv.nr
.\".so figs/mbufrcv.grn
.sh 1 "Receive"
.pp
This module reorders incoming TPDUs if necessary,
depacketizes data, passes it to the socket code module,
and determines when acknowledgments should be sent.
The function 
\fItp_stash()\fR
takes an DT TPDU as an argument, and if the TPDU is not in
sequence, it saves the TPDU in a \fItp_rtc\fR structure in
a list, with the TPDUs
kept in order.
When the next expected TPDU arrives, the
list of out-of-order TPDUs is scanned for 
more TPDUs in sequence, updating
a field in the pcb, \fItp_rcvnxt\fR which
always contains the sequence
number of 
the next expected TPDU.
If an acknowledgment is to be generated
at any time, the value of tp_rcvnxt goes into the
\fIYR-TU-NR\fR\** field of the acknowledgment TPDU.
.(f
\** 
This is the name used in ISO 8073 for the field
which indicates the sequence number of the next expected DT TPDU.
.)f
.pp
\fITp_stash()\fR returns true if an acknowledgment needs to be generated
immediately, false not.
The acknowledgment strategy is therefore implemented in this routine.
Acknowledgments may be generated for one or more of several reasons,
listed below.
\fITp_stash()\fR increments a counter for each of these reasons
for which an acknowledgment is generated, and a counter for TPDUs
that are not acknowledged immediately.
.ip "ACK_STRAT_EACH" 5
The acknowledgment strategy in use calls for acknowledging each 
data packet with an AK TPDU.
.ip "ACK_STRAT_FULLWIN" 5
The acknowledgment strategy in use calls for acknowledging 
upon receiving the DT TPDU that represents the upper window
edge of the last advertised window.
.ip "ACK_DUP" 5
A duplicate data TPDU was received.
.ip "ACK_REORDER" 5
A DT TPDU arrived in the window but out of order.
.ip "ACK_EOT" 5
A DT TPDU arrived, and it had the end-of-TSDU flag set.
.pp
Upon receipt of a DT TPDU that is in order, and upon reordering
DT TPDUs, 
\fItp_stash()\fR
places the TSDUs into the socket's receive
socket buffer, \fIso->so_rcv\fR in mbuf chains, with
TSDUs delimited by mbufs of the \fIm_type\fR MT_EOT,
which is a new type with the ARGO kernel.
.CF
illustrates the structure of the receiving socket buffer used
for normal data.
.pp
A separate socket buffer, \fItpcb->tp_Xrcv\fR,
is used for
buffering expedited data.
Only one expedited data packet may reside in this buffer at a time
because the TP standard limits the size of the window on expedited flow
to be 1.
This means the data structures are straightforward;
there is no need to distinguish between separate TSDUs in this socket buffer.
.pp
Credit is determined 
by dividing the total amount of available
space in the receive buffer
by the negotiated maximum TPDU size.
TP can often offer a larger credit than this if it uses
an average of the measured actual TPDU sizes.
This strategy was once an option in the ARGO kernel,
but it was removed because unless the actual TPDU size
is constant, it leads to reneging of credit,
retransmissions, and decreased performance.
It does not work well when there is any fluctuation in the sizes
of TPDUs and it carries the penalty of lengthening the critical path
of the TP entity.
.sh 1 "Major Data Structures and Types"
.pp
In addition to the types commonly used in the kernel,
such as 
.(b
\fC
.TS
tab(+);
l l l l.
 +typedef+unsigned char+u_char;
 +typedef+unsigned int+u_int;
 +typedef+unsigned short+u_short;
.TE
\fR
.)b
TP uses the following types:
.(b
\fC
.TS
tab(+);
l l l l.
 +typedef+unsigned int+SeqNum
 +typedef+unsigned short+RefNum;
 +typedef+int+ProtoHook;
.TE
\fR
.)b
.pp
Sequence numbers can be either 7 or 31 bits.
An unsigned integer is used in all cases, and the proper type
of arithmetic is performed with bit masks.
Reference numbers are 16 bits.
ProtoHook is the type of the procedures that are in switch
tables, which,
although they are not functions,
are declared \fIint\fR rather than \fIvoid\fR
to be consistent with the rest of the kernel.
.pp
The following structures are fundamental
types used throughout TP,
in addition to those already described in the 
section,
"The Design of the Transport Entity".
.(b
\fC
.TS
tab(+);
l s s s.
struct tp_ref {
.T&
l l l l.
+u_char+tpr_state;+/* REF_FROZEN...*/
+struct Ccallout+tpr_callout[N_CTIMERS];+/* C timers */
+struct Ecallout+tpr_calltodo;+/* E timers list */
+struct tp_pcb+*tpr_pcb;+/* --> PCB */
};
.TE
\fR
.)b
.lp
The reference structure is logically a part of the protocol
control block and it is linked to a pcb, but it may outlive
a pcb.
When a connection is dissolved, the pcb may be recycled
but the reference structure must remain until the reference
timer goes off.
The field \fItpr_state\fR takes the values
REF_FROZEN (a reference timer is ticking),
REF_OPEN (in use, has timers and an associated pcb),
REF_OPENING (has a pcb but no timers), and
REF_FREE (free to reallocate).
.pp
The TP protocol control block is too large to fit into
one mbuf structure so it comprises two structures
linked together, the 
\fItp_pcb\fR structure and the.
\fItp_pcb_aux\fR structure.
The \fItp_pcb_aux\fR structure contains
items that are used less frequently than those in
the former structure, since each access to these
items requires a second pointer dereference.
.(b
\fC
.TS
tab(+);
l s s s.
struct tp_pcb_aux {
.T&
l l l s.
 +struct sockbuf+tpa_Xsnd;+/* for expedited data */
+struct sockbuf+tpa_Xrcv;+/* for expedited data */
+u_char +tpa_vers;+/* protocol version */
+u_char +tpa_peer_acktime;+/* to compute DT TPDU
+++retrans timer value */
+SeqNum+tpa_Xsndnxt;+/* seq # of
+++next XPD to send */
+SeqNum+tpa_Xuna;+/* seq # of 
+++unacked XPD */
+SeqNum+tpa_Xrcvnxt;+/* next XPD seq #
+++expect to recv */
+/* addressing */
+u_short+tpa_domain;+/* domain AF_ISO,...*/
+u_short+tpa_fsuffixlen;+/* foreign suffix */
+u_char+tpa_fsuffix[MAX_TSAP_SEL_LEN];+
+u_short+tpa_lsuffixlen;+/* local suffix */
+u_char+tpa_lsuffix[MAX_TSAP_SEL_LEN];+
.T&
l s s s.
 +/* AK subsequencing */
.T&
l l l s.
 +u_short+tpa_s_subseq;+/* next subseq to send */
+u_short+tpa_r_subseq;+/* highest recv subseq */
};
.TE
\fR
.)b
.pp
The major portion of the protocol control block is in the
\fItp_pcb\fR structure:
.(b
\fC
.TS
tab(%);
l s s s.
struct tp_pcb {
.\" *************************************** 
.T&
l l l l.
.\" The next line sets the spacing for the table: 1+3 17+3 17+3 13+3
 %                 %                 %
.\"456789 123456789- 123456789 123456-789 123456789 1234567890
.\"
 %struct tp_ref%*tp_refp;%    
.T&
l l l s.
%%/* reference structure */%
.\" *************************************** 
.T&
l l l l.
 %struct tp_pcb_aux%*tp_aux;% 
.T&
l l l s.
 %%/*rest of tpcb (auxiliary struct)*/%
.\" *************************************** 
.T&
l l l l.
 %caddr_t%tp_npcb;%/* to ll pcb */
%struct nl_protosw%*tp_nlproto;%
.T&
l l l s.
 % %/* domain-dependent routines */%
.\" *************************************** 
.T&
l l l l.
 %struct socket%*tp_sock;%/* back ptr */
.\" *************************************** 
.T&
l s s s.

/* local and foreign reference numbers: */
.T&
l l l l.
 %RefNum%tp_lref;% 
%RefNum%tp_fref;%
.\" *************************************** 
.T&
l s s s.
.\"456789 123456789 123456789 123456789 123456789 1234567890

/* Stuff for sequence space arithmetic: 
 * Maintaining 2 sequence spaces is a pain so we set these
 * values once at connection establishment time. Sequence
 * number arithmetic is a set of macros which uses these.
 * Sequence numbers are stored as 32 bits.
 * tp_seqmask tells which of the 32 bits is used.
 * tp_seqibt  is the lsb that is not used.  When set,
 *   it indicates wraparound has occurred.
 * tp_seqhalf is the value that is half the sequence space.
 *   (or half plus one).
 */
.T&
l l l l.
%u_int%tp_seqmask;%/* mask */
%u_int%tp_seqbit;%/* wraparound */
%u_int%tp_seqhalf;%/* half space */
.\" *************************************** 
.T&
l s s s.

/* flags:  values are defined in tp_user.h.
 * Here we keep such info as which options 
 * are in use: checksum, extended format,
 * flow control in class 2, etc.
 * See tp(4p) man page.
 */
.\" *************************************** 
.T&
l l l l.
 %u_short%tp_state;%/* fsm */
%short%tp_retrans;%
.T&
l l l s.
 % % /* # times to retransmit */% 
.\" *************************************** 
.T&
l s s s.

/* credit & sequencing info for SENDING: */
.T&
l l l s.
 %u_short%tp_fcredit;%
 % %/* remote real window */%
 %u_short%tp_cong_win;%
 % %/* remote congestion window */%
.\" *************************************** 
%SeqNum%tp_snduna;%
.T&
l l l s.
 % %/* seq # of lowest unacked DT */% 
.\" *************************************** 
.T&
l l l l.
 %struct tp_rtc    %*tp_snduna_rtc;% 
.T&
l l l s.
 % %/* ptr to mbufs containing lowest% 
%% * unacked TPDUs sent so far%
%% */%
.\" *************************************** 
.T&
l l l l.
 %SeqNum%tp_sndhiwat;% 
.T&
l l l s.
 % %/* highest DT sent yet */% 
.\" *************************************** 
.T&
l l l l.
 %struct tp_rtc%*tp_sndhiwat_rtc;% 
.T&
l l l s.
 % %/* ptr to mbufs containing the last% 
%% * DT sent - this is the last item %
%% * on the list that starts%
%% * at tp_snduna_rtc%
%% */%
.\" *************************************** 
.T&
l l l l.
 %int %tp_Nwindow;%/* for perf. measmt */
.\" *************************************** 
.T&
l s s s.

/* credit & sequencing info for RECEIVING: */
.\" *************************************** 
.T&
l l l s.
 %SeqNum%tp_sent_lcdt;%
 %%/* cdt according to last AK sent */%
 %SeqNum%tp_sent_uwe;% 
 % %/* upper window edge, according to% 
%% * the last AK sent %
%% */*
 %SeqNum%tp_sent_rcvnxt;% 
 % %/* rcvnxt, according to% 
%% * the last AK sent%
%% */*
.\" *************************************** 
.T&
l l l l.
 %short%tp_lcredit;%/* local */
.\" *************************************** 
.T&
l l l l.
 %SeqNum%tp_rcvnxt;% 
.T&
l l l s.
 % %/* next DT seq# we expect to recv */% 
.\" *************************************** 
.T&
l l l l.
 %struct tp_rtc%*tp_rcvnxt_rtc;% 
.T&
l l l s.
 % %/* ptr to mbufs containing unacked % 
%% * DTs received out of order, and %
%% * which we haven't acknowledged%
%% */%
.\" *************************************** 
.TE
.TS
tab(%);
l s s s.
/* Items kept in the aux structure: */

.\" *************************************** 
.T&
l s s l.
#define  tp_vers%tp_aux->tpa_vers
#define  tp_peer_acktime%tp_aux->tpa_peer_acktime
#define  tp_Xsnd%tp_aux->tpa_Xsnd
#define  tp_Xrcv%tp_aux->tpa_Xrcv
#define  tp_Xrcvnxt%tp_aux->tpa_Xrcvnxt
#define  tp_Xsndnxt%tp_aux->tpa_Xsndnxt
#define  tp_Xuna%tp_aux->tpa_Xuna
#define  tp_domain%tp_aux->tpa_domain
#define  tp_fsuffixlen%tp_aux->tpa_fsuffixlen
#define  tp_fsuffix%tp_aux->tpa_fsuffix
#define  tp_lsuffixlen%tp_aux->tpa_lsuffixlen
#define  tp_lsuffix%tp_aux->tpa_lsuffix
#define  tp_s_subseq%tp_aux->tpa_s_subseq
#define  tp_r_subseq%tp_aux->tpa_r_subseq
.\" *************************************** 
.T&
l s s s.
 % % % 
/* parameters per-connection controllable by user: */
.\" *************************************** 
.T&
l l l l.
 %struct%tp_conn_param%_tp_param; 
 % % %
.\" *************************************** 
.T&
l s s l.
#define  tp_Nretrans%_tp_param.p_Nretrans
#define  tp_dr_ticks%_tp_param.p_dr_ticks
#define  tp_cc_ticks%_tp_param.p_cc_ticks
#define  tp_dt_ticks%_tp_param.p_dt_ticks
#define  tp_xpd_ticks%_tp_param.p_x_ticks
#define  tp_cr_ticks%_tp_param.p_cr_ticks
#define  tp_keepalive_ticks%_tp_param.p_keepalive_ticks
#define  tp_sendack_ticks%_tp_param.p_sendack_ticks
#define  tp_refer_ticks%_tp_param.p_ref_ticks
#define  tp_inact_ticks%_tp_param.p_inact_ticks
#define  tp_xtd_format%_tp_param.p_xtd_format
#define  tp_xpd_service%_tp_param.p_xpd_service
#define  tp_ack_strat%_tp_param.p_ack_strat
#define  tp_rx_strat%_tp_param.p_rx_strat
#define  tp_use_checksum%_tp_param.p_use_checksum
#define  tp_tpdusize%_tp_param.p_tpdusize
#define  tp_class%_tp_param.p_class
#define  tp_winsize%_tp_param.p_winsize
#define  tp_netservice%_tp_param.p_netservice
#define  tp_no_disc_indications%_tp_param.p_no_disc_indications
#define  tp_dont_change_params%_tp_param.p_dont_change_params
.\" *************************************** 
.TE
.\" *************************************** 
.\" *************************************** 
.\" *************************************** 
.TS
tab(%);
l l l l.
.\" The next line sets the spacing for the table: 1+3 17+3 17+3 13+3
.\"456789 123456789- 123456789 123456-789 123456789 1234567890
.\"
.T&
l l l s.
 %%/* log2(the negotiated max size) */% 
.T&
l l l l.
 %int%tp_l_tpdusize;%/* # bytes */
.\" *************************************** 
 %struct timeval%tp_rtt;% 
.T&
l l l s.
 % %/* smoothed avg round-trip time */% 
 %struct timeval%tp_rtv;% 
 % %/* std deviation of round-trip time */% 
%struct timeval%tp_rttemit[ TP_RTT_NUM + 1 ];%
%%/* times that the last TP_RTT_NUM %
%% * DT_TPDUs were transmitted %
%% */%
.\" *************************************** 
 %unsigned % % 
%  tp_sendfcc:1,%/* shall next ack %
% %include flow control conf. param? */%
.\" *************************************** 
.T&
l l l s.
 %  tp_trace:1,%/* is this pcb being traced?% 
%% * (not used yet) %
%% */%
.\" *************************************** 
%  tp_perf_on:1,%/* statistics being kept? */% 
.\" *************************************** 
%  tp_reneged:1,%/* have we reneged on credit%
%% * since the last AK TPDU was sent? %
%% */%
%  tp_decbit:4,%/* congestion experienced? */%
%  tp_flags:8,%/* see #defines below */%
.\" *************************************** 
%  tp_unused:16;%%
.T&
l s s l.
#define  TPF_XPD_PRESENT%TPFLAG_XPD_PRESENT
#define  TPF_NLQOS_PDN%TPFLAG_NLQOS_PDN
#define  TPF_PEER_ON_SAMENET%TPFLAG_PEER_ON_SAMENET
%%%
.\" *************************************** 
.T&
l l l l.
 %struct tp_pmeas%*tp_p_meas;% 
.T&
l l l s.
 % %/* ptr to mbuf to hold the perf.% 
%% * statistics structure %
%% */%
.\" *************************************** 
};
.TE
\fR
.\"
.\" end of tpcb structure (thank you)
.\"
.)b
.fi
.sh 1 "Sequence Number Arithmetic"
.pp
Sequence numbers in TP can be either 7 bits 
(\*(lqnormal format\*(rq)
or 31 bits
(\*(lqextended format\*(rq).
Sequence numbers are unsigned integers,
regardless of their format.
Three fields are kept in the pcb to manage the sequence
number arithmetic:
.(b
\fC
.TS
tab(+);
l l l l.
 +u_int+tp_seqmask;+/* mask for seq space */
 +u_int+tp_seqbit;+/* bit for seq # wraparound */
 +u_int+tp_seqhalf;+/* half the seq space */
.TE
\fR
.)b
.lp
\fITp_seqmask\fR 
is a bit mask indicating which bits are legitimate 
for a sequence number of either format.
It takes the value 0x7f if 7-bit sequence numbers are in use,
and 0x7fffffff if 31-bit sequence numbers are in use.
\fITp_seqbit\fR 
is the bit that becomes set when a sequence number wraps around
while being incremented.
Its value is 0x80 for normal format, 0x80000000 for extended format.
\fITp_seqhalf\fR 
takes the value which is in the middle of the sequence space,
0x40 for normal format,
and
0x40000000 for extended format.
.(b
.nf
The macro 
.fi
\fC
.TS
tab(+);
l l l l.
     SEQ(tpcb, x)
.TE
\fR
.)b
.lp
extracts a sequence number from the location
in which it is stored.
.pp
The macros
.(b
\fC
.TS
tab(+);
l l s s l.
 +SEQ_GT(tpcb, seq, t)+is seq > t?
 +SEQ_GEQ(tpcb, seq, t)+is seq >= t?
 +SEQ_LT(tpcb, seq, t)+is seq < t?
 +SEQ_LEQ(tpcb, seq, t)+is seq <= t?
 +SEQ_INC(tpcb, seq)+seq\+\+
 +SEQ_DEC(tpcb, seq)+seq--
 +SEQ_SUB(tpcb, seq, amt)+seq -= amt
 +SEQ_ADD(tpcb, seq, amt)+seq \+= amt
.TE
\fR
.)b
.lp
perform the indicated comparisons and arithmetic
on their arguments.
.pp
An example of how these macros
are used is as follows.
To determine if a sequence
number \fIseq\fR is in a receive window
bounded by
\fIlwe\fR and \fIuwe\fR,
we define the
macro
.(b
\fC
.TS
tab(+);
l l.
#define+IN_RWINDOW(tpcb, seq, lwe, uwe)\\
+( SEQ_GEQ(tpcb, seq, lwe) && SEQ_LT(tpcb, seq, uwe) )
.TE
\fR
.)b
.sh 1 "TP Implementation Options"
.pp
The transport protocol specification leaves several
things to the discretion of the implementor,
some of which may affect the performance
of individual connections and
aggregate performance.
Wherever different strategies are likely to favor
the performance of
individual connections to the detriment of aggregate performance
or vice versa, the
various strategies are under the control of options via the
\fIgetsockopt()\fR and
\fIsetsockopt()\fR system calls (see the manual pages
\fIgetsockopt(2)\fR,
\fIsetsockopt(2)\fR  
and
\fItp(4p)\fR  
for details).
In some cases the preferred strategies differ for the different
subnetworks, so the strategies chosen will be determined
by the subnetwork in use.
.sh 2 "TPDU size"
.pp
The limitation of the maximum TPDU size to a power of two is
unfortunate in the LAN environment.
For example, if the maximum NSDU size is around 1500, as in the case of an
Ethernet,
using a maximum TPDU size of 1024 reduces
the possible throughput by approximately 30%.
TP negotiates a maximum TPDU size of 2048 and
generates TPDUs of size around 1500.
Obviously this works well only when the peer is known to be 
using the same scheme (so that the peer
doesn't send TPDUs of size 2048 and cause its
network layer to fragment the TPDUs).
This is likely to be the case in a LAN where
all protocol entities are under the same administrative
control.
The maximum TPDU size negotiated is under the control of the user,
so
it is possible to prevent this scheme from being used
by default
when the peer is not on the same LAN, by
setting the \fItp.tpdusize\fR parameter in the ARGO directory service
file to
something less than the network's maximum transmission
unit.
.\"***********************************************************
.sh 2 "Congestion Window Strategy"
.pp
The congestion window strategy from the
DoD Internet 
was adapted for use with TP.
The strategy is intended to minimize the 
adverse effect
of transport's retransmission on an
already congested network.
.pp
A TP entity keeps two notions of the peer's window:
the real window, which is that advertised by the peer
in AK TPDUs, and the congestion window, which is a locally
controlled window.
TP uses the smaller of the two windows when transmitting.
The congestion window starts small, which keeps a
new connection from overloading the network with a sudden
burst of packets
immediately after connection establishement.
This is called \fIslow start\fR. 
For each successful acknowledgment received, the congestion
window grows by one, until eventually the real window
is the one in use.
If a retransmission timer expires, the congestion window
is reset to size one.
.pp
The congestion window strategy is used for class 4 unless
the transport user requests that it not be used.
The slow start strategy is used for traffic over a PDN
unless
the transport user requests that it not be used.
Slow start is not used for traffic over a LAN unless
its use is requested by the transport user.
.\"***********************************************************
.sh 2 "Retransmission strategies"
.pp
A retransmission timer is invoked for each set of DT TPDUs
sent in one send operation (call to \fItp_send()\fR).
This set of packets is called the \fIsend window\fR for the purpose
of this discusssion.
.pp
The number of TPDUs 
in a send window
depends on the remote credit and the amount of data
in the local send buffers.
When a retransmission timer goes off, the lower
window edge 
is reevaluated but the upper window edge is not reevaluated.
.pp
There are several retransmission strategies implemented in
ARGO TP.
The choice of strategies is the user's, and is made with the 
\fIsetsockopt()\fR system call.
The strategies are summarized here:
.ip "Retransmit LWE TPDU only:" 5
Only the TPDU representing the new lower window edge 
is retransmitted.
This is the default retransmission strategy.
.ip "Retransmit whole send window:" 5
Retransmission begins with the new lower window edge
and continues up to the old upper window edge.
.pp
The value of the data retransmission timer
adapts to the average round trip time and the standard deviation of
the round trip time.
A round trip time is the time that passes between
the moment of a packet's first transmission and 
the moment it is first acknowledged.
The average round trip time
is kept by the sending side of TP, using
a formula for 
smoothing the average:
.(b
\fC
.TS
tab(+);
l l l l.
#define+TP_RTT_ALPHA+3
#define+TP_RTV_ALPHA+2
+++
#define+SMOOTH(alpha, old, new) \\
+(((new-old) >> alpha ) \+ (old) )
.TE
\fR
.)b
.lp
The times included in the average are chosen as follows.
The time of 
each packet's initial transmission is kept (for the last
\fIN\fR packets, where \fIN\fR is a defined constant).
When an AK TPDU arrives, ARGO TP subtracts the initial transmission
time for the lowest unacknowledged sequence number that was
acknowledged by this AK TPDU from the current time,
and apply the resulting time to the average.
Hence, not all packets are included in this average,
which is as it should be since
the purpose of this measurement is 
to find a good value for the retransmission timer.
.pp
Each time part of a window is retransmitted,
the retransmission timer for that window is increased.
This does not affect the retransmission timers for other windows.
.\"***********************************************************
.sh 2 "Acknowledgment strategies"
.pp
The transport protocol specification
requires acknowledgments to be sent immediately
upon receipt
of  CC TPDUs (in class 4), XPD TPDUs, and DT TPDUs containing an
EOT marker, and at other times as required for flow control,
otherwise acknowledgments may be delayed.
In addition to the times when an acknowledgment is required,
ARGO TP transmits an AK TPDU whenever the user receives some data,
thereby increasing the size of the window.
For those times when
immediate acknowledgment is optional,
ARGO TP offers two acknowledgment strategies:
.ip "     Acknowledge each TPDU" 10
Upon receipt of a DT TPDU and AK TPDU is sent.
.ip "     Acknowledge full window" 10
Acknowledgment is issued
upon receipt of enough data to
consume the last advertised credit.
.pp
The latter strategy
requires a timer to trigger an acknowledgment
in case the peer doesn't send the entire window 
quickly.
This timer is called the
\fIsendack timer\fR.
The upper bound on the value of this timer 
is called the \fIlocal acknowledgment time\fR.
The local acknowledgment time may be "advertised" to the 
peer during connection establishment, and the
peer may choose to use this value to
adjust its retransmission timers.
The ARGO TP entity advertises its local acknowledgment time
on a CR TPDU, but it is not 
constrained by 
the remote acknowledge time, should the peer 
advertise it.
Instead,
ARGO TP adapts its sendack timer
to the behavior of the connection.
.pp
Under the assumption that the round trip time is
often 
symmetric,
and lacking 
a method to measure
the round trip time in the other direction,
ARGO TP uses the measured average round trip time
to adjust the sendack timer.
.pp
The choice of strategies is made with the
\fIsetsockopt()\fR system call.
The default strategy is
to
delay acknowledgments until the most recently advertised window is filled.
