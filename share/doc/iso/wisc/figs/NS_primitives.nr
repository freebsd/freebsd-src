.(b
.TS
tab(+) center box;
c c 
a | a .
service primitive & arguments+kernel procedure call & arguments
=
N_CONNECT.request+\fIcons_openvc(copcb,dstaddr,so)\fR
called address+argument \fIdstaddr\fR
calling address, expedited data selection+not implemented
receipt confirmation selection+not implemented
quality of service, NS-user data+not implemented
_
N_CONNECT.indication+not implemented
_
N_CONNECT.response+not implemented
_
N_CONNECT.confirm+return from \fIcons_openvc()\fR
responding address, quality of service+not implemented
receipt confirmation selection+not implemented
expedited data selection, NS-user data+not implemented
=
N_DATA.request+\fIcons_output(isop,m,len,isdgm)\fR, and
 +\fIcosns_output(ifp,m,dstaddr)\fR
NS-user data+argument m (mbuf chain)
confirmation request+not implemented
_
N_DATA.indication+software interrupt (CLNP), procedure
+call to \fItp_input()\fR
NS-user data+mbuf chain on \fIclnlintrq\fR or
+argument to \fItp_input()\fR
confirmation request+not implemented
=
N_DATA_ACKNOWLEDGE.request+not implemented
_
N_DATA_ACKNOWLEDGE.indication+not implemented
=
N_EXPEDITED_DATA.request+not implemented
_
N_EXPEDITED_DATA.indication+not implemented
=
N_RESET.request+not implemented
_
N_RESET.response+not implemented
_
N_RESET.indication+higher layer \fIpr_ctlinput(
+PRC_ROUTEDEAD, faddr, copcb)\fR
originator+argument \fIfaddr\fR
reason+implemented with so->so_errno for sockets
+that are attached to CONS PCBs
_
N_RESET.confirm+not implemented
=
N_DISCONNECT.request+\fIcons_netcmd(CONN_CLOSE,
+isop, channel, isdgm)\fR
reason, NS-user data, responding address+not implemented
_
N_DISCONNECT.indication+higher layer \fIpr_ctlinput(
+PRC_ROUTEDEAD, faddr, copcb)\fR
originator+argument \fIfaddr\fR
reason+implemented with so->so_errno for sockets
+that are attached to CONS PCBs
NS-user data, responding address+not implemented
.TE
.(c
\fBFigure \n+(FG\fR: Network Service Primitives
.\")
.)c
.)b
