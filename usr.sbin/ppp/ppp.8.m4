.\" $Id: ppp.8,v 1.104 1998/06/12 20:12:26 brian Exp $
.Dd 20 September 1995
.Os FreeBSD
.Dt PPP 8
.Sh NAME
.Nm ppp
.Nd Point to Point Protocol (a.k.a. user-ppp) 
.Sh SYNOPSIS
.Nm
.Oo
.Fl auto |
.Fl background |
.Fl ddial |
.Fl direct |
.Fl dedicated
.Oc
.Op Fl alias
.Op Ar system
.Sh DESCRIPTION
This is a user process
.Em PPP
software package.  Normally,
.Em PPP
is implemented as a part of the kernel (e.g. as managed by
.Xr pppd 8 )
and it's thus somewhat hard to debug and/or modify its behaviour.
However, in this implementation
.Em PPP
is done as a user process with the help of the
tunnel device driver (tun).
.Sh Major Features
.Bl -diag
.It Provides interactive user interface.
Using its command mode, the user can
easily enter commands to establish the connection with the remote end, check
the status of connection and close the connection.  All functions can
also be optionally password protected for security.
.It Supports both manual and automatic dialing.
Interactive mode has a
.Dq term
command which enables you to talk to your modem directly.  When your
modem is connected to the remote peer and it starts to talk
.Em PPP ,
.Nm
detects it and switches to packet mode automatically.  Once you have
determined the proper sequence for connecting with the remote host, you
can write a chat script to define the necessary dialing and login
procedure for later convenience.
.It Supports on-demand dialup capability.
By using
.Fl auto
mode,
.Nm
will act as a daemon and wait for a packet to be sent over the
.Em PPP
link.  When this happens, the daemon automatically dials and establishes the
connection.
In almost the same manner
.Fl ddial
mode (direct-dial mode) also automatically dials and establishes the
connection.  However, it differs in that it will dial the remote site
any time it detects the link is down, even if there are no packets to be
sent.  This mode is useful for full-time connections where we worry less
about line charges and more about being connected full time.
A third
.Fl dedicated
mode is also available.  This mode is targeted at a dedicated link
between two machines.
.Nm Ppp
will never voluntarily quit from dedicated mode - you must send it the
.Dq quit all
command via its diagnostic socket.  A
.Dv SIGHUP
will force an LCP renegotiation, and a
.Dv SIGTERM
will force it to exit.
.It Supports packet aliasing.
Packet aliasing (a.k.a. IP masquerading) allows computers on a
private, unregistered network to access the Internet.  The
.Em PPP
host acts as a masquerading gateway.  IP addresses as well as TCP and
UDP port numbers are aliased for outgoing packets and de-aliased for
returning packets.
.It Supports background PPP connections.
In background mode, if
.Nm
successfully establishes the connection, it will become a daemon.
Otherwise, it will exit with an error.  This allows the setup of
scripts that wish to execute certain commands only if the connection
is successfully established.
.It Supports server-side PPP connections.
In direct mode,
.nm
acts as server which accepts incoming
.Em PPP
connections on stdin/stdout.
.It Supports PAP and CHAP authentication.
With PAP or CHAP, it is possible to skip the Unix style
.Xr login 1
procedure, and use the
.Em PPP
protocol for authentication instead.  If the peer requests Microsoft
CHAP authentication and
.Nm
is compiled with DES support, an appropriate MD4/DES response will be
made.
.It Supports Proxy Arp.
When
.Em PPP
is set up as server, you can also configure it to do proxy arp for your
connection.
.It Supports packet filtering.
User can define four kinds of filters: the
.Em in
filter for incoming packets, the
.Em out
filter for outgoing packets, the
.Em dial
filter to define a dialing trigger packet and the
.Em alive
filter for keeping a connection alive with the trigger packet.
.It Tunnel driver supports bpf.
The user can use
.Xr tcpdump 1
to check the packet flow over the
.Em PPP
link.
.It Supports PPP over TCP capability.
If a device name is specified as
.Em host Ns No : Ns Em port ,
.Nm
will open a TCP connection for transporting data rather than using a
conventional serial device.
.It Supports IETF draft Predictor-1 and DEFLATE compression.
.Nm
supports not only VJ-compression but also Predictor-1 and DEFLATE compression.
Normally, a modem has built-in compression (e.g. v42.bis) and the system
may receive higher data rates from it as a result of such compression.
While this is generally a good thing in most other situations, this
higher speed data imposes a penalty on the system by increasing the
number of serial interrupts the system has to process in talking to the
modem and also increases latency.  Unlike VJ-compression, Predictor-1 and
DEFLATE compression pre-compresses
.Em all
network traffic flowing through the link, thus reducing overheads to a
minimum.
.It Supports Microsoft's IPCP extensions.
Name Server Addresses and NetBIOS Name Server Addresses can be negotiated
with clients using the Microsoft
.Em PPP
stack (ie. Win95, WinNT)
.It Supports Multi-link PPP
It is possible to configure
.Nm
to open more than one physical connection to the peer, combining the
bandwidth of all links for better throughput.
.Sh PERMISSIONS
.Nm Ppp
is installed as user
.Dv root
and group
.Dv network ,
with permissions
.Dv 4550 .
By default, 
.Nm
will not run if the invoking user id is not zero.  This may be overridden
by using the
.Dq allow users
command in
.Pa /etc/ppp/ppp.conf .
When running as a normal user,
.Nm
switches to user id 0 in order to alter the system routing table, set up
system lock files and read the ppp configuration files.  All
external commands (executed via the "shell" or "!bg" commands) are executed
as the user id that invoked
.Nm ppp .
Refer to the
.Sq ID0
logging facility if you're interested in what exactly is done as user id
zero.
.Sh GETTING STARTED
When you first run
.Nm
you may need to deal with some initial configuration details.
.Bl -bullet
.It
Your kernel must include a tunnel device (the GENERIC kernel includes
one by default).  If it doesn't, or if you require more than one tun
interface, you'll need to rebuild your kernel with the following line in
your kernel configuration file:
.Pp
.Dl pseudo-device tun N
.Pp
where
.Ar N
is the maximum number of
.Em PPP
connections you wish to support.
.It
Check your
.Pa /dev
directory for the tunnel device entries
.Pa /dev/tunN ,
where
.Sq N
represents the number of the tun device, starting at zero.
If they don't exist, you can create them by running "sh ./MAKEDEV tunN".
This will create tun devices 0 through
.Ar N .
.It
Make sure that your system has a group named
.Dq network
in the
.Pa /etc/group
file and that that group contains the names of all users expected to use
.Nm ppp .
Refer to the
.Xr group 5
manual page for details.  Each of these uses must also be given access
using the
.Dq allow users
command in
.Pa /etc/ppp/ppp.conf .
.It
Create a log file.
.Nm Ppp
uses 
.Xr syslog 3
to log information.  A common log file name is
.Pa /var/log/ppp.log .
To make output go to this file, put the following lines in the
.Pa /etc/syslog.conf
file:
.Bd -literal -offset indent
!ppp
*.*<TAB>/var/log/ppp.log
.Ed
.Pp
Make sure you use actual TABs here.  If you use spaces, the line will be
silently ignored by
.Xr syslogd 8 .
.Pp
It is possible to have more than one
.Em PPP
log file by creating a link to the
.Nm
executable:
.Pp
.Dl # cd /usr/sbin
.Dl # ln ppp ppp0
.Pp
and using
.Bd -literal -offset indent
!ppp0
*.*<TAB>/var/log/ppp0.log
.Ed
.Pp
in
.Pa /etc/syslog.conf .
Don't forget to send a
.Dv HUP
signal to
.Xr syslogd 8
after altering
.Pa /etc/syslog.conf .
.It
Although not strictly relevant to
.Nm ppp Ns No s
operation, you should configure your resolver so that it works correctly.
This can be done by configuring a local DNS
.Pq using Xr named 8
or by adding the correct
.Sq name-server
lines to the file
.Pa /etc/resolv.conf .
Refer to the
.Xr resolv.conf 5
manual page for details.
.El
.Sh MANUAL DIALING
In the following examples, we assume that your machine name is
.Dv awfulhak .
when you invoke
.Nm
(see
.Em PERMISSIONS
above) with no arguments, you are presented with a prompt:
.Bd -literal -offset indent
ppp ON awfulhak>
.Ed
.Pp
The
.Sq ON
part of your prompt should always be in upper case.  If it is in lower
case, it means that you must supply a password using the
.Dq passwd
command.  This only ever happens if you connect to a running version of
.Nm
and have not authenticated yourself using the correct password.
.Pp
You can start by specifying the device name, speed and parity for your modem,
and whether CTS/RTS signalling should be used (CTS/RTS is used by
default).  If your hardware does not provide CTS/RTS lines (as
may happen when you are connected directly to certain PPP-capable
terminal servers),
.Nm
will never send any output through the port; it waits for a signal
which never comes.  Thus, if you have a direct line and can't seem
to make a connection, try turning CTS/RTS off:
.Bd -literal -offset indent
ppp ON awfulhak> set line /dev/cuaa0
ppp ON awfulhak> set speed 38400
ppp ON awfulhak> set parity even
ppp ON awfulhak> set ctsrts on
ppp ON awfulhak> show modem
* Modem related information is shown here *
ppp ON awfulhak>
.Ed
.Pp
The term command can now be used to talk directly with your modem:
.Bd -literal -offset indent
ppp ON awfulhak> term
at
OK
atdt123456
CONNECT
login: ppp
Password:
Protocol: ppp
.Ed
.Pp
When the peer starts to talk in
.Em PPP ,
.Nm
detects this automatically and returns to command mode.
.Bd -literal -offset indent
ppp ON awfulhak>
Ppp ON awfulhak>
PPp ON awfulhak>
PPP ON awfulhak>
.Ed
.Pp
If it does not, it's possible that the peer is waiting for your end to
start negotiating.  To force
.Nm
to start sending PPP configuration packets to the peer, use the
.Dq ~p
command to enter packet mode.
.Pp
You are now connected!  Note that
.Sq PPP
in the prompt has changed to capital letters to indicate that you have
a peer connection.  If only some of the three Ps go uppercase, wait 'till
either everything is uppercase or lowercase.  If they revert to lowercase,
it means that
.Nm
couldn't successfully negotiate with the peer.  This is probably because
your PAP or CHAP authentication name or key is incorrect.  A good first step
for troubleshooting at this point would be to
.Dq set log local phase .
Refer to the
.Dq set log
command description below for further details.
.Pp
When the link is established, the show command can be used to see how
things are going:
.Bd -literal -offset indent
PPP ON awfulhak> show modem
* Modem related information is shown here *
PPP ON awfulhak> show ccp
* CCP (compression) related information is shown here *
PPP ON awfulhak> show lcp
* LCP (line control) related information is shown here *
PPP ON awfulhak> show ipcp
* IPCP (IP) related information is shown here *
PPP ON awfulhak> show link
* Link (high level) related information is shown here *
PPP ON awfulhak> show bundle
* Logical (high level) connection related information is shown here *
.Ed
.Pp
At this point, your machine has a host route to the peer.  This means
that you can only make a connection with the host on the other side
of the link.  If you want to add a default route entry (telling your
machine to send all packets without another routing entry to the other
side of the
.Em PPP
link), enter the following command:
.Bd -literal -offset indent
PPP ON awfulhak> add default HISADDR
.Ed
.Pp
The string
.Sq HISADDR
represents the IP address of the connected peer.  It is possible to
use the keyword
.Sq INTERFACE
in place of
.Sq HISADDR .
This will create a direct route on the tun interface.
If it fails due to an existing route, you can overwrite the existing
route using
.Bd -literal -offset indent
PPP ON awfulhak> add! default HISADDR
.Ed
.Pp
You can now use your network applications (ping, telnet, ftp etc.)
in other windows on your machine.
Refer to the
.Em PPP COMMAND LIST
section for details on all available commands.
.Sh AUTOMATIC DIALING
To use automatic dialing, you must prepare some Dial and Login chat scripts.
See the example definitions in
.Pa /etc/ppp/ppp.conf.sample
(the format of
.Pa /etc/ppp/ppp.conf
is pretty simple).
Each line contains one comment, inclusion, label or command:
.Bl -bullet
.It
A line starting with a
.Pq Dq #
character is treated as a comment line.  Leading whitespace are ignored
when identifying comment lines.
.It
An inclusion is a line beginning with the word
.Sq !include .
It must have one argument - the file to include.  You may wish to
.Dq !include ~/.ppp.conf
for compatibility with older versions of
.Nm ppp .
.It
A label name starts in the first column and is followed by
a colon
.Pq Dq \&: .
.It
A command line must contain a space or tab in the first column.
.El
.Pp
The
.Pa /etc/ppp/ppp.conf
file should consist of at least a
.Dq default
section.  This section is always executed.  It should also contain
one or more sections, named according to their purpose, for example,
.Dq MyISP
would represent your ISP, and
.Dq ppp-in
would represent an incoming
.Nm
configuration.
You can now specify the destination label name when you invoke
.Nm ppp .
Commands associated with the
.Dq default
label are executed, followed by those associated with the destination
label provided.  When
.Nm
is started with no arguments, the
.Dq default
section is still executed.  The load command can be used to manually
load a section from the
.Pa /etc/ppp/ppp.conf
file:
.Bd -literal -offset indent
PPP ON awfulhak> load MyISP
.Ed
.Pp
Once the connection is made, the
.Sq ppp
portion of the prompt will change to
.Sq PPP :
.Bd -literal -offset indent
# ppp MyISP
...
ppp ON awfulhak> dial
dial OK!
login OK!
PPP ON awfulhak>
.Ed
.Pp
If the
.Pa /etc/ppp/ppp.linkup
file is available, its contents are executed
when the
.Em PPP
connection is established.  See the provided
.Dq pmdemand
example in
.Pa /etc/ppp/ppp.conf.sample
which runs a script in the background after the connection is established.
The literal strings
.Dv HISADDR ,
.Dv MYADDR
and
.Dv INTERFACE
may be used, and will be replaced with the relevant IP addresses and interface
name.  Similarly, when a connection is closed, the
contents of the
.Pa /etc/ppp/ppp.linkdown
file are executed.
Both of these files have the same format as
.Pa /etc/ppp/ppp.conf .
.Pp
In previous versions of
.Nm ppp ,
it was necessary to re-add routes such as the default route in the
.Pa ppp.linkup
file.
.Nm Ppp
now supports
.Sq sticky routes ,
where all routes that contain the
.Dv HISADDR
or
.Dv MYADDR
literals will automatically be updated when the values of
.Dv HISADDR
and/or
.Dv MYADDR
change.
.Sh BACKGROUND DIALING
If you want to establish a connection using
.Nm
non-interactively (such as from a
.Xr crontab 5
entry or an
.Xr at 1
job) you should use the
.Fl background
option.
When
.Fl background
is specified,
.Nm
attempts to establish the connection immediately.  If multiple phone
numbers are specified, each phone number will be tried once.  If the
attempt fails,
.Nm
exits immediately with a non-zero exit code.
If it succeeds, then
.Nm
becomes a daemon, and returns an exit status of zero to its caller.
The daemon exits automatically if the connection is dropped by the
remote system, or it receives a
.Dv TERM
signal.
.Sh DIAL ON DEMAND
Demand dialing is enabled with the
.Fl auto
or
.Fl ddial
options.  You must also specify the destination label in
.Pa /etc/ppp/ppp.conf
to use.  It must contain the
.Dq set ifaddr
command to define the remote peers IP address. (refer to
.Pa /etc/ppp/ppp.conf.sample )
.Bd -literal -offset indent
# ppp -auto pmdemand
.Ed
.Pp
When
.Fl auto
or
.Fl ddial
is specified,
.Nm
runs as a daemon but you can still configure or examine its
configuration by using the
.Dq set server
command in
.Pa /etc/ppp/ppp.conf ,
.Pq for example, Dq set server +3000 mypasswd
and connecting to the diagnostic port as follows:
.Bd -literal -offset indent
# pppctl 3000	(assuming tun0 - see the ``set server'' description)
Password:
PPP ON awfulhak> show who
tcp (127.0.0.1:1028) *
.Ed
.Pp
The
.Dq show who
command lists users that are currently connected to
.Nm
itself.  If the diagnostic socket is closed or changed to a different
socket, all connections are immediately dropped.
.Pp
In
.Fl auto
mode, when an outgoing packet is detected,
.Nm
will perform the dialing action (chat script) and try to connect
with the peer.  In
.Fl ddial
mode, the dialing action is performed any time the line is found
to be down.
If the connect fails, the default behaviour is to wait 30 seconds
and then attempt to connect when another outgoing packet is detected.
This behaviour can be changed with
.Bd -literal -offset indent
set redial seconds|random[.nseconds|random] [dial_attempts]
.Ed
.Pp
.Sq Seconds
is the number of seconds to wait before attempting
to connect again. If the argument is
.Sq random ,
the delay period is a random value between 0 and 30 seconds.
.Sq Nseconds
is the number of seconds to wait before attempting
to dial the next number in a list of numbers (see the
.Dq set phone
command).  The default is 3 seconds.  Again, if the argument is
.Sq random ,
the delay period is a random value between 0 and 30 seconds.
.Sq dial_attempts
is the number of times to try to connect for each outgoing packet
that is received. The previous value is unchanged if this parameter
is omitted.  If a value of zero is specified for
.Sq dial_attempts ,
.Nm
will keep trying until a connection is made.
.Bd -literal -offset indent
set redial 10.3 4
.Ed
.Pp
will attempt to connect 4 times for each outgoing packet that is
detected with a 3 second delay between each number and a 10 second
delay after all numbers have been tried.  If multiple phone numbers
are specified, the total number of attempts is still 4 (it does not
attempt each number 4 times).
Modifying the dial delay is very useful when running
.Nm
in demand
dial mode on both ends of the link. If each end has the same timeout,
both ends wind up calling each other at the same time if the link
drops and both ends have packets queued.
At some locations, the serial link may not be reliable, and carrier
may be lost at inappropriate times.  It is possible to have
.Nm
redial should carrier be unexpectedly lost during a session.
.Bd -literal -offset indent
set reconnect timeout ntries
.Ed
.Pp
This command tells
.Nm
to re-establish the connection
.Ar ntries
times on loss of carrier with a pause of
.Ar timeout
seconds before each try.  For example,
.Bd -literal -offset indent
set reconnect 3 5
.Ed
.Pp
tells
.Nm
that on an unexpected loss of carrier, it should wait
.Ar 3
seconds before attempting to reconnect.  This may happen up to
.Ar 5
times before
.Nm
gives up.  The default value of ntries is zero (no reconnect).  Care
should be taken with this option.  If the local timeout is slightly
longer than the remote timeout, the reconnect feature will always be
triggered (up to the given number of times) after the remote side
times out and hangs up.
NOTE:  In this context, losing too many LQRs constitutes a loss of
carrier and will trigger a reconnect.
If the
.Fl background
flag is specified, all phone numbers are dialed at most once until
a connection is made.  The next number redial period specified with
the
.Dq set redial
command is honoured, as is the reconnect tries value.  If your redial
value is less than the number of phone numbers specified, not all
the specified numbers will be tried.
To terminate the program, type
.Bd -literal -offset indent
PPP ON awfulhak> close
ppp ON awfulhak> quit all
.Ed
.Pp
A simple
.Dq quit
command will terminate the
.Xr pppctl 8
or
.Xr telnet 1
connection but not the
.Nm
program itself.
You must use
.Dq quit all
to terminate
.Nm
as well.
.Sh RECEIVING INCOMING PPP CONNECTIONS (Method 1)
To handle an incoming
.Em PPP
connection request, follow these steps:
.Bl -enum
.It
Make sure the modem and (optionally)
.Pa /etc/rc.serial
is configured correctly.
.Bl -bullet -compact
.It
Use Hardware Handshake (CTS/RTS) for flow control.
.It
Modem should be set to NO echo back (ATE0) and NO results string (ATQ1).
.El
.Pp
.It
Edit
.Pa /etc/ttys
to enable a
.Xr getty 8
on the port where the modem is attached.
For example:
.Pp
.Dl ttyd1  "/usr/libexec/getty std.38400" dialup on secure
.Pp
Don't forget to send a
.Dv HUP
signal to the
.Xr init 8
process to start the
.Xr getty 8 :
.Pp
.Dl # kill -HUP 1
.It
Create a
.Pa /usr/local/bin/ppplogin
file with the following contents:
.Bd -literal -offset indent
#! /bin/sh
exec /usr/sbin/ppp -direct incoming
.Ed
.Pp
Direct mode
.Pq Fl direct
lets
.Nm
work with stdin and stdout.  You can also use
.Xr pppctl 8
to connect to a configured diagnostic port, in the same manner as with
client-side
.Nm ppp .
.Pp
Here, the
.Ar incoming
label must be set up in
.Pa /etc/ppp/ppp.conf .
.It
Prepare an account for the incoming user.
.Bd -literal
ppp:xxxx:66:66:PPP Login User:/home/ppp:/usr/local/bin/ppplogin
.Ed
.Pp
Refer to the manual entries for
.Xr adduser 8
and
.Xr vipw 8
for details.
.It
Support for IPCP Domain Name Server and NetBIOS Name Server negotiation
can be enabled using the
.Dq enable dns
and
.Dq set nbns
commands.  Refer to their descriptions below.
.El
.Pp
.Sh RECEIVING INCOMING PPP CONNECTIONS (Method 2)
This method differs in that we use
.Nm ppp
to authenticate the connection rather than
.Xr login 1 :
.Bl -enum
.It
Configure your default section in
.Pa /etc/gettytab
with automatic ppp recognition by specifying the
.Dq pp
capability:
.Bd -literal
default:\\
	:pp=/usr/local/bin/ppplogin:\\
	.....
