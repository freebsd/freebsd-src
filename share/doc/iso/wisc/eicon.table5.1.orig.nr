.TS
center,box,tab(:);
c s s s s s s
c|c s s|c s|c
c|c|c|c|c|c|c
l|c|c|c|l|l|l.
\fBNormal Completion Handling\fR
_
\fINCB:Usage:Options:Action Based on Normal Competion of\fR
\fICommand:CONS\(->Driver:Driver\(->Board:Driver\(->CONS:To Board:From Board:Driver\(->Board Command\fR
=
\fINCB_RESTART\fR:T{
.na
(ECN_RESTART) 
T}:\(sr::none:none:T{
.na
dequeue the request, and issue an NCB_LISTEN request to the board.
T}
_
\fINCB_CALL\fR:(ECN_CALL):\(sr:T{
.na  
(ECN_ACCEPT) 
T}:none:connected:T{
.na
dequeue the request, pass an ECN_ACCEPT reply to CONS, and issue a RECEIVE to
the board.
T}
_
\fINCB_LISTEN\fR::\(sr:T{
.na  
(ECN_CONNECT)
T}:T{
.na
use zero-length Call User Data and a zero-length Calling DTE address
T}:none:T{
.na
dequeue the request, pass an ECN_CONNECT to CONS, and issue a RECEIVE to the
board. Re-issue another NCB_LISTEN
for another possible virtual circuit connection.
T}
_
\fINCB_CLEAR\fR:(ECN_CLEAR):\(sr:(ECN_CLEAR):T{
.na
normal clearing with all outstanding ACKs returned 
T}:none:T{
.na
dequeue the request.
T}
:_:_:_:_:_:_
::\(sr::T{
.na
immediate clearing 
T}:none:T{
.na
dequeue the request.
T}
_
\fINCB_SEND\fR:(ECN_SEND):\(sr::T{
.na
normal send
T}:none:T{
.na
dequeue the request and reply to CONS with an ECN_ACK.
T}
:_:_:_:_:_:_
:T{
.na  
(ECN_RESET)
T}:\(sr::T{
.na
reset the virtual circuit
T}:none:T{
dequeue the request.
T}
_
\fINCB_RECEIVE\fR::\(sr:(ECN_RECEIVE):none:T{
.na
normal, uncomplicated receive
T}:T{
.na
dequeue the request and bcopy the data into the request's associated mbuf. Ship to CONS. Re-issue another NCB_RECEIVE.
T}
:_:_:_:_:_:_
:::(ECN_RECEIVE):none:T{
.na
m-bit set
T}:T{
.na
same as above (board does the resegmentation automatically).
T}
:_:_:_:_:_:_
:::(ECN_RECEIVE):none:T{
.na
d-bit set
T}:T{
.na
same as above.
T}
:_:_:_:_:_:_
:::(ECN_RECEIVE):none:T{
.na
q-bit set
T}:T{
.na
same as above.
T}
:_:_:_:_:_:_
:::(ECN_RECEIVE):none:T{
.na
interrupt received
T}:T{
.na
same as above.
T}
:_:_:_:_:_:_
:::T{
.na   
(ECN_RESET)
T}:none:T{
.na
reset received
T}:T{
dequeue the request, send an ECN_RESET back up to CONS, and issue another 
receive.
T}
.TE
