.TL
OpenSSH Channel Close Protocol 2.0 Implementation
.SH
Channel Input State Diagram
.PS
reset
l=1
s=1.2
ellipsewid=s*ellipsewid
boxwid=s*boxwid
ellipseht=s*ellipseht
S1: ellipse "INPUT" "OPEN"
move right 2*l from last ellipse.e
S3: ellipse invis
move down l from last ellipse.s
S4: ellipse "INPUT" "CLOSED"
move down l from 1st ellipse.s
S2: ellipse "INPUT" "WAIT" "DRAIN"
arrow from S1.e to S4.n
box invis "rcvd CLOSE/" "shutdown_read" with .sw at last arrow.c
arrow "ibuf_empty ||" "rcvd CLOSE/" "send EOF" "" from S2.e to S4.w
arrow from S1.s to S2.n
box invis "read_failed/" "shutdown_read" with .e at last arrow.c
ellipse wid .9*ellipsewid ht .9*ellipseht at S4
arrow "start" "" from S1.w+(-0.5,0) to S1.w
.PE
.SH
Channel Output State Diagram
.PS
S1: ellipse "OUTPUT" "OPEN"
move right 2*l from last ellipse.e
S3: ellipse invis
move down l from last ellipse.s
S4: ellipse "OUTPUT" "CLOSED"
move down l from 1st ellipse.s
S2: ellipse "OUTPUT" "WAIT" "DRAIN"
arrow from S1.e to S4.n
box invis "write_failed/" "shutdown_write" with .sw at last arrow.c
arrow "obuf_empty ||" "write_failed/" "shutdown_write" "" from S2.e to S4.w
arrow from S1.s to S2.n
box invis "rcvd EOF ||" "rcvd CLOSE/" "-" with .e at last arrow.c
ellipse wid .9*ellipsewid ht .9*ellipseht at S4
arrow "start" "" from S1.w+(-0.5,0) to S1.w
.PE
.SH
Notes
.PP
The input buffer is filled with data from the socket
(the socket represents the local consumer/producer of the
forwarded channel).
The data is then sent over the INPUT-end (transmit-end) of the channel to the
remote peer.
Data sent by the peer is received on the OUTPUT-end (receive-end),
saved in the output buffer and written to the socket.
.PP
If the local protocol instance has forwarded all data on the
INPUT-end of the channel, it sends an EOF message to the peer.
.PP
A CLOSE message is sent to the peer if
both the INPUT- and the OUTOUT-half of the local
end of the channel are closed.
.PP
The channel can be deallocated by a protocol instance
if a CLOSE message he been both sent and received.
