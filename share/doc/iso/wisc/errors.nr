.\"$Header: /pub/FreeBSD/FreeBSD-CVS/src/share/doc/iso/wisc/errors.nr,v 1.1.1.1 1994/05/30 19:08:05 rgrimes Exp $
.\"$Source: /pub/FreeBSD/FreeBSD-CVS/src/share/doc/iso/wisc/errors.nr,v $
.NC "Error Handling"
This section describes the various ways that the ARGO kernel 
handles errors.
For the purpose of this description,
errors are divided into
three classes : user errors, remote-end errors, and internal errors.
These three classes of errors and the way 
the ARGO kernel handles them are described below.
.sh 1 "Network Layer Errors"
.pp
The following section describes how errors are handled by CLNP.
.sh 2 "User Errors"
.pp
User errors occur when attempting to send a CLNP packet. These errors
are reflected back to the caller of \fIclnp_output()\fR as the return value
of the function. The following table indicates the types of errors possible
and their associated return codes:
.(b L
.TS
tab(+), expand box;
l l.
Problem+Return Code
=
Unsupported option selected+EINVAL
Incorrect address+ENAMETOOLONG
Insufficient \fImbufs\fR+ENOBUFS
Can't route packet+ENETUNREACH,EHOSTUNREACH
Insufficient \fImbufs\fR+ENOBUFS
.TE
.)b
.sh 2 "Remote-end Errors"
.pp
An error that occurs as the result of incoming NPDU
is a remote-end error.  
.pp
In the case of CONS, 
the majority of these are addressing problems,
PDN-generated errors (network or gateway congestion, number busy),
or higher layer negotiation problems.
All ISO 8208 diagnostic codes that may appear in a call clearing packet
are passed up to the higher layer.
Some of the higher layer protocols pass this error indication to the
user level program as well.
The CONS statistics that are maintained by the "glue" module
include counters for each of the possible
ISO 8208 diagnostic codes seen on incoming packets.
In addition to these error codes, there are some codes that may appear
due to device driver problems when an NPDU arrives, for example,
the driver may run out of buffers.
All possible errors that may occur in the CONS module are listed
in the file
\fC<netargo/iso_errno.h>\fR,
and the values listed in this file are passed to the user level
program in the global integer variable \fIerrno\fR.
The ARGO library
\fClibisodir.a\fR
includes an expanded version of
\fIperror()\fR that interprets these extra values.
.pp
In the case of CLNP,
the most remote-end errors are parsing errors.
When a remote-end error is discovered, processing of the NPDU stops. The
NPDU is discarded, and if error reporting is not disabled, and ER NPDU
is sent back to the source of the offending packet. The following
tables show the errors that may occur, and the error reason 
that will specified when the ER NPDU is returned.
.pp
The following general errors may occur while parsing an NPDU:
.(b L
.TS
tab(+), box, expand;
l l.
Problem+Error Reason
=
NPDU arrives before interface is configured+ADDR_DESTUNREACH
Packet too short or too big+GEN_INCOMPLETE
Protocol identification wrong+GEN_HDRSYNTAX
Version wrong+DISC_UNSUPPVERS
Lifetime expired+TTL_EXPTRANSIT
Incorrect checksum+GEN_BADCSUM
Address section too short+GEN_INCOMPLETE
Segment section too short+GEN_INCOMPLETE
Options section too short+GEN_INCOMPLETE
Unknown packet type+GEN_HDRSYNTAX
Can't route packet (forwarding)+ADDR_DESTUNREACH
.TE
.)b
The following errors are related to options processing:
.(b L
.TS
tab(+), box, expand;
l l.
Problem+Error Reason
=
Duplicate option+GEN_DUPOPT
Unknown option+DISC_UNSUPPTOPT
Security format bad+GEN_HDRSYNTAX
Security option present+DISC_UNSUPPSECURE
Source route format bad+SRCRT_SYNTAX
Record route too short+GEN_INCOMPLETE
Record route format bad+GEN_HDRSYNTAX
QOS format bad+GEN_HDRSYNTAX
Priority format bad+GEN_HDRSYNTAX
Error reason format bad+GEN_HDRSYNTAX
Error reason on non-ER NPDU+DISC_UNSUPPOPT
Error reason absent from ER NPDU+GEN_HDRSYNTAX
.TE
.)b
.sh 2 "Internal Errors"
.pp
Internal errors occur as a result of a programmer error. These errors
will result in a kernel \fIpanic()\fR. The following panics have been
coded into CLNP:
.(b L
.TS
tab(+), box, expand;
l l.
\fIPanic()\fR message+Reason
=
clnp_init: no raw clnp+The raw clnp protocol is not
+configured into the kernel.
_
clnp_srcaddr: ifp does not match interface+The ifp
+passed to \fIclnp_srcaddr()\fR is invalid.
.TE
.)b
.sh 1 "Transport Layer Errors"
.pp
.sh 2 "User Errors"
.pp
TP handles these errors in the "standard"
way for 4.3BSD:
it causes an E\fIxxx\fR error constant (from the
list in /sys/h/errno.h)
to be put into the user program's
global variable \fIerrno\fR.
In most routines, in particular
those routines called directly or indirectly
the by system-call routines,
this is done
by simply returning
this integer value. 
The errors that fall into this category are described
in the following table:
.(b L
.TS
expand box tab(+);
l l.
Error+Meaning
=
EAFNOSUPPORT+Attempting to use an address family
 +other than AF_ISO and AF_INET.
_
ENOPROTOOPT+TP was not configured at boot time.
_
ESOCKTNOSUPPORT+The given socket type is not supported.
_
EPROTOTYPE+Attempting to use an inappropriate transport
 +class for the network service. (e.g. class 0 over CLNS)
 +or attempting to use an unknown network service.
_
EISCONN+Attempting to perform on a connected socket an action 
 +that is permitted only on unconnected sockets.
_
ENOTCONN+Attempting to perform on an unconnected socket an
 +action that is permitted only on connected sockets.
_
EMSGSIZE+Trying to send more data than are permitted on 
 +connect, disconnect, or expedited data PDUs.
_
ENOTSOCK+The integer argument passed in the system
 +call is not a socket descriptor or is a socket but
 +has no transport pcb.
_
EINVAL+Some argument to the system call is invalid.
_
EOPNOTSUPP+Some command argument to the system call is invalid
 +or the operation is not supported.
_
EACCES+An unprivileged user tried to use a privileged command.
_
ETOOMANYREFS+TP ran out of reference blocks.
_
ENOBUFS+TP ran out of memory (mbufs).
.TE
.)b
Errors that should be reported to the user
by \fIerrno\fR but which occur asynchronously
are detected by the socket layer when the value
of the field \fIso_error\fR in the socket
structure is non-zero.
This is used to report such errors as 
ECONNRESET,
ECONNABORTED, and
ECONNTIMEDOUT, which are really remote-end errors.
.sh 2 "Remote-end Errors"
.pp
An error that occurs is the result of a timer
or is a result of an 
incoming TPDU
is a remote-end error.
The majority of these errors are parsing errors.
They also include some protocol errors.
Some of these errors cause the connection to be
closed locally.
It is unfortunate that when a connection is closed,
the kernel will not permit the user program to perform
anything on the socket in question, so the user cannot
inquire about the reason for disconnection.
There is no clean way to pass this information to a
signal handler either, since the process being signalled
may be swapped out at the time.
Some of these errors cause TP to return an ER TPDU
or a DR TPDU to the sending site.
Some have no effect on the connection locally.
These errors and their effects are described below.
.(b L
.TS
expand box tab(+);
l l l.
Error+Meaning+Return code or action taken
=
Retransmission+The remote end has not responded +ETIMEDOUT
timeout+to repeated attempts to send.+ 
 +This can occur during connection+ 
 +or after connection establishment.+ 
_
Inactivity+The remote end has not sent anything +ETIMEDOUT
timeout+within the last \fIx\fR time, where+
+\fIx\fR is a locally defined+
+large value.+ 
_
Unacceptable+An unacceptable TPDU has arrived, and the+TPDU dropped
TPDU +remote end can be identified.+possibly DR/ER returned
_
DR TPDU+A DR TPDU arrived, with any+Disconnect indication,
arrived+value in the reason field.+so_error == ECONNRESET
_
ER TPDU+An ER TPDU arrived, with any+Disconnect indication,
arrived+anything in the reason field.+so_error == ECONNABORTED
.TE
.)b
TPDUs may be unacceptable for a variety of reasons:
.(b L
.TS
expand box tab(+);
l l.
Problem+Action taken by TP
=
No connection at destination+Respond with DR, reason: session entity
reference or reference frozen+not attached to TSAP
_
Invalid destination reference+Respond with DR, reason: mismatched 
+references
_
Invalid parameter code+Respond with ER, cause: inval. param. code
_
Invalid DU type+Respond with ER, cause: invalid TPDU type
_
Invalid version number+ Respond with ER, cause: inval. param. code
_
Invalid suffix value+Respond with ER, cause: inval. param. value
_
Suffix missing or is of+Respond with DR, reason:
invalid length+header or parameter length invalid
_
Invalid checksum+packet discarded
_
Can't find a connection+Respond with DR, reason:
for (dest ref, src ref) pair+mismatched references
_
Old ACK TPDU+packet discarded, possibly send ACK w/ FCC
_
Class requested isn't supported+Respond with DR, reason: +negotiation failed
_
Invalid TPDU size parameter+Respond with ER, cause: inval. param. value
_
Illegal amount of data+Respond with DR, reason:
on CR, CC, DR, or XPD+header or parameter length invalid
_
Header length and length+Respond with DR, reason:
indicator field of TPDU don't agree+header or parameter length invalid
.TE
.)b
.lp
The file \fC<argo/iso_errno.h>\fR is a list
of the error codes and diagnostic that can be returned
from the peer transport entity in a DR TPDU or an ER TPDU,
and those that can be returned from the CONS, initiated by the DCE,
the remote DTE, or by the local network adapter.
These error values are too numerous to list here.
Most of them are taken from the ISO 8208 standard and the ISO 8073 standard.
The ARGO distribution contains an expanded form of the BSD library
routine \fIperror()\fR that prints an error messages for a given
\fIerrno\fR value.
.sh 2 "Internal Errors"
.pp
Some internal errors are the result of 
a lack of resources such as buffers.
These are reported to the user with the
global variable
\fIerrno\fR
set to a value from
\fC<errno.h>\fR.
The errors that fall into this category are described
in the following table:
.(b L
.TS
expand box tab(+);
l l.
Return code+Problem
=
ENOBUFS+TP ran out of mbufs.
_
EPROTONOOPT+TP hasn't been configured.
_
ETOOMANYREFS+TP ran out of (unfrozen) reference numbers.
.TE
.)b
.pp
Other
internal errors are coding errors
or errors of misinterpretation of a specification.
They result in the printing of a message on the 
console followed by a system panic.
The following panics have been coded into TP:
.(b L
.TS
expand box tab(+);
l l.
\fIPanic()\fR message+Problem
=
tp_emit CR/CC+The length indicator field of a TPDU is longer than the
 +amount of space in an mbuf; TP is attempting to send a
 +CR TPDU that is too large (perhaps legal but too large for 
 +this implementation to manage).
_
tp_rcvoob: unexpected cluster+An incoming XPD TPDU was put into a cluster 
 +mbuf by a lower layer.
_
tp timeout table overflow+The system ran out of structures for TP timers.
_
tp: T_DETACH+The connected socket that is being detached has 
 +no parent socket.
_
tp_soisdisconnected+The socket head queue is 
 +corrupted.
_
tp_soisdisconnecting+The socket head queue is 
 +corrupted.
_
tpclnp_input: bad clnp_len +The length parameter passed by clnp
 +is bad.
_
iso_control: SIOCDIFADDR+ioctl() system call passed down
iso_control: SIOCSIFADDR+a null interface pointer
_
sofree dq+The list of socket structures is
 +is inconsistent.
.TE
.)b
