.\" $Id: ppp.8,v 1.88 1997/12/21 01:07:13 brian Exp $
.Dd 20 September 1995
.Os FreeBSD
.Dt PPP 8
.Sh NAME
.Nm ppp
.Nd Point to Point Protocol (a.k.a. iijppp) 
.Sh SYNOPSIS
.Nm
.\" SOMEONE FIX ME ! The .Op macro can't handle enough args !
[
.Fl auto |
.Fl background |
.Fl ddial |
.Fl direct |
.Fl dedicated
]
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
proceedure, and use the
.Em PPP
protocol for authentication instead.
.It Supports Proxy Arp.
When
.Em PPP
is set up as server, you can also configure it to do proxy arp for your
connection.
.It Supports packet filtering.
User can define four kinds of filters:
.Em ifilter
for incoming packets,
.Em ofilter
for outgoing packets,
.Em dfilter
to define a dialing trigger packet and
.Em afilter
for keeping a connection alive with the trigger packet.
.It Tunnel driver supports bpf.
The user can use
.Xr tcpdump 1
to check the packet flow over the
.Em PPP
link.
.It Supports PPP over TCP capability.
.It Supports IETF draft Predictor-1 compression.
.Nm
supports not only VJ-compression but also Predictor-1 compression.
Normally, a modem has built-in compression (e.g. v42.bis) and the system
may receive higher data rates from it as a result of such compression.
While this is generally a good thing in most other situations, this
higher speed data imposes a penalty on the system by increasing the
number of serial interrupts the system has to process in talking to the
modem and also increases latency.  Unlike VJ-compression, Predictor-1
compression pre-compresses
.Em all
data flowing through the link, thus reducing overhead to a minimum.
.It Supports Microsoft's IPCP extensions.
Name Server Addresses and NetBIOS Name Server Addresses can be negotiated
with clients using the Microsoft
.Em PPP
stack (ie. Win95, WinNT)
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
you may need to deal with some initial configuration details.  First,
your kernel should include a tunnel device (the GENERIC kernel includes
one by default).  If it doesn't, or if you require more than one tun
interface, you'll need to rebuild your kernel with the following line in
your kernel configuration file:
.Dl pseudo-device tun N
where
.Ar N
is the maximum number of
.Em PPP
connections you wish to support.
Second, check your
.Pa /dev
directory for the tunnel device entries
.Pa /dev/tunN ,
where
.Sq N
represents the number of the tun device, starting at zero.
If they don't exist, you can create them by running "sh ./MAKEDEV tunN".
This will create tun devices 0 through
.Ar N .
Last of all, create a log file.
.Nm Ppp
uses 
.Xr syslog 3
to log information.  A common log file name is
.Pa /var/log/ppp.log .
To make output go to this file, put the following lines in the
.Pa /etc/syslog.conf
file:
.Dl !ppp
.Dl *.*<TAB>/var/log/ppp.log
Make sure you use actual TABs here.  If you use spaces, the line will be
silently ignored.
It is possible to have more than one
.Em PPP
log file by creating a link to the
.Nm
executable:
.Dl # cd /usr/sbin
.Dl # ln ppp ppp0
and using
.Dl !ppp0
.Dl *.* /var/log/ppp0.log
in
.Pa /etc/syslog.conf .
Don't forget to send a
.Dv HUP
signal to
.Xr syslogd 8
after altering
.Pa /etc/syslog.conf .
.Sh MANUAL DIALING
In the following examples, we assume that your machine name is
.Dv awfulhak .
If you set your host name and password in
.Pa /etc/ppp/ppp.secret ,
you can't do anything except run the help, passwd and quit commands.
.Bd -literal -offset indent
ppp on "your host name"> help
 help    : Display this message
 passwd  : Password for security
 quit    : Quit the PPP program
