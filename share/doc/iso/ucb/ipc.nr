.NC "The Design of Unix IPC"
.sh 1 "General"
.pp
The ARGO implementation of 
TP and CLNP was designed to fit into the AOS
kernel
as easily as possible.
All the standard protocol hooks are used.
To understand the design, it is useful to have
read 
Leffler, Joy, and Fabry:
\*(lq4.2 BSD Networking Implementation Notes\*(rq July 1983.
This section describes the
design of the IPC support in the AOS kernel.
.sh 1 "Functional Unit Overview"
.pp
The 
AOS
kernel
is a monolithic program of considerable size and complexity.
The code can be separated into parts of distinct function,
but there are no kernel processes per se.
The kernel code is either executed on behalf of a user
process, in which case the kernel was entered by a system call, 
or it is executed on behalf of a hardware or software interrupt.
The following sections describe briefly the major functional units 
of the kernel.
.\" FIGURE
.so ../wisc/figs/func_units.nr
.CF
shows the arrangement of these kernel units and 
their interactions.
.sh 2 "The file system."
.pp
.sh 2 "Virtual memory support."
.pp
This includes protection, swapping, paging, and
text sharing.
.sh 2  "Blocked device drivers (disks, tapes)."
.pp
All these drivers share some minor functional units,
such as buffer management and bus support
for the various types of busses on the machine.
.sh 2 "Interprocess communication (IPC)."
.pp
This includes 
support for various protocols, 
buffer management, and a standard interface for inter-protocol
communication.
.sh 2 "Network interface drivers." 
.pp
These drivers are closely tied to the IPC support. 
They use the IPC's buffer management unit rather
than the buffers used by the blocked device drivers.
The interface between these drivers and the rest of the kernel 
differs from the interface used by the blocked devices.
.sh 2 "Tty driver" 
.pp
This is terminal support, including the user interface
and the device drivers.
.sh 2 "System call interface." 
.pp
This handles signals, traps, and system calls.
.sh 2 "Clock." 
.pp
The clock is used in various forms by many
other units.
.sh 2 "User process support (the rest)." 
.pp
This includes support for accounting, process creation, 
control, scheduling, and destruction.
.pp
.sh 2 "IPC"
.pp
The major functional unit that supports IPC
can be divided into the following smaller functional
units.
.sh 3 "Buffer management." 
.pp
All protocols share a pool of buffers called \fImbufs\fR.
The internal structure has changed considerably since 4.3:
.(b
\fC
.TS
tab(+);
l s s s.
struct mbuf {
.T&
l l l l.
+struct mbuf+*m_next;+/* next buffer in chain */
+struct mbuf+*m_act;+/* link in 2-d structure */
+u_long+m_len;+/* amount of data */
+char *+m_data;+/* location of data */
+short+m_type;+/* type of data */
+short+m_flags;+/* note if EOR, Packet HDR, Ext. stored */
+++/* If packet header add: */
int+m_pkthdr.len;+/* total packet length */
struct ifnet+*m_pkthdr.recvif;+/* rcv interface*/
+++/* If external storage add: */
+char +*m_ext.ext_buf;+/* start of buffer */
+void+(*m_ext.ext_free)();+/* free routine if not the usual */
+u_int+m_ext.ext_size;+/* size of buffer, for ext_free */
+++/* For non external */
+char+m_dat[depending];+/* done by unions, etc. */
};
.TE
\fR
.)b
.pp
There are two forms of mbufs - with and without external storage.
Small ones are 128 octets in 4.4BSD.
The data in these mbufs are located
in the mbuf structure itself.
Large mbufs, called \fIclusters\fR, are page-sized
and page-aligned.
They may be \*(lqcopied\*(rq by multiply mapping the pages they occupy.
They consist of a page of memory plus a small mbuf structure 
whose fields are used
to link clusters into chains, but whose \fIm_dat\fR array is 
not used.
The \fIm_data\fR field of the structure 
is a pointer to the active data in all cases.
The remainder of the description in the argo document
is generally obsolete, and I am merely deleting the
rest of it at this point.
.sh 3 "Routing." 
.pp
Routing decisions in the kernel are made by the procedure \fIrtalloc()\fR.
This procedure will scan the kernel routing tables (stored in mbufs)
looking for a route.
The argo document here also is quite obsolete.
We know keep a tree structure routing table,
and do matching under masks.
The structure for the routing entry contains tree related
stuff pointers (parent, l-r child for internal nodes, mask and address
for external nodes), and may be completely revised again
to make use of patricia trees.
.pp
If a route is not found, then a default route is used (if present). 
.pp
If a route is found, the entity which called \fIrtalloc()\fR can use information
from the \fIrtentry\fR structure to dispatch the datagram. Specifically, the
datagram is queued on the interface identified by the interface 
pointer \fIrt_ifp\fR.
.sh 3 "Socket code." 
.pp
This is the protocol-independent part of the IPC support.
Each communication endpoint (which may or may not be associated
with a connection) is represented by the following structure:
.(b
\fC
.TS
tab(+);
l s s s.
struct socket {
.T&
l l l l.
+short+so_type;+/* type, e.g. SOCK_DGRAM  */
+short+so_options;+/* from socket call */
+short+so_linger;+/* time to linger @ close */
+short+so_state;+/* internal state flags */
+caddr_t+so_pcb;+/* network layer pcb */
+struct protosw+*so_proto;+/* protocol handle */
+struct socket+*so_head;+/* ptr to accept socket */
+struct socket+*so_q0;+/* queue of partial connX */
+short+so_q0len;+/* # partials on so_q0 */
+struct socket+*so_q;+/* queue of incoming connX */
+short+so_qlen;+/* # connections on so_q */
+short+so_qlimit;+/* max # queued connX */
+struct sockbuf+{
++short+sb_cc;+/* actual chars in buffer */
++short+sb_hiwat;+/* max actual char count */
++short+sb_mbcnt;+/* chars of mbufs used */
++short+sb_mbmax;+/* max chars of mbufs to use */
++short+sb_lowat;+/* low water mark (not used yet) */
++short+sb_timeo;+/* timeout (not used ) */
++struct mbuf+*sb_mb;+/* the mbuf chain */
++struct proc+*sb_sel;+/* process selecting */
++short+sb_flags;+/* flags, see below */
+} so_rcv, so_snd;
+short+so_timeo;+/* connection timeout */
+u_short+so_error;+/* error affecting connX */
+short+so_oobmark;+/* oob mark (TCP only) */
+short+so_pgrp;+/* pgrp for signals */
}
.TE
\fR
.)b
.pp
The socket code maintains a pair of queues for each socket,
\fIso_rcv\fR and \fIso_snd\fR.
Each queue is associated with a count of the number of characters
in the queue, the maximum number of characters allowed to be put
in the queue, some status information (\fIsb_flags\fR), and
several unused fields.
For a send operation, data are copied from the user's address space
into chains of mbufs.
This is done by the socket module, which then calls the underlying
transport protocol module to place the data
on the send queue. 
This is generally done by 
appending to the chain beginning at \fIsb_mb\fR.
The socket module copies data from the \fIso_rcv\fR queue
to the user's address space to effect a receive operation.
The underlying transport layer is expected to have put incoming
data into \fIso_rcv\fR by calling procedures in this module.
.in -5
.sh 3 "Transport protocol management."
.pp
All protocols and address types must be \*(lqregistered\*(rq in a
common way in order to use the IPC user interface.
Each protocol must have an entry in a protocol switch table.
Each entry takes the form:
.(b
\fC
.TS
tab(+);
l s s s.
struct protosw {
.T&
l l l l.
+short+pr_type;+/* socket type used for */
+short+pr_family;+/* protocol family */
+short+pr_protocol;+/* protocol # from the database */
+short+pr_flags;+/* status information */
+++/* protocol-protocol hooks */
+int+(*pr_input)();+/* input (from below) */
+int+(*pr_output)();+/* output (from above) */
+int+(*pr_ctlinput)();+/* control input */
+int+(*pr_ctloutput)();+/* control output */
+++/* user-protocol hook */
+int+(*pr_usrreq)();+/* user request: see list below */
+++/* utility hooks */
+int+(*pr_init)();+/* initialization hook */
+int+(*pr_fasttimo)();+/* fast timeout (200ms) */
+int+(*pr_slowtimo)();+/* slow timeout (500ms) */
+int+(*pr_drain)();+/* free some space (not used) */
}
.TE
\fR
.)b
.pp
Associated with each protocol are the types of socket
abstractions supported by the protocol (\fIpr_type\fR), the
format of the addresses used by the protocol (\fIpr_family\fR),
the routines to be called to perform
a standard set of protocol functions (\fIpr_input\fR,...,\fIpr_drain\fR),
and some status information (\fIpr_flags\fR).
The field pr_flags keeps such information as
SS_ISCONNECTED (this socket has a peer),
SS_ISCONNECTING	(this socket is in the process of establishing
a connection),
SS_ISDISCONNECTING (this socket is in the process of being disconnected),
SS_CANTSENDMORE (this socket is half-closed and cannot send),
SS_CANTRCVMORE (this socket is half-closed and cannot receive).
There are some flags that are specific to the TCP concept
of out-of-band data.
A flag SS_OOBAVAIL was added for the ARGO implementation, to support
the TP concept of out-of-band data (expedited data).
.sh 3 "Network Interface Drivers" 
.pp
The drivers for the devices attaching a Unix machine to a network
medium share a common interface to the protocol
software.
There is a common data structure for managing queues,
not surprisingly, a chain of mbufs.
There is a set of macros that are used to enqueue and
dequeue mbuf chains at high priority.
A driver 
delivers an indication to a protocol entity when
an incoming packet has been placed on a queue by 
issuing a
software
interrupt.
.sh 3 "Support for individual protocols." 
.pp
Each protocol is written as a separate functional unit.
Because all protocols share the clock and the mbuf pool, they
are not entirely insulated from each other.
The details of TP are described in a section that
follows.
.\"*****************************************************
.\" FIGURE
.so ../wisc/figs/unix_ipc.nr
.pp
.CF
shows the arrangement of the IPC  support.
.pp
The AOS
IPC was designed for DoD Internet protocols, all of
which run over DoD IP.
The assumptions that DoD Internet is the domain
and that DoD IP is the network layer 
appear in the code and data structures in numerous places.
An example is that the transport protocols all directly call
IP routines.
There are no hooks in the data structures through
which the transport layer can choose a network level protocol.
Another example is that headers are assumed to
fit in one small mbuf (112 bytes for data in AOS).
Another example is this:
It is assumed in many places that buffer space is managed
in units of characters or octets.
The user data are copied from user address space into the kernel mbufs
amorphously
by the socket code, a protocol-independent part of the kernel.
This is fine for a stream protocol, but it means that a
packet protocol, in order to \*(lqpacketize\*(rq the data,
must perform a memory-to-memory copy
that might have been avoided had the protocol layer done the original
copy from user address space.
Furthermore, protocols that count credit in terms of packets or
buffers rather than characters do not work efficiently because
the computation of buffer space is not in the protocol module,
but rather it is in the socket code module.
This list of examples is not complete.
.pp
To summarize, adding a new transport protocol to the kernel consists of
adding entries to the tables in the protocol management
unit, 
modifying the network interface driver(s) to recognize
new network protocol identifiers, 
adding the
new system calls to the kernel and to the user library,
and
adding code modules for each of the protocols,
and correcting deficiencies in the socket code,
where the assumptions made about the nature of 
transport protocols do not apply. 
.i
(Touchy touchy, aren't we!?! -- Sklower)
