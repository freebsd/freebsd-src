.NC "Transport Service Interface"
.sh 1 "General"
.pp
It is assumed that the reader is acquainted with the
set of system calls and library routines that
compose the
Berkeley
Unix interprocess communication service (IPC).
To every extent possible 
the ARGO transport service is provided by the same IPC mechanisms
that support
the transport-level services 
included in the
AOS distribution.
In some instances, the interface
provided by AOS does not support
the services required by ISO 8073,
so system calls were added to support these services.
It is felt that this is superior to modifying
existing system calls, in order to avoid the
recoding of existing Unix utilities. 
.pp
What follows is a description of the system calls
that are used to provide the transport service.
According to Unix custom,
the return value of a system call is 0
if the  call succeeds and -1 if
the call fails for any reason.
In the latter case,
the global variable
\fIerrno\fR contains more information
about the error that caused the failure.
In the descriptions of all the system calls for which
this custom is followed,
the return value is named
\fIstatus\fR.
.sh 1 "Connection establishment"
.pp
Establishing a TP connection is similar to
establishing a connection using any other
transport protocol supported by Unix.
The same system calls are used, and the passive open
is required.  
Some of the parameters to the system calls differ. 
.pp
The following call creates a communication endpoint called a
\fIsocket\fR.
It returns
a positive integer called a
\fIsocket descriptor\fR, 
which
will be a parameter in all communication primitives.
.(b
\fC
.TS
tab(+);
l s s s.
s = socket( af, type, protocol )
.T&
l l l.
 +int+s,af,type,protocol;
.TE
\fR
.)b
.pp
The \fIaf\fR parameter describes the format of addresses
used in this communication.
Existing formats include AF_INET (DoD Internet addresses), 
AF_PUP (Xerox PUP-I Internet addresses), and AF_UNIX
(addresses are Unix file names, for intra-machine IPC only).
TP runs in either the Internet domain or the ISO domain (AF_ISO).
When using the Internet domain, the network layer is the DoD Internet IP 
with Internet-style addresses. The ISO domain uses the ISO
network service and ISO-style addresses\**.
.(f
\**ISO/DP 8348/DAD2 Addendum to the Network
Service Definition Covering Network Layer Addressing.
.)f
Regardless of the address family used, an address takes the
general form,
.(b
\fC
.TS
tab(+);
l s s s.
struct sockaddr {
.T&
l l l l.
 +short+sa_family;+/* address family */
 +char+sa_data[14];+/* space for an address */
}+
.TE
\fR
.)b
.sp 1
When viewed as an Internet address, it takes the form
.(b
\fC
.TS
tab(+);
l s s s.
struct sockaddr_in {
.T&
l l l l.
 +short+sin_family;+/* address family */
 +u_short+sin_port;+/* internet port */
 +struct in_addr+sin_addr;+/* network addr A.B.C.D */
 +char+sin_zero[8];+/* unused */
}
.TE
\fR
.)b
.sp 1
When viewed as an ISO address, it takes the form
.(b
\fC
.TS
tab(+);
l s s s.
struct sockaddr_iso {
.T&
l l l l.
 +short+siso_family;+/* address family */
 +u_short+siso_tsuffix;+/* transport suffix */
 +struct iso_addr+siso_addr;+/* ISO NSAP addr */
 +char+siso_zero[2];+/* unused */
}
.TE
\fR
.)b
The address described by a \fIsockaddr_iso\fR structure
is a TSAP-address (transport service access point address).
It is made of an NSAP-address (network service access point address)
and a TSAP selector (also called a transport suffix or
transport selector, hereafter called a TSEL).
The structure \fIsockaddr_iso\fR contains a 2-byte TSEL.
This is for compatibility with Internet addressing.
ARGO supports
TSELs of length 1-64 bytes.
TSELs of any length other than 2
are called \*(lqextended TSELs\*(rq.
They are described in detail in the section \fB\*(lqExtended TSELs\*(rq\fR.
If extended TSELs are not requested, 2-byte TSELs are used by default.
.pp
Refer to Chapter Five for more information about ISO NSAP-addresses.
.pp
The \fItype\fR parameter in the \fIsocket()\fR call
distinguishes 
datagram protocols, stream protocols, sequenced
packet protocols, reliable datagram protocols, and
"raw" protocols (in other words, the absence of a transport protocol).
Unix provides manifest named constants for each of these types.
TP supports the sequenced packet protocol abstraction, to which
the manifest constant SOCK_SEQPACKET applies.
.pp
The \fIprotocol\fR
parameter is an integer that identifies the protocol to be used.
Unix provides a database of protocol names  and their associated
protocol numbers.
Unix also provides user-level tools
for searching the database.
The tools take the form of library routines.
A protocol number for TP has been chosen
by the Internet NIC to allow TP to run in the Internet domain, and this
has been added to the Unix network protocol database.
The standard Internet database tools that serve TCP users
can
also serve user of TP
in the Internet domain, if the TP protocol number is added to the
proper Internet database file, 
\fC/etc/protocols\fR.
This change must be made for TP to run in either the Internet or
in the ISO domain.
The ARGO package contains a set of tools and a database
for use with TP in the ISO domain.
This set of tools is described in the manual pages
\fIisodir(5)\fR and
\fIisodir(3)\fR.
.pp
When a socket is created, it is not given an address.
Since a socket cannot be reached by a remote entity unless it has an address, 
the user must request that a socket be given an address by
using the \fIbind()\fR system call:
.(b
\fC
.TS
tab(+);
l s s s.
status = bind( s, addr, addrlen )
.T&
l l l.
 +int+s;
 +struct sockaddr+*addr;
 +int+addrlen;
.TE
\fR
.)b
.pp
The address is expected to be in the format specified by the
\fIaf\fR parameter to the \fIsocket()\fR
call that yielded the socket descriptor \fIs\fR.
If the user 
passes an address parameter with a zero-valued transport suffix,
the transport layer 
assigns an unused 2-byte transport selector.
This is a 4.3 Unix convention; it is not part of any ISO standard.
.pp
The \fIconnect()\fR system call effects an active open.
It is used to establish a connection with an entity that is
passively waiting for connection requests, and whose
transport address is known.
.(b
\fC
.TS
tab(+);
l s s s.
status = connect( s, addr, addrlen )
.T&
l l l.
 +int+s;
 +struct sockaddr+*addr;
 +int+addrlen;
.TE
\fR
.)b
.pp
The first parameter is a socket descriptor.
The \fIaddr\fR parameter is a transport address in the format
specified by the \fIaf\fR parameter to the \fIsocket()\fR
call that yielded the socket descriptor \fIs\fR.
.pp
A passive open is accomplished with two system calls,
\fIlisten()\fR followed by \fIaccept()\fR.
.(b
\fC
.TS
tab(+);
l s s s.
status = listen( s, queuelen )
.T&
l l l.
 +int+s;
 +int+queuelen;
.TE
\fR
.)b
.pp
The \fIqueuelen\fR argument specifies the maximum
number of pending connection
requests that will be queued for acceptance by this user.
Connections are then accepted by the
system call \fIaccept()\fR.  
There is no way to refuse connections.
The functional equivalent of connection
refusal is accomplished by accepting a connection and immediately
disconnecting.
.(b
\fC
.TS
tab(+);
l s s s.
new_s = accept( s, addr, addrlen )
.T&
l l l.
 +int+new_s, s;
 +struct sockaddr+*addr;
 +int+addrlen;
.TE
\fR
.)b
.pp
The \fIaccept()\fR call completes the connection
establishment. If a connection request from a prospective peer
is pending on the socket described by \fIs\fR, it is removed and
a new socket is created for use with this connection.
A socket descriptor for the new socket is returned by the
system call.
If no connection requests are pending, this call blocks.
If the \fIaccept()\fR call fails, -1 is returned.
The transport address of the entity requesting the connection
is returned in the \fIaddr\fR parameter, and the length
of the address is returned in the \fIaddrlen\fR parameter.
The address associated with the new socket is inherited
from the socket on which the \fIlisten()\fR and \fIaccept()\fR were performed.
.pp
It is possible for the \fIaccept()\fR call to be interrupted
by an asynchronous event such as the arrival of expedited
data.
When system calls are interrupted, Unix returns the value -1
to the caller and puts the constant
EINTR in the global variable \fIerrno\fR.
This can create problems with the system call \fIaccept()\fR.
In the case of incoming expedited data, the interruption does
not indicate a problem, but the data may have arrived before
the caller has received the new socket descriptor, which is the
socket descriptor on which the expedited data are to be received.
In order to prevent this problem from occurring, the caller must
prevent the issuance of asynchronous indications until the
\fIaccept()\fR
call has returned.
Asynchronous indications are discussed below, in 
the section titled
"Indications from the transport layer to the transport user".
.pp
It is possible to discover the 
address bound to a 
socket with the 
\fIgetsockname()\fR system call.
.(b
\fC
.TS
tab(+);
l s s s.
status = getsockname( s, addr, addrlen )
.T&
l l l.
 +int+s;
 +struct sockaddr+*addr;
 +int+addrlen;
.TE
\fR
.)b
.pp
If the socket has a peer, that is, it is connected,
the system call
\fIgetpeername()\fR
is used to discover the peer's address.
.(b
\fC
.TS
tab(+);
l s s s.
status = getpeername( s, addr, addrlen )
.T&
l l l.
 +int+s;
 +struct sockaddr+*addr;
 +int+addrlen;
.TE
\fR
.)b
.lp
The names returned by 
\fIgetsockname()\fR and \fIgetpeername()\fR 
do not contain extended TSELs.
Extended TSELs can be retrieved with 
the \fIgetsockopt()\fR and 
\fIsetsockopt()\fR system calls, described below.
.pp
Unix supports several protocol-independent options
and protocol-specific options
associated with sockets.
These options can be inspected and changed by using
the \fIgetsockopt()\fR and 
\fIsetsockopt()\fR system calls.
.(b
\fC
.TS
tab(+);
l s s s.
status = getsockopt( s, level, option, value, valuelen )
.T&
l l l.
 +int+s, level, option;
 +char+*value;
 +int+*valuelen;
.TE
\fR
.)b
.(b
\fC
.TS
tab(+);
l s s s.
status = setsockopt( s, level, option, value, valuelen )
.T&
l l l.
 +int+s, level, option;
 +char+*value;
 +int+valuelen;
.TE
\fR
.)b
.pp
The \fIlevel\fR argument may indicate
either
that this option applies to sockets or that it applies to
a specific protocol.
The constants SOL_SOCKET, SOL_TRANSPORT, and SOL_NETWORK
are possible values for the \fIlevel\fR argument.
The \fIoption\fR argument is an integer that identifies
the option chosen.
.\" LIST THE OPTIONS HERE
The options available to TP users provide the 
user with the ability to control various TP protocol options
including but not limited to
TP class, TPDU size negotiated, TPDU format used,
acknowledgment and retransmission strategies.
For a detail list of the options, see the manual page \fItp(4p)\fR.
.sh 1 "Extended TSELs"
.pp
ARGO supports TSELs
of length 1 byte - 64 bytes for sockets bound to addresses in the
AF_ISO address family.
The ARGO user program uses the
\fIgetsockopt()\fR
and 
\fIsetsockopt()\fR
system calls to 
discover and assign extended TSELs.
.pp
To create a socket with an extended TSEL,
the process 
.ip \(bu 5
opens a socket with \fCsocket(AF_ISO, SOCK_SEQPACKET, ISOPROTO_TP)\fR
.ip \(bu 5
binds an NSAP-address to the socket with \fIbind()\fR.
The address bound may contain a 2-byte selector (\fIiso_tsuffix\fR).
.ip \(bu 5
uses \fIsetsockopt()\fR with the command TPOPT_MY_TSEL,
to assign a TSEL to the socket.
.ip \(bu 5
calls \fIlisten(), connect()\fR, or any other appopriate system calls
to use the socket as desired.
.lp
To connect to a transport entity that is bound to a TSAP-address with
an extended TSEL, the
process 
.ip \(bu 5
opens a socket with \fCsocket(AF_ISO, SOCK_SEQPACKET, ISOPROTO_TP)\fR
.ip \(bu 5
uses \fIsetsockopt()\fR, with the command TPOPT_PEER_TSEL,
to assign a PEER TSEL to the socket.
This TSEL is used by the transport entity 
for all subsequent connect requests made on this socket, 
unless the peer TSEL is changed by another call to
\fIsetsockopt()\fR employing the command TPOPT_PEER_TSEL.
.lp
To discover the TSEL of the peer of a connected socket,
the process 
.ip \(bu 5
uses \fIgetsockopt()\fR with the command TPOPT_PEER_TSEL.
.lp
To discover the TSEL of socket's own address,
the process 
.ip \(bu 5
uses \fIgetsockopt()\fR with the command TPOPT_MY_TSEL.
.sh 1 "Data transfer"
.pp
The system calls provided by AOS for data transfer have
semantics that are unsuitable for TP,
and in fact they are seriously deficient for the correct
operation of any user program that uses out-of-band or expedited
data in any way except to cause the program to abort.
The problem lies in the manner in which the kernel
handles interrupted system calls.
The send and receive primitives
may be interrupted by signals.
A signal is the mechanism used to indicate
the presence of expedited data or out-of-band data.
If the send or receive primitive is interrupted before completion,
the user needs to know how many octets of data were sent or received.
The existing system call interface does not provide this
information, nor does it permit TP to provide
this information.
All forms of the existing interface 
(\fIsend()\fR, 
\fIrecv()\fR, 
\fIsendmsg()\fR, 
\fIrecvmsg()\fR, 
\fIsendto()\fR, 
\fIrecvfrom()\fR, 
\fIwrite()\fR, 
\fIread\fR, 
\fIwritev()\fR, 
and \fIreadv()\fR system calls)
return an octet count
when the system call completes, and return an error
indication (-1, \fIerrno\fR == EINTR) if the system call is interrupted. 
To change the semantics
of these calls would create havoc with existing user-level software.
Instead two new system calls are provided to support data transfer.
(The existing interface may be used if the user does not need the additional
service provided by the new system calls.)
.pp
The two new system calls are patterned after 
\fIreadv()\fR and \fIwritev()\fR,
the scatter-gather or "vectored"
versions of \fIread()\fR and \fIwrite()\fR.
.(b
\fC
.TS
tab(+);
l s s s.
cc = sendv( s, iov, iovlen, flags )
.T&
l l l.
 +int+s:
 +io_vector+iov;
 +int+iovlen;
 +unsigned int+*flags;
.TE
\fR
.)b
.(b
\fC
.TS
tab(+);
l s s s.
cc = recvv( s, iov, iovlen, flags )
.T&
l l l.
 +int+s:
 +io_vector+iov;
 +int+iovlen;
 +unsigned int+*flags;
.TE
\fR
.)b
.pp
The \fIiov\fR argument is an \fIio_vector\fR,
an array of pointers and lengths that
describe the areas from (or to) which the data will be gathered (or scattered).
The \fIiovlen\fR argument is an integer that tells how many parts are in
the io_vector.
The \fIflags\fR parameter serves several purposes.
The TP specification requires that TSDUs be unlimited in size.
System calls cannot pass unlimited amounts of data between the user
and the kernel, so
there cannot be a one-to-one correspondence between TSDUs and
system calls. 
The \fIflags\fR
parameter is used to mark the end-of-TSDU on both sending and
receiving.
This way one TSDU can span several system calls.
When sending, 
the user sets
this flag
to indicate that this  request completes a TSDU.
When receiving, 
TP sets this flag when
the end of a TSDU is reached.
In the latter case, the end of the data received by the transport user
with a given system call
coincides with the end of the TSDU
if 
the TP has set the end-of-TSDU bit
in the \fIflags\fR parameter of the \fIrecv()\fR system call.
It is possible for the peer to send an empty TPDU with the end-of-TSDU
flag set, in which case the transport user
may receive zero octets with the end-of-TSDU flag set.
See the manual pages 
\fIrecvv(2)\fR 
and
\fIsendv(2)\fR 
for details.
.pp
The \fIflags\fR parameter also serves to distinguish data transfer primitives
from expedited data transfer primitives.
The flag bit MSG_OOB is provided for "out of band data" in the
DoD Internet protocols.  It is also used to provide the expedited data service
of the ISO protocols.
The transport layer will deliver one expedited datum (there will be a
one-to-one correspondence between expedited TSDUs and XPD TPDUs)
at a time.  
The user must receive the datum before the transport
layer will accept more expedited data.
Each expedited datum my contain up to 16 octets.
.pp
.sh 1 "Disconnection"
.pp
The \fIclose\fR system call will disconnect any association
between two TP entities.
.(b
\fC
.TS
tab(+);
l s s s.
status = close( s )
.T&
l l l.
 +int+s;
.TE
\fR
.)b
.pp
The argument \fIs\fR is a socket descriptor.
If a Unix user process terminates, Unix will close all files and
sockets associated with the process, which means all transport
connections associated with the process will be disconnected.
.sh 1 "Indications from the transport layer to the transport user"
.pp
While the above set of system calls allows you to establish
a connection, transfer data, and disconnect, 
several elements of the transport service are not supported
by these system calls alone.
These system calls do not support 
any way to indicate to the
to the transport user
the
presence of expedited data or
a disconnection initiated by the peer or by one of the
cooperating TP entities.
.pp
The Unix signal mechanism is used to provide these
service elements.
When an expedited data TSDU arrives, the TP interrupts
the user with a SIGURG signal ("urgent condition present on socket").
The user must have previously registered a procedure to handle
the signal by using the \fIsigvec()\fR system call or the
\fIsignal()\fR library routine provided for that purpose.
The signal handler takes the form
.(b
\fC
.TS
tab(+);
l s s s.
int  sighandler( signal_number)
.T&
l l l.
 +int+signal_number;
.TE
\fR
.)b
.pp
The \fIsignal_number\fR argument will be the well-known constant SIGURG.
There are two reasons for 
the transport layer to issue
a SIGURG:  
expedited data
are present or
disconnection was initiated by a transport entity or by the peer.
Should the user have more than one transport connection open,
another system call is used to determine to which socket(s)
the urgent condition applies.
This is the \fIselect()\fR system call, described below.
.pp
When the SIGURG indicates a disconnection, there may be
user data from the peer present.
TP 
discards all queued normal data and expedited data.
It saves the disconnect data for the user to receive via the
\fIgetsockopt()\fR system call.
Unfortunately, the socket is already considered closed
by the kernel, so there is no way
for the user to read the incoming disconnect data, so receipt
of disconnect data is not supported.
.\"
.\"If the user does not receive the disconnect data before the
.\"reference timer expires, the data will be discarded and the
.\"socket will be closed.
.pp
Transport service users may use more than one transport
connection at a time.
The \fIselect()\fR system call facilitates this.
.(b
\fC
.TS
tab(+);
l s s s.
#include <sys/types.h>
+
nfound = select( num_to_scan, recvmask, sendmask, 
+exceptmask, timeout )
.T&
l l l.
 +int+nfound, num_to_scan;
 +fd_set+*recvmask, *sendmask, *exceptmask;
 +time+timeout;
.TE
\fR
.)b
.pp
This system call takes as parameters a set of masks 
that specify a subset of the socket descriptors that are in
use by the user program.
\fISelect()\fR inspects the sockets to see if they have data
to be received, can service a send without blocking, or
have an exceptional condition pending, respectively.
The masks will be set upon return to indicate the socket descriptors
for which the respective conditions exist.
The \fInum_to_scan\fR argument limits the number of sockets that are
inspected.
The call will return within the amount of time given in the
\fItimeout\fR parameter, or, if the parameter is zero, \fIselect()\fR
will block indefinitely.
.\" FIGURE
.so figs/TS_primitives.nr
.pp
.CF
summarizes the mapping of the transport service primitives
to Unix facilities.