ppp on awfulhak> pass <password>
.Ed
.Pp
The "on" part of your prompt will change to "ON" if you specify the
correct password.
.Bd -literal -offset indent
ppp ON awfulhak>
.Ed
.Pp
You can now specify the device name, speed and parity for your modem,
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
PPP ON awfulhak>
.Ed
.Pp
You are now connected!  Note that
.Sq PPP
in the prompt has changed to capital letters to indicate that you have
a peer connection.  The show command can be used to see how things are
going:
.Bd -literal -offset indent
PPP ON awfulhak> show lcp
* LCP related information is shown here *
PPP ON awfulhak> show ipcp
* IPCP related information is shown here *
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
PPP ON awfulhak> add 0 0 HISADDR
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
.Bl -bullet -compact
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
which adds a default route.  The strings
.Dv HISADDR ,
.Dv MYADDR
and
.Dv INTERFACE
are available as the relevent IP addresses and interface name.
Similarly, when a connection is closed, the
contents of the
.Pa /etc/ppp/ppp.linkdown
file are executed.
Both of these files have the same format as
.Pa /etc/ppp/ppp.conf .
.Sh BACKGROUND DIALING
If you want to establish a connection using
.Nm
non-interactively (such as from a
.Xr crontab 5
entry or an
.Xr at 1
job) you should use the
.Fl background
option.  You must also specify the destination label in
.Pa /etc/ppp/ppp.conf
to use.  This label must contain the
.Dq set ifaddr
command to define the remote peers IP address. (refer to
.Pa /etc/ppp/ppp.conf.sample )
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
...
#
.Ed
.Pp
When
.Fl auto
or
.Fl ddial
is specified,
.Nm
runs as a daemon but you can still configure or examine its
configuration by using the diagnostic port as follows (this
can be done in
.Fl background
and
.Fl direct
mode too):
.Bd -literal -offset indent
# pppctl -v 3000 show ipcp
Password:
IPCP [Opened]
  his side: xxxx
  ....
