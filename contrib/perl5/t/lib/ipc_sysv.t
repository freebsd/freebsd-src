#!./perl

BEGIN {
    chdir 't' if -d 't';

    @INC = '../lib';

    require Config; import Config;

    unless ($Config{'d_msg'} eq 'define' &&
	    $Config{'d_sem'} eq 'define') {
	print "1..0\n";
	exit;
    }
}

# These constants are common to all tests.
# Later the sem* tests will import more for themselves.

use IPC::SysV qw(IPC_PRIVATE IPC_NOWAIT IPC_STAT IPC_RMID
		 S_IRWXU S_IRWXG S_IRWXO);
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

if ($Config{'d_msgget'} eq 'define' &&
    $Config{'d_msgctl'} eq 'define' &&
    $Config{'d_msgsnd'} eq 'define' &&
    $Config{'d_msgrcv'} eq 'define') {
    $msg = msgget(IPC_PRIVATE, S_IRWXU | S_IRWXG | S_IRWXO);
    # Very first time called after machine is booted value may be 0 
    die "msgget failed: $!\n" unless defined($msg) && $msg >= 0;

    print "ok 1\n";

    #Putting a message on the queue
    my $msgtype = 1;
    my $msgtext = "hello";

    msgsnd($msg,pack("L a*",$msgtype,$msgtext),0) or print "not ";
    print "ok 2\n";

    my $data;
    msgctl($msg,IPC_STAT,$data) or print "not ";
    print "ok 3\n";

    print "not " unless length($data);
    print "ok 4\n";

    my $msgbuf;
    msgrcv($msg,$msgbuf,256,0,IPC_NOWAIT) or print "not ";
    print "ok 5\n";

    my($rmsgtype,$rmsgtext) = unpack("L a*",$msgbuf);

    print "not " unless($rmsgtype == $msgtype && $rmsgtext eq $msgtext);
    print "ok 6\n";
} else {
    for (1..6) {
	print "ok $_\n"; # fake it
    }
}

if($Config{'d_semget'} eq 'define' &&
   $Config{'d_semctl'} eq 'define') {

    use IPC::SysV qw(IPC_CREAT GETALL SETALL);

    $sem = semget(IPC_PRIVATE, 10, S_IRWXU | S_IRWXG | S_IRWXO | IPC_CREAT);
    # Very first time called after machine is booted value may be 0 
    die "semget: $!\n" unless defined($sem) && $sem >= 0;

    print "ok 7\n";

    my $data;
    semctl($sem,0,IPC_STAT,$data) or print "not ";
    print "ok 8\n";

    print "not " unless length($data);
    print "ok 9\n";

    my $template;

    # Find the pack/unpack template capable of handling native C shorts.

    if      ($Config{shortsize} == 2) {
	$template = "s";
    } elsif ($Config{shortsize} == 4) {
	$template = "l";
    } elsif ($Config{shortsize} == 8) {
	# Try quad last because not supported everywhere.
	foreach my $t (qw(i q)) {
	    # We could trap the unsupported quad template with eval
	    # but if we get this far we should have quad support anyway.
	    if (length(pack($t, 0)) == 8) {
		$template = $t;
		last;
	    }
	}
    }

    die "$0: cannot pack native shorts\n" unless defined $template;

    $template .= "*";

    my $nsem = 10;

    semctl($sem,0,SETALL,pack($template,(0) x $nsem)) or print "not ";
    print "ok 10\n";

    $data = "";
    semctl($sem,0,GETALL,$data) or print "not ";
    print "ok 11\n";

    print "not " unless length($data) == length(pack($template,(0) x $nsem));
    print "ok 12\n";

    my @data = unpack($template,$data);

    my $adata = "0" x $nsem;

    print "not " unless @data == $nsem and join("",@data) eq $adata;
    print "ok 13\n";

    my $poke = 2;

    $data[$poke] = 1;
    semctl($sem,0,SETALL,pack($template,@data)) or print "not ";
    print "ok 14\n";
    
    $data = "";
    semctl($sem,0,GETALL,$data) or print "not ";
    print "ok 15\n";

    @data = unpack($template,$data);

    my $bdata = "0" x $poke . "1" . "0" x ($nsem-$poke-1);

    print "not " unless join("",@data) eq $bdata;
    print "ok 16\n";
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