.Ed
.It
Configure your serial device(s), enable a
.Xr getty 8
and create
.Pa /usr/local/bin/ppplogin
as in the first three steps for method 1 above.
.It
Add either
.Dq enable chap
or
.Dq enable pap
.Pq or both
to
.Pa /etc/ppp/ppp.conf
under the
.Sq incoming
label (or whatever label
.Pa ppplogin
uses).
.It
Create an entry in
.Pa /etc/ppp/ppp.secret
for each incoming user:
.Bd -literal
Pfred<TAB>xxxx
Pgeorge<TAB>yyyy
.Ed
.Pp
Now, as soon as
.Xr getty 8
detects a ppp connection (by recognising the HDLC frame headers), it runs
.Dq /usr/local/bin/ppplogin .
.Pp
It is
.Em VITAL
that either PAP or CHAP are enabled as above.  If they are not, you are
allowing anybody to establish ppp session with your machine
.Em without
a password, opening yourself up to all sorts of potential attacks.
.Sh AUTHENTICATING INCOMING CONNECTIONS
Normally, the receiver of a connection requires that the peer
authenticates itself.  This may be done using
.Xr login 1 ,
but alternatively, you can use PAP or CHAP.  CHAP is the more secure
of the two, but some clients may not support it.  Once you decide which
you wish to use, add the command
.Sq enable chap
or
.Sq enable pap
to the relevant section of
.Pa ppp.conf .
.Pp
You must then configure the
.Pa /etc/ppp/ppp.secret
file.  This file contains one line per possible client, each line
containing up to four fields:
.Bd -literal -offset indent
name key [hisaddr [label]]
.Ed
.Pp
The
.Ar name
and
.Ar key
specify the client as expected.  If
.Ar key
is
.Dq \&*
and PAP is being used,
.Nm
will look up the password database
.Pq Xr passwd 5
when authenticating.  If the client does not offer a suitable
response based on any
.Ar name No / Ar key
combination in
.Pa ppp.secret ,
authentication fails.
.Pp
If authentication is successful,
.Ar hisaddr
.Pq if specified
is used when negotiating IP numbers.  See the
.Dq set ifaddr
command for details.
.Pp
If authentication is successful and
.Ar label
is specified, the current system label is changed to match the given
.Ar label .
This will change the subsequent parsing of the
.Pa ppp.linkup
and
.Pa ppp.linkdown
files.
.Sh PPP OVER TCP (a.k.a Tunnelling)
Instead of running
.Nm
over a serial link, it is possible to
use a TCP connection instead by specifying a host and port as the
device:
.Dl set device ui-gate:6669
Instead of opening a serial device,
.Nm
will open a TCP connection to the given machine on the given
socket.  It should be noted however that
.Nm
doesn't use the telnet protocol and will be unable to negotiate
with a telnet server.  You should set up a port for receiving this
.Em PPP
connection on the receiving machine (ui-gate).  This is
done by first updating
.Pa /etc/services
to name the service:
.Dl ppp-in 6669/tcp # Incoming PPP connections over TCP
and updating
.Pa /etc/inetd.conf
to tell
.Xr inetd 8
how to deal with incoming connections on that port:
.Dl ppp-in stream tcp nowait root /usr/sbin/ppp ppp -direct ppp-in
Don't forget to send a
.Dv HUP
signal to
.Xr inetd 8
after you've updated
.Pa /etc/inetd.conf .
Here, we use a label named
.Dq ppp-in .
The entry in
.Pa /etc/ppp/ppp.conf
on ui-gate (the receiver) should contain the following:
.Bd -literal -offset indent
ppp-in:
 set timeout 0
 set ifaddr 10.0.4.1 10.0.4.2
 add 10.0.1.0/24 10.0.4.2
