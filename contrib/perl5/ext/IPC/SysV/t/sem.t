
use IPC::SysV qw(
	SETALL
	IPC_PRIVATE
	IPC_CREAT
	IPC_RMID
	IPC_NOWAIT
	IPC_STAT
	S_IRWXU
	S_IRWXG
	S_IRWXO
);
use IPC::Semaphore;

print "1..10\n";

$sem = new IPC::Semaphore(IPC_PRIVATE, 10, S_IRWXU | S_IRWXG | S_IRWXO | IPC_CREAT)
	|| die "semget: ",$!+0," $!\n";

print "ok 1\n";

my $st = $sem->stat || print "not ";
print "ok 2\n";

$sem->setall( (0) x 10) || print "not ";
print "ok 3\n";

my @sem = $sem->getall;
print "not " unless join("",@sem) eq "0000000000";
print "ok 4\n";

$sem[2] = 1;
$sem->setall( @sem ) || print "not ";
print "ok 5\n";

@sem = $sem->getall;
print "not " unless join("",@sem) eq "0010000000";
print "ok 6\n";

my $ncnt = $sem->getncnt(0);
print "not " if $sem->getncnt(0) || !defined($ncnt);
print "ok 7\n";

$sem->op(2,-1,IPC_NOWAIT) || print "not ";
print "ok 8\n";

print "not " if $sem->getncnt(0);
print "ok 9\n";

$sem->remove || print "not ";
print "ok 10\n";
