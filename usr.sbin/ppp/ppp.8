.\" manual page [] for ppp 0.94 beta2 + alpha
.\" $Id: ppp.8,v 1.6 1995/05/21 17:32:35 jkh Exp $
.\" SH section heading
.\" SS subsection heading
.\" LP paragraph
.\" IP indented paragraph
.\" TP hanging label
.TH PPP 8
.SH NAME
ppp \- Point to Point Protocol (aka iijppp)
.SH SYNOPSIS
.B ppp
[
.I -auto | -direct -dedicated
] [
.I system
]
.SH DESCRIPTION
.LP
This is a user process \fIPPP\fR software package.  Normally, \fIPPP\fR
is implemented as a part of the kernel (e.g. as managed by pppd) and it's
thus somewhat hard to debug and/or modify its behavior.  However, in this
implementation \fIPPP\fR is done as a user process with the help of the
tunnel device driver (tun).
.LP

.SH Major Features

.TP
o Provides interactive user interface.
Using its command mode, the user can
easily enter commands to establish the connection with the remote end, check
the status of connection and close the connection.  All functions can
also be optionally password protected for security.

.TP
o Supports both manual and automatic dialing. 
Interactive mode has a ``term'' command which enables you to talk to your modem
directly.  When your modem is connected to the remote peer and it starts to
talk \fIPPP\fR, the \fIPPP\fR software detects it and switches to packet
mode automatically. Once you have determined the proper sequence for connecting
with the remote host, you can write a chat script to define the necessary dialing
and login procedure for later convenience.

.TP
o Supports on-demand dialup capability.
By using auto mode, the \fIPPP\fR
program will act as a daemon and wait for a packet to be sent over the \fIPPP\fR
link.  When this happens, the daemon automatically dials and establishes the
connection.

.TP
o Supports server-side \fIPPP\fR connections.
Can act as server which accepts incoming \fIPPP\fR connections. 
 
.TP
o Supports PAP and CHAP authentication.                                     

.TP
o Supports Proxy Arp.
When \fIPPP\fR is set up as server, you can also configure it to do proxy arp
for your connection.

.TP
o Supports packet filtering.
User can define four kinds of filters:
ifilter for incoming packets, \fIofilter\fR for outgoing packets, \fIdfilter\fR
to define a dialing trigger packet and \fIafilter\fR for keeping a connection
alive with the trigger packet.

.TP
o Tunnel driver supports bpf.
That is, user can use
.IR tcpdump (1)
to check the packet flow over the \fIPPP\fR link.

.TP 
o Supports \fIPPP\fR over TCP capability. 

.TP
o Supports IETF draft Predictor-1 compression.  
\fIPPP\fR supports not only VJ-compression but also Predictor-1
compression. Normally, a modem has built-in compression (e.g. v42.bis)
and the system may receive higher data rates from it as a result of
such compression.  While this is generally a good thing in most
other situations, this higher speed data imposes a penalty on
the system by increasing the number of serial interrupts the system
has to process in talking to the modem.  Unlike VJ-compression,
Predictor-1 compression pre-compresses \fBall\fR data flowing through
the link, thus reducing overhead to a minimum.

.TP
o Runs under BSDI-1.1 and FreeBSD.
Patches for NeXTSTEP 3.2 are also available on the net.

.SH GETTING STARTED
.LP

When you first run \fIPPP\fR, you may need to deal with some
initial configuration details.  First, your kernel should
include a tunnel device (the default in FreeBSD 2.0.5 and later).
If it doesn't, you'll need to rebuild your kernel with the following
line in your kernel configuration file:

.TP
pseudo-device   tun             1

.LP
You should set the numeric field to the maximum number of 
\fIPPP\fR connections you wish to support.

.LP
Second, check your /dev directory for the tunnel device entry
/dev/tun0.  If it doesn't exist, you can create it by running
"MAKEDEV tun0"

.SH MANUAL DIALING

.LP
% ppp
  User Process PPP written by Toshiharu OHNO.
 -- If you set your hostname and password in /etc/ppp/ppp.secret, you can't do
     anything except run the quit and help commands --

ppp on "your hostname"> help
  passwd  : Password for security
  quit    : Quit the PPP program    
  help    : Display this message