.Ed
.Pp
Currently,
.Xr telnet 1
may also be used to talk interactively.
.Pp
In order to achieve this, you must use the
.Dq set server
command as described below.  It is possible to retrospectively make a running
.Nm
program listen on a diagnostic port by configuring
.Pa /etc/ppp/ppp.secret ,
and sending it a
.Dv USR1
signal.
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
  PPP ON awfulhak> close
  ppp ON awfulhak> quit all
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
.Dl ttyd1  "/usr/libexec/getty std.38400" dialup on secure
Don't forget to send a
.Dv HUP
signal to the
.Xr init 8
process to start the
.Xr getty 8 .
.Dl # kill -HUP 1
.It
Prepare an account for the incoming user.
.Bd -literal
ppp:xxxx:66:66:PPP Login User:/home/ppp:/usr/local/bin/ppplogin
.Ed
.Pp
.It
Create a
.Pa /usr/local/bin/ppplogin
file with the following contents:
.Bd -literal -offset indent
#!/bin/sh -p
exec /usr/sbin/ppp -direct
.Ed
.Pp
(You can specify a label name for further control.)
.Pp
Direct mode
.Pq Fl direct
lets
.Nm
work with stdin and stdout.  You can also use
.Xr pppctl 8
or
.Xr telnet 1
to connect to a configured diagnostic port, in the same manner as with
client-side
.Nm ppp .
.It
Optional support for Microsoft's IPCP Name Server and NetBIOS
Name Server negotiation can be enabled use
.Dq enable msext
and 
.Dq set ns pri-addr [sec-addr]
along with
.Dq set nbns pri-addr [sec-addr]
in your
.Pa /etc/ppp/ppp.conf
file.
.El
.Pp
.Sh RECEIVING INCOMING PPP CONNECTIONS (Method 2)
This method differs in that it recommends the use of 
.Em mgetty+sendfax
to handle the modem connections.  The latest versions (0.99 and higher)
can be compiled with the
.Dq AUTO_PPP
option to allow detection of clients speaking
.Em PPP
to the login prompt.
Follow these steps:
.Bl -enum
.It
Get, configure, and install mgetty+sendfax v0.99 or later making
sure you have used the AUTO_PPP option.
.It
Edit
.Pa /etc/ttys
to enable a mgetty on the port where the modem is attached.  For
example:
.Dl cuaa1  "/usr/local/sbin/mgetty -s 57600"       dialup on
.It
Prepare an account for the incoming user.
.Bd -literal
Pfred:xxxx:66:66:Fred's PPP:/home/ppp:/etc/ppp/ppp-dialup
.Ed
.Pp
.It
Examine the files
.Pa /etc/ppp/sample.ppp-dialup ,
.Pa /etc/ppp/sample.ppp-pap-dialup
and
.Pa /etc/ppp/ppp.conf.sample
for ideas.
.Pa /etc/ppp/ppp-pap-dialup
is supposed to be called from
.Pa /usr/local/etc/mgetty+sendfax/login.conf
from a line like
.Dl /AutoPPP/ -     -       /etc/ppp/ppp-pap-dialup
.El
.Pp
.Sh PPP OVER TCP (a.k.a Tunneling)
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
 add 10.0.1.0 255.255.255.0 10.0.4.1
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
The entry in
.Pa /etc/ppp/ppp.conf
on awfulhak (the initiator) should contain the following:
.Bd -literal -offset indent
ui-gate:
 set escape 0xff
 set device ui-gate:ppp-in
 set dial
 set timeout 30 5 4 
 set log Phase Chat Connect Carrier hdlc LCP IPCP CCP tun
 set ifaddr 10.0.4.2 10.0.4.1
 add 10.0.2.0 255.255.255.0 10.0.4.2
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
filters; ifilter, ofilter, dfilter and afilter.  Here are the basics:
.Bl -bullet -compact
.It
A filter definition has the following syntax:
set filter-name rule-no action [src_addr/src_width] [dst_addr/dst_width]
[proto [src [lt|eq|gt] port ]] [dst [lt|eq|gt] port] [estab]
.Bl -enum
.It
.Sq filter-name
should be one of ifilter, ofilter, dfilter or afilter.
.It
There are two actions:
.Sq permit
and
.Sq deny .
If a given packet
matches the rule, the associated action is taken immediately.
.It
.Sq src_width
and
.Sq dst_width
work like a netmask to represent an address range.
.It
.Sq proto
must be one of icmp, udp or tcp.
.It
.Sq port number
can be specified by number and service name from
.Pa /etc/services .
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
.Dq set filter-name -1
to flush all rules.
.El
.Pp
See
.Pa /etc/ppp/ppp.conf.filter.example .
.Sh SETTING IDLE, LINE QUALITY REQUEST, RETRY TIMER
To check/set idle timer, use the
.Dq show timeout
and
.Dq set timeout [lqrtimer [retrytimer]]
commands:
.Bd -literal -offset indent
ppp ON awfulhak> set timeout 600
.Ed
.Pp
The timeout period is measured in seconds, the  default values for which
are timeout = 180 or 3 min, lqrtimer = 30sec and retrytimer = 3sec.
To disable the idle timer function, use the command
.Bd -literal -offset indent
ppp ON awfulhak> set timeout 0
.Ed
.Pp
In
.Fl auto
mode, an idle timeout causes the
.Em PPP
session to be
closed, though the
.Nm
program itself remains running.  Another trigger packet will cause it to
attempt to reestablish the link.
.Sh PREDICTOR-1 COMPRESSION
This version supports CCP and Predictor type 1 compression based on
the current IETF-draft specs. As a default behaviour,
.Nm
will attempt to use (or be willing to accept) this capability when the
peer agrees (or requests it).
To disable CCP/predictor1 functionality completely, use the
.Dq disable pred1
and
.Dq deny pred1
commands.
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
and
.Sq dst_addr
default to 0.0.0.0, and
.Sq netmask
defaults to whatever mask is appropriate for
.Sq src_addr .
It is only possible to make
.Sq netmask
smaller than the default.  The usual value is 255.255.255.255.
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
.Dl set ifaddr 192.244.177.38/24 192.244.177.2/20
A number followed by a slash (/) represent the number of bits significant in
the IP address.  The above example signifies that:
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
set ifaddr 10.10.10.10/0 10.10.11.11/0 0.0.0.0 0.0.0.0
.Ed
.Pp
.It
In most cases, your ISP will also be your default router.  If this is
the case, add the lines
.Bd -literal -offset indent
delete ALL
add 0 0 HISADDR
.Ed
.Pp
to
.Pa /etc/ppp/ppp.conf .
.Pp
This tells
.Nm
to delete all non-direct routing entries for the tun interface that
.Nm
is running on, then to add a default route to 10.10.11.11.
.Pp
If you're using dynamic IP numbers, you must also put these two lines
in the
.Pa /etc/ppp/ppp.linkup
file:
.Bd -literal -offset indent
delete ALL
add 0 0 HISADDR
.Ed
.Pp
HISADDR is a macro meaning the "other side"s IP number, and is
available once an IP number has been agreed (using IPCP).
Now, once a connection is established,
.Nm
will delete all non-direct interface routes, and add a default route
pointing at the peers IP number.  You should use the same label as the
one used in
.Pa /etc/ppp/ppp.conf .
.Pp
If commands are being typed interactively, the only requirement is
to type
.Bd -literal -offset indent
add 0 0 HISADDR
.Ed
.Pp
after a successful dial.
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
.It Li Carrier	Log Chat lines with 'CARRIER'
.It Li CCP	Generate a CCP packet trace
.It Li Chat	Generate Chat script trace log
.It Li Command	Log commands executed
.It Li Connect	Generate complete Chat log
.It Li Debug	Log (very verbose) debug information
.It Li HDLC	Dump HDLC packet in hex
.It Li ID0	Log all function calls specifically made as user id 0.
.It Li IPCP	Generate an IPCP packet trace
.It Li LCP	Generate an LCP packet trace
.It Li Link	Log address assignments and link up/down events
.It Li LQM	Generate LQR report
.It Li Phase	Phase transition log output
.It Li TCP/IP	Dump all TCP/IP packets
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
.Dq set log Carrier Link Phase .
.Pp
It is also possible to log directly to the screen.  The syntax is
the same except that the word
.Dq local
should immediately follow
.Dq set log .
The default is
.Dq set log local
(ie. no direct screen logging).
.Pp
If The first argument to
.Dq set log Op local
begins with a '+' or a '-' character, the current log levels are
not cleared, for example:
.Bd -literal -offset indent
PPP ON awfulhak> set log carrier link phase
PPP ON awfulhak> show log
Log:   Carrier Link Phase Warning Error Alert
Local: Warning Error Alert
PPP ON awfulhak> set log -link +tcp/ip -warning
PPP ON awfulhak> set log local +command
PPP ON awfulhak> show log
Log:   Carrier Phase TCP/IP Warning Error Alert
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
.It USR1
This signal, when not in interactive mode, tells
.Nm
to close any existing server socket and open an Internet socket using
port 3000 plus the current tunnel device number.  This can only be
achieved if a suitable local password is specified in
.Pa /etc/ppp/ppp.secret .
.It USR2
This signal, tells
.Nm
to close any existing server socket.
.El
.Pp
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
.It accept|deny|enable|disable option....
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
challenge.  Refer to the description of the
.Dq set encrypt
command for further details.
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
.Dq PPP Magnalink Variable Resource Compression
in
.Pa rfc1975 Ns No !
.Nm Ppp
is capable of negotiating with
.Nm pppd ,
but only if
.Dq pppd-deflate
is
.Ar enable Ns No d
and
.Ar accept Ns No ed .
.It lqr
Default: Disabled and Accepted.  This option decides if Link Quality
Requests will be sent.  LQR is a protocol that allows
.Nm
to determine that the link is down without relying on the modems
carrier detect.
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
.It pppd-deflate
Default: Disabled and Denied.  This is a variance of the
.Ar deflate
option, allowing negotiation with the
.Xr pppd 8
program.  Refer to the
.Ar deflate
section above for details.  It is disabled by default as it violates
.Pa rfc1975 .
.It pred1
Default: Enabled and Accepted.  This option decides if Predictor 1
compression will be used by the Compression Control Protocol (CCP).
.It protocomp
Default: Enabled and Accepted.  This option is used to negotiate
PFC (Protocol Field Compression), a mechanism where the protocol
field number is reduced to one octet rather than two.
.It vjcomp
Default: Enabled and Accepted.  This option decides if Van Jacobson
header compression will be used.
.El
.Pp
The following options are not actually negotiated with the peer.
Therefore, accepting or denying them makes no sense.
.Bl -tag -width 20
.It msext
Default: Disabled.  This option allows the use of Microsoft's
.Em PPP
extensions, supporting the negotiation of the DNS and the NetBIOS NS.
Enabling this allows us to pass back the values given in "set ns"
and "set nbns".
.It passwdauth
Default: Disabled.  Enabling this option will tell the PAP authentication
code to use the password file (see
.Xr passwd 5 )
to authenticate the caller rather than the
.Pa /etc/ppp/ppp.secret
file.
.It proxy
Default: Disabled.  Enabling this option will tell
.Nm
to proxy ARP for the peer.
.It throughput
Default: Disabled.  Enabling this option will tell
.Nm
to gather thoroughput statistics.  Input and output is sampled over
a rolling 5 second window, and current, best and total figures are
retained.  This data is output when the relevent
.Em PPP
layer shuts down, and is also available using the
.Dq show
command.  Troughput statistics are available at the
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
.It add dest mask gateway
.Ar Dest
is the destination IP address and
.Ar mask
is its mask.
.Ar 0 0
refers to the default route, and it is possible to use the symbolic name
.Sq default
in place of both the
.Ar dest
and
.Ar mask
arguments.
.Ar Gateway
is the next hop gateway to get to the given
.Ar dest
machine/network.  It is possible to use the symbolic names
.Sq HISADDR
or
.Sq INTERFACE
as the
.Ar gateway .
.Sq INTERFACE
is replaced with the current interface name and
.Sq HISADDR
is replaced with the current interface address.  If the current interface
address has not yet been assigned, the current
.Sq INTERFACE
is used instead.
.It allow .....
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
.It allow user|users logname...
By default, only user id 0 is allowed access.  If this command is specified,
all of the listed users are allowed access to the section in which the
.Dq allow users
command is found.  The
.Sq default
section is always checked first (although it is only ever automatically
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
.It allow mode|modes modelist...
By default, access using all
.Nm
modes is possible.  If this command is used, it restricts the access
modes allowed to load the label under which this command is specified.
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
.El
.Pp
.It alias .....
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
.It alias port [proto targetIP:targetPORT [aliasIP:]aliasPORT]
This command allows us to redirect connections arriving at
.Dq aliasPORT
for machine [aliasIP] to
.Dq targetPORT
on
.Dq targetIP .
If proto is specified, only connections of the given protocol
are matched.  This option is useful if you wish to run things like
Internet phone on the machines behind your gateway.
.It alias addr [addr_local addr_alias]
This command allows data for
.Dq addr_alias
to be redirected to
.Dq addr_local .
It is useful if you own a small number of real IP numbers that
you wish to map to specific machines behind your gateway.
.It alias deny_incoming [yes|no]
If set to yes, this command will refuse all incoming connections
by dropping the packets in much the same way as a firewall would.
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
.It alias help|?
This command gives a summary of available alias commands.
.El
.Pp
.It [!]bg command
The given command is executed in the background.
Any of the pseudo arguments
.Dv HISADDR ,
.Dv INTERFACE
and
.Dv MYADDR
will be replaced with the appropriate values.  If you wish to pause
.Nm
while the command executes, use the
.Dv shell
command instead.
.It close
Close the current connection (but don't quit).
.It delete dest
This command deletes the route with the given
.Ar dest
IP address.  If
.Ar dest
is specified as
.Sq ALL ,
all non-direct entries in the routing for the current interface that
.Nm
is using are deleted.  This means all entries for tunN, except the entry
representing the actual link.  If
.Ar dest
is specified as
.Sq default ,
the default route is deleted.
.It dial|call [remote]
If
.Dq remote
is specified, a connection is established using the
.Dq dial
and
.Dq login
scripts for the given
.Dq remote
system.  Otherwise, the current settings are used to establish
the connection.
.It display
Displays the current status of the negotiable protocol
values as specified under
.Dq accept|deny|enable|disable option....
above.
.It down
Bring the link down ungracefully, as if the physical layer had become
unavailable.  It's not considered polite to use this command.
.It help|? [command]
Show a list of available commands.  If
.Dq command
is specified, show the usage string for that command.
.It load [remote]
Load the given
.Dq remote
label.  If
.Dq remote
is not given, the
.Dq default
label is assumed.
.It passwd pass
Specify the password required for access to the full
.Nm
command set.  This password is required when connecting to the diagnostic
port (see the
.Dq set server
command).
.Ar Pass
may be specified either on the
.Dq set server
command line or by putting an entry in
.Pa /var/log/ppp.secret
for the local host.  The value of
.Ar pass
is not logged when
.Ar command
logging is active, instead, the literal string
.Dq ********
is logged.
.It quit|bye [all]
Exit
.Nm ppp .
If
.Nm
is in interactive mode or if the
.Dq all
argument is given,
.Nm
will exit, closing the connection.  A simple
.Dq quit
issued from a
.Xr pppctl 8
or
.Xr telnet 1
session will not close the current connection.
.It save
This option is not (yet) implemented.
.It set[up] var value
This option allows the setting of any of the following variables:
.Bl -tag -width 20
.It set accmap hex-value
ACCMap stands for Asyncronous Control Character Map.  This is always
negotiated with the peer, and defaults to a value of 0x00000000.
This protocol is required to defeat hardware that depends on passing
certain characters from end to end (such as XON/XOFF etc).
.It set filter-name rule-no action [src_addr/src_width]
[dst_addr/dst_width] [proto [src [lt|eq|gt] port ]]
[dst [lt|eq|gt] port] [estab]
.Pp
.Nm Ppp
supports four filter sets.  The afilter specifies packets that keep
the connection alive - reseting the idle timer.  The dfilter specifies
packets that cause
.Nm
to dial when in
.Fl auto
mode.  The ifilter specifies packets that are allowed to travel
into the machine and the ofilter specifies packets that are allowed
out of the machine.  By default all filter sets allow all packets
to pass.
Rules are processed in order according to
.Dq n .
Up to 20 rules may be given for each set.  If a packet doesn't match
any of the rules in a given set, it is discarded.  In the case of
ifilters and ofilters, this means that the packet is dropped.  In
the case of afilters it means that the packet will not reset the
idle timer and in the case of dfilters it means that the packet will
not trigger a dial.
Refer to the section on PACKET FILTERING above for further details.
.It set authkey|key value
This sets the authentication key (or password) used in client mode
PAP or CHAP negotiation to the given value.  It can also be used to
specify the password to be used in the dial or login scripts in place
of the '\\P' sequence, preventing the actual password from being logged.  If
.Ar command
logging is in effect,
.Ar value
is logged as
.Ar ********
for security reasons.
.It set authname id
This sets the authentication id used in client mode PAP or CHAP negotiation.
.It set ctsrts
This sets hardware flow control and is the default.
.It set device|line value
This sets the device to which
.Nm
will talk to the given
.Dq value .
All serial device names are expected to begin with
.Pa /dev/ .
If
.Dq value
does not begin with
.Pa /dev/ ,
it must be of the format
.Dq host:port .
If this is the case,
.Nm
will attempt to connect to the given
.Dq host
on the given
.Dq port .
Refer to the section on
.Em PPP OVER TCP
above for further details.
.It set dial chat-script
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
.It set hangup chat-script
This specifies the chat script that will be used to reset the modem
before it is closed.  It should not normally be necessary, but can
be used for devices that fail to reset themselves properly on close.
.It set encrypt MSChap|MD5
This specifies the encryption algorithm to request and use when issuing
the CHAP challenge, and defaults to MD5.  If this is set to MSChap,
.Nm
will behave like a Microsoft RAS when sending the CHAP challenge (assuming
CHAP is enabled).  When responding to a challenge,
.Nm
determines how to encrypt the response based on the challenge, so this
setting is ignored.
.Bl -tag -width NOTE:
.It NOTE:
Because the Microsoft encryption algorithm uses a combination of MD4 and DES,
if you have not installed DES encryption software on your machine
before building
.Nm ppp ,
this option will not be available - only MD5 will be used.
.El
.Pp
.It set escape value...
This option is similar to the
.Dq set accmap
option above.  It allows the user to specify a set of characters that
will be `escaped' as they travel across the link.
.It set ifaddr [myaddr [hisaddr [netmask [triggeraddr]]]]
This command specifies the IP addresses that will be used during
IPCP negotiation.  Addresses are specified using the format
.Dl a.b.c.d/n
Where a.b.c.d is the preferred IP, but n specifies how many bits
of the address we will insist on.  If the /n bit is omitted, it
defaults to /32 unless the IP address is 0.0.0.0 in which case
the mask defaults to /0.
.Pp
.Ar Hisaddr
may also be specified as a range of IP numbers in the format
.Dl a.b.c.d[-d.e.f.g][,h.i.j.k[-l,m,n,o]]...
for example:
.Dl set ifaddr 10.0.0.1 10.0.1.2-10.0.1.10,10.0.1.20
will only negotiate
.Ar 10.0.0.1
as the local IP number, but will assign any of the given 10 IP
numbers to the peer.  If the peer requests one of these numbers,
and that number is not already in use,
.Nm
will grant the peers request.  This is useful if the peer wants
to re-establish a link using the same IP number as was previously
allocated.  If the peer requests an IP number that's either outside
of this range or is already in use,
.Nm
will start by suggesting a random unused IP number from the range.
If the peer doesn't subsequently agree,
.Nm
will suggest each of the other numbers in succession until a number
is chosen or until too many IPCP Configure Requests have been sent.
.Pp
If
.Ar triggeraddr
is specified, it is used in place of
.Ar myaddr
in the initial IPCP negotiation.  However, only an address in the
.Ar myaddr
range will be accepted.
.It set loopback on|off
When set to
.Ar on
(the default),
.Nm
will automatically loop back packets being sent
out with a destination address equal to that of the
.Em PPP
interface.  If set to
.Ar off ,
.Nm
will send the packet, probably resulting in an ICMP redirect from
the other end.
.It set log [local] [+|-]value...
This command allows the adjustment of the current log level.  Refer
to the Logging Facility section for further details.
.It set login chat-script
This
.Ar chat-script
compliments the dial-script.  If both are specified, the login
script will be executed after the dial script.  Escape sequences
available in the dial script are also available here.
.It set mru value
The default MRU is 1500.  If it is increased, the other side *may*
increase its mtu.  There is no use decreasing the MRU to below the
default as the
.Em PPP
protocol *must* be able to accept packets of at
least 1500 octets.
.It set mtu value
The default MTU is 1500.  This may be increased by the MRU specified
by the peer.  It may only be subsequently decreased by this option.
Increasing it is not valid as the peer is not necessarily able to
receive the increased packet size.
.It set openmode active|passive
By default,
.Ar openmode
is always
.Ar active .
That is,
.Nm
will always initiate LCP/IPCP/CCP negotiation.  If you want to wait
for the peer to initiate negotiations, you may use the value
.Ar passive .
.It set parity odd|even|none|mark
This allows the line parity to be set.  The default value is
.Ar none .
.It set phone telno[|telno]...[:telno[|telno]...]...
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
.It set reconnect timeout ntries
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
.It set redial seconds[.nseconds] [attempts]
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
.It set stopped [LCPseconds [IPCPseconds [CCPseconds]]]
If this option is set,
.Nm
will time out after the given FSM (Finite State Machine) has been in
the stopped state for the given number of
.Dq seconds .
This option may be useful if you see
.Nm
failing to respond in the stopped state, or if you wish to
.Dq set openmode passive
and time out if the peer doesn't send a Configure Request within the
given time.  Use
.Dq set log +lcp +ipcp +ccp
to make
.Nm
log all state transitions.
.Pp
The default value is zero, where
.Nm
doesn't time out in the stopped state.
.It set server|socket TcpPort|LocalName|none [password] [mask]
This command tells
.Nm
to listen on the given socket or
.Sq diagnostic port
for incoming command connections.  This is not possible if
.Nm
is in interactive mode.  The word
.Ar none
instructs
.Nm
to close any existing socket.  If you wish to specify a unix domain
socket,
.Ar LocalName
must be specified as an absolute file name, otherwise it is assumed
to be the name or number of a TCP port.  You may specify the octal umask that
should be used with unix domain sockets as a four character octal number
beginning with
.Sq 0 .
Refer to
.Xr umask 2
for umask details.  Refer to
.Xr services 5
for details of how to translate TCP port names.
.Pp
You may also specify the password that must be used by the client when
connecting to this socket.  If the password is not specified here,
.Pa /etc/ppp/ppp.secret
is searched for a machine name that's the same as your local host name
without any domain suffix.  Refer to
.Xr hostname 1
for further details.  If a password is specified as the empty string,
no password is required.
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
.It set speed value
This sets the speed of the serial device.
.It set timeout Idle [ lqr [ retry ] ]
This command allows the setting of the idle timer, the LQR timer (if
enabled) and the retry timer.
.It set ns x.x.x.x y.y.y.y
This option allows the setting of the Microsoft DNS servers that
will be negotiated.
.It set nbns x.x.x.x y.y.y.y
This option allows the setting of the Microsoft NetBIOS DNS servers that
will be negotiated.
.It set help|?
This command gives a summary of available set commands.
.El
.Pp
.It shell|! [command]
If
.Dq command
is not specified a shell is invoked according to the
.Dv SHELL
environment variable.  Otherwise, the given command is executed.
Any of the pseudo arguments
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
.It show var
This command allows the user to examine the following:
.Bl -tag -width 20
.It show [adio]filter
List the current rules for the given filter.
.It show auth
Show the current authname and encryption values.  If you have built
.Nm
without DES support, the encryption value is not displayed as it will
always be
.Ar MD5 .
.It show ccp
Show the current CCP statistics.
.It show compress
Show the current compress statistics.
.It show escape
Show the current escape characters.
.It show hdlc
Show the current HDLC statistics.
.It show ipcp
Show the current IPCP statistics.
.It show lcp
Show the current LCP statistics.
.It show loopback
Show the current loopback status.
.It show log
Show the current log values.
.It show mem
Show current memory statistics.
.It show modem
Show current modem statistics.
.It show mru
Show the current MRU.
.It show mtu
Show the current MTU.
.It show proto
Show current protocol totals.
.It show reconnect
Show the current reconnect values.
.It show redial
Show the current redial values.
.It show stopped
Show the current stopped timeouts.
.It show route
Show the current routing tables.
.It show timeout
Show the current timeout values.
.It show msext
Show the current Microsoft extension values.
.It show version
Show the current version number of
.Nm ppp .
.It show help|?
Give a summary of available show commands.
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
.Bl -bullet -compact
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
commands.
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
.El
.Pp
.Sh SEE ALSO
.Xr at 1 ,
.Xr chat 8 ,
.Xr crontab 5 ,
.Xr ftp 1 ,
.Xr getty 8 ,
.Xr gzip 1 ,
.Xr hostname 1 ,
.Xr inetd 8 ,
.Xr init 8 ,
.Xr login 1 ,
.Xr passwd 5 ,
.Xr ping 8 ,
.Xr pppctl 8 ,
.Xr pppd 8 ,
.Xr syslog 3 ,
.Xr syslog.conf 5 ,
.Xr syslogd 8 ,
.Xr tcpdump 1 ,
.Xr telnet 1 ,
.Xr traceroute 8 ,
.Xr uucplock 3
.Sh HISTORY
This program was originally written by Toshiharu OHNO (tony-o@iij.ad.jp),
and was submitted to FreeBSD-2.0.5 by Atsushi Murai (amurai@spec.co.jp).
It has since had an enormous face lift and looks substantially different.
.Pp
The zlib compression algorithms used in the DEFLATE protocol are provided
thanks to Jean-loup Gailly (Copyright 1995).
