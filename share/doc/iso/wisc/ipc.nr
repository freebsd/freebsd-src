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
.so figs/func_units.nr
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
All protocols share a pool of buffers called \fImbufs\fR:
.(b
\fC
.TS
tab(+);
l s s s.
struct mbuf {
.T&
l l l l.
+struct mbuf+*m_next;+/* next buffer in chain */
+u_long+m_off;+/* offset of data */
+short+m_len;+/* amount of data */
+short+m_type;+/* mbuf type (0 == free) */
+u_char+m_dat[MLEN];+/* data storage */
+struct mbuf+*m_act;+/* link in 2-d structure */
};
.TE
\fR
.)b
.pp
There are two forms of mbufs - small ones and large ones.
Small ones are 128 octets in 
AOS
and 256 octets
in the ARGO release. Small mbufs are copied by byte-to-byte
copies.
The data in these mbufs are kept in the character
array field \fIm_dat\fR in the mbuf structure
itself.
For this type of mbuf, the field \fIm_off\fR is positive,
and is the offset to the beginning of the data from
the beginning of the mbuf structure itself.
Large mbufs, called \fIclusters\fR, are page-sized
and page-aligned.
They may be \*(lqcopied\*(rq by multiply mapping the pages they occupy.
They consist of a page of memory plus a small mbuf structure 
whose fields are used
to link clusters into chains, but whose \fIm_dat\fR array is 
not used.
The \fIm_off\fR field of the structure 
is the offset (positive or negative) from the
beginning of the mbuf structure to the beginning
of the data page part of the cluster.
In the case of clusters, the offset is always out of the
bounds of the \fIm_dat\fR array and so it is alway possible
to tell from the \fIm_off\fR field whether an mbuf structure
is part of a cluster or is a small mbuf.
All mbufs permanently reside in memory.
The mbuf management unit manages its own page table. 
The mbuf manager keeps limited statistics on the quantities and
types of buffers in use.
Mbufs are used for many purposes, and most of these purposes
have a type associated with them.
Some of the types that buffers may take are
MT_FREE (not allocated), MT_DATA,
MT_HEADER, MT_SOCKET (socket structure),
MT_PCB (protocol control block),
MT_RTABLE (routing tables),
and
MT_SOOPTS (arguments passed to \fIgetsockopt()\fR and 
\fIsetsockopt()\fR.
Data are passed among functional units by means
of queues, the contents of which are
either chains of mbufs or groups of chains of mbufs.
Mbufs are linked into chains with the \fIm_next\fR field.
Chains of mbufs are linked into groups with the \fIm_act\fR
field.
The \fIm_act\fR field allows a protocol to retain packet
boundaries in a queue of mbufs.
.sh 3 "Routing." 
.pp
Routing decisions in the kernel are made by the procedure \fIrtalloc()\fR.
This procedure will scan the kernel routing tables (stored in mbufs)
looking for a route. A route is represented by
.(b
\fC
.TS
tab(+);
l s s s.
struct rtentry {
.T&
l l l l.
+u_long+rt_hash;+/* to speed lookups */
+struct sockaddr+rt_dst;+/* key */
+struct sockaddr+rt_gateway;+/* value */
+short+rt_flags;+/* up/down?, host/net */
+short+rt_refcnt;+/* # held references */
+u_long+rt_use;+/* raw # packets forwarded */
+struct ifnet+*rt_ifp;+/* interface to use */
}
.TE
\fR
.)b
When looking for a route, \fIrtalloc()\fR will first hash the entire destination
address, and scan the routing tables looking for a complete route. If a route
is not found, then \fIrtalloc()\fR will rescan the table looking for a route
which matches the \fInetwork\fR portion of the address. If a route is still
not found, then a default route is used (if present). 
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
.so figs/unix_ipc.nr
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
For example, it is assumed that addresses can be compared
by a bitwise comparison of 4 octets.
Another example is that the transport protocols all directly call
IP routines.
There are no hooks in the data structures through
which the transport layer can choose a network level protocol.
A third example is that the host's local addresses
are stored in the network interface drivers and the drivers
have only one address - an Internet address.
A fourth example is that headers are assumed to
fit in one small mbuf (112 bytes for data in AOS).
A fifth example is this:
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
