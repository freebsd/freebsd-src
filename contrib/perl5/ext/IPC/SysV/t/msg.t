use IPC::SysV qw(IPC_PRIVATE IPC_RMID IPC_NOWAIT IPC_STAT S_IRWXU S_IRWXG S_IRWXO);

use IPC::Msg;
#Creating a message queue

print "1..9\n";

$msq = new IPC::Msg(IPC_PRIVATE, S_IRWXU | S_IRWXG | S_IRWXO)
	|| die "msgget: ",$!+0," $!\n";
	
print "ok 1\n";

#Putting a message on the queue
$msgtype = 1;
$msg = "hello";
$msq->snd($msgtype,$msg,0) || print "not ";
print "ok 2\n";

#Check if there are messages on the queue
$ds = $msq->stat() or print "not ";
print "ok 3\n";

print "not " unless $ds && $ds->qnum() == 1;
print "ok 4\n";

#Retreiving a message from the queue
$rmsgtype = 0; # Give me any type
$rmsgtype = $msq->rcv($rmsg,256,$rmsgtype,IPC_NOWAIT) || print "not ";
print "ok 5\n";

print "not " unless $rmsgtype == $msgtype && $rmsg eq $msg;
print "ok 6\n";

$ds = $msq->stat() or print "not ";
print "ok 7\n";

print "not " unless $ds && $ds->qnum() == 0;
print "ok 8\n";

$msq->remove || print "not ";
print "ok 9\n";