ppp on tama> pass <password>
 -- "on" will change to "ON" if you specify the correct password.

ppp ON tama>
 -- You can specify the device name and speed for your modem using
     the following commands:

ppp ON tama> set line /dev/cuaa0

ppp ON tama> set speed 38400

ppp ON tama> set parity even

ppp ON tama> show modem
 -- Modem related parameters are shown in here

ppp ON tama>
 -- Use term command to talk with your modem

ppp ON tama> term
 at
 OK
 atdt123456
 CONNECT

 login: ppp
 Password:

-- PPP started in remote side ---

-- When the peer start to talk PPP, the program will detect it
-- automatically and return to command mode.

ppp ON tama>

\fBPPP\fR ON TAMA>

-- NOW, you are connected!  Note that prompt has changed to
-- capital letters to indicate this.

PPP ON tama> show lcp

-- You'll see LCP status --

PPP ON tama> show ipcp

-- You'll see IPCP status --
-- At this point, your machine has a host route to the peer.
-- If you want to add a default route entry, then enter

PPP ON tama> add 0 0 HISADDR

-- Here string `HISADDR' represents the IP address of connected peer.

PPP ON tama>

-- Use applications (i.e. ping, telnet, ftp) in other windows

PPP ON tama> show log

-- Gives you some logging messages

PPP ON tama> close

-- Connection is closed and modem will be disconnected.

ppp ON tama> quit

%
.LP

.SH AUTOMATIC DIALING

.LP
To use automatic dialing, you must prepare some Dial and Login chat scripts.
See the example definitions in /etc/ppp/ppp.conf.sample (the format of ppp.conf is
pretty simple).

.TP 2
o
Each line contains one command, label or comment.

.TP 2
o 
A line starting with a `#' character is treated as a comment line.

.TP 2
o
A label name has to start in the first column and should be followed by a colon (:).

.TP 2
o
A command line must contain a space or tab in the first column.

.LP
Once ppp.conf is ready, specify the destination label name when you invoke
ppp. Commands associated with the destination label are then executed.
Note that the commands associated with the ``default'' label are ALWAYS executed.

Once the connection is made, you'll find that prompt has changed to

 capital \fIPPP\fR on tama>.

   % ppp pm2
   ...
   ppp ON tama> dial
   dial OK!
   login OK!
   PPP ON tama>

If an /etc/ppp/ppp.linkup file is available, its contents are executed when
the \fIPPP\fR connection is established.  See the provided example which adds
a default route.  The string HISADDR represents the IP address of the remote peer.


.SH DIAL ON DEMAND

.LP
 To play with demand dialing, you must use the -auto option.  You
must also specify the destination label in /etc/ppp/ppp.conf to use.
It should contain the ``ifaddr'' command to define the remote
peer's IP address. (refer to /etc/ppp/ppp.conf.sample)


   % ppp -auto pm2demand
   ...
   %

.LP
When -auto is specified, \fIPPP\fR program runs as a daemon but
you can still configure or examine its configuration by using
the diagnostic port as follows:


  % telnet localhost 3000
    Trying 127.0.0.1...
    Connected to localhost.spec.co.jp.
    Escape character is '^]'.
    User Process PPP. Written by Toshiharu OHNO.
    Working as auto mode. 
    PPP on tama> show ipcp
    what ?
    PPP on tama> pass xxxx
    PPP ON tama> show ipcp
    IPCP [OPEND]
      his side: xxxx
      ....

.LP
Each ppp daemon has an associated port number which is computed as "3000 +
tunnel_device_number". If 3000 is not good base number, edit defs.h in
the ppp sources (/usr/src/usr.sbin/ppp) and recompile it.
When an outgoing packet is detected, \fIPPP\fR will perform the
dialing action (chat script) and try to connect with the peer.  If dialing fails,
it will wait for 30 seconds and retry.

 To terminate the program, type

  PPP ON tama> close
  \fBppp\fR ON tama> quit all

.LP
A simple ``quit'' command will terminate the telnet connection but
not the \fIPPP\fR program itself. You must use ``quit all'' to terminate
the \fRPPP\fR program as well.
.LP

.SH PACKET FILTERING

.LP
This implementation supports packet filtering. There are three kinds of filters:
ifilter, ofilter and dfilter.  Here are the basics:
.LP

