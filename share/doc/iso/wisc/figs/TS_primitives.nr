.(b
.TS
center expand box;
c c 
a | a .
service primitive & arguments	Unix system calls & arguments
=
T_CONNECT.request	\fIsocket(), connect(), setsockopt()\fR
called address	\fIconnect()\fR argument
calling address	\fIconnect()\fR argument
quality of service	not implemented
buffer management	\fIsetsockopt()\fR argument 
security	not implemented
data	\fIsetsockopt(), getsockopt()\fR
_
T_CONNECT.indication	return from \fIaccept(); getsockopt()\fR
called address	\fIaccept()\fR argument
calling address	\fIaccept()\fR argument
quality of service	not implemented
security	not implemented
data	\fIsetsockopt(), getsockopt()\fR
_
T_CONNECT.response	no applicable system calls
_
T_CONNECT.confirm	return from \fIconnect()\fR
quality of service	\fIgetsockopt()\fR argument
data	\fIsetsocktopt, getsockopt()\fR
=
T_DATA.request	\fIrecvv(), sendv()\fR
_
T_DATA.indication	return from \fIrecvv()\fR, \fIsendv()\fR, or \fIselect()\fR; 
	or signal SIGIO 
	ioctl(FIONREAD) tells how much has been
	queued to read
=
T_EXPEDITED_DATA.request	\fIsendv()\fR with MSG_OOB flag
_
T_EXPEDITED_DATA.indication	SIGURG, \fIgetsockopt()\fR with TPFLAG_XPD,
	return from \fIselect()\fR with exceptional 
	conditions mask
=
T_DISCONNECT.request	\fIclose()\fR
data	\fIsetsockopt()\fR
_
T_DISCONNECT.indication	SIGURG, 
	error return on other primitives
reason	 errno
data	\fIgetsockopt()\**\fR
=
T_STATUS.request	\fIgetsockopt()\fR, \fItpstat\fR utility program
_
T_STATUS.indication	\fIgetsockopt()\fR, \fIselect()\fR, \fItpstat\fR 
.TE
.(c
\fBFigure \n+(FG\fR: Transport Service Primitives
.)c
.)b
.(f
\** data on disconnect is not supported at this time.
.)f
