#!./perl

BEGIN {
    chdir 't' if -d 't';

    @INC = '../lib';

    require Config; import Config;

    my $reason;

    if ($Config{'extensions'} !~ /\bIPC\/SysV\b/) {
      $reason = 'IPC::SysV was not built';
    } elsif ($Config{'d_sem'} ne 'define') {
      $reason = '$Config{d_sem} undefined';
    } elsif ($Config{'d_msg'} ne 'define') {
      $reason = '$Config{d_msg} undefined';
    }
    if ($reason) {
	print "1..0 # Skip: $reason\n";
	exit 0;
    }
}

# These constants are common to all tests.
# Later the sem* tests will import more for themselves.

use IPC::SysV qw(IPC_PRIVATE IPC_NOWAIT IPC_STAT IPC_RMID S_IRWXU);
use strict;

print "1..16\n";

my $msg;
my $sem;

$SIG{__DIE__} = 'cleanup'; # will cleanup $msg and $sem if needed

# FreeBSD is known to throw this if there's no SysV IPC in the kernel.
$SIG{SYS} = sub {
    print STDERR <<EOM;
SIGSYS caught.
It may be that your kernel does not have SysV IPC configured.

EOM
    if ($^O eq 'freebsd') {
	print STDERR <<EOM;
You must have following options in your kernel:

options         SYSVSHM
options         SYSVSEM
options         SYSVMSG

See config(8).
EOM
    }
    exit(1);
};

my $perm = S_IRWXU;

if ($Config{'d_msgget'} eq 'define' &&
    $Config{'d_msgctl'} eq 'define' &&
    $Config{'d_msgsnd'} eq 'define' &&
    $Config{'d_msgrcv'} eq 'define') {

    $msg = msgget(IPC_PRIVATE, $perm);
    # Very first time called after machine is booted value may be 0 
    die "msgget failed: $!\n" unless defined($msg) && $msg >= 0;

    print "ok 1\n";

    #Putting a message on the queue
    my $msgtype = 1;
    my $msgtext = "hello";

    my $test2bad;
    my $test5bad;
    my $test6bad;

    unless (msgsnd($msg,pack("L! a*",$msgtype,$msgtext),IPC_NOWAIT)) {
	print "not ";
	$test2bad = 1;
    }
    print "ok 2\n";
    if ($test2bad) {
	print <<EOM;
#
# The failure of the subtest #2 may indicate that the message queue
# resource limits either of the system or of the testing account
# have been reached.  Error message "Operating would block" is
# usually indicative of this situation.  The error message was now:
# "$!"
#
# You can check the message queues with the 'ipcs' command and
# you can remove unneeded queues with the 'ipcrm -q id' command.
# You may also consider configuring your system or account
# to have more message queue resources.
#
# Because of the subtest #2 failing also the substests #5 and #6 will
# very probably also fail.
#
EOM
    }

    my $data;
    msgctl($msg,IPC_STAT,$data) or print "not ";
    print "ok 3\n";

    print "not " unless length($data);
    print "ok 4\n";

    my $msgbuf;
    unless (msgrcv($msg,$msgbuf,256,0,IPC_NOWAIT)) {
	print "not ";
	$test5bad = 1;
    }
    print "ok 5\n";
    if ($test5bad && $test2bad) {
	print <<EOM;
#
# This failure was to be expected because the subtest #2 failed.
#
EOM
    }

    my($rmsgtype,$rmsgtext);
    ($rmsgtype,$rmsgtext) = unpack("L! a*",$msgbuf);
    unless ($rmsgtype == $msgtype && $rmsgtext eq $msgtext) {
	print "not ";
	$test6bad = 1;
    }
    print "ok 6\n";
    if ($test6bad && $test2bad) {
	print <<EOM;
#
# This failure was to be expected because the subtest #2 failed.
#
EOM
     }
} else {
    for (1..6) {
	print "ok $_\n"; # fake it
    }
}

if($Config{'d_semget'} eq 'define' &&
   $Config{'d_semctl'} eq 'define') {

    if ($Config{'d_semctl_semid_ds'} eq 'define' ||
	$Config{'d_semctl_semun'}    eq 'define') {

	use IPC::SysV qw(IPC_CREAT GETALL SETALL);

	$sem = semget(IPC_PRIVATE, 10, $perm | IPC_CREAT);
	# Very first time called after machine is booted value may be 0 
	die "semget: $!\n" unless defined($sem) && $sem >= 0;

	print "ok 7\n";

	my $data;
	semctl($sem,0,IPC_STAT,$data) or print "not ";
	print "ok 8\n";
	
	print "not " unless length($data);
	print "ok 9\n";

	my $nsem = 10;

	semctl($sem,0,SETALL,pack("s!*",(0) x $nsem)) or print "not ";
	print "ok 10\n";

	$data = "";
	semctl($sem,0,GETALL,$data) or print "not ";
	print "ok 11\n";

	print "not " unless length($data) == length(pack("s!*",(0) x $nsem));
	print "ok 12\n";

	my @data = unpack("s!*",$data);

	my $adata = "0" x $nsem;

	print "not " unless @data == $nsem and join("",@data) eq $adata;
	print "ok 13\n";

	my $poke = 2;

	$data[$poke] = 1;
	semctl($sem,0,SETALL,pack("s!*",@data)) or print "not ";
	print "ok 14\n";
    
	$data = "";
	semctl($sem,0,GETALL,$data) or print "not ";
	print "ok 15\n";

	@data = unpack("s!*",$data);

	my $bdata = "0" x $poke . "1" . "0" x ($nsem-$poke-1);

	print "not " unless join("",@data) eq $bdata;
	print "ok 16\n";
    } else {
	for (7..16) {
	    print "ok $_ # skipped, no semctl possible\n";
	}
    }
} else {
    for (7..16) {
	print "ok $_\n"; # fake it
    }
}

sub cleanup {
    msgctl($msg,IPC_RMID,0)       if defined $msg;
    semctl($sem,0,IPC_RMID,undef) if defined $sem;
}

cleanup;
