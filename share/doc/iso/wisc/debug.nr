.\"$Header: debug.nr,v 1.4 88/12/06 16:05:36 nhall Exp $
.\"$Source: /usr/argo/doc/kernel/RCS/debug.nr,v $
.\"
.\"	Program names should be in italics
.\"
.NC "Debugging, Testing, and Statistics"
.sh 1 "Introduction"
.pp
This section describes the methods used 
to test and debug the ARGO kernel.
Three facilities are used in combination: 
debug options,
simple statistics gathering, 
and
protocol event tracing.
Many of the debug options
simply cause information to be printed on the console, but
several of these options cause
TP to behave pathologically
so that errors are be introduced in desirable ways.
.pp
TP and CLNP keep simple statistics.
These statistics include such things as the total
number of PDUs that are sent and received, 
a count of various types of errors
that can be encountered when parsing an incoming PDU,
and the average and standard deviation of round trip times for 
transport PDUs.
These statistics are useful for debugging.
For example,
if an incoming CC TPDU is rejected because one of the optional
parameters is faulty, this are noted in the statistics.
The statistics are kept on a system-wide basis rather than
on a per-connection basis.
They can be printed or cleared by user-level utility programs.
.pp
The tracing facility allows selective tracing of events.
Events are grouped into categories relating to different
functions of TP.
For example, it is possible to 
trace only the events that pertain to acknowledgments.
.pp
At run time the debugging and tracing options can
be set and cleared by privileged utility programs.
Each of these facilities is described in more
detail below.
.sh 1 "Debugging"
.pp
Most of the debugging options
print messages on the console.
Kernel printing is done by busy-waiting at high priority,
so debugging options should be used very sparingly.
A sample of the code is:
.(b
.nf
\fC
IFDEBUG(D_TPINPUT)
     printf("tp_input m 0x%x tpdu_len 0x%x\n", m, tpdu_len);
ENDDEBUG
\fR
.fi
.)b
.sp 1
.lp
IFDEBUG and ENDDEBUG are macros that are defined in one of two ways.
If the system is configured with the ARGO_DEBUG
option, an array 
\fCargo_debug[128]\fR
is declared, and
IFDEBUG and ENDDEBUG are defined thus:
.(b 
.nf
\fC
#define IFDEBUG(option) if(argo_debug[option]) {
#define ENDDEBUG  ; }
\fR
.fi
.)b
.lp
If the system is configured without the ARGO_DEBUG option, these
two macros resolve to the C comment delimiters, \fC/*\fR and \fC*/\fR,
causing all the debugging code lying between the macros
to be elided.
.pp
TP, CLNP, and CONS debugging can be enabled independently. 
All debugging requires that the code be compiled with the 
option ARGO_DEBUG. 
The \fIconfig(8)\fR option CLNP_DEBUG will include debugging printfs for CLNP. 
TP_DEBUG has the same effect for TP.
.pp
The array elements of \fCargo_debug[]\fR are set by
the utility program
\fIbark\fR, 
which reads and writes
\fC/dev/kmem\fIN\fR.
See the manual page \fIbark(8)\fR.
.pp
Several debugging options cause TP to behave pathologically,
for the purpose of reproducing difficult-to-reproduce
error conditions that the protocol must correct.
For example, the 
\fID_DROP\fR, or \fIbark -on T.drop\fR
option causes 
\fItp_input()\fR
to discard TPDUs on a pseudo-random basis.
These will be described below.
.sh 1 "Statistics"
.pp
.sh 2 "CLNP Statistics"
.pp
CLNP keeps a set of statistics related to its operation. 
Statistics include such things as NPDUs sent, received, and dropped. 
These statistics are stored in the global structure \fIclnp_stat\fR.
The utility program \fInetstat(8)\fR may be used to print these statistics.
.sh 2 "TP Statistics"
.pp
TP keeps a set of running counts of certain events.
The statistics include such things as the numbers 
of each type of TPDU sent and received, TPDUs dropped,
and the numbers of occurrences of certain types of errors.
The statistics are stored in the global structure \fItp_stat\fR.
The utility programs 
\fItpstat\fR and
\fItpmon\fR
read \fC/dev/kmem\fIN\fR
and prints the contents of the statistics structure
in a human-readable form.
\fITpstat\fR prints the statistics on any ascii screen or printer.
\fITpmon\fR uses the \fIcurses\fR library and assumes that is has
a screen or window of size 53(long) X 80(wide), and it updates the
screen every 30 seconds.
.pp
\fITpstat\fR and \fItpmon\fR can be used to clear the statistics (set them
all to zero); the \fB-c\fR option causes the statistics to be cleared.
.pp
Statistics are observed using \fItpstat(8)\fR
to clear statistics before a test, and to print
the statistics after the test.
See the manual pages \fItpstat(8)\fR and \fItpmon(8)\fR.
.sh 1 "Tracing"
.pp
.sh 2 "CLNP Tracing"
.pp
CLNP does not support event tracing.
.sh 2 "TP Tracing" 
.pp
The tracing facility consists of a circular buffer (an array)
of structures that are written by the kernel at various times, 
and a utility program that reads \fC/dev/kmem\fIN\fR
to interpret the contents of the buffer.
The trace structure is a union of the structures that
will be interpreted by the utility program.
A trace event consists of a call to the trace routine \fItpTrace\fR
with a set of arguments containing the information relevant to the
event being traced.
The procedure tpTrace is actually called through a macro \fItptrace\fR.
For example,
.(b
.nf
\fC
IFTRACE(D_INPUT)
     tptrace(TPPTtpduin, h->tpdu_type, h, h->tpdu_li+1, 0, 0);
ENDTRACE
\fR
.fi
.)b
.pp
The tracing macros are defined in the same manner as the 
debugging macros:
.(b
.nf
\fC
#define IFTRACE(option) if(tp_traceflags[option]) {
#define ENDTRACE  }
\fR
.fi
.)b
.lp
If the kernel is configured with the option TPPT, these macros
are defined as shown above, but if the TPPT option is not
used, these macros become C-style comment delimiters.
.pp
The tracing procedure copies \fIh->tpdu_li + 1\fR bytes beginning at
location \fIh\fR into a trace structure in the circular buffer.
The utility program \fItppt\fR
reads the trace structure, 
interprets the data as a TPDU header,  
and prints the header in hexadecimal form, with a banner identifying
the event as an incoming TPDU:
.(b
.nf
\fC
1a: Ref 22 	arg 14(0xe), 	@ 91990 : 0000.453125  	tpdu
INPUT  total len 22
HDRLEN:  21+1 	CR_TPDU_type 	cdt 0(0x0) 	dref 0x0
		 + 0: 0x15 0xe0 0x00 0x00   4: 0x00 0x03 0x00 0xc1 
		 + 8: 0x06 0x74 0x70 0x70  12: 0x69 0x6e 0x67 0xc2 
		 +16: 0x02 0x00 0x07 0xc0  20: 0x01 0x08 0x00 0x00 

\fR
.fi
.)b
.pp
In addition to the data copied from the arguments to tpTrace(), 
each trace structure contains
a time stamp and an event sequence number, and in many cases, the
connection reference to which the traced event applies.
The utility program \fItppt\fR is be used to turn on and off the
tracing options.
.pp
This facility can be used for debugging the source
code as well as for studying the behavior of the protocol.
For example, by adding the appropriate trace events,
it is possible to "see" the resequencing function of TP
working when a debug option is used to cause
TPDUs to be dropped occasionally.
.pp
See the manual page \fItppt(8)\fR. 
.sh 1 "Testing"
.pp
.sh 2 "CLNP Testing"
.pp
CLNP was tested in two rather different ways. 
The first method of testing used the
raw CLNP interface with the program \fIclnptest\fR.
\fIclnptest\fR allows a user to send or receive CLNP NSDUs. 
With \fIclnptest\fR, a user can send CLNP NSDUs with various
combinations of options and observe the result.
.pp
The second method of testing CLNP was to have TP use CLNP as its network
layer protocol. 
This method provides a good stress test for CLNP. 
Unfortunately, TP generally calls CLNP in the same manner, so that not all
of the CLNP options are exercised.
.sh 3 "Clnptest"
.pp
The program \fIclnptest\fR can be invoked as either 
a reader or as a writer:
.(b
\fC
clnptest <options>
\fR
.)b
The \fI-r\fR option invokes \fIclnptest\fR as a reader, the
\fI-w\fR option invokes it as a writer. 
Other options allow the user to indicate the destination, number of NSDUs, 
size of NSDUs,
and NSDUs options. 
See \fIclnptest(8)\fR for more information.
.pp
\fIclnptest\fR is normally used in the following manner. 
On one machine, invoke \fIclnptest\fR as a reader:
.(b
\fC
clnptest -r
\fR
.)b
On a different machine, transmit an NSDU.
For example, to test the source route function, one invokes:
.(b
\fC
clnptest -w -h a -oR "b, c, d"
\fR
.)b
This sends an NSDU to host 'a', source routing it via
hosts 'b', 'c', and 'd'.
.sh 3 "The Troll"
In order to test CLNP reassembly certain errors must be generated. 
The mechanism used has two parts, 
the user program \fIclnptroll\fR, which enables and disables
the generation of these errors, and the 
kernel resident error-creation routines.
.pp
Troll options allow one to duplicate an NSDU with a specified frequency. 
The kernel must be compiled with the \fIconfig\fR option \fITROLL\fR
in order to include troll code.
See \fIclnptroll(8)\fR for more information.
.sh 3 "Debugging CLNP"
.pp
The following sections describe the \fIbark\fR options 
appropriate for testing parts of CLNP. 
Refer to \fIbark(8)\fR for more information about debugging
using \fIbark\fR..
.sh 4 "Sending NSDUs"
.pp
Turning on the \fIbark\fR
option \fIC.output\fR causes information to be
printed whenever an NSDU is transmitted. 
Translation of NSAP addresses to SNPA can be monitored by turning on 
the \fIC.un\fR, or \fIC.lan\fR options. 
Parts of outgoing NSDUs can be dumped when the \fIC.dumpout\fR
option is on.
Routing activity can be watched by turning on \fIC.route\fR and \fIC.iso\fR.
Information about CLNP option processing is available with \fIC.options\fR.
.sh 4 "Forwarding NSDUs"
.pp
The \fIforward\fR switch will cause debugging information to be displayed
whenever NSDUs are forwarded.
.sh 4 "Receiving NSDUs"
.pp
Information is displayed about incoming NSDUs when the \fIC.input\fR
option is enabled. 
A portion of incoming NSDUs can be dumped by turning on the 
\fIC.dumpin\fR option.
.sh 4 "Fragmentation and Reassembly"
.pp
The options \fIC.frag\fR and \fIC.reass\fR turn on debugging for the 
CLNP fragmentation and reassembly functions.
.sh 2 "TP Testing"
.pp
Five services were used for most of the testing:
the \fIdiscard\fR service,
the \fIecho\fR service, 
the \fIremote login\fR service, 
the \fIremote shell\fR service, 
and
the \fIsimple file transfer\fR service.\**
.(f
\** In fact, ancestors of these services were used for testing the
ARGO transport implementation during development.
These programs in their original forms were very cumbersome to use;
consequently they evolved to become the services described here.
.)f
Each service consists of a daemon process or server that listens
on a well-known transport selector (which is listed in the 
ARGO directory service), and an active process that contacts the
server.
Four of these services,  
discard, echo, remote login, and remote shell,
are supported by the
\fIisod\fR suite of daemons, which is a
version of the \fIinetd\fR programs that uses
the ISO protocol suite.
.sh 3 "The Discard Service"
The discard server listens on the transport selector
registered in the ARGO directory service for the application
"discard".
The server accepts incoming connection requests, 
receives TSDUs, and throws away the TSDUs.
It never initiates a disconnect, but expects its peer
to disconnect the transport connection.
.PP
The program \fItpdiscard\fR connects to the
discard server.
The transport service and protocol options it uses are those
indicated in the ARGO directory service.
By changing the directory service entry for the 
discard service, each of the transport service options and
protocol options can be demonstrated.
See the manual pages
\fItpdiscard(8)\fR,
\fItp(4p)\fR,
and 
\fIisodir(5)\fR
for more information.
.sh 3 "The Echo Service"
The echo server listens on the transport selector
registered in the ARGO directory service for the application
"echo".
The server accepts incoming connection requests, 
receives TSDUs, and returns the TSDUs to the sender.
It never initiates a disconnect, but expects its peer
to disconnect the transport connection.
.pp
The 
program \fItpping\fR connects to the
echo server.
The transport service and protocol options it uses are those
indicated in the ARGO directory service.
By changing the directory service entry for the 
echo service, each of the transport service options and
protocol options can be demonstrated.
See the manual pages
\fItpping(8)\fR,
\fItp(4p)\fR,
and 
\fIisodir(5)\fR
for more information.
.sh 3 "The Remote Login Service"
The remote login server listens on the transport selector
registered in the ARGO directory service for the application
"login".
The server accepts incoming connection requests,
implements the BSD remote login protocol, checks permissions using
the \fC~/.rhosts\fR, and \fC/etc/passwd\fR files, and
uses the ARGO directory service to discover name-to-NSAP-address
mappings.
If the remote user is authorized to log in to the end system on which
the server runs, a login is started.
.pp
The program \fIrlogin.iso\fR connects to the remote login server.
The transport service and protocol options it uses are those
indicated in the ARGO directory service.
By changing the directory service entry for the 
login service, each of the transport service options and
protocol options can be demonstrated.
See the manual pages
\fIrlogin.iso(8)\fR,
\fItp(4p)\fR,
and 
\fIisodir(5)\fR
for more information.
.sh 3 "The Remote Shell Service"
The remote shell server listens on the transport selector
registered in the ARGO directory service for the application
"shell".
The server accepts incoming connection requests,
implements the BSD remote command authorization protocol, 
checks permissions using
the \fC~/.rhosts\fR, and \fC/etc/passwd\fR files, and
uses the ARGO directory service to discover name-to-NSAP-address
mappings.
If the remote user is authorized to execute a shell on
the end system on which
the server runs, a shell is started.
.pp
The program \fIrcp.iso\fR connects to the remote shell server to 
effect a remote copy.
The transport service and protocol options it uses are those
indicated in the ARGO directory service.
By changing the directory service entry for the 
shell service, each of the transport service options and
protocol options can be demonstrated.
See the manual pages
\fIrcp.iso(8)\fR,
\fItp(4p)\fR,
and 
\fIisodir(5)\fR
for more information.
.sh 3 "The Simple File Transfer Service"
.pp
The last service consists of a pair of programs,
\fItpfileget\fR and
\fItpfileput\fR,
which cooperate to transfer one file.
The passive program, \fItpfileget\fR,
listens on the transport selector registered in the ARGO directory service
to support the application named "tptestsel".
The sending program, \fItpfileput\fR, 
connects to the passive program, transfers in one TSDU
the file named on the \fItpfileput\fR command line, and waits for the
passive end to close the connection.
\fITpfileget\fR 
opens a file of the name given on its command line,
accepts one connection request, receives 
one TSDU, writes the contents of that TSDU to the opened file,
and when it receives the end-of-TSDU indication,
\fItpfileget\fR closes the transport connection.
The transport service options and protocol options used by
\fItpfileput\fR are determined by the ARGO directory service
record that describes the applicaition "tptestsel".
See the manual pages
\fItpfileget(8)\fR,
\fItp(4p)\fR,
and 
\fIisodir(5)\fR
for more information.
.sh 3 "Internal TP Testing"
.pp
The methods used to test each of the various functions
of TP are described in this section.
One or more of the services described above were used, while
the TP activity was observed with tracing or debugging or both.
The statistics were cleared before each test and inspected
after each test.
Each test can be run with different protocol and service options,
by changing the transport parameters in records
in the ARGO directory service file.
See the manual pages
\fItpstat(8)\fR,
\fItpmon(8)\fR,
\fItppt(8)\fR,
\fIbark(8)\fR,
\fItp(4p)\fR,
and 
\fIisodir(5)\fR
for more information.
.sh 4 "Normal and Expedited Data Transfer:"
.pp
TSDUs are 
distinguished by the presence or absence of the
EOTSDU bit in the \fIflags\fR parameter of the
\fIsendv()\fR system call.
The data of a TSDU are copied into chains of \fImbufs\fR
in the kernel so that the end of a TSDU lies in an mbuf
with the \fIm_act\fR field non-zero.
The end of a TSDU never lies in the middle of an
mbuf.
This is true on the receiving side as well.
On output, the segmenting function,
the function that copies user data into mbuf chains
reorganizes mbuf chains into TPDUs,
is observed using several debug options
and trace options
in the routines \fIsosend()\fR 
and \fItp_sbsend()\fR.
On input, the reassembling mechanism
is observed in the routine \fItp_stash()\fR.
The debug options 
\fBT.ndata\fR, 
\fBT.sb\fR, and 
\fBT.xpd\fR
print information 
pertinent to this function.
.pp
Expedited data complicates the matter of segmenting
because markers must be kept in the chains of outgoing
TPDUs to indicate the precedence of expedited data TPDUs
over normal data TPDUs.
The pertinent trace options are \fBT.sb\fR and \fBT.ndata\fR.
With the trace and (or) debugging options on,
and with \fItpdiscard\fR running, one can observe the segmentation
and reassembly of TPDUs.
.pp
Using the file transfer programs to transfer a file,
then transferring it back with \fIrcp\fR (the TCP version) if necessary, and
using
\fIdiff\fR, one can see that data are transferred correctly.
The \fBT.input\fR trace option creates a readable hexadecimal dump of incoming TPDUs.
The 
\fBT.emit\fR
trace option creates the same sort of dump for outgoing
TPDUs in \fItp_emit()\fR.
Sequencing
can be observed by using the 
\fBT.ndata\fR
and 
\fBT.input\fR
or 
\fBT.emit\fR
trace options
to see the sequence numbers assigned to TPDUs.
.pp
The
\fBT.drop\fR
debug option causes \fItp_input()\fR
to throw away occasional TPDUs.
(The formula for determining when to discard a TPDU
is ad hoc and simplistic.  It causes TPDUs to be
discarded frequently but not so frequently that the 
receiving side has no chance to recover.)
With tracing on and the file transfer programs running,
resequencing can be observed
and the correctness of the transferred data
can be verified with \fIdiff(1)\fR.
.pp
The use of both normal and extended formats
can be observed with the \fBT.input\fR and \fBT.emit\fR trace options.
.pp
The following statistics are of interest:
.(b
.nf
\fIn\fR connections used extended format
\fIn\fR connections allowed transport expedited data
\fIn\fR connections turned off checksumming
\fIn\fR connections dropped due to retrans limit
\fIn\fR EOT bits on incoming TPDUs
\fIn\fR EOT bits on outgoing TPDUs
\fIn\fR XPD marks discarded
\fIn\fR XPD stopped data flow \fIm\fR times
\fIn\fR DTs out of order
\fIn\fR DTs not in window 
\fIn\fR duplicate DTs
\fIn\fR XPDs not in window
\fIn\fR XPDs w/o credit to stash
\fIn\fR DT (sent)
\fIn\fR DT (received)
\fIn\fR DT (retransmitted)
\fIn\fR XPD (sent)
\fIn\fR XPD (received)
\fIn\fR random DTs dropped
.fi
.)b
.sh 4 "Checksumming, use and non-use:"
.pp
The checksum generation and checking
routines were first written and debugged as user-level
routines before they were modified for kernel use.
The kernel routines may be observed with the 
\fBT.chksum\fR
debug option.
Various sizes of mbufs can be created by creative use of the
ARGO directory service, particularly by changing the value of the
attribute \fItp.tpdusize\fR.
There is no trace option for checksumming.
Checksumming has been used with transfers to and from at least
one other TP implementation.
.pp
The statistics that are pertinent to checksumming are:
.(b
.nf
\fIn\fR connections turned off checksumming
\fIn\fR invalid checksums
.fi
.)b
.sh 4 "Acknowledgment:"
.pp
Acknowledgment can be observed by using the 
debug and trace options
\fBT.aks\fR,
\fBT.akr\fR,
\fBT.input\fR,
\fBT.emit\fR,
and
\fBT.driver\fR.
The transport driver (finite state machine) and the routine 
\fItp_goodack()\fR dump information appropriate to acknowledgments.
If the \fBT.ndata\fR, and \fBT.emit\fR or \fBT.input\fR trace options are used 
along with the \fBT.aks\fR and \fBT.akr\fR trace options,
a complete picture of the data transfer and acknowledgment
activity can be created.
The acknowledgments for expedited data are traced with
the 
\fBT.xpd\fR
trace option.
The routine \fItp_goodXack()\fR and the finite state
machine dump information when the
\fBT.xpd\fR
debug and trace options are used.
To cause expedited data to be generated,
the -e or -E option on the discard programs or the file
transfer programs are used.
To observe the different acknowledgment strategies,
the protocol options were changed in the ARGO directory service.
.pp
The pertinent statistics are:
.(b
.nf
\fIn\fR AK (received)
\fIn\fR AK (sent)
ACK reasons:
\fIn\fR not acked immediately
\fIn\fR strategy==each
\fIn\fR strategy==fullwindow
\fIn\fR duplicate DT
\fIn\fR EOTSDU
\fIn\fR reordered
\fIn\fR user rcvd
\fIn\fR fcc reqd
.fi
.)b
.pp
The smoothed average round trip time is kept
for outgoing TPDUs for each transport connection 
and for the entire TP entity.
The time each TPDU is transmitted is recorded, and when an acknowledgment
arrives, the round trip time is computed for the lowest
sequence number that this AK TPDU acknowledges.
The computation of round trip times can be observed
in a trace with the
\fBT.rtt\fR
option.
.pp
In addition to average round trip times, the kernel
maintains the standard deviation of the round trip times.
This statistic is kept for each connection and for the entire
TP entity.
In fact, four such sets of statistics are kept for the TP entity:
.np
for traffic not on a public data network (PDN) and on the same network as this end system,
.np
for traffic not on a public data network (PDN) and on not the same network as this end system,
.np
for traffic on a public data network (PDN) and on the same network as this end system,
and
.np
for traffic on a public data network (PDN) and not on the same network as this end system.
The determination of whether traffic is on the same network as this end system
is based on the "network portion" of the peer's NSAP-address.
For more information about this, see the section of this document titled
\fB"Network Layer Routing"\fR.
.pp
The smoothed average round trip time statistics for a given
can be observed with the -t option to
\fItpstat(8)\fR.
The global round trip statistics can be observed with  the -s option to
\fItpmon(8)\fR.
.sh 4 "Flow Control:"
.pp
Flow control activity is the transfer of credit information
from end to end and the proper use of that information.
To see that it works properly, one must observe three
things:
the receiving window must shut down and reopen, 
the sender must transmit enough TPDUs to fill the receiver's window 
but no more, and the receiver must renege previously advertised credit.
These three behaviors have been observed as follows.
.pp
Tracing with the 
\fBT.ndata\fR, 
\fBT.aks, \fR
\fBT.akr, \fR
\fBT.emit\fR 
and 
\fBT.input\fR 
trace options
are used.
The program \fItpdiscard\fR or a simple file transfer
is run with various window
and maximum TPDU sizes, various acknoledgment strategies, and
various retransmission strategies,
and the activity is observed with the trace.
The debug option
\fBT.reneg\fR
must be used to fake a reneging of credit, since
the ARGO transport entity does not renege its advertised credit
under normal operation.
At the beginning of a connection a closed window may almost always
be observed.
The receiving user process may be stopped 
to force a window to shut down.
The interesting statistics are
.(b
.nf
\fIn\fR times local cdt reneged (receiving)
\fIn\fR foreign window closed (sending)
\fIn\fR faked reneging of cdt
\fIn\fR DT TPDUs (sent)
\fIn\fR DT TPDUs (received)
\fIn\fR AK TPDUs (sent)
\fIn\fR AK TPDUs (received)
ACK reasons:
\fIn\fR not acked immediately
\fIn\fR strategy==each
\fIn\fR strategy==fullwindow
\fIn\fR duplicate DT
\fIn\fR EOTSDU
\fIn\fR reordered
\fIn\fR user rcvd
\fIn\fR fcc reqd
.fi
.)b
.sh 4 "Retransmission and retention until acknowledgment:"
.pp
To observe that the sender retains TPDUs until they are
acknowledged, one needs only to use the
\fBT.drop\fR
debug option to cause TPDUs to be dropped by the receiving side.
They are then retransmitted by the sender
and finally dropped when the acknowledgment arrives.
That the buffers used to hold retained TPDUs are freed 
can be observed by 
running \fInetstat(8)\fR with the -m option
on a quiescent system to observe the number of mbufs in use,
then
running a test with the 
\fBT.drop\fR debug option on to cause retransmission,
and 
finally
running netstat -m again after the test is over,
to see that all the mbufs have been freed by TP.
The actual retransmission activity can be observed in a trace
with the
\fBT.ndata, \fR
\fBT.emit\fR and 
\fBT.input\fR trace options.
The retransmission strategy to be used is controlled by the
ARGO directory service.
The statistics
.(b
.nf
\fIn\fR DT (retransmissions)
\fIn\fR XPD (retransmissions)
\fIn\fR CR (retransmissions)
\fIn\fR CC (retransmissions)
\fIn\fR DR (retransmissions)
.fi
.)b
.lp
indicate the number of retransmissions that actually occurred.
.sh 4 "Timers:"
.pp
The debug and trace option
\fBT.timer\fR
dumps information about the timers as they are set,
as they are cancelled, and as they expire.
The statistics
.(b
.nf
\fIn\fR ticks 
\fIn\fR timers set 
\fIn\fR timers expired 
\fIn\fR timers cancelled
\fIn\fR inactive timers cancelled
\fIn\fR connections dropped due to retrans limit
\fIn\fR CCs sent to zero dref
.fi
.)b
.lp
are printed for both the E-type timers and for the C-type timers.
The effect of timers can be seen by observing retransmissions.
Two simple ways to force retransmissions are:
.np
to use the \fBT.zdref\fR debug option,
which causes CCs to contain a zero destination reference,
so that connection establishment will time out, and
.np
to attempt to open a connection to a transport selector on which
no process is listening.
Either of these actions, along with the 
\fBT.connect\fR
trace or debug option will permit
observation of the timeout facilities.
.sh 4 "TPDU creation and parsing:"
.pp
TPDUs are created for output in 
\fItp_emit()\fR.
The 
\fBT.emit\fR
trace
option dumps TPDUs as they are transmitted.
\fITp_input()\fR parses TPDUs on input.
The 
\fBT.input\fR
trace
option dumps TPDUs as they are received.
The debug options \fBT.emit\fR and \fBT.input\fR dump a lot of excess information
to the console, and are used primarily for debugging
extremely pathological behavior.
.pp
By tracing the execution of 
\fItpdiscard\fR or a simple file transfer,
with a variety protocol and service options,
using the 
\fBT.connect,\fR
\fBT.emit,\fR
\fBT.input,\fR
\fBT.ndata,\fR
\fBT.aks,\fR
\fBT.akr,\fR
and 
\fBT.xpd\fR
options,
one can verify the correct placement of TPDU options
on all types of TPDUs.
The interesting statistics are
.(b
.nf
\fIn\fR variable parameters ignored 
\fIn\fR invalid parameter codes
\fIn\fR invalid parameter values
\fIn\fR invalid dutypes
\fIn\fR invalid negotiation failures
\fIn\fR invalid destinagion referencess
\fIn\fR invalid suffix parameters
\fIn\fR invalid checksums
\fIn\fR connections used extended format
\fIn\fR connections allowed transport XPD
\fIn\fR connections turned off checksumming
\fIn\fR TP 4 connections 
\fIn\fR TP 0 connections 
.fi
.)b
.sh 4 "Separation and concatenation:"
.pp
Concatenation is not supported by this implementation.
Separation is supported, however, and to test separation,
some sort of concatenated TPDUs had to be created.
The code for this is no longer supported.
After testing locally with some temporary code to create concatenated
TPDUs,
the ARGO transport implementation was tested with another transport
implementation that does generate concatenated TPDUs.
The interesting statistics are:
.(b
.nf
\fIn\fR concatenated TPDUs
\fIn\fR TPDUs input
.fi
.)b
.sh 4 "Length limits for TPDUs:"
.pp
Some TPDUs may take user data:
the CR, CC, DR, DT, and XPD.
All of these but the DT TPDU have limits to the amount
of data they may carry.
The limits are enforced for CR, CC, and DR TPDUs by
\fItp_ctloutput()\fR,
the routine that accepts data from the user.
The limit for XPD TPDUs is enforced by
\fItp_usrreq()\fR, which accepts expedited
data from the user.
To test the effectiveness of the checks on output, one may attempt
to send expedited data with amounts larger than the limit (16 bytes).
.pp
On input the limits are checked in
\fItp_input()\fR.
To test the effectiveness of the checks on input, it was necessary
to create an illegally large TPDU.
The
\fBT.badsize\fR
debug option
does this - it will turn a legitimate
XPD TPDU into a XPD TPDU with 18 bytes
of expedited data.
The interesting statistics are:
.(b
.nf
\fIn\fR illegally large XPD TPDU
\fIn\fR invalid length
.fi
.)b
.sh 4 "Frozen References:"
.pp
The key issue here is to see that references are not reassigned
until the reference timer expires.
This can be observed by watching the timer activity as described
above and by observing the reference numbers chosen for sockets
with the \fBT.emit\fR
or \fBT.input\fR trace options, which trace the TPDU headers.
.sh 4 "Inactivity Control:"
.pp
Inactivity control can be observed by turning on the trace options
\fBT.aks\fR, \fBT.akr\fR and \fBT.emit\fR
during a simple file transfer.
In the middle of the transfer, if the sender process
is stopped, both TP entities continue
to send acknowledgments. 
This can be observed in the trace.
If the file tranfer is between machines, taking down one of the machines
will cause the inactivity timer on the other machine to expire.
The TP entity will respond to this by sending a DR TPDU
to close the connection, which can be observed in the trace.
The expiration of the timer can be observed in a trace if the
\fBT.driver\fR option is used.
This option traces all events and state changes in the 
TP
finite state machine.
.sh 4 "Connection establishment:"
.pp
The process of connection establishment can be observed with the
\fBT.connect\fR
trace and debug options, and
the 
\fBT.driver \fR
trace option.
Various states of the connection establishment state machine
may be observed with the
the debug option
\fBT.zdref\fR.
This option
causes \fItp_input()\fR
to change the foreign reference on an incoming CC TPDU to zero,
eventually causing the CC TPDU to be dropped,
retransmissions of the CC to occur,
and the connection to time out before being established.
The statistics of interest are:
.(b
.nf
\fIn\fR CCs sent to zero dref
\fIn\fR invalid dest refs
\fIn\fR CC (received)
\fIn\fR CC (sent)
\fIn\fR CC (retransmitted)
\fIn\fR (connections) timed out on retrans
.fi
.)b
.sh 4 "Disconnection:"
.pp
Various states of the connection breakdown part of the state machine
may be observed with the
the trace options
\fBT.input\fR
or
\fBT.emit\fR, 
\fBT.driver\fR
and
running the
discard or file transfer programs.
.sh 4 "Association of TPDUs with Transport Connection:"
.pp
The problem of locating a transport connection
given a TPDU is handled in 
\fItp_input()\fR in one of two ways.
For an incoming CR TPDU, the transport suffix
is used to locate a transport protocol
control block (PCB), to which a transport
connection is made by creating a new socket and PCB.
For all other TPDU types, the destination reference is used
to locate the appropriate transport connection.
This is done by scanning the list of reference blocks.
Debug and trace options
were used to debug these sections of code but have since been
removed due to their effect on the readability 
and maintainability of this code.
The trace options
\fBT.connect\fR
and
\fBT.newsock\fR
creates trace records that contain the address of the
socket as well as that of the PCB
when a socket is opened.
When a TPDU arrives for a given socket,
the trace records created by the
\fBT.input \fR
option will also contain the address of the PCB that is found
in 
\fItp_input()\fR.
These two addresses can be compared in the trace output to observe
that the proper association is made.
.sh 4 "Protocol Errors and the ER TPDU:"
.pp
Certain types of errors are intended to evoke the response
from the TP entity of sending an ER or a DR TPDU.
The routine
\fItp_error_emit()\fR
creates ER and DR TPDUs for this purpose.
The debug and trace option
\fBT.erroremit\fR
dumps information about the activity of this routine.
Since ER TPDUs are not generated under normal circumstances,
the parsing of ER TPDUs was tested in this
implementation by code that generated (illegitimate) ER TPDUs,
This code was removed because it significantly complicated code maintenance.
.sh 4 "User Interface:"
.pp
The debug and trace options
\fBT.request\fR, 
\fBT.params,\fR
\fBT.indication\fR 
and
\fBT.syscall\fR and
dump information about the user interface.
Most of the debugging code added to the socket-layer
routines for debugging has since been removed so that
the source (which is functionally unchanged from the 4.3 release)
would not unnecessarily be changed.
\fIRlogin.iso\fR, the TP version of remote login,
exercises some of the original BSD data transfer system calls 
(\fIread()\fR and \fIwrite()\fR)
rather than \fIsendv()\fR and \fIrecv()\fR.
The interesting statistics are
.(b
.nf
\fIn\fR EOT indications
\fIn\fR user rcvd
\fIn\fR connections used extended format
\fIn\fR connections allowed transport XPD
\fIn\fR connections turned off checksumming
\fIn\fR TP 4 connections 
\fIn\fR TP 0 connections 
.fi
.)b