.TP
o A filter definition has the following syntax:

   set filter-name rule-no action [src_addr/src_width] [dst_addr/dst_width]
       [proto [src [lt|eq|gt] port ] [dst [lt|eq|gt] port] [estab]

   a) filter-name should be ifilter, ofilter or dfiler.
   
   b) There are two actions: permit and deny. If a given packet is matched
      against the rule, the associated action is taken immediately.

   c) src_width and dst_width works like a netmask to represent an address range.

   d) proto must be one of icmp, udp or tcp.

.TP
o Each filter can hold up to 20 rules, starting from rule 0.
The entire rule set is not effective until rule 0 is defined.

.TP 2
o
If no rule is matched to a packet, that packet will be discarded (blocked).

.TP
o Use ``set filer-name -1'' to flush all rules.

.LP
 See /etc/ppp/ppp.conf.filter.example
.LP

.SH RECEIVING INCOMING PPP CONNECTIONS

.LP
 To handle an incoming \fIPPP\fR connection request, follow these steps:
.LP

 a) Make sure the modem and (optionally) /etc/rc.serial is configured correctly.
    - Use Hardware Handshake (CTS/RTS) for flow control.
    - Modem should be set to NO echo back (ATE0) and NO results string (ATQ1)

 b) Edit /etc/ttys to enable a getty on the port where the modem is attached.
    For example:

	ttyd1  "/usr/libexec/getty std.38400" dialup on secure

    Don't forget to send a HUP signal to the init process to start the getty.

	# kill -HUP 1

 c) Prepare an account for the incoming user.

    ppp:xxxx:66:66:PPP Login User:/home/ppp:/usr/local/bin/ppplogin

 d) Create a /usr/local/bin/ppplogin file with the following contents:

	#!/bin/sh
	/usr/sbin/ppp -direct

    You can specify a label name for further control.

.LP
 Direct mode (-direct) lets \fIPPP\fR work with stdin and stdout.
You can also telnet to 3000 to get command mode control, as with
client-side \fIPPP\fR.
.LP

.SH SETTING IDLE TIMER

.LP
 To check/set idletimer, use the ``show timeout'' and ``set timeout'' commands.
.LP

 Ex. ppp ON tama> set timeout 600

.LP
 The timeout period is measured in seconds, the  default value for which is 180 or 3 min.
 To disable the idle timer function, ``set timeout 0''.
.LP

.LP
 In -auto mode, an idle timeout causes the \fIPPP\fR session to be closed, though
the \fIPPP\fR program itself remains running.  Another trigger packet will cause it
to attempt to reestablish the link.
.LP

.SH Predictor-1 compression

.LP
 This version supports CCP and Predictor type 1 compression based on
the current IETF-draft specs. As a default behavior, \fIPPP\fR will
attempt to use (or be willing to accept) this capability when the
peer agrees (or requests it).
.LP

.LP
 To disable CCP/predictor functionality completely, use the ``disable pred''
and ``deny pred'' commands.
.LP

.SH Controlling IP address

.LP
 \fIPPP\fR uses IPCP to negotiate IP addresses. Each side of the connection
specifies the IP address that it's willing to use, and if the requested
IP address is acceptable then \fIPPP\fR returns ACK to the requester.
Otherwise, \fIPPP\fR returns NAK to suggest that the peer use a
different IP address. When both sides of the connection agree to accept the
received request (and send ACK), IPCP is set to the open state and
a network level connection is established.


.LP
 To control this IPCP behavior, this implementation has the ``set ifaddr'' command
for defining the local and remote IP address:

	ifaddr src_addr dst_addr

.LP
Where, src_addr is the IP address that the local side is willing to use and
dst_addr is the IP address which the remote side should use.
.LP

ifaddr 192.244.177.38 192.244.177.2

For example, the above specification means:

o I strongly want to use 192.244.177.38 as my side. I'll disagree if the
peer suggests that I use another address.

o I strongly insist that peer use 192.244.177.2 as own side address and
don't permit it to use any IP address but 192.244.177.2.  When peer
request another IP address, I always suggest that it use 192.244.177.2.

