.(b
.TS
tab(+) center expand box;
c c 
a | a .
service primitive & arguments+provided by
=
N_CONNECT.request+cons_openvc(... faddr, ...)
called address+argument faddr
calling address+not implemented
receipt confirmation+not implemented
expedited data+not implemented
quality of service+not implemented
NS-user data+not implemented
_
N_CONNECT.indication+not implemented
_
N_CONNECT.response+cons_netcmd( CONN_REFUSE )
+ or cons_netcmd( CONN_CONFIRM )
+ however, net connection has already
+ been accepted.  If REFUSE, it will
+ be cleared with E_CO_HLI_REJT
+ (higher layer rejects connection)
responding address+not implemented
receipt confirmation+not implemented
expedited data+not implemented
quality of service+not implemented
NS-user data+not implemented
_
N_CONNECT.confirm+not implemented
=
N_DATA.request+cons_output(... m, ...) 
+and cosns_output(... m, ...)
confirmation+not implemented
data+mbuf chain m
_
N_DATA.indication+pr_input( m, ... )
+or software interrupt
confirmation+not implemented
data+mbuf chain
_
N_DATA_ACKNOWLEDGE.request+not implemented
_
N_DATA_ACKNOWLEDGE.indication+not implemented
_
N_EXPEDITED_DATA.request+not implemented
_
N_EXPEDITED_DATA.indication+not implemented
=
N_RESET.request+not implemented
N_RESET.indication+socket->so_error = reason
+or pr_ctlinput( PRC_ROUTEDEAD )
originator+not implemented
reason+from X.25 packet or ecn driver
N_RESET.response+not implemented
N_RESET.confirm+not implemented
=
N_DISCONNECT.request+cons_netcmd( CONN_CLOSE )
reason+uses E_CO_HLI_DISCN (normal 
+disconnect from higher layer)
responding address+not implemented
NS_user data+not implemented
_
N_DISCONNECT.indication+socket->so_error = reason
+or pr_ctlinput( PRC_ROUTEDEAD )
originator+not implemented
reason+from X.25 packet or ecn driver
responding address+not implemented
NS_user data+not implemented
.TE
.(c
\fBFigure \n+(FG\fR: Transport Service Primitives
.)c
.)b
.(f
\** data on disconnect is not supported at this time.
.)f
