.sh 2 "X.25 Public Data Network Support"
.pp
This ARGO release includes support for an X.25 Public Data Network (PDN)
in the form of a device driver for the Eicon Technology
Network Adapter \**.
.(f
This adapter, its software, and its documentation are
available from Eicon Technology Corporation, 3452 Ashby Street, Montreal,
Quebec, Canada H4R 2C1.
.)f
The adapter and its software, together with
the ARGO \fIecn(4)\fR driver, implement
the X.25 packet layer protocol as required to support the OSI connection
oriented network service.
The remainder of this section of this manual
destribes the ARGO device driver (hereinafter called "the driver")
for the Eicon Technology Network Adapter (hereinafter called "the adapter"),
the interface between the driver
and the CONS software described above, and the interface
between the driver and the software on the adapter.
.sh 3 "Software Modules"
.lp
The modules relevant to the design of the driver are listed below.
.ip "\fICONS -\fR"
The Connection Oriented Network Service (CONS) provides the upper ISO layers
with an interface to the PDN. 
In this release,
the PDN is 1980 X.25, although support for 1984 X.25 is included.
CONS can receive requests
from the CLNP entity and
from the OSI transport entity.
In addition, the CONS module 
supports \fIioctl()\fR commands used by 
\fIifconfig(8)\fR to configure 
the X.25 network address and to
declare the adapter to be up or down. 
See \fIcons(4p)\fR.
.ip "\fIDriver -\fR"
The driver accepts commands from CONS, formats these commands 
for the adapter, and interprets error indications delivered by the adapter.  
This driver supports all the UNIX configuration device structures. 
See \fIecn(4)\fR.
.ip "\fIEcnconf -\fR"
\fIEcnconf\fR is a program that allows the privileged user to
reconfigure
the options offered by the software on the adapter. 
\fIEcnconf\fR can be run at any time. 
See \fIecnconf(8)\fR.
.ip "\fIEcnload -\fR"
\fIEcnload\fR is a program that downloads Eicon Technology software
to the adapter and passes the configuration changes made 
with the \fIecnconf\fR program to the driver. 
\fIEcnload must be run only when the X.25 link is considered down\fR.
See \fIecnload(8)\fR. 
.ip "\fIEcnstat -\fR"
\fIEcnstat\fR is a program that
prints the connection state information and counters kept by
the adapter and by the driver.
The statistics include the number of sends and receives,
active connections, and errors.
For more information, see \fI ecnstat(8)\fR.
.ip "\fIAdapter -\fR"
The adapter's interface to the driver is the Network Control
Block and Request Vector that exist within the adapter's shared memory (often
called the "Common Data Area").
This is described in detail below.
.sh 3 "Interactions Among the Modules"
.lp
The commands
passed between CONS and the driver can be any one of the \fIECN\fR
commands outlined in the
sections "ECN Requests" and "ECN Replies", below.
CONS uses the \fCecnrestart()\fR procedure for restart requests,
\fCecnshutdown()\fR procedure for shutdown requests, and 
\fCecnoutput()\fR procedure for all
normal data transfer requests. 
CONS uses the \fCecnioctl()\fR procedure call for
servicing adapter status requests from the X.25 statistics program \fIecnstat\fR.
.lp
Commands passed between the driver and the adapter can
be any one of the network control block (\fINCB\fR)
commands described in the section "NCB Commands", below.
All commands to and from the adapter are
communicated in the Network Control Block and Request Vector within
the adapter's Common Data Area.
.lp
\fIEcnload\fR starts the 
Eicon Technology Network Adapter software on the adapter
and downloads the validated configuration
to the driver.
.sh 3 "ECN Requests"
.lp
The \fIECN\fR request types that CONS can pass to the driver are listed below.
.ip "\fIECN_STOP - (by calling ecnshutdown())\fR"
This request instructs the driver to restart the network but not to listen for 
any incoming calls. 
CONS issues this request in response to an \fIioctl()\fR command
issued by the utility program \fIifconfig\fR, when
\fIifconfig\fR is used to bring down the adapter.
.ip "\fIECN_RESTART - (by calling ecnrestart())\fR"
This request instructs the driver to restart the network \fIand\fR to listen 
and accept any incoming calls. 
CONS issues this request in response to an \fIioctl()\fR command
issued by the utility program \fIifconfig\fR, when
\fIifconfig\fR is used to bring the adapter up.
.ip "\fIECN_CALL - 0x90\fR"
This request instructs the driver to place
a call request
to the specified DTE.
.ip "\fIECN_CLEAR - 0x92\fR"
This request instructs the driver to clear a given virtual circuit. 
All outbound data are acknowledged by the remote DTE
before the circuit is cleared.
.ip "\fIECN_SEND - 0x94\fR"
This request instructs the driver to transmit a data buffer across a given 
virtual circuit.
.ip "\fIECN_RESET - 0x04\fR"
This request instructs the driver to reset the given virtual circuit and
clear out all outstanding requests
associated with that virtual circuit.
.ip "\fIECN_STATUS - 0xb4 (exclusively through ecnioctl())\fR"
This requests instructs the driver to 
solicit the adapter's current connection state information and
counters.
.sh 3 "ECN Replies"
.lp
The \fIECN\fR responses
the driver can give
to CONS are listed below.
.ip "\fIECN_CONNECT - 0x01\fR"
This reply notifies CONS that the driver has established a virtual circuit
connection initiated by the remote DTE.
.ip "\fIECN_ACCEPT - 0x03\fR"
This reply notifies CONS that an ECN_CALL request has succeeded. The
reply contains a pointer to a protocol control block.
.ip "\fIECN_REFUSE - 0x02\fR"
This reply notifies CONS that a previous \fIECN_CALL\fR request has failed.
The reply contains a pointer to a protocol control block.
.ip "\fIECN_CLEAR - 0x92\fR"
This reply notifies CONS that a given virtual circuit has been cleared
either by the DCE or by the remote DTE.
.ip "\fIECN_RECEIVE - 0x95\fR"
This reply notifies CONS that the driver has received a data packet from
the remote DTE.
.ip "\fIECN_RESET - 0x04\fR"
This reply notifies CONS that the virtual circuit has been reset either
by the DCE or by the remote DTE.
.ip "\fIECN_ACK - 0x05\fR"
This reply tells CONS that the associated ECN_SEND request has been been
completed by the adapter.
.sh 3 "NCB Commands"
.lp
The driver hides from the CONS module
many of the idiosyncrasies of the adapter's 
software interface
by mapping many of the above \fIECN\fR requests into corresponding
\fINCB\fR commands. Below is a list of requests that the driver can place to
the adapter. For each request that the driver places to the adapter, the adapter
returns with a command completion.
.ip "\fINCB_CALL - 0x90\fR"
This command creates a virtual circuit. 
.ip "\fINCB_LISTEN - 0x91\fR"
This command tells the adapter that our host is
willing to accept incoming calls. 
.ip "\fINCB_CLEAR (and NCB_ABORT) - 0x92\fR"
This command clears a virtual circuit. An option exists to clear the circuit
immediately, without waiting first for outstanding acknowledgments.
.ip "\fINCB_SEND (and NCB_RESET) - 0x94\fR"
This command sends data to the remote DTE. An option is
available for resetting the
virtual circuit. This command can return a status indicating that the
circuit has been cleared by the DCE or the remote DTE.
.ip "\fINCB_RECEIVE - 0x95\fR"
This command tells the adapter that our host is
willing to receive data on a given virtual circuit. This command can return
received data, a reset circuit, M-, D-, and Q-bits, interrupt packets,
or a cleared circuit.
.ip "\fINCB_STATUS - 0xb4\fR"
This command queries the adapter about 
the status of a virtual circuit.
The driver uses this command to support the ECN_STATUS request.
.ip "\fINCB_RESTART - 0xb2\fR"
This command restarts the network. This command requires that a corresponding
configuration file be passed down to the adapter.
.bp
.sh 3 "ECN Request and Reply Structure"
.lp
Below is
the data structure used in CONS-driver 
communications.
This data structure is a parameter to the 
\fIecnoutput()\fR procedure.
\fC
.nf
/* Eicon Driver Request Structure -- used between CONS and the driver */