o This is all fine when each side has a pre-determined IP address,
however it is often the case that one side is acting as a server which
controls all IP addresses and the other side should obey the direction from
it.  In order to allow more flexible behavior, `ifaddr' command
allows the user to specify IP address more loosely:

ifaddr 192.244.177.38/24 192.244.177.2/20

 Number followed by slash (/) represent the number of bits significant in
teh IP address. That is, the above example signifies that:

o I'd like to use 192.244.177.38 as my address if it is possible, but
I'll also accept any IP address between 192.244.177.0 and 192.244.177.255.
 
o I'd like to make him use 192.244.177.2 as his own address, but
I'll also permit him to use any IP address between 192.244.176.0 and
192.244.191.255.

o As you may have already noticed, 192.244.177.2 is equivalent to saying
192.244.177.2/32.

o As an exception, 0 is equivalent to 0.0.0.0/0, meaning that I have no preferred
IP address and will obey the remote peer's selection.

o 192.244.177.2/0 means that I'll accept/permit any IP address but
I'll try to insist that 192.244.177.2 be used first.

.SH Connecting with your service provider

.LP
  1) Describe provider's phone number in DialScript: Use the ``set dial'' or
     ``set phone'' commands.

  2) Describe login procedure in LoginScript: Use the ``set login'' command.

.TP
3) Use ``set ifaddr'' command to define the IP address.

 o If you know what IP address provider uses, then use it as the remote address.

 o If provider has assigned a particular IP address to you, then use it
   as your address.

 o If provider assigns your address dynamically, use 0 as your address.

 o If you have no idea which IP addresses to use, then try

	set ifaddr 0 0

.TP 2
4) If provider requests that you use PAP/CHAP authentication methods,
add the next lines to your ppp.conf file:

.TP 3
.B enable pap (or enable chap)
.TP 3
.B disable chap	(or disable pap)
.TP 3
.B set authname MyName
.TP 3
.B set authkey MyPassword
.TP 3

.LP
Please refer to /etc/ppp/ppp.conf.iij for some real examples.
.LP

.SH Logging facility

.LP
 \fI\fIPPP\fR\fR is able to generate the following log info into
/var/log/ppp.log:

.TP
.B Phase
Phase transition log output
.TP 
.B Chat
Generate Chat script trace log
.TP 
.B LQM
Generate LQR report
.TP 
.B LCP
Generate LCP/IPCP packet trace
.TP 
.B TCP/IP
Dump TCP/IP packet
.TP 
.B HDLC
Dump HDLC packet in hex
.TP 
.B Async
Dump async level packet in hex

.LP
``set debug'' command allows you to set logging output level, of which
multiple levels can be specified.  The default is equivalent to ``set
debug phase lcp''.

.SH MORE DETAILS

.TP 2
o Please read the Japanese doc for complete explanation.
It may not be useful for non-japanese readers, 
but examples in the document may help you to guess.

.TP 2
o
Please read example configuration files.

.TP 2
o
Use ``help'', ``show ?'' and ``set ?'' commands.

.TP 2
o NetBSD and BSDI-1.0 were supported in previous releases but are no
longer supported in this release.  Please contact the author if you
need old driver code.

.SH FILES
.LP
\fIPPP\fR may refer to three files: ppp.conf, ppp.linkup and ppp.secret.
These files are placed in /etc/ppp, but the user can create his own files
under his HOME directory as .ppp.conf,.ppp.linkup and .ppp.secret.
\fIPPP\fR will always try to consult the user's personal setup first.

.TP
.B $HOME/ppp/.ppp.[conf|linkup|secret]
User dependant configuration files.

.TP
.B /etc/ppp/ppp.conf
System default configuration file.

.TP
.B /etc/ppp/ppp.secret
An authorization file for each system.

.TP
.B /etc/ppp/ppp.linkup
A file to check when
.I ppp
establishes a network level connection.

.TP
.B /var/log/ppp.log
Logging and debugging information file.

.TP
.B /var/spool/lock/Lck..* 
tty port locking file.

.SH HISTORY
This program was submitted to the FreeBSD core team for FreeBSD-2.0.5 by Atsushi
Murai (amurai@spec.co.jp).

.SH AUTHORS
Toshiharu OHNO (tony-o@iij.ad.jp)

Jordan Hubbard (jkh@freebsd.org) - significantly edited this document.