.Ed
.Pp
You may also want to enable PAP or CHAP for security.  To enable PAP, add
the following line:
.Bd -literal -offset indent
 enable PAP
.Ed
.Pp
You'll also need to create the following entry in
.Pa /etc/ppp/ppp.secret :
.Bd -literal -offset indent
MyAuthName MyAuthPasswd
.Ed
.Pp
If
.Ar MyAuthPasswd
is a
.Pq Dq * ,
the password is looked up in the
.Xr passwd 5
database.
.Pp
The entry in
.Pa /etc/ppp/ppp.conf
on awfulhak (the initiator) should contain the following:
.Bd -literal -offset indent
ui-gate:
 set escape 0xff
 set device ui-gate:ppp-in
 set dial
 set timeout 30
 set log Phase Chat Connect hdlc LCP IPCP CCP tun
 set ifaddr 10.0.4.2 10.0.4.1
 add 10.0.2.0/24 10.0.4.1
.Ed
.Pp
Again, if you're enabling PAP, you'll also need:
.Bd -literal -offset indent
 set authname MyAuthName
 set authkey MyAuthKey
.Ed
.Pp
We're assigning the address of 10.0.4.1 to ui-gate, and the address
10.0.4.2 to awfulhak.
To open the connection, just type
.Dl awfulhak # ppp -background ui-gate
The result will be an additional "route" on awfulhak to the
10.0.2.0/24 network via the TCP connection, and an additional
"route" on ui-gate to the 10.0.1.0/24 network.
The networks are effectively bridged - the underlying TCP
connection may be across a public network (such as the
Internet), and the
.Em PPP
traffic is conceptually encapsulated
(although not packet by packet) inside the TCP stream between
the two gateways.
The major disadvantage of this mechanism is that there are two
"guaranteed delivery" mechanisms in place - the underlying TCP
stream and whatever protocol is used over the
.Em PPP
link - probably TCP again.  If packets are lost, both levels will
get in each others way trying to negotiate sending of the missing
packet.
.Sh PACKET ALIASING
The
.Fl alias
command line option enables packet aliasing.  This allows the
.Nm
host to act as a masquerading gateway for other computers over
a local area network.  Outgoing IP packets are aliased so that
they appear to come from the
.Nm
host, and incoming packets are de-aliased so that they are routed
to the correct machine on the local area network.
Packet aliasing allows computers on private, unregistered
subnets to have Internet access, although they are invisible
from the outside world.
In general, correct
.Nm
operation should first be verified with packet aliasing disabled.
Then, the 
.Fl alias
option should be switched on, and network applications (web browser,
.Xr telnet 1 ,
.Xr ftp 1 ,
.Xr ping 8 ,
.Xr traceroute 8 )
should be checked on the
.Nm
host.  Finally, the same or similar applications should be checked on other
computers in the LAN.
If network applications work correctly on the
.Nm
host, but not on other machines in the LAN, then the masquerading
software is working properly, but the host is either not forwarding
or possibly receiving IP packets.  Check that IP forwarding is enabled in
.Pa /etc/rc.conf
and that other machines have designated the
.Nm
host as the gateway for the LAN.
.Sh PACKET FILTERING
This implementation supports packet filtering. There are four kinds of
filters; the
.Em in
filter, the
.Em out
filter, the
.Em dial
filter and the
.Em alive
filter.  Here are the basics:
.Bl -bullet
.It
A filter definition has the following syntax:
.Pp
set filter
.Ar name
.Ar rule-no
.Ar action
.Op Ar src_addr Ns Op / Ns Ar width
.Op Ar dst_addr Ns Op / Ns Ar width
[
.Ar proto
.Op src Op Ar cmp No Ar port
.Op dst Op Ar cmp No Ar port
.Op estab
.Op syn
.Op finrst
]
.Bl -enum
.It
.Ar Name
should be one of
.Sq in ,
.Sq out ,
.Sq dial
or
.Sq alive .
.It
.Ar Rule-no
is a numeric value between
.Sq 0
and
.Sq 19
specifying the rule number.  Rules are specified in numeric order according to
.Ar rule-no ,
but only if rule
.Sq 0
is defined.
.It
.Ar Action
is either
.Sq permit
or
.Sq deny .
If a given packet
matches the rule, the associated action is taken immediately.
.It
.Op Ar src_addr Ns Op / Ns Ar width
and
.Op Ar dst_addr Ns Op / Ns Ar width
are the source and destination IP number specifications.  If
.Op / Ns Ar width
is specified, it gives the number of relevant netmask bits,
allowing the specification of an address range.
.It
.Ar Proto
must be one of
.Sq icmp ,
.Sq udp
or
.Sq tcp .
.It
.Ar Cmp
is one of
.Sq \&lt ,
.Sq \&eq
or
.Sq \&gt ,
meaning less-than, equal and greater-than respectively.
.Ar Port
can be specified as a numeric port or by service name from
.Pa /etc/services .
.It
The
.Sq estab ,
.Sq syn ,
and
.Sq finrst
flags are only allowed when
.Ar proto
is set to
.Sq tcp ,
and represent the TH_ACK, TH_SYN and TH_FIN or TH_RST TCP flags respectively.
.El
.Pp
.It
Each filter can hold up to 20 rules, starting from rule 0.
The entire rule set is not effective until rule 0 is defined,
ie. the default is to allow everything through.
.It
If no rule is matched to a packet, that packet will be discarded
(blocked).
.It
Use
.Dq set filter Ar name No -1
to flush all rules.
.El
.Pp
See
.Pa /etc/ppp/ppp.conf.example .
.Sh SETTING THE IDLE TIMER
To check/set the idle timer, use the
.Dq show bundle
and
.Dq set timeout
commands:
.Bd -literal -offset indent
ppp ON awfulhak> set timeout 600
.Ed
.Pp
The timeout period is measured in seconds, the  default value for which
is 180 seconds
.Pq or 3 min .
To disable the idle timer function, use the command
.Bd -literal -offset indent
ppp ON awfulhak> set timeout 0
.Ed
.Pp
In
.Fl ddial
and
.Fl direct
modes, the idle timeout is ignored.  In
.Fl auto
mode, when the idle timeout causes the
.Em PPP
session to be
closed, the
.Nm
program itself remains running.  Another trigger packet will cause it to
attempt to re-establish the link.
.Sh PREDICTOR-1 and DEFLATE COMPRESSION
.Nm Ppp
supports both Predictor type 1 and deflate compression.
By default,
.Nm
will attempt to use (or be willing to accept) both compression protocols
when the peer agrees
.Pq or requests them .
The deflate protocol is preferred by
.Nm ppp .
Refer to the
.Dq disable
and
.Dq deny
commands if you wish to disable this functionality.
.Pp
It is possible to use a different compression algorithm in each direction
by using only one of
.Dq disable deflate
and
.Dq deny deflate
.Pq assuming that the peer supports both algorithms .
.Pp
By default, when negotiating DEFLATE,
.Nm
will use a window size of 15.  Refer to the
.Dq set deflate
command if you wish to change this behaviour.
.Pp
A special algorithm called DEFLATE24 is also available, and is disabled
and denied by default.  This is exactly the same as DEFLATE except that
it uses CCP ID 24 to negotiate.  This allows
.Nm
to successfully negotiate DEFLATE with
.Nm pppd
version 2.3.*.
.Sh CONTROLLING IP ADDRESS
.Nm
uses IPCP to negotiate IP addresses. Each side of the connection
specifies the IP address that it's willing to use, and if the requested
IP address is acceptable then
.Nm
returns ACK to the requester.  Otherwise,
.Nm
returns NAK to suggest that the peer use a different IP address. When
both sides of the connection agree to accept the received request (and
send ACK), IPCP is set to the open state and a network level connection
is established.
To control this IPCP behaviour, this implementation has the
.Dq set ifaddr
command for defining the local and remote IP address:
.Bd -literal -offset indent
set ifaddr [src_addr [dst_addr [netmask [trigger_addr]]]]
.Ed
.Pp
where,
.Sq src_addr
is the IP address that the local side is willing to use,
.Sq dst_addr
is the IP address which the remote side should use and
.Sq netmask
is the netmask that should be used.
.Sq Src_addr
defaults to the current
.Xr hostname 1 ,
.Sq dst_addr
defaults to 0.0.0.0, and
.Sq netmask
defaults to whatever mask is appropriate for
.Sq src_addr .
It is only possible to make
.Sq netmask
smaller than the default.  The usual value is 255.255.255.255, as
most kernels ignore the netmask of a POINTOPOINT interface.
.Pp
Some incorrect
.Em PPP
implementations require that the peer negotiates a specific IP
address instead of
.Sq src_addr .
If this is the case,
.Sq trigger_addr
may be used to specify this IP number.  This will not affect the
routing table unless the other side agrees with this proposed number.
.Bd -literal -offset indent
set ifaddr 192.244.177.38 192.244.177.2 255.255.255.255 0.0.0.0
.Ed
.Pp
The above specification means:
.Pp
.Bl -bullet -compact
.It
I will first suggest that my IP address should be 0.0.0.0, but I
will only accept an address of 192.244.177.38.
.It
I strongly insist that the peer uses 192.244.177.2 as his own
address and won't permit the use of any IP address but 192.244.177.2.
When the peer requests another IP address, I will always suggest that
it uses 192.244.177.2.
.It
The routing table entry will have a netmask of 0xffffffff.
.El
.Pp
This is all fine when each side has a pre-determined IP address, however
it is often the case that one side is acting as a server which controls
all IP addresses and the other side should obey the direction from it.
In order to allow more flexible behaviour, `ifaddr' variable allows the
user to specify IP address more loosely:
.Pp
.Dl set ifaddr 192.244.177.38/24 192.244.177.2/20
.Pp
A number followed by a slash (/) represent the number of bits significant in
the IP address.  The above example signifies that:
.Pp
.Bl -bullet -compact
.It
I'd like to use 192.244.177.38 as my address if it is possible, but I'll
also accept any IP address between 192.244.177.0 and 192.244.177.255.
.It
I'd like to make him use 192.244.177.2 as his own address, but I'll also
permit him to use any IP address between 192.244.176.0 and
192.244.191.255.
.It
As you may have already noticed, 192.244.177.2 is equivalent to saying
192.244.177.2/32.
.It
As an exception, 0 is equivalent to 0.0.0.0/0, meaning that I have no
preferred IP address and will obey the remote peers selection.  When
using zero, no routing table entries will be made until a connection
is established.
.It
192.244.177.2/0 means that I'll accept/permit any IP address but I'll
try to insist that 192.244.177.2 be used first.
.El
.Pp
.Sh CONNECTING WITH YOUR INTERNET SERVICE PROVIDER
The following steps should be taken when connecting to your ISP:
.Bl -enum
.It
Describe your providers phone number(s) in the dial script using the
.Dq set phone
command.  This command allows you to set multiple phone numbers for
dialing and redialing separated by either a pipe (|) or a colon (:)
.Bd -literal -offset indent
set phone "111[|222]...[:333[|444]...]...
.Ed
.Pp
Numbers after the first in a pipe-separated list are only used if the
previous number was used in a failed dial or login script.  Numbers
separated by a colon are used sequentially, irrespective of what happened
as a result of using the previous number.  For example:
.Bd -literal -offset indent
set phone "1234567|2345678:3456789|4567890"
.Ed
.Pp
Here, the 1234567 number is attempted.  If the dial or login script fails,
the 2345678 number is used next time, but *only* if the dial or login script
fails.  On the dial after this, the 3456789 number is used.  The 4567890
number is only used if the dial or login script using the 3456789 fails.  If
the login script of the 2345678 number fails, the next number is still the
3456789 number.  As many pipes and colons can be used as are necessary
(although a given site would usually prefer to use either the pipe or the
colon, but not both).  The next number redial timeout is used between all
numbers.  When the end of the list is reached, the normal redial period is
used before starting at the beginning again.
The selected phone number is substituted for the \\\\T string in the
.Dq set dial
command (see below).
.It
Set up your redial requirements using
.Dq set redial .
For example, if you have a bad telephone line or your provider is
usually engaged (not so common these days), you may want to specify
the following:
.Bd -literal -offset indent
set redial 10 4
.Ed
.Pp
This says that up to 4 phone calls should be attempted with a pause of 10
seconds before dialing the first number again.
.It
Describe your login procedure using the
.Dq set dial
and
.Dq set login
commands.  The
.Dq set dial
command is used to talk to your modem and establish a link with your
ISP, for example:
.Bd -literal -offset indent
set dial "ABORT BUSY ABORT NO\\\\sCARRIER TIMEOUT 4 \\"\\" \e
  ATZ OK-ATZ-OK ATDT\\\\T TIMEOUT 60 CONNECT"
.Ed
.Pp
This modem "chat" string means:
.Bl -bullet
.It
Abort if the string "BUSY" or "NO CARRIER" are received.
.It
Set the timeout to 4 seconds.
.It
Expect nothing.
.It
Send ATZ.
.It
Expect OK.  If that's not received within the 4 second timeout, send ATZ
and expect OK.
.It
Send ATDTxxxxxxx where xxxxxxx is the next number in the phone list from
above.
.It
Set the timeout to 60.
.It
Wait for the CONNECT string.
.El
.Pp
Once the connection is established, the login script is executed.  This
script is written in the same style as the dial script, but care should
be taken to avoid having your password logged:
.Bd -literal -offset indent
set authkey MySecret
set login "TIMEOUT 15 login:-\\\\r-login: awfulhak \e
  word: \\\\P ocol: PPP HELLO"
.Ed
.Pp
This login "chat" string means:
.Bl -bullet
.It
Set the timeout to 15 seconds.
.It
Expect "login:".  If it's not received, send a carriage return and expect
"login:" again.
.It
Send "awfulhak"
.It
Expect "word:" (the tail end of a "Password:" prompt).
.It
Send whatever our current
.Ar authkey
value is set to.
.It
Expect "ocol:" (the tail end of a "Protocol:" prompt).
.It
Send "PPP".
.It
Expect "HELLO".
.El
.Pp
The
.Dq set authkey
command is logged specially (when using
.Ar command
logging) so that the actual password is not compromised
(it is logged as
.Sq ******** Ns
), and the '\\P' is logged when
.Ar chat
logging is active rather than the actual password.
.Pp
Login scripts vary greatly between ISPs.
.It
Use
.Dq set line
and
.Dq set speed
to specify your serial line and speed, for example:
.Bd -literal -offset indent
set line /dev/cuaa0
set speed 115200
.Ed
.Pp
Cuaa0 is the first serial port on FreeBSD.  If you're running
.Nm
on OpenBSD, cua00 is the first.  A speed of 115200 should be specified
if you have a modem capable of bit rates of 28800 or more.  In general,
the serial speed should be about four times the modem speed.
.It
Use the
.Dq set ifaddr
command to define the IP address.
.Bl -bullet
.It
If you know what IP address your provider uses, then use it as the remote
address (dst_addr), otherwise choose something like 10.0.0.2/0 (see below).
.It
If your provider has assigned a particular IP address to you, then use
it as your address (src_addr).
.It
If your provider assigns your address dynamically, choose a suitably
unobtrusive and unspecific IP number as your address.  10.0.0.1/0 would
be appropriate.  The bit after the / specifies how many bits of the
address you consider to be important, so if you wanted to insist on
something in the class C network 1.2.3.0, you could specify 1.2.3.1/24.
.It
If you find that your ISP accepts the first IP number that you suggest,
specify third and forth arguments of
.Dq 0.0.0.0 .
This will force your ISP to assign a number.  (The third argument will
be ignored as it is less restrictive than the default mask for your
.Sq src_addr .
.El
.Pp
An example for a connection where you don't know your IP number or your
ISPs IP number would be:
.Bd -literal -offset indent
set ifaddr 10.0.0.1/0 10.0.0.2/0 0.0.0.0 0.0.0.0
.Ed
.Pp
.It
In most cases, your ISP will also be your default router.  If this is
the case, add the line
.Bd -literal -offset indent
add default HISADDR
.Ed
.Pp
to
.Pa /etc/ppp/ppp.conf .
.Pp
This tells
.Nm
to add a default route to whatever the peer address is
.Pq 10.0.0.2 in this example .
This route is
.Sq sticky ,
meaning that should the value of
.Dv HISADDR
change, the route will be updated accordingly.
.Pp
Previous versions of
.Nm
required a similar entry in the
.Pa /etc/ppp/ppp.linkup
file.  Since the advent of
.Sq sticky routes ,
his is no longer required.
.It
If your provider requests that you use PAP/CHAP authentication methods, add
the next lines to your
.Pa /etc/ppp/ppp.conf
file:
.Bd -literal -offset indent
set authname MyName
set authkey MyPassword
.Ed
.Pp
Both are accepted by default, so
.Nm
will provide whatever your ISP requires.
.El
.Pp
Please refer to
.Pa /etc/ppp/ppp.conf.sample
and
.Pa /etc/ppp/ppp.linkup.sample
for some real examples.  The pmdemand label should be appropriate for most
ISPs.
.Sh LOGGING FACILITY
.Nm
is able to generate the following log info either via
.Xr syslog 3
or directly to the screen:
.Bl -column SMMMMMM -offset indent
.It Li Async	Dump async level packet in hex
.It Li CCP	Generate a CCP packet trace
.It Li Chat	Generate Chat script trace log
.It Li Command	Log commands executed
.It Li Connect	Generate complete Chat log
.It Li Debug	Log debug information
.It Li HDLC	Dump HDLC packet in hex
.It Li ID0	Log all function calls specifically made as user id 0.
.It Li IPCP	Generate an IPCP packet trace
.It Li LCP	Generate an LCP packet trace
.It Li LQM	Generate LQR report
.It Li Phase	Phase transition log output
.It Li TCP/IP	Dump all TCP/IP packets
.It Li Timer	Log timer manipulation
.It Li TUN	Include the tun device on each log line
.It Li Warning	Output to the terminal device.  If there is currently no
terminal, output is sent to the log file using LOG_WARNING.
.It Li Error	Output to both the terminal device and the log file using
LOG_ERROR.
.It Li Alert	Output to the log file using LOG_ALERT
.El
.Pp
The
.Dq set log
command allows you to set the logging output level.  Multiple levels
can be specified on a single command line.  The default is equivalent to
.Dq set log Phase .
.Pp
It is also possible to log directly to the screen.  The syntax is
the same except that the word
.Dq local
should immediately follow
.Dq set log .
The default is
.Dq set log local
(ie. only the un-maskable warning, error and alert output).
.Pp
If The first argument to
.Dq set log Op local
begins with a '+' or a '-' character, the current log levels are
not cleared, for example:
.Bd -literal -offset indent
PPP ON awfulhak> set log phase
PPP ON awfulhak> show log
Log:   Phase Warning Error Alert
Local: Warning Error Alert
PPP ON awfulhak> set log +tcp/ip -warning
PPP ON awfulhak> set log local +command
PPP ON awfulhak> show log
Log:   Phase TCP/IP Warning Error Alert
Local: Command Warning Error Alert
.Ed
.Pp
Log messages of level Warning, Error and Alert are not controllable
using
.Dq set log Op local .
.Pp
The
.Ar Warning
level is special in that it will not be logged if it can be displayed
locally.
.Sh SIGNAL HANDLING
.Nm Ppp
deals with the following signals:
.Bl -tag -width 20
.It INT
Receipt of this signal causes the termination of the current connection
(if any).  This will cause
.Nm
to exit unless it is in
.Fl auto
or
.Fl ddial
mode.
.It HUP, TERM & QUIT
These signals tell
.Nm
to exit.
.It USR2
This signal, tells
.Nm
to close any existing server socket, dropping all existing diagnostic
connections.
.El
.Pp
.Sh MULTI-LINK PPP
If you wish to use more than one physical link to connect to a
.Em PPP
peer, that peer must also understand the
.Em MULTI-LINK PPP
protocol.  Refer to RFC 1990 for specification details.
.Pp
The peer is identified using a combination of his
.Dq endpoint discriminator
and his
.Dq authentication id .
Either or both of these may be specified.  It is recommended that
at least one is specified, otherwise there is no way of ensuring that
all links are actually connected to the same peer program, and some
confusing lock-ups may result.  Locally, these identification variables
are specified using the
.Dq set enddisc
and
.Dq set authname
commands.  The
.Sq authname
.Pq and Sq authkey
must be agreed in advance with the peer.
.Pp
Multi-link capabilities are enabled using the
.Dq set mrru
command (set maximum reconstructed receive unit).  Once multi-link
is enabled,
.Nm
will attempt to negotiate a multi-link connection with the peer.
.Pp
By default, only one
.Sq link
is available
.Pq called Sq deflink .
To create more links, the
.Dq clone
command is used.  This command will clone existing links, where all
characteristics are the same except:
.Bl -enum
.It
The new link has its own name as specified on the
.Dq clone
command line.
.It
The new link is an
.Sq interactive
link.  It's mode may subsequently be changed using the
.Dq set mode
command.
.It
The new link is in a
.Sq closed
state.
.El
.Pp
A summary of all available links can be seen using the
.Dq show links
command.
.Pp
Once a new link has been created, command usage varies.  All link
specific commands must be prefixed with the
.Dq link Ar name
command, specifying on which link the command is to be applied.  When
only a single link is available,
.Nm
is smart enough not to require the
.Dq link Ar name
prefix.
.Pp
Some commands can still be used without specifying a link - resulting
in an operation at the
.Sq bundle
level.  For example, once two or more links are available, the command
.Dq show ccp
will show CCP configuration and statistics at the multi-link level, and
.Dq link deflink show ccp
will show the same information at the
.Dq deflink
link level.
.Pp
Armed with this information, the following configuration might be used:
.Pp
.Bd -literal -offset indent
mp:
 set timeout 0
 set log phase chat
 set device /dev/cuaa0 /dev/cuaa1 /dev/cuaa2
 set phone "123456789"
 set dial "ABORT BUSY ABORT NO\\sCARRIER TIMEOUT 5 \\"\\" ATZ \e
           OK-AT-OK \\\\dATDT\\\\T TIMEOUT 45 CONNECT"
 set login
 set ifaddr 10.0.0.1/0 10.0.0.2/0
 set authname ppp
 set authkey ppppassword

 set mrru 1500
 clone 1,2,3
 link deflink remove
.Ed
.Pp
Note how all cloning is done at the end of the configuration.  Usually,
the link will be configured first, then cloned.  If you wish all links
to be up all the time, you can add the following line to the end of your
configuration.
.Pp
.Bd -literal -offset indent
  link 1,2,3 set mode ddial
.Ed
.Pp
If you want the links to dial on demand, this command could be used:
.Pp
.Bd -literal -offset indent
  link * set mode auto
.Ed
.Pp
Links may be tied to specific names by removing the
.Dq set device
line above, and specifying the following after the
.Dq clone
command:
.Pp
.Bd -literal -offset indent
 link 1 set device /dev/cuaa0
 link 2 set device /dev/cuaa1
 link 3 set device /dev/cuaa2
.Ed
.Pp
Use the
.Dq help
command to see which commands require context (using the
.Dq link
command), which have optional
context and which should not have any context.
.Pp
When
.Nm
has negotiated
.Em MULTI-LINK
mode with the peer, it creates a local domain socket in the
.Pa /var/run
directory.  This socket is used to pass link information (including
the actual link file descriptor) between different
.Nm
invocations.  This facilitates
.Nm ppp Ns No s
ability to be run from a
.Xr getty 8
or directly from
.Pa /etc/gettydefs
(using the
.Sq pp=
capability), without needing to have initial control of the serial
line.  Once
.Nm
negotiates multi-link mode, it will pass its open link to any
already running process.  If there is no already running process,
.Nm
will act as the master, creating the socket and listening for new
connections.
.Sh PPP COMMAND LIST
This section lists the available commands and their effect.  They are
usable either from an interactive
.Nm
session, from a configuration file or from a
.Xr pppctl 8
or
.Xr telnet 1
session.
.Bl -tag -width 20
.It accept|deny|enable|disable Ar option....
These directives tell
.Nm
how to negotiate the initial connection with the peer.  Each
.Dq option
has a default of either accept or deny and enable or disable.
.Dq Accept
means that the option will be ACK'd if the peer asks for it.
.Dq Deny
means that the option will be NAK'd if the peer asks for it.
.Dq Enable
means that the option will be requested by us.
.Dq Disable
means that the option will not be requested by us.
.Pp
.Dq Option
may be one of the following:
.Bl -tag -width 20
.It acfcomp
Default: Enabled and Accepted.  ACFComp stands for Address and Control
Field Compression.  Non LCP packets usually have very similar address
and control fields - making them easily compressible.
.It chap
Default: Disabled and Accepted.  CHAP stands for Challenge Handshake
Authentication Protocol.  Only one of CHAP and PAP (below) may be
negotiated.  With CHAP, the authenticator sends a "challenge" message
to its peer.  The peer uses a one-way hash function to encrypt the
challenge and sends the result back.  The authenticator does the same,
and compares the results.  The advantage of this mechanism is that no
passwords are sent across the connection.
A challenge is made when the connection is first made.  Subsequent
challenges may occur.  If you want to have your peer authenticate
itself, you must
.Dq enable chap .
in
.Pa /etc/ppp/ppp.conf ,
and have an entry in
.Pa /etc/ppp/ppp.secret
for the peer.
.Pp
When using CHAP as the client, you need only specify
.Dq AuthName
and
.Dq AuthKey
in
.Pa /etc/ppp/ppp.conf .
CHAP is accepted by default.
Some
.Em PPP
implementations use "MS-CHAP" rather than MD5 when encrypting the
challenge.  MS-CHAP is a combination of MD4 and DES.  If
.Nm
was built on a machine with DES libraries available, it will respond
to MS-CHAP authentication requests, but will never request them.
.It deflate
Default: Enabled and Accepted.  This option decides if deflate
compression will be used by the Compression Control Protocol (CCP).
This is the same algorithm as used by the
.Xr gzip 1
program.
Note: There is a problem negotiating
.Ar deflate
capabilities with
.Xr pppd 8
- a
.Em PPP
implementation available under many operating systems.
.Nm Pppd
(version 2.3.1) incorrectly attempts to negotiate
.Ar deflate
compression using type
.Em 24
as the CCP configuration type rather than type
.Em 26
as specified in
.Pa rfc1979 .
Type
.Ar 24
is actually specified as
.Dq PPP Magna-link Variable Resource Compression
in
.Pa rfc1975 Ns No !
.Nm Ppp
is capable of negotiating with
.Nm pppd ,
but only if
.Dq deflate24
is
.Ar enable Ns No d
and
.Ar accept Ns No ed .
.It deflate24
Default: Disabled and Denied.  This is a variance of the
.Ar deflate
option, allowing negotiation with the
.Xr pppd 8
program.  Refer to the
.Ar deflate
section above for details.  It is disabled by default as it violates
.Pa rfc1975 .
.It dns
Default: Enabled and Denied.  This option allows DNS negotiation.
.Pp
If
.Dq enable Ns No d,
.Nm
will request that the peer confirms the entries in
.Pa /etc/resolv.conf .
If the peer NAKs our request (suggesting new IP numbers),
.Pa /etc/resolv.conf
is updated and another request is sent to confirm the new entries.
.Pp
If
.Dq accept Ns No ed,
.Nm
will answer any DNS queries requested by the peer rather than rejecting
them.  The answer is taken from
.Pa /etc/resolv.conf
unless the
.Dq set dns
command is used as an override.
.It lqr
Default: Disabled and Accepted.  This option decides if Link Quality
Requests will be sent or accepted.  LQR is a protocol that allows
.Nm
to determine that the link is down without relying on the modems
carrier detect.  When LQR is enabled,
.Nm
sends the
.Em QUALPROTO
option (see
.Dq set lqrperiod
below) as part of the LCP request.  If the peer agrees, both sides will
exchange LQR packets at the agreed frequency, allowing detailed link
quality monitoring by enabling LQM logging.  If the peer doesn't agree,
ppp will send ECHO LQR requests instead.  These packets pass no
information of interest, but they
.Em MUST
be replied to by the peer.
.Pp
Whether using LQR or ECHO LQR,
.Nm
will abruptly drop the connection if 5 unacknowledged packets have been
sent rather than sending a 6th.  A message is logged at the
.Em PHASE
level, and any appropriate
.Dq reconnect
values are honoured as if the peer were responsible for dropping the
connection.
.It pap
Default: Disabled and Accepted.  PAP stands for Password Authentication
Protocol.  Only one of PAP and CHAP (above) may be negotiated.  With
PAP, the ID and Password are sent repeatedly to the peer until
authentication is acknowledged or the connection is terminated.  This
is a rather poor security mechanism.  It is only performed when the
connection is first established.
If you want to have your peer authenticate itself, you must
.Dq enable pap .
in
.Pa /etc/ppp/ppp.conf ,
and have an entry in
.Pa /etc/ppp/ppp.secret
for the peer (although see the
.Dq passwdauth
option below).
.Pp
When using PAP as the client, you need only specify
.Dq AuthName
and
.Dq AuthKey
in
.Pa /etc/ppp/ppp.conf .
PAP is accepted by default.
.It pred1
Default: Enabled and Accepted.  This option decides if Predictor 1
compression will be used by the Compression Control Protocol (CCP).
.It protocomp
Default: Enabled and Accepted.  This option is used to negotiate
PFC (Protocol Field Compression), a mechanism where the protocol
field number is reduced to one octet rather than two.
.It shortseq
Default: Enabled and Accepted.  This option determines if
.Nm
will request and accept requests for short
.Pq 12 bit
sequence numbers when negotiating multi-link mode.  This is only
applicable if our MRRU is set (thus enabling multi-link).
.It vjcomp
Default: Enabled and Accepted.  This option determines if Van Jacobson
header compression will be used.
.El
.Pp
The following options are not actually negotiated with the peer.
Therefore, accepting or denying them makes no sense.
.Bl -tag -width 20
.It idcheck
Default: Enabled.  When
.Nm
exchanges low-level LCP, CCP and IPCP configuration traffic, the
.Em Identifier
field of any replies is expected to be the same as that of the request.
By default,
.Nm
drops any reply packets that do not contain the expected identifier
field, reporting the fact at the respective log level.  If
.Ar idcheck
is disabled,
.Nm
will ignore the identifier field.
.It loopback
Default: Enabled.  When
.Ar loopback
is enabled,
.Nm
will automatically loop back packets being sent
out with a destination address equal to that of the
.Em PPP
interface.  If disabled,
.Nm
will send the packet, probably resulting in an ICMP redirect from
the other end.  It is convenient to have this option enabled when
the interface is also the default route as it avoids the necessity
of a loopback route.
.It passwdauth
Default: Disabled.  Enabling this option will tell the PAP authentication
code to use the password database (see
.Xr passwd 5 )
to authenticate the caller if they cannot be found in the
.Pa /etc/ppp/ppp.secret
file.
.Pa /etc/ppp/ppp.secret
is always checked first.  If you wish to use passwords from
.Xr passwd 5 ,
but also to specify an IP number or label for a given client, use
.Dq \&*
as the client password in
.Pa /etc/ppp/ppp.secret .
.It proxy
Default: Disabled.  Enabling this option will tell
.Nm
to proxy ARP for the peer.
.It sroutes
Default: Enabled.  When the
.Dq add
command is used with the
.Dv HISADDR
or
.Dv MYADDR
values, entries are stored in the
.Sq stick route
list.  Each time
.Dv HISADDR
or
.Dv MYADDR
change, this list is re-applied to the routing table.
.Pp
Disabling this option will prevent the re-application of sticky routes,
although the
.Sq stick route
list will still be maintained.
.It throughput
Default: Enabled.  This option tells
.Nm
to gather throughput statistics.  Input and output is sampled over
a rolling 5 second window, and current, best and total figures are
retained.  This data is output when the relevant
.Em PPP
layer shuts down, and is also available using the
.Dq show
command.  Throughput statistics are available at the
.Dq IPCP
and 
.Dq modem
levels.
.It utmp
Default: Enabled.  Normally, when a user is authenticated using PAP or
CHAP, and when
.Nm
is running in
.Fl direct
mode, an entry is made in the utmp and wtmp files for that user.  Disabling
this option will tell
.Nm
not to make any utmp or wtmp entries.  This is usually only necessary if
you require the user to both login and authenticate themselves.
.El
.Pp
.It add[!] Ar dest[/nn] [mask] gateway
.Ar Dest
is the destination IP address.  The netmask is specified either as a
number of bits with
.Ar /nn
or as an IP number using
.Ar mask .
.Ar 0 0
or simply
.Ar 0
with no mask refers to the default route.  It is also possible to use the
literal name
.Sq default
instead of
.Ar 0 .
.Ar Gateway
is the next hop gateway to get to the given
.Ar dest
machine/network.  Refer to the
.Xr route 8
command for further details.
.Pp
It is possible to use the symbolic names
.Sq MYADDR
or
.Sq HISADDR
as the destination, and either
.Sq HISADDR
or
.Sq INTERFACE
as the
.Ar gateway .
.Sq MYADDR
is replaced with the interface address,
.Sq HISADDR
is replaced with the interface destination address and
.Sq INTERFACE
is replaced with the current interface name.  If the interfaces destination
address has not yet been assigned
.Pq via Dq set ifaddr ,
the current
.Sq INTERFACE
is used instead of
.Sq HISADDR .
.Pp
If the
.Ar add!
command is used
.Pq note the trailing Dq \&! ,
then if the route already exists, it will be updated as with the
.Sq route change
command (see
.Xr route 8
for further details).
.Pp
Routes that contain the
.Dq HISADDR
or
.Dq MYADDR
constants are considered
.Sq sticky .
They are stored in a list (use
.Dq show ipcp
to see the list), and each time the value of
.Dv HISADDR
or
.Dv MYADDR
changes, the appropriate routing table entries are updated.  This facility
may be disabled using
.Dq disable sroutes .
.It allow Ar command Op Ar args
This command controls access to
.Nm
and its configuration files.  It is possible to allow user-level access,
depending on the configuration file label and on the mode that
.Nm
is being run in.  For example, you may wish to configure
.Nm
so that only user
.Sq fred
may access label
.Sq fredlabel
in
.Fl background
mode.
.Pp
User id 0 is immune to these commands.
.Bl -tag -width 20
.It allow user[s] Ar logname...
By default, only user id 0 is allowed access to
.Nm ppp .
If this command is used, all of the listed users are allowed access to
the section in which the
.Dq allow users
command is found.  The
.Sq default
section is always checked first (even though it is only ever automatically
loaded at startup).  Each successive
.Dq allow users
command overrides the previous one, so it's possible to allow users access
to everything except a given label by specifying default users in the
.Sq default
section, and then specifying a new user list for that label.
.Pp
If user
.Sq *
is specified, access is allowed to all users.
.It allow mode[s] Ar modelist...
By default, access using any
.Nm
mode is possible.  If this command is used, it restricts the access
mode allowed to load the label under which this command is specified.
Again, as with the
.Dq allow users
command, each
.Dq allow modes
command overrides the previous, and the
.Sq default
section is always checked first.
.Pp
Possible modes are:
.Sq interactive ,
.Sq auto ,
.Sq direct ,
.Sq dedicated ,
.Sq ddial ,
.Sq background
and
.Sq * .
.Pp
When running in multi-link mode, a section can be loaded if it allows
.Em any
of the currently existing line modes.
.El
.Pp
.It alias Ar command Op Ar args
This command allows the control of the aliasing (or masquerading)
facilities that are built into
.Nm ppp .
Until this code is required, it is not loaded by
.Nm ppp ,
and it is quite possible that the alias library is not installed
on your system (some administrators consider it a security risk).
If aliasing is enabled on your system, the following commands are
possible:
.Bl -tag -width 20
.It alias enable [yes|no]
This command either switches aliasing on or turns it off.
The
.Fl alias
command line flag is synonymous with
.Dq alias enable yes .
.It alias port Op Ar proto targetIP:targetPORT [aliasIP:]aliasPORT
This command allows us to redirect connections arriving at
.Ar aliasPORT
for machine
.Ar aliasIP
to
.Ar targetPORT
on
.Ar targetIP .
.Ar Proto
may be either
.Sq tcp
or
.Sq udp ,
and only connections of the given protocol
are matched.  This option is useful if you wish to run things like
Internet phone on the machines behind your gateway.
.It alias addr Op Ar addr_local addr_alias
This command allows data for
.Ar addr_alias
to be redirected to
.Ar addr_local .
It is useful if you own a small number of real IP numbers that
you wish to map to specific machines behind your gateway.
.It alias deny_incoming [yes|no]
If set to yes, this command will refuse all incoming connections
by dropping the packets in much the same way as a firewall would.
.It alias help|?
This command gives a summary of available alias commands.
.It alias log [yes|no]
This option causes various aliasing statistics and information to
be logged to the file
.Pa /var/log/alias.log .
.It alias same_ports [yes|no]
When enabled, this command will tell the alias library attempt to
avoid changing the port number on outgoing packets.  This is useful
if you want to support protocols such as RPC and LPD which require
connections to come from a well known port.
.It alias use_sockets [yes|no]
When enabled, this option tells the alias library to create a
socket so that it can guarantee a correct incoming ftp data or
IRC connection.
.It alias unregistered_only [yes|no]
Only alter outgoing packets with an unregistered source ad-
dress.  According to RFC 1918, unregistered source addresses
are 10.0.0.0/8, 172.16.0.0/12 and 192.168.0.0/16.
.El
.Pp
These commands are also discussed in the file
.Pa README.alias
which comes with the source distribution.
.Pp
.It [!]bg Ar command
The given
.Ar command
is executed in the background.  Any of the pseudo arguments
.Dv HISADDR ,
.Dv INTERFACE
and
.Dv MYADDR
will be replaced with the appropriate values.  If you wish to pause
.Nm
while the command executes, use the
.Dv shell
command instead.
.It clear modem|ipcp Op current|overall|peak...
Clear the specified throughput values at either the
.Dq modem
or
.Dq ipcp
level.  If
.Dq modem
is specified, context must be given (see the
.Dq link
command below).  If no second argument is given, all values are
cleared.
.It clone Ar name[,name]...
Clone the specified link, creating one or more new links according to the
.Ar name
argument(s).  This command must be used from the
.Dq link
command below unless you've only got a single link (in which case that
link  becomes the default).  Links may be removed using the
.Dq remove
command below.
.Pp
The default link name is
.Dq deflink .
.It close Op lcp|ccp[!]
If no arguments are given, the relevant protocol layers will be brought
down and the link will be closed.  If
.Dq lcp
is specified, the LCP layer is brought down, but
.Nm
will not bring the link offline.  It is subsequently possible to use
.Dq term
.Pq see below
to talk to the peer machine if, for example, something like
.Dq slirp
is being used.  If
.Dq ccp
is specified, only the relevant compression layer is closed.  If the
.Dq \&!
is used, the compression layer will remain in the closed state, otherwise
it will re-enter the STOPPED state, waiting for the peer to initiate
further CCP negotiation.  In any event, this command does not disconnect
the user from
.Nm
or exit
.Nm ppp .
See the
.Dq quit
command below.
.It delete[!] Ar dest
This command deletes the route with the given
.Ar dest
IP address.  If
.Ar dest
is specified as
.Sq ALL ,
all non-direct entries in the routing table for the current interface,
and all
.Sq sticky route
entries are deleted.  If
.Ar dest
is specified as
.Sq default ,
the default route is deleted.
.Pp
If the
.Ar delete!
command is used
.Pq note the trailing Dq \&! ,
.Nm
will not complain if the route does not already exist.
.It dial|call Op Ar label
If
.Ar label
is specified, a connection is established using the
.Dq dial
and
.Dq login
scripts for the given
.Ar label .
Otherwise, the current settings are used to establish
the connection, and all closed links are brought up.
.It down Op Ar lcp|ccp
Bring the relevant layer down ungracefully, as if the underlying layer
had become unavailable.  It's not considered polite to use this command on
a Finite State Machine that's in the OPEN state.  If no arguments are
supplied,
.Sq lcp
is assumed.
.It help|? Op Ar command
Show a list of available commands.  If
.Ar command
is specified, show the usage string for that command.
.It [data]link Ar name[,name...] command Op Ar args
This command may prefix any other command if the user wishes to
specify which link the command should affect.  This is only
applicable after multiple links have been created in Multi-link
mode using the
.Dq clone
command.
.Pp
.Ar Name
specifies the name of an existing link.  If
.Ar name
is a comma separated list,
.Ar command
is executed on each link.  If
.Ar name
is
.Dq * ,
.Ar command
is executed on all links.
.It load Op Ar label
Load the given
.Ar label
from the
.Pa ppp.conf
file.  If
.Ar label
is not given, the
.Ar default
label is used.
.It open Op lcp|ccp
This is the opposite of the
.Dq close
command.  Using
.Dq open
with no arguments or with the
.Dq lcp
argument is the same as using
.Dq dial
in that all closed links are brought up.  If the
.Dq ccp
argument is used, the relevant compression layer is opened.
.It passwd Ar pass
Specify the password required for access to the full
.Nm
command set.  This password is required when connecting to the diagnostic
port (see the
.Dq set server
command).
.Ar Pass
is specified on the
.Dq set server
command line.  The value of
.Ar pass
is not logged when
.Ar command
logging is active, instead, the literal string
.Sq ********
is logged.
.It quit|bye [all]
If
.Dq quit
is executed from the controlling connection or from a command file,
ppp will exit after closing all connections.  Otherwise, if the user
is connected to a diagnostic socket, the connection is simply dropped.
.Pp
If the
.Ar
all argument is given,
.Nm
will exit despite the source of the command after closing all existing
connections.
.It remove|rm
This command removes the given link.  It is only really useful in
multi-link mode.  A link must be
in the
.Dv CLOSED
state before it is removed.
.It rename|mv Ar name
This command renames the given link to
.Ar name .
It will fail if
.Ar name
is already used by another link.
.Pp
The default link name is
.Sq deflink .
Renaming it to
.Sq modem ,
.Sq cuaa0
or
.Sq USR
may make the log file more readable.
.It save
This option is not (yet) implemented.
.It set[up] Ar var value
This option allows the setting of any of the following variables:
.Bl -tag -width 20
.It set accmap Ar hex-value
ACCMap stands for Asynchronous Control Character Map.  This is always
negotiated with the peer, and defaults to a value of 00000000 in hex.
This protocol is required to defeat hardware that depends on passing
certain characters from end to end (such as XON/XOFF etc).
.Pp
For the XON/XOFF scenario, use
.Dq set accmap 000a0000 .
.It set authkey|key Ar value
This sets the authentication key (or password) used in client mode
PAP or CHAP negotiation to the given value.  It can also be used to
specify the password to be used in the dial or login scripts in place
of the '\\P' sequence, preventing the actual password from being logged.  If
.Ar command
logging is in effect,
.Ar value
is logged as
.Sq ********
for security reasons.
.It set authname Ar id
This sets the authentication id used in client mode PAP or CHAP negotiation.
.It set autoload Ar max-duration max-load [min-duration min-load]
These settings apply only in multi-link mode and all default to zero.
When more than one
.Ar demand-dial
.Pq also known as Fl auto
mode link is available, only the first link is made active when
.Nm
first reads data from the tun device.  The next
.Ar demand-dial
link will be opened only when at least
.Ar max-load
packets have been in the send queue for
.Ar max-duration
seconds.  Because both values default to zero,
.Ar demand-dial
links will simply come up one at a time by default.
.Pp
If two or more links are open, at least one of which is a
.Ar demand-dial
link, a
.Ar demand-dial
link will be closed when there is less than
.Ar min-packets
in the queue for more than
.Ar min-duration .
If
.Ar min-duration
is zero, this timer is disabled.  Because both values default to zero,
.Ar demand-dial
links will stay active until the bundle idle timer expires.
.It set ctsrts|crtscts on|off
This sets hardware flow control.  Hardware flow control is
.Ar on
by default.
.It set deflate Ar out-winsize Op Ar in-winsize
This sets the DEFLATE algorithms default outgoing and incoming window
sizes.  Both
.Ar out-winsize
and
.Ar in-winsize
must be values between
.Em 8
and
.Em 15 .
If
.Ar in-winsize
is specified,
.Nm
will insist that this window size is used and will not accept any other
values from the peer.
.It set dns Op Ar primary Op Ar secondary
This command specifies DNS overrides for the
.Dq accept dns
command.  Refer to the
.Dq accept
command description above for details.  This command does not affect the
IP numbers requested using
.Dq enable dns .
.It set device|line Ar value[,value...]
This sets the device(s) to which
.Nm
will talk to the given
.Dq value .
All serial device names are expected to begin with
.Pa /dev/ .
If
.Dq value
does not begin with
.Pa /dev/ ,
it must either begin with an exclamation mark
.Pq Dq \&!
or be of the format
.Dq host:port .
.Pp
If it begins with an exclamation mark, the rest of the device name is
treated as a program name, and that program is executed when the device
is opened.  Standard input, output and error are fed back to
.Nm
and are read and written as if they were a regular device.
.Pp
If a
.Dq host:port
pair is given,
.Nm
will attempt to connect to the given
.Dq host
on the given
.Dq port .
Refer to the section on
.Em PPP OVER TCP
above for further details.
.Pp
If multiple
.Dq values
are specified,
.Nm
will attempt to open each one in turn until it succeeds or runs out of
devices.
.It set dial Ar chat-script
This specifies the chat script that will be used to dial the other
side.  See also the
.Dq set login
command below.  Refer to
.Xr chat 8
and to the example configuration files for details of the chat script
format.
It is possible to specify some special
.Sq values
in your chat script as follows:
.Bd -literal -offset indent
.It \\\\\\\\\\\\\\\\c
When used as the last character in a
.Sq send
string, this indicates that a newline should not be appended.
.It \\\\\\\\\\\\\\\\d
When the chat script encounters this sequence, it delays two seconds.
.It \\\\\\\\\\\\\\\\p
When the chat script encounters this sequence, it delays for one quarter of
a second.
.It \\\\\\\\\\\\\\\\n
This is replaced with a newline character.
.It \\\\\\\\\\\\\\\\r
This is replaced with a carriage return character.
.It \\\\\\\\\\\\\\\\s
This is replaced with a space character.
.It \\\\\\\\\\\\\\\\t
This is replaced with a tab character.
.It \\\\\\\\\\\\\\\\T
This is replaced by the current phone number (see
.Dq set phone
below).
.It \\\\\\\\\\\\\\\\P
This is replaced by the current
.Ar authkey
value (see
.Dq set authkey
above).
.It \\\\\\\\\\\\\\\\U
This is replaced by the current
.Ar authname
value (see
.Dq set authname
above).
.Ed
.Pp
Note that two parsers will examine these escape sequences, so in order to
have the
.Sq chat parser
see the escape character, it is necessary to escape it from the
.Sq command parser .
This means that in practice you should use two escapes, for example:
.Bd -literal -offset indent
set dial "... ATDT\\\\T CONNECT"
.Ed
.Pp
It is also possible to execute external commands from the chat script.
To do this, the first character of the expect or send string is an
exclamation mark
.Pq Dq \&! .
When the command is executed, standard input and standard output are
directed to the modem device (see the
.Dq set device
command), and standard error is read by
.Nm
and substituted as the expect or send string.  If
.Nm
is running in interactive mode, file descriptor 3 is attached to
.Pa /dev/tty .
.Pp
For example (wrapped for readability);
.Bd -literal -offset indent
set login "TIMEOUT 5 \\"\\" \\"\\" login:--login: ppp \e
word: ppp \\"!sh \\\\\\\\-c \\\\\\"echo \\\\\\\\-n label: >&2\\\\\\"\\" \e
\\"!/bin/echo in\\" HELLO"
.Ed
.Pp
would result in the following chat sequence (output using the
.Sq set log local chat
command before dialing):
.Bd -literal -offset indent
Dial attempt 1 of 1
dial OK!
Chat: Expecting: 
Chat: Sending: 
Chat: Expecting: login:--login:
Chat: Wait for (5): login:
Chat: Sending: ppp
Chat: Expecting: word:
Chat: Wait for (5): word:
Chat: Sending: ppp
Chat: Expecting: !sh \\-c "echo \\-n label: >&2"
Chat: Exec: sh -c "echo -n label: >&2"
Chat: Wait for (5): !sh \\-c "echo \\-n label: >&2" --> label:
Chat: Exec: /bin/echo in
Chat: Sending: 
Chat: Expecting: HELLO
Chat: Wait for (5): HELLO
login OK!
.Ed
.Pp
Note (again) the use of the escape character, allowing many levels of
nesting.  Here, there are four parsers at work.  The first parses the
original line, reading it as three arguments.  The second parses the
third argument, reading it as 11 arguments.  At this point, it is
important that the
.Dq \&-
signs are escaped, otherwise this parser will see them as constituting
an expect-send-expect sequence.  When the
.Dq \&!
character is seen, the execution parser reads the first command as three
arguments, and then
.Xr sh 1
itself expands the argument after the
.Fl c .
As we wish to send the output back to the modem, in the first example
we redirect our output to file descriptor 2 (stderr) so that
.Nm
itself sends and logs it, and in the second example, we just output to stdout,
which is attached directly to the modem.
.Pp
This, of course means that it is possible to execute an entirely external
.Dq chat
command rather than using the internal one.  See
.Xr chat 8
for a good alternative.
.It set enddisc Op label|IP|MAC|magic|psn value
This command sets our local endpoint discriminator.  If set prior to
LCP negotiation,
.Nm
will send the information to the peer using the LCP endpoint discriminator
option.  The following discriminators may be set:
.Bd -literal -offset indent
.It label
The current label is used.
.It IP
Our local IP number is used.  As LCP is negotiated prior to IPCP, it is
possible that the IPCP layer will subsequently change this value.  If
it does, the endpoint discriminator stays at the old value unless manually
reset.
.It MAC
This is similar to the
.Ar IP
option above, except that the MAC address associated with the local IP
number is used.  If the local IP number is not resident on any Ethernet
interface, the command will fail.
.Pp
As the local IP number defaults to whatever the machine host name is,
.Dq set enddisc mac
is usually done prior to any
.Dq set ifaddr
commands.
.It magic
A 20 digit random number is used.
.It psn Ar value
The given
.Ar value
is used.
.Ar Value
should be set to an absolute public switched network number with the
country code first.
.Ed
.Pp
If no arguments are given, the endpoint discriminator is reset.
.It set escape Ar value...
This option is similar to the
.Dq set accmap
option above.  It allows the user to specify a set of characters that
will be `escaped' as they travel across the link.
.It set filter dial|alive|in|out rule-no permit|deny Ar "[src_addr/width] [dst_addr/width] [proto [src [lt|eq|gt port]] [dst [lt|eq|gt port]] [estab] [syn] [finrst]]"
.Nm Ppp
supports four filter sets.  The
.Em alive
filter specifies packets that keep the connection alive - reseting the
idle timer.  The
.Em dial
filter specifies packets that cause
.Nm
to dial when in
.Fl auto
mode.  The
.Em in
filter specifies packets that are allowed to travel
into the machine and the
.Em out
filter specifies packets that are allowed out of the machine.
.Pp
Filtering is done prior to any IP alterations that might be done by the
alias engine.  By default all filter sets allow all packets to pass.
Rules are processed in order according to
.Ar rule-no .
Up to 20 rules may be given for each set.  If a packet doesn't match
any of the rules in a given set, it is discarded.  In the case of
.Em in
and
.Em out
filters, this means that the packet is dropped.  In the case of
.Em alive
filters it means that the packet will not reset the idle timer and in
the case of
.Em dial
filters it means that the packet will not trigger a dial.  A packet failing
to trigger a dial will be dropped rather than queued.  Refer to the
section on PACKET FILTERING above for further details.
.It set hangup Ar chat-script
This specifies the chat script that will be used to reset the modem
before it is closed.  It should not normally be necessary, but can
be used for devices that fail to reset themselves properly on close.
.It set help|? Op Ar command
This command gives a summary of available set commands, or if
.Ar command
is specified, the command usage is shown.
.It set ifaddr Ar [myaddr [hisaddr [netmask [triggeraddr]]]]
This command specifies the IP addresses that will be used during
IPCP negotiation.  Addresses are specified using the format
.Pp
.Dl a.b.c.d/n
.Pp
Where
.Ar a.b.c.d
is the preferred IP, but
.Ar n
specifies how many bits of the address we will insist on.  If
.Ar /n
is omitted, it defaults to
.Ar /32
unless the IP address is 0.0.0.0 in which case it defaults to
.Ar /0 .
.Pp
.Ar Hisaddr
may also be specified as a range of IP numbers in the format
.Pp
.Dl a.b.c.d[-d.e.f.g][,h.i.j.k[-l,m,n,o]]...
.Pp
for example:
.Pp
.Dl set ifaddr 10.0.0.1 10.0.1.2-10.0.1.10,10.0.1.20
.Pp
will only negotiate
.Ar 10.0.0.1
as the local IP number, but may assign any of the given 10 IP
numbers to the peer.  If the peer requests one of these numbers,
and that number is not already in use,
.Nm
will grant the peers request.  This is useful if the peer wants
to re-establish a link using the same IP number as was previously
allocated (thus maintaining any existing tcp connections).
.Pp
If the peer requests an IP number that's either outside
of this range or is already in use,
.Nm
will suggest a random unused IP number from the range.
.Pp
If
.Ar triggeraddr
is specified, it is used in place of
.Ar myaddr
in the initial IPCP negotiation.  However, only an address in the
.Ar myaddr
range will be accepted.  This is useful when negotiating with some
.Dv PPP
implementations that will not assign an IP number unless their peer
requests
.Ar 0.0.0.0 .
.Pp
It should be noted that in
.Fl auto
mode,
.Nm
will configure the interface immediately upon reading the
.Dq set ifaddr
line in the config file.  In any other mode, these values are just
used for IPCP negotiations, and the interface isn't configured
until the IPCP layer is up.
.Pp
Note that the
.Ar HISADDR
argument may be overridden by the third field in the
.Pa ppp.secret
file once the client has authenticated itself
.Pq if PAP or CHAP are Dq enabled .
Refer to the
.Em AUTHENTICATING INCOMING CONNECTIONS
section for details.
.Pp
In all cases, if the interface is already configured,
.Nm
will try to maintain the interface IP numbers so that any existing
bound sockets will remain valid.
.It set ccpretry Ar period
.It set chapretry Ar period
.It set ipcpretry Ar period
.It set lcpretry Ar period
.It set papretry Ar period
These commands set the number of seconds that
.Nm
will wait before resending Finite State Machine (FSM) Request packets.
The default
.Ar period
for all FSMs is 3 seconds (which should suffice in most cases).
.It set log [local] [+|-] Ns Ar value...
This command allows the adjustment of the current log level.  Refer
to the Logging Facility section for further details.
.It set login Ar chat-script
This
.Ar chat-script
compliments the dial-script.  If both are specified, the login
script will be executed after the dial script.  Escape sequences
available in the dial script are also available here.
.It set lqrperiod Ar frequency
This command sets the
.Ar frequency
in seconds at which
.Em LQR
or
.Em ECHO LQR
packets are sent.  The default is 30 seconds.  You must also use the
.Dq enable lqr
command if you wish to send LQR requests to the peer.
.It set mode Ar interactive|auto|ddial|background
This command allows you to change the
.Sq mode
of the specified link.  This is normally only useful in multi-link mode,
but may also be used in uni-link mode.
.Pp
It is not possible to change a link that is
.Sq direct
or
.Sq dedicated .
.It set mrru Ar value
Setting this option enables Multi-link PPP negotiations, also known as
Multi-link Protocol or MP.  There is no default MRRU (Maximum
Reconstructed Receive Unit) value.
.Em PPP
protocol *must* be able to accept packets of at
least 1500 octets.
.It set mru Ar value
The default MRU (Maximum Receive Unit) is 1500.  If it is increased, the
other side *may* increase its mtu.  There is no point in decreasing the
MRU to below the default as the
.Em PPP
protocol *must* be able to accept packets of at
least 1500 octets.
.It set mtu Ar value
The default MTU is 1500.  At negotiation time,
.Nm
will accept whatever MRU or MRRU that the peer wants (assuming it's
not less than 296 bytes).  If the MTU is set,
.Nm
will not accept MRU/MRRU values less that the set value.  When
negotiations are complete, the MTU is assigned to the interface, even
if the peer requested a higher value MRU/MRRU.  This can be useful for
limiting your packet size (giving better bandwidth sharing at the expense
of more header data).
.It set nbns Op Ar x.x.x.x Op Ar y.y.y.y
This option allows the setting of the Microsoft NetBIOS name server
values to be returned at the peers request.  If no values are given,
.Nm
will reject any such requests.
.It set openmode active|passive Op Ar delay
By default,
.Ar openmode
is always
.Ar active
with a one second
.Ar delay .
That is,
.Nm
will always initiate LCP/IPCP/CCP negotiation one second after the line
comes up.  If you want to wait for the peer to initiate negotiations, you
can use the value
.Ar passive .
If you want to initiate negotiations immediately or after more than one
second, the appropriate
.Ar delay
may be specified here in seconds.
.It set parity odd|even|none|mark
This allows the line parity to be set.  The default value is
.Ar none .
.It set phone Ar telno[|telno]...[:telno[|telno]...]...
This allows the specification of the phone number to be used in
place of the \\\\T string in the dial and login chat scripts.
Multiple phone numbers may be given separated by a pipe (|) or
a colon (:).  Numbers after the pipe are only dialed if the dial or login
script for the previous number failed.  Numbers separated by a colon are
tried sequentially, irrespective of the reason the line was dropped.
If multiple numbers are given,
.Nm
will dial them according to these rules until a connection is made, retrying
the maximum number of times specified by
.Dq set redial
below.  In
.Fl background
mode, each number is attempted at most once.
.It set reconnect Ar timeout ntries
Should the line drop unexpectedly (due to loss of CD or LQR
failure), a connection will be re-established after the given
.Ar timeout .
The line will be re-connected at most
.Ar ntries
times.
.Ar Ntries
defaults to zero.  A value of
.Ar random
for
.Ar timeout
will result in a variable pause, somewhere between 0 and 30 seconds.
.It set redial Ar seconds[.nseconds] [attempts]
.Nm Ppp
can be instructed to attempt to redial
.Ar attempts
times.  If more than one number is specified (see
.Dq set phone
above), a pause of
.Ar nseconds
is taken before dialing each number.  A pause of
.Ar seconds
is taken before starting at the first number again.  A value of
.Ar random
may be used here too.
.It set server|socket Ar TcpPort|LocalName|none password Op Ar mask
This command tells
.Nm
to listen on the given socket or
.Sq diagnostic port
for incoming command connections.
.Pp
The word
.Ar none
instructs
.Nm
to close any existing socket.
.Pp
If you wish to specify a local domain socket,
.Ar LocalName
must be specified as an absolute file name, otherwise it is assumed
to be the name or number of a TCP port.  You may specify the octal umask that
should be used with local domain sockets as a four character octal number
beginning with
.Sq 0 .
Refer to
.Xr umask 2
for umask details.  Refer to
.Xr services 5
for details of how to translate TCP port names.
.Pp
You must also specify the password that must be entered by the client
(using the
.Dq passwd
command above) when connecting to this socket.  If the password is
specified as an empty string, no password is required for connecting clients.
.Pp
When specifying a local domain socket, the first
.Dq %d
sequence found in the socket name will be replaced with the current
interface unit number.  This is useful when you wish to use the same
profile for more than one connection.
.Pp
In a similar manner TCP sockets may be prefixed with the
.Dq +
character, in which case the current interface unit number is added to
the port number.
.Pp
When using
.Nm
with a server socket, the
.Xr pppctl 8
command is the preferred mechanism of communications.  Currently,
.Xr telnet 1
can also be used, but link encryption may be implemented in the future, so
.Xr telnet 1
should not be relied upon.
.It set speed Ar value
This sets the speed of the serial device.
.It set stopped Ar [LCPseconds [CCPseconds]]
If this option is set,
.Nm
will time out after the given FSM (Finite State Machine) has been in
the stopped state for the given number of
.Dq seconds .
This option may be useful if the peer sends a terminate request,
but never actually closes the connection despite our sending a terminate
acknowledgement.  This is also useful if you wish to
.Dq set openmode passive
and time out if the peer doesn't send a Configure Request within the
given time.  Use
.Dq set log +lcp +ccp
to make
.Nm
log the appropriate state transitions.
.Pp
The default value is zero, where
.Nm
doesn't time out in the stopped state.
.Pp
This value should not be set to less than the openmode delay (see
.Dq set openmode
above).
.It set timeout Ar idleseconds
This command allows the setting of the idle timer.  Refer to the
section titled
.Dq SETTING THE IDLE TIMER
for further details.
.It set vj slotcomp on|off
This command tells
.Nm
whether it should attempt to negotiate VJ slot compression.  By default,
slot compression is turned
.Ar on .
.It set vj slots Ar nslots
This command sets the initial number of slots that
.Nm
will try to negotiate with the peer when VJ compression is enabled (see the
.Sq enable
command above).  It defaults to a value of 16.
.Ar Nslots
must be between
.Ar 4
and
.Ar 16
inclusive.
.El
.Pp
.It shell|! Op Ar command
If
.Ar command
is not specified a shell is invoked according to the
.Dv SHELL
environment variable.  Otherwise, the given
.Ar command
is executed.  Any of the pseudo arguments
.Dv HISADDR ,
.Dv INTERFACE
and
.Dv MYADDR
will be replaced with the appropriate values.  Use of the ! character
requires a following space as with any other commands.  You should note
that this command is executed in the foreground -
.Nm
will not continue running until this process has exited.  Use the
.Dv bg
command if you wish processing to happen in the background.
.It show Ar var
This command allows the user to examine the following:
.Bl -tag -width 20
.It show bundle
Show the current bundle settings.
.It show ccp
Show the current CCP compression statistics.
.It show compress
Show the current VJ compression statistics.
.It show escape
Show the current escape characters.
.It show filter Op Ar name
List the current rules for the given filter.  If
.Ar name
is not specified, all filters are shown.
.It show hdlc
Show the current HDLC statistics.
.It show help|?
Give a summary of available show commands.
.It show ipcp
Show the current IPCP statistics.
.It show lcp
Show the current LCP statistics.
.It show [data]link
Show high level link information.
.It show links
Show a list of available logical links.
.It show log
Show the current log values.
.It show mem
Show current memory statistics.
.It show modem
Show low level link information.
.It show proto
Show current protocol totals.
.It show route
Show the current routing tables.
.It show stopped
Show the current stopped timeouts.
.It show timer
Show the active alarm timers.
.It show version
Show the current version number of
.Nm ppp .
.El
.Pp
.It term
Go into terminal mode.  Characters typed at the keyboard are sent to
the modem.  Characters read from the modem are displayed on the
screen.  When a
.Nm
peer is detected on the other side of the modem,
.Nm
automatically enables Packet Mode and goes back into command mode.
.El
.Pp
.Sh MORE DETAILS
.Bl -bullet
.It
Read the example configuration files.  They are a good source of information.
.It
Use
.Dq help ,
.Dq show ? ,
.Dq alias ? ,
.Dq set ?
and
.Dq set ? <var>
to get online information about what's available.
.It
The following urls contain useful information:
.Bl -bullet -compact
.It
http://www.FreeBSD.org/FAQ/userppp.html
.It
http://www.FreeBSD.org/handbook/userppp.html
.El
.Pp
.El
.Pp
.Sh FILES
.Nm Ppp
refers to four files:
.Pa ppp.conf ,
.Pa ppp.linkup ,
.Pa ppp.linkdown
and
.Pa ppp.secret .
These files are placed in the
.Pa /etc/ppp
directory.
.Bl -tag -width flag
.It Pa /etc/ppp/ppp.conf
System default configuration file.
.It Pa /etc/ppp/ppp.secret
An authorisation file for each system.
.It Pa /etc/ppp/ppp.linkup
A file to check when
.Nm
establishes a network level connection.
.It Pa /etc/ppp/ppp.linkdown
A file to check when
.Nm
closes a network level connection.
.It Pa /var/log/ppp.log
Logging and debugging information file.  Note, this name is specified in
.Pa /etc/syslogd.conf .
See
.Xr syslog.conf 5
for further details.
.It Pa /var/spool/lock/LCK..* 
tty port locking file.  Refer to
.Xr uucplock 3
for further details.
.It Pa /var/run/tunN.pid
The process id (pid) of the
.Nm
program connected to the tunN device, where
.Sq N
is the number of the device.  This file is only created in
.Fl background ,
.Fl auto
and
.Fl ddial
modes.
.It Pa /var/run/ttyXX.if
The tun interface used by this port.  Again, this file is only created in
.Fl background ,
.Fl auto
and
.Fl ddial
modes.
.It Pa /etc/services
Get port number if port number is using service name.
.It Pa /var/run/ppp-authname-class-value
In multi-link mode, local domain sockets are created using the peer
authentication name
.Pq Sq authname ,
the peer endpoint discriminator class
.Pq Sq class
and the peer endpoint discriminator value
.Pq Sq value .
As the endpoint discriminator value may be a binary value, it is turned
to HEX to determine the actual file name.
.Pp
This socket is used to pass links between different instances of
.Nm ppp .
.El
.Pp
.Sh SEE ALSO
.Xr adduser 8 ,
.Xr at 1 ,
.Xr chat 8 ,
.Xr crontab 5 ,
.Xr ftp 1 ,
.Xr getty 8 ,
.Xr group 5 ,
.Xr gzip 1 ,
.Xr hostname 1 ,
.Xr inetd 8 ,
.Xr init 8 ,
.Xr login 1 ,
.Xr named 8 ,
.Xr passwd 5 ,
.Xr ping 8 ,
.Xr pppctl 8 ,
.Xr pppd 8 ,
.Xr route 8 ,
.Xr resolv.conf 5 ,
.Xr syslog 3 ,
.Xr syslog.conf 5 ,
.Xr syslogd 8 ,
.Xr tcpdump 1 ,
.Xr telnet 1 ,
.Xr traceroute 8 ,
.Xr uucplock 3 ,
.Xr vipw 8
.Sh HISTORY
This program was originally written by Toshiharu OHNO (tony-o@iij.ad.jp),
and was submitted to FreeBSD-2.0.5 by Atsushi Murai (amurai@spec.co.jp).
.Pp
It was substantially modified during 1997 by Brian Somers
(brian@Awfulhak.org), and was ported to OpenBSD in November that year
(just after the 2.2 release).
.Pp
Most of the code was rewritten by Brian Somers in early 1998 when
multi-link ppp support was added.