struct eicon_request {
    struct ecn_ncb  eicon_req_ncb;   /* the network control block       */
    caddr_t         eicon_req_pcb;   /* CONS pcb used on CALL requests  */
    int             eicon_req_state; /* used internally by the driver   */
    int             eicon_retry_cnt; /* used internally by the driver   */
    int             eicon_more;      /* used internally by the driver   */
    u_char          eicon_reason;    /* source of CLEAR requests        */
};
\fR
.lp
The \fCeicon_req_ncb\fR field in the eicon request structure is of
type \fCecn_ncb\fR, defined in the following section. This structure stores 
the command block
that the driver uses in communicating with the adapter. 
The command block contains a \fIlogical session number\fR (LSN),
which identifies a virtual circuit.
Requests such as ECN_CALL are made without an LSN to identify
a circuit.
When an LSN is not available, the request is identified by
the field
\fCeicon_req_pcb\fR, which is a pointer to a CONS protocol control block. 
The \fCeicon_req_state\fR field is used by the driver to keep track 
of the status of the given request. 
The following list defines the various values for this field:
.ip "\fIREQ_NEW\fR"
The driver recognizes a new request, has placed the request into the driver's 
own request queue, but has yet to interrupt the
adapter. (The driver maintains a pointer \fCecn_pending_req\fR that indicates
whether an interrupt to the adapter is outstanding. If one is outstanding, the
driver places any new requests in this \fIREQ_NEW\fR state. If an interrupt 
is not
outstanding, the driver places the request immediately in the 
\fIREQ_INTERRUPT\fR state defined below.)
.ip "\fIREQ_INTERRUPT\fR"
The driver has dequeued the CONS request, assigned \fCecn_pending_req\fR to
point to the request, and
interrupted the adapter for a chance to post this request.
.ip "\fIREQ_POSTED\fR"
The driver has sent the request to the adapter.
.ip "\fIREQ_COMPLETE\fR"
The driver has just completed the request, and if necessary, is now posting 
it to CONS.
.lp
The \fCeicon_retry_cnt\fR field in the eicon request structure keeps track
of how many times the driver has tried posting this command to the adapter.
After the second retry, the driver gives up and performs the appropriate
error routine. 
The \fCeicon_more\fR field defines a \fIRECEIVE\fR request that
has been re-posted to the adapter to take care of m-bit transfers.
The \fCeicon_reason\fR field quantifies the reason for a connection being
cleared. These reasons are defined in the include file \fCiso_errno.h\fR.
.lp
Any data associated with the request are linked to the request through the
request mbuf's \fCm_next\fR field. 
This is done so that when
the driver calls the \fIMFREE_M\fR deallocation routine, both the request 
and the data are freed together.
.lp
The following chart defines those fields within the eicon request structure
that are relevant in any CONS request
to the driver via the \fIecnoutput()\fR call. 
.sp
.sz 8
.TS
center,box,tab(:);
c s s s s
c||c s s s
c||c|c|c|c
l||l|l|l|l.
\fBField Definitions for CONS \(-> Driver Requests\fR
_
\fI:Request Types (CONS \(-> Driver)\fR
\fIField:ECN_CALL:ECN_CLEAR:ECN_SEND:ECN_RESET\fR
=
\fIncb\(->command\fR:0x90:0x92:0x94:0x04 
_
\fIncb\(->loc_ses_num\fR:T{
.na
leave as zero
T}:VC #:VC #:VC #
_
\fIncb\(->info\fR:0x0:0x0:0x0:0x2
_
\fIeicon_req_pcb\fR:T{
.na
address of CONS' protocol control block
T}:NULL:NULL:NULL
_
\fIeicon_req_data\fR:T{
.na
address of mbuf containing contents of Call Request packet (including DTE address, facilities, and call user data)
T}:T{
.na
NULL or address of mbuf containing contents of Clear Request packet
T}:T{
.na
address of mbuf containing contents of user data
T}:T{
.na
NULL or the address of mbuf containing a one byte Reset Diagnostic code
T}
.TE
.sz 10
.sh 3 "Structure of the Network Control Block (NCB)"
.lp
The \fCecn_ncb\fR structure is used by the driver to
make requests of the adapter. 
\fC
.nf
/* Network Control Block -- used between the driver and the Eicon adapter */

struct ecn_ncb {
    u_char      command;        /* command field                         */
    u_char      retcode;        /* return code field                     */
    u_char      lsn;            /* local session number                  */
    u_char      info;           /* additional information                */
    caddr_t     buffer;         /* pointer to data buffer's mbuf         */
    u_short     length;         /* buffer length                         */
    u_char      callname[16];   /* module name on NA "X25"               */
    u_char      appl_name[16];  /* application name                      */
    u_char      rxto;           /* receive timeout in secs               */
    u_char      txto;           /* send(tx) timeout in secs              */
    caddr_t     post;           /* NULL                                  */
    u_char      lana_num;       /* specifies Eicon Tech NA               */
    u_char      cmd_cplt;       /* command status                        */
    u_char      reserve[14];    /* reserved area                         */
};
\fR
.sp
.lp
The chart below defines those fields that are relevant in any
reply passed by the driver back up to CONS.
.sp
.sz 7
.TS
center,box,tab(:);
c s s s s s s
c||c s s s s s 
c||c|c|c|c|c|c
l||l|l|l|l|l|l.
\fBField Definitions for Driver \(-> CONS Replies\fR
_
\fI:Reply Types (Driver \(-> CONS)\fR
\fIField:ECN_CONNECT:ECN_ACCEPT:ECN_REFUSE:ECN_CLEAR:ECN_RECEIVE:ECN_RESET\fR
=
\fIncb\(->command\fR:0x01:0x03:0x02:0x92:0x95:0x04
_
\fIncb\(->loc_ses_num\fR:VC #:VC #:ignore:VC #:VC #:VC #
_
\fIncb\(->info\fR:ignore:ignore:ignore:ignore:T{
.na
Interrupt received (bit 0), D-bit set (bit 6), and/or Q-bit set (bit 7). Zero
info field implies a normal receive.
T}:ignore
_
\fIeicon_req_pcb\fR:NULL:T{
.na
address of CONS's protocol control block
T}:T{
.na
address of CONS's protocol control block
T}:ignore:ignore:ignore
_
\fIeicon_req_data\fR:T{
.na
NULL or address of mbuf containing contents of Call Indication packet 
T}:T{
.na
NULL or address of mbuf containing contents of Call Connected data 
T}:T{
.na
NULL or address of mbuf containing contents of Call Cleared data
T}:T{
.na
NULL or address of mbuf containing contents of Call Cleared data
T}:T{
.na
address of mbuf containing contents of user data
T}:T{
.na
NULL or address of mbuf containing one byte Reset Diagnostic code
T}
_
\fIeicon_reason\fR:ignore:ignore:T{
.na
reason for refusal
T}:T{
.na
reason for clear
.T}:ignore:T{
.na
reason for reset
T}
.TE
.sz 10
.bp
.sh 3 "Internal Driver Data Sructures"
.lp
The main driver data structure
is the \fIecn_softc\fR structure.
This structure keeps track of the interface request queue 
(\fCecn_if\fR and \fCecn_pending_req\fR), 
magic addresses on the adapter (\fCecn_iom_base, ecn_mem_base,\fR and 
\fCecn_data_base\fR), 
error statistics (\fCecn_errors\fR), the state
of each virtual circuit (\fCecn_vc_state\fR), the state of the \fILISTEN\fR
request (\fCecn_listen_pending\fR), and the current caller (\fCecn_cause\fR).
\fC
.nf
struct ecn_softc {
    int             ecn_errors[NCB_MAX][ST_MAX]; 
    int             ecn_cause[CAUSE_MAX];  /* ecn_work() causes              */
    struct mbuf     *ecn_pending_req;      /* waiting for command req        */
    char            ecn_listen_pending;    /* boolean = listen req pending?  */
    char            ecn_vc_state[LSN_MAX]; /* the current state of each vc   */
    struct ecn_device      
                    *ecn_iom_base;         /* base address of io map         */
    struct ecn_request_vector
                    *ecn_mem_base;         /* base address of memory map     */
    caddr_t         ecn_data_base;         /* base address for data area     */
    struct ifnet    ecn_if;                /* queue of new requests          */
}
\fR
.so figs/ecn_queue.nr
.sh 2 "Queueing in the Driver"
.lp
.CF
illustrates the queueing mechanism used by the driver.
.lp
CONS queues its data transfer requests at the end of the queue managed by
\fCecn_if\fR field in the \fCecn_softc\fR structure. 
At this point, each request has the state value of 
\fIREQ_NEW\fR. 
Once the driver notifies the adapter that it has a command to post,
the driver dequeues the first request from the \fCecn_if\fR queue 
and sets the pointer
\fCecn_pending_req\fR to point to the request. 
At this point, the request is in the \fIREQ_INTERRUPT\fR state.
.lp
Once the driver posts the request to the adapter, it 
dequeues the next request in the \fCecn_if\fR queue, reassigns the 
\fCecn_pending_req\fR pointer, and then indicates to the adapter 
that it is ready to post another request. 
The driver no longer has to keep track of the previous request, 
because for every reply, the adapter includes the associated 
mbuf pointer. 
While the request is outstanding, the request is in the \fIREQ_POSTED\fR state.
.so figs/ecn_vc.nr
.lp
After the adapter completes the command, the driver may want to reply to CONS.
It does this by placing its reply in CONS's \fCconsintrq\fR queue, defined as
an external \fCifqueue\fR in the driver code.
.sh 2 "Virtual Circuit States"
.lp
The \fCecn_vc_state\fR array in the \fCecn_softc\fR structure above keeps track
of the state of each virtual circuit (VC).
This is necessary to avoid handing
the adapter any commands that may not apply during a given state. 
This mechanism
is especially useful in dealing with unexpected aborts or clears where there
is the potential for all outstanding commands to complete with errors. 
By changing
states, the driver can prevent redundant commands (like clears and aborts)
from being passed either to the adapter or to CONS. 
.lp
The driver only keeps track of four different states, as illustrated in 
.CF
.
These states are:
.ip "\fIVC_NO_CONNECTION\fR"
When a virtual circuit is in this state, the virtual circuit does not exits.
Only \fICALL\fR and \fILISTEN\fR commands are valid.
.ip "\fIVC_DATA_XFER\fR"
All commands, except \fICALL\fR and \fILISTEN\fR commands are valid once the
connection exists.
.ip "\fIVC_RESET_IN_PROGRESS\fR"
In this state, either the driver has issued an \fINCB_RESET\fR or it has 
received a reset error code on the completion of a command. 
Only reissued \fIRESET\fR commands and \fIRECEIVE\fRs are
valid. 
\fIRECEIVE\fR is valid in this state because the adapter uses the
completion of this command to hand back the cause of the reset (the RESET
INDICATION packet).
.ip "\fIVC_CLEAR_IN_PROGRESS\fR"
The driver has either issued an \fINCB_CLEAR\fR command or has just
received a clear error code on the completion of a command. 
Within this state, only reissued
\fICLEAR\fR and \fIABORT\fR commands are valid.
.sh 2 "Error Statistics"
.lp
With the \fCecn_errors\fR field in the \fCecn_softc\fR structure,
the driver maintains a two dimensional array of counters
if the frequencies of errors.
In order to inspect this array easily with
the kernel debugger, the first index to every command ( <command, 0> ) is
reserved for a four character ASCII command identifier.
.bp
.sh 3 "The Driver State Machine"
.sh 2 "Handling of Normal Command Completions"
.lp
The chart below lists
all the available adapter request types, at what level each of
these requests can be used, options, and the driver's action after a normal
completion of the command.
.sp
.sz 7
.TS
center,box,tab(:);
c s s s
c|c s|c
c|c|c|c
l|l|l|l.
\fBNormal Completion Handling\fR
_
\fINCB:Options:Action Based on Normal Competion of\fR
\fICommand:To Adapter:From Adapter:Driver\(->Adapter Command\fR
=
\fINCB_RESTART\fR:none:none:T{
.na
dequeue the request, and issue an NCB_LISTEN request to the adapter.
T}
_
\fINCB_CALL\fR:none:connected:T{
.na
dequeue the request, pass an ECN_ACCEPT reply to CONS, and issue a RECEIVE to
the adapter.
T}
_
\fINCB_LISTEN\fR:T{
.na
use zero-length Call User Data and a zero-length Calling DTE address
T}:none:T{
.na
dequeue the request, pass an ECN_CONNECT to CONS, and issue a RECEIVE to the
adapter. Re-issue another NCB_LISTEN
for another possible virtual circuit connection.
T}
_
\fINCB_CLEAR\fR:T{
.na
normal clearing with all outstanding ACKs returned 
T}:none:T{
.na
dequeue the request.
T}
:_:_:_
:T{
.na
immediate clearing 
T}:none:T{
.na
dequeue the request.
T}
_
\fINCB_SEND\fR:T{
.na
normal send
T}:none:T{
.na
dequeue the request and reply to CONS with an ECN_ACK.
T}
:_:_:_
:T{
.na
reset the virtual circuit
T}:none:T{
dequeue the request.
T}
_
\fINCB_RECEIVE\fR:none:T{
.na
normal, uncomplicated receive
T}:T{
.na
dequeue the request and bcopy the data into the request's associated mbuf. Ship to CONS. Re-issue another NCB_RECEIVE.
T}
:_:_:_
:none:T{
.na
m-bit set
T}:T{
.na
same as above (adapter does the resegmentation automatically).
T}
:_:_:_
:none:T{
.na
d-bit set
T}:T{
.na
same as above.
T}
:_:_:_
:none:T{
.na
q-bit set
T}:T{
.na
same as above.
T}
:_:_:_
:none:T{
.na
interrupt received
T}:T{
.na
same as above.
T}
:_:_:_
:none:T{
.na
reset received
T}:T{
dequeue the request, send an ECN_RESET back up to CONS, and issue another 
receive.
T}
.TE
.sz 10
.sp
.uh "CONS \(-> Driver"
.lp
All entries in this column indicate that the CONS module can send this request 
down to the driver.  Command names in parenthesis define the mapping between
the \fIECN\fR and \fINCB\fR commands.
.uh "Driver \(-> Adapter"
.lp
All checks in this column indicate that the driver can send this request
to the adapter. The last column in the above table defines what the driver must
do upon normal completion of the command from the adapter. 
Note that not all driver-to-adapter
commands have a CONS-to-driver equivalent. 
This shows that this 
command request is generated within the driver, rather than originating from
the CONS driver.
.uh "Driver \(-> CONS"
.lp
All entries in this column indicate that the driver can send this reply  
back to CONS. Command names in parenthesis define  the mapping between 
the \fIECN\fR and \fINCB\fR commands.
.bp
.sh 3 "Handling of Errors upon Command Completion"
.lp
Below is listed all the driver request and pseudo request types, along with the
actions the driver must perform given a command completion error delivered by
the Eicon Network Adapter.
.sp
.sz 7
.TS
center,box,tab(:);
c  s s s s s s s
c||c s s s s s s
c||c|c|c|c|c|c|c
c||c|c|c|c|c|c|c
l||l|l|l|l|l|l|l.
\fBError Completion Handling\fR
_
:\fIAction Based on Error Completion of Driver \(-> Adapter Command\fR
\fIError Returned\fR:_:_:_:_:_:_:_
\fI:NCB_CALL:NCB_LISTEN:NCB_CLEAR:NCB_ABORT:NCB_RESET:NCB_SEND:NCB_RECEIVE\fR
=
\fIST_BAD_LEN\fR:<soft-error>:<soft-error>:<soft-error>:<soft-error>:<soft-error>:<soft-error>:<soft-error>
_
\fIST_INVALID\fR:<soft-error>:<soft-error>:<dequeue>:<dequeue>:<dequeue>:<dequeue>:<dequeue>
_
\fIST_COMMAND_TO\fR:<retry>:<retry>:<retry>:<retry>:<abort>:<abort>:<retry>
_
\fIST_ISSUE_ANOTHER_RCV\fR:<refuse>:<retry>:<retry>:<retry>:<abort>:<abort>:T{
.na
requeue request and increment "more" count
T}
_
\fIST_BAD_LSN\fR:<soft-error>:<soft-error>:<dequeue>:<dequeue>:<dequeue>:<dequeue>:<dequeue>
_
\fIST_NO_RESOURCES\fR:<retry>:<retry>:<retry>:<retry>:<abort>:<abort>:<retry>
_
\fIST_CALL_CLEARED\fR:<refuse>:<retry>:<retry>:<retry>:<clear>:<clear>:<clear>
_
\fIST_COMMAND_CANCELLED\fR:<refuse>:<retry>:<retry>:<retry>:<abort>:<abort>:<abort>:
_
\fIST_NO_CIRCUITS\fR:<refuse>:<retry>:<retry>:<retry>:<abort>:<abort>:<abort>
_
\fIST_CALL_UNSUCCESSFUL\fR:<refuse>:<retry>:<retry>:<retry>:<abort>:<abort>:<abort>
_
\fIST_INCORRECT_CALLNAME\fR:<soft-error>:<soft-error>:<soft-error>:<soft-error>:<soft-error>:<soft-error>:<soft-error>
_
\fIST_X25_RESET\fR:<refuse>:<retry>:<retry>:<retry>:<dequeue>:<dequeue>:<retry>
_
\fIST_TOO_MANY_COMMANDS\fR:<retry>:<retry>:<retry>:<retry>:<abort>:<abort>:<retry>
_
\fIST_L1_NO_DATA_SET_READY\fR:<refuse>:<retry>:<retry>:<retry>:<abort>:<abort>:<abort>
_
\fIST_L1_NO_CLEAR_TO_SEND\fR:<refuse>:<retry>:<retry>:<retry>:<abort>:<abort>:<abort>
_
\fIST_L1_NO_CLOCK\fR:<refuse>:<retry>:<retry>:<retry>:<abort>:<abort>:<abort>
.TE
.sz 10
.sp
.lp
Each of the actions from the above chart are defined as follows.
.ip "\fI<abort>\fR -"
The driver should clear the connection by issuing an \fINCB_ABORT\fR 
to the adapter and sending an \fIECN_CLEAR\fR to CONS.
.ip "\fI<refuse>\fR -"
The driver should send an \fIECN_REFUSE\fR back to CONS.
.ip "\fI<dequeue>\fR -"
The driver should simply dequeue the request. Usually these errors occur when a
reset or clear occurs on the adapter while the driver is in the midst of
issuing the command which subsequently completes with an error status.
.ip "\fI<clear>\fR -"
The driver should send an \fIECN_CLEAR\fR back up to CONS.
.ip "\fI<retry>\fR -"
The driver should requeue the request if and only if the 
\fCecn_retry_cnt\fR field in the request structure does not exceed the
retry maximum. 
.ip "\fI<soft-error>\fR -"
This action only takes place when a software error has occurred. The driver 
should
print the error to the console in big bold letters and then panic.
.bp
.sh 3 "The IFP Flags"
.lp
The IFP flags in the standard \fCifnet\fR structure
should be used in the following way.
.ip "\fIIFF_UP on -\fR"
This flag is set by the driver only after the procedure \fIecnrestart()\fR
successfully completes.
.ip "\fIIFF_UP off -\fR"
This flag is set immediately upon entry into the procedure \fIecnshutdown()\fR.
.ip "\fIIFF_RUNNING on -\fR"
This flag is set on whenever the \fIecnwork()\fR procedure is active, eg. the
driver is actually doing something.
.ip "\fIIFF_RUNNING off -\fR"
This flag is turned off upon exit from the \fIecnwork()\fR procedure.
