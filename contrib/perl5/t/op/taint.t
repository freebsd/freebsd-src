#!./perl -T
#
# Taint tests by Tom Phoenix <rootbeer@teleport.com>.
#
# I don't claim to know all about tainting. If anyone sees
# tests that I've missed here, please add them. But this is
# better than having no tests at all, right?
#

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
}

use strict;
use Config;

# We do not want the whole taint.t to fail
# just because Errno possibly failing.
eval { require Errno; import Errno };

use vars qw($ipcsysv); # did we manage to load IPC::SysV?

BEGIN {
  if ($^O eq 'VMS' && !defined($Config{d_setenv})) {
      $ENV{PATH} = $ENV{PATH};
      $ENV{TERM} = $ENV{TERM} ne ''? $ENV{TERM} : 'dummy';
  }
  if ($Config{'extensions'} =~ /\bIPC\/SysV\b/
      && ($Config{d_shm} || $Config{d_msg})) {
      eval { require IPC::SysV };
      unless ($@) {
	  $ipcsysv++;
	  IPC::SysV->import(qw(IPC_PRIVATE IPC_RMID IPC_CREAT S_IRWXU));
      }
  }
}

my $Is_VMS = $^O eq 'VMS';
my $Is_MSWin32 = $^O eq 'MSWin32';
my $Is_Dos = $^O eq 'dos';
my $Invoke_Perl = $Is_VMS ? 'MCR Sys$Disk:[]Perl.' :
                  $Is_MSWin32 ? '.\perl' : './perl';
my @MoreEnv = qw/IFS CDPATH ENV BASH_ENV/;

if ($Is_VMS) {
    my (%old, $x);
    for $x ('DCL$PATH', @MoreEnv) {
	($old{$x}) = $ENV{$x} =~ /^(.*)$/ if exists $ENV{$x};
    }
    eval <<EndOfCleanup;
	END {
	    \$ENV{PATH} = '' if $Config{d_setenv};
	    warn "# Note: logical name 'PATH' may have been deleted\n";
	    \@ENV{keys %old} = values %old;
	}
EndOfCleanup
}

# Sources of taint:
#   The empty tainted value, for tainting strings
my $TAINT = substr($^X, 0, 0);
#   A tainted zero, useful for tainting numbers
my $TAINT0 = 0 + $TAINT;

# This taints each argument passed. All must be lvalues.
# Side effect: It also stringifies them. :-(
sub taint_these (@) {
    for (@_) { $_ .= $TAINT }
}

# How to identify taint when you see it
sub any_tainted (@) {
    not eval { join("",@_), kill 0; 1 };
}
sub tainted ($) {
    any_tainted @_;
}
sub all_tainted (@) {
    for (@_) { return 0 unless tainted $_ }
    1;
}

sub test ($$;$) {
    my($serial, $boolean, $diag) = @_;
    if ($boolean) {
	print "ok $serial\n";
    } else {
	print "not ok $serial\n";
	for (split m/^/m, $diag) {
	    print "# $_";
	}
	print "\n" unless
	    $diag eq ''
	    or substr($diag, -1) eq "\n";
    }
}

# We need an external program to call.
my $ECHO = ($Is_MSWin32 ? ".\\echo$$" : "./echo$$");
END { unlink $ECHO }
open PROG, "> $ECHO" or die "Can't create $ECHO: $!";
print PROG 'print "@ARGV\n"', "\n";
close PROG;
my $echo = "$Invoke_Perl $ECHO";

print "1..155\n";

# First, let's make sure that Perl is checking the dangerous
# environment variables. Maybe they aren't set yet, so we'll
# taint them ourselves.
{
    $ENV{'DCL$PATH'} = '' if $Is_VMS;

    $ENV{PATH} = '';
    delete @ENV{@MoreEnv};
    $ENV{TERM} = 'dumb';

    test 1, eval { `$echo 1` } eq "1\n";

    if ($Is_MSWin32 || $Is_VMS || $Is_Dos) {
	print "# Environment tainting tests skipped\n";
	for (2..5) { print "ok $_\n" }
    }
    else {
	my @vars = ('PATH', @MoreEnv);
	while (my $v = $vars[0]) {
	    local $ENV{$v} = $TAINT;
	    last if eval { `$echo 1` };
	    last unless $@ =~ /^Insecure \$ENV{$v}/;
	    shift @vars;
	}
	test 2, !@vars, "\$$vars[0]";

	# tainted $TERM is unsafe only if it contains metachars
	local $ENV{TERM};
	$ENV{TERM} = 'e=mc2';
	test 3, eval { `$echo 1` } eq "1\n";
	$ENV{TERM} = 'e=mc2' . $TAINT;
	test 4, eval { `$echo 1` } eq '';
	test 5, $@ =~ /^Insecure \$ENV{TERM}/, $@;
    }

    my $tmp;
    if ($^O eq 'os2' || $^O eq 'amigaos' || $Is_MSWin32 || $Is_Dos) {
	print "# all directories are writeable\n";
    }
    else {
	$tmp = (grep { defined and -d and (stat _)[2] & 2 }
		     qw(sys$scratch /tmp /var/tmp /usr/tmp),
		     @ENV{qw(TMP TEMP)})[0]
	    or print "# can't find world-writeable directory to test PATH\n";
    }

    if ($tmp) {
	local $ENV{PATH} = $tmp;
	test 6, eval { `$echo 1` } eq '';
	test 7, $@ =~ /^Insecure directory in \$ENV{PATH}/, $@;
    }
    else {
	for (6..7) { print "ok $_ # Skipped: all directories are writeable\n" }
    }

    if ($Is_VMS) {
	$ENV{'DCL$PATH'} = $TAINT;
	test 8,  eval { `$echo 1` } eq '';
	test 9, $@ =~ /^Insecure \$ENV{DCL\$PATH}/, $@;
	if ($tmp) {
	    $ENV{'DCL$PATH'} = $tmp;
	    test 10, eval { `$echo 1` } eq '';
	    test 11, $@ =~ /^Insecure directory in \$ENV{DCL\$PATH}/, $@;
	}
	else {
	    for (10..11) { print "ok $_ # Skipped: can't find world-writeable directory to test DCL\$PATH\n" }
	}
	$ENV{'DCL$PATH'} = '';
    }
    else {
	for (8..11) { print "ok $_ # Skipped: This is not VMS\n"; }
    }
}

# Let's see that we can taint and untaint as needed.
{
    my $foo = $TAINT;
    test 12, tainted $foo;

    # That was a sanity check. If it failed, stop the insanity!
    die "Taint checks don't seem to be enabled" unless tainted $foo;

    $foo = "foo";
    test 13, not tainted $foo;

    taint_these($foo);
    test 14, tainted $foo;

    my @list = 1..10;
    test 15, not any_tainted @list;
    taint_these @list[1,3,5,7,9];
    test 16, any_tainted @list;
    test 17, all_tainted @list[1,3,5,7,9];
    test 18, not any_tainted @list[0,2,4,6,8];

    ($foo) = $foo =~ /(.+)/;
    test 19, not tainted $foo;

    $foo = $1 if ('bar' . $TAINT) =~ /(.+)/;
    test 20, not tainted $foo;
    test 21, $foo eq 'bar';

    {
      use re 'taint';

      ($foo) = ('bar' . $TAINT) =~ /(.+)/;
      test 22, tainted $foo;
      test 23, $foo eq 'bar';

      $foo = $1 if ('bar' . $TAINT) =~ /(.+)/;
      test 24, tainted $foo;
      test 25, $foo eq 'bar';
    }

    $foo = $1 if 'bar' =~ /(.+)$TAINT/;
    test 26, tainted $foo;
    test 27, $foo eq 'bar';

    my $pi = 4 * atan2(1,1) + $TAINT0;
    test 28, tainted $pi;

    ($pi) = $pi =~ /(\d+\.\d+)/;
    test 29, not tainted $pi;
    test 30, sprintf("%.5f", $pi) eq '3.14159';
}

# How about command-line arguments? The problem is that we don't
# always get some, so we'll run another process with some.
{
    my $arg = "./arg$$";
    open PROG, "> $arg" or die "Can't create $arg: $!";
    print PROG q{
	eval { join('', @ARGV), kill 0 };
	exit 0 if $@ =~ /^Insecure dependency/;
	print "# Oops: \$@ was [$@]\n";
	exit 1;
    };
    close PROG;
    print `$Invoke_Perl "-T" $arg and some suspect arguments`;
    test 31, !$?, "Exited with status $?";
    unlink $arg;
}

# Reading from a file should be tainted
{
    my $file = './TEST';
    test 32, open(FILE, $file), "Couldn't open '$file': $!";

    my $block;
    sysread(FILE, $block, 100);
    my $line = <FILE>;
    close FILE;
    test 33, tainted $block;
    test 34, tainted $line;
}

# Globs should be forbidden, except under VMS,
#   which doesn't spawn an external program.
if (1  # built-in glob
    or $Is_VMS) {
    for (35..36) { print "ok $_\n"; }
}
else {
    my @globs = eval { <*> };
    test 35, @globs == 0 && $@ =~ /^Insecure dependency/;

    @globs = eval { glob '*' };
    test 36, @globs == 0 && $@ =~ /^Insecure dependency/;
}

# Output of commands should be tainted
{
    my $foo = `$echo abc`;
    test 37, tainted $foo;
}

# Certain system variables should be tainted
{
    test 38, all_tainted $^X, $0;
}

# Results of matching should all be untainted
{
    my $foo = "abcdefghi" . $TAINT;
    test 39, tainted $foo;

    $foo =~ /def/;
    test 40, not any_tainted $`, $&, $';

    $foo =~ /(...)(...)(...)/;
    test 41, not any_tainted $1, $2, $3, $+;

    my @bar = $foo =~ /(...)(...)(...)/;
    test 42, not any_tainted @bar;

    test 43, tainted $foo;	# $foo should still be tainted!
    test 44, $foo eq "abcdefghi";
}

# Operations which affect files can't use tainted data.
{
    test 45, eval { chmod 0, $TAINT } eq '', 'chmod';
    test 46, $@ =~ /^Insecure dependency/, $@;

    # There is no feature test in $Config{} for truncate,
    #   so we allow for the possibility that it's missing.
    test 47, eval { truncate 'NoSuChFiLe', $TAINT0 } eq '', 'truncate';
    test 48, $@ =~ /^(?:Insecure dependency|truncate not implemented)/, $@;

    test 49, eval { rename '', $TAINT } eq '', 'rename';
    test 50, $@ =~ /^Insecure dependency/, $@;

    test 51, eval { unlink $TAINT } eq '', 'unlink';
    test 52, $@ =~ /^Insecure dependency/, $@;

    test 53, eval { utime $TAINT } eq '', 'utime';
    test 54, $@ =~ /^Insecure dependency/, $@;

    if ($Config{d_chown}) {
	test 55, eval { chown -1, -1, $TAINT } eq '', 'chown';
	test 56, $@ =~ /^Insecure dependency/, $@;
    }
    else {
	for (55..56) { print "ok $_ # Skipped: chown() is not available\n" }
    }

    if ($Config{d_link}) {
	test 57, eval { link $TAINT, '' } eq '', 'link';
	test 58, $@ =~ /^Insecure dependency/, $@;
    }
    else {
	for (57..58) { print "ok $_ # Skipped: link() is not available\n" }
    }

    if ($Config{d_symlink}) {
	test 59, eval { symlink $TAINT, '' } eq '', 'symlink';
	test 60, $@ =~ /^Insecure dependency/, $@;
    }
    else {
	for (59..60) { print "ok $_ # Skipped: symlink() is not available\n" }
    }
}

# Operations which affect directories can't use tainted data.
{
    test 61, eval { mkdir $TAINT0, $TAINT } eq '', 'mkdir';
    test 62, $@ =~ /^Insecure dependency/, $@;

    test 63, eval { rmdir $TAINT } eq '', 'rmdir';
    test 64, $@ =~ /^Insecure dependency/, $@;

    test 65, eval { chdir $TAINT } eq '', 'chdir';
    test 66, $@ =~ /^Insecure dependency/, $@;

    if ($Config{d_chroot}) {
	test 67, eval { chroot $TAINT } eq '', 'chroot';
	test 68, $@ =~ /^Insecure dependency/, $@;
    }
    else {
	for (67..68) { print "ok $_ # Skipped: chroot() is not available\n" }
    }
}

# Some operations using files can't use tainted data.
{
    my $foo = "imaginary library" . $TAINT;
    test 69, eval { require $foo } eq '', 'require';
    test 70, $@ =~ /^Insecure dependency/, $@;

    my $filename = "./taintB$$";	# NB: $filename isn't tainted!
    END { unlink $filename if defined $filename }
    $foo = $filename . $TAINT;
    unlink $filename;	# in any case

    test 71, eval { open FOO, $foo } eq '', 'open for read';
    test 72, $@ eq '', $@;		# NB: This should be allowed

    # Try first new style but allow also old style.
    test 73, $!{ENOENT} ||
	$! == 2 || # File not found
	($Is_Dos && $! == 22) ||
	($^O eq 'mint' && $! == 33);

    test 74, eval { open FOO, "> $foo" } eq '', 'open for write';
    test 75, $@ =~ /^Insecure dependency/, $@;
}

# Commands to the system can't use tainted data
{
    my $foo = $TAINT;

    if ($^O eq 'amigaos') {
	for (76..79) { print "ok $_ # Skipped: open('|') is not available\n" }
    }
    else {
	test 76, eval { open FOO, "| x$foo" } eq '', 'popen to';
	test 77, $@ =~ /^Insecure dependency/, $@;

	test 78, eval { open FOO, "x$foo |" } eq '', 'popen from';
	test 79, $@ =~ /^Insecure dependency/, $@;
    }

    test 80, eval { exec $TAINT } eq '', 'exec';
    test 81, $@ =~ /^Insecure dependency/, $@;

    test 82, eval { system $TAINT } eq '', 'system';
    test 83, $@ =~ /^Insecure dependency/, $@;

    $foo = "*";
    taint_these $foo;

    test 84, eval { `$echo 1$foo` } eq '', 'backticks';
    test 85, $@ =~ /^Insecure dependency/, $@;

    if ($Is_VMS) { # wildcard expansion doesn't invoke shell, so is safe
	test 86, join('', eval { glob $foo } ) ne '', 'globbing';
	test 87, $@ eq '', $@;
    }
    else {
	for (86..87) { print "ok $_ # Skipped: this is not VMS\n"; }
    }
}

# Operations which affect processes can't use tainted data.
{
    test 88, eval { kill 0, $TAINT } eq '', 'kill';
    test 89, $@ =~ /^Insecure dependency/, $@;

    if ($Config{d_setpgrp}) {
	test 90, eval { setpgrp 0, $TAINT } eq '', 'setpgrp';
	test 91, $@ =~ /^Insecure dependency/, $@;
    }
    else {
	for (90..91) { print "ok $_ # Skipped: setpgrp() is not available\n" }
    }

    if ($Config{d_setprior}) {
	test 92, eval { setpriority 0, $TAINT, $TAINT } eq '', 'setpriority';
	test 93, $@ =~ /^Insecure dependency/, $@;
    }
    else {
	for (92..93) { print "ok $_ # Skipped: setpriority() is not available\n" }
    }
}

# Some miscellaneous operations can't use tainted data.
{
    if ($Config{d_syscall}) {
	test 94, eval { syscall $TAINT } eq '', 'syscall';
	test 95, $@ =~ /^Insecure dependency/, $@;
    }
    else {
	for (94..95) { print "ok $_ # Skipped: syscall() is not available\n" }
    }

    {
	my $foo = "x" x 979;
	taint_these $foo;
	local *FOO;
	my $temp = "./taintC$$";
	END { unlink $temp }
	test 96, open(FOO, "> $temp"), "Couldn't open $temp for write: $!";

	test 97, eval { ioctl FOO, $TAINT, $foo } eq '', 'ioctl';
	test 98, $@ =~ /^Insecure dependency/, $@;

	if ($Config{d_fcntl}) {
	    test 99, eval { fcntl FOO, $TAINT, $foo } eq '', 'fcntl';
	    test 100, $@ =~ /^Insecure dependency/, $@;
	}
	else {
	    for (99..100) { print "ok $_ # Skipped: fcntl() is not available\n" }
	}

	close FOO;
    }
}

# Some tests involving references
{
    my $foo = 'abc' . $TAINT;
    my $fooref = \$foo;
    test 101, not tainted $fooref;
    test 102, tainted $$fooref;
    test 103, tainted $foo;
}

# Some tests involving assignment
{
    my $foo = $TAINT0;
    my $bar = $foo;
    test 104, all_tainted $foo, $bar;
    test 105, tainted($foo = $bar);
    test 106, tainted($bar = $bar);
    test 107, tainted($bar += $bar);
    test 108, tainted($bar -= $bar);
    test 109, tainted($bar *= $bar);
    test 110, tainted($bar++);
    test 111, tainted($bar /= $bar);
    test 112, tainted($bar += 0);
    test 113, tainted($bar -= 2);
    test 114, tainted($bar *= -1);
    test 115, tainted($bar /= 1);
    test 116, tainted($bar--);
    test 117, $bar == 0;
}

# Test assignment and return of lists
{
    my @foo = ("A", "tainted" . $TAINT, "B");
    test 118, not tainted $foo[0];
    test 119,     tainted $foo[1];
    test 120, not tainted $foo[2];
    my @bar = @foo;
    test 121, not tainted $bar[0];
    test 122,     tainted $bar[1];
    test 123, not tainted $bar[2];
    my @baz = eval { "A", "tainted" . $TAINT, "B" };
    test 124, not tainted $baz[0];
    test 125,     tainted $baz[1];
    test 126, not tainted $baz[2];
    my @plugh = eval q[ "A", "tainted" . $TAINT, "B" ];
    test 127, not tainted $plugh[0];
    test 128,     tainted $plugh[1];
    test 129, not tainted $plugh[2];
    my $nautilus = sub { "A", "tainted" . $TAINT, "B" };
    test 130, not tainted ((&$nautilus)[0]);
    test 131,     tainted ((&$nautilus)[1]);
    test 132, not tainted ((&$nautilus)[2]);
    my @xyzzy = &$nautilus;
    test 133, not tainted $xyzzy[0];
    test 134,     tainted $xyzzy[1];
    test 135, not tainted $xyzzy[2];
    my $red_october = sub { return "A", "tainted" . $TAINT, "B" };
    test 136, not tainted ((&$red_october)[0]);
    test 137,     tainted ((&$red_october)[1]);
    test 138, not tainted ((&$red_october)[2]);
    my @corge = &$red_october;
    test 139, not tainted $corge[0];
    test 140,     tainted $corge[1];
    test 141, not tainted $corge[2];
}

# Test for system/library calls returning string data of dubious origin.
{
    # No reliable %Config check for getpw*
    if (eval { setpwent(); getpwent(); 1 }) {
	setpwent();
	my @getpwent = getpwent();
	die "getpwent: $!\n" unless (@getpwent);
	test 142,(    not tainted $getpwent[0]
	          and     tainted $getpwent[1]
	          and not tainted $getpwent[2]
	          and not tainted $getpwent[3]
	          and not tainted $getpwent[4]
	          and not tainted $getpwent[5]
	          and     tainted $getpwent[6]		# ge?cos
	          and not tainted $getpwent[7]
		  and     tainted $getpwent[8]);	# shell
	endpwent();
    } else {
	for (142) { print "ok $_ # Skipped: getpwent() is not available\n" }
    }

    if ($Config{d_readdir}) { # pretty hard to imagine not
	local(*D);
	opendir(D, "op") or die "opendir: $!\n";
	my $readdir = readdir(D);
	test 143, tainted $readdir;
	closedir(OP);
    } else {
	for (143) { print "ok $_ # Skipped: readdir() is not available\n" }
    }

    if ($Config{d_readlink} && $Config{d_symlink}) {
	my $symlink = "sl$$";
	unlink($symlink);
	symlink("/something/naughty", $symlink) or die "symlink: $!\n";
	my $readlink = readlink($symlink);
	test 144, tainted $readlink;
	unlink($symlink);
    } else {
	for (144) { print "ok $_ # Skipped: readlink() or symlink() is not available\n"; }
    }
}

# test bitwise ops (regression bug)
{
    my $why = "y";
    my $j = "x" | $why;
    test 145, not tainted $j;
    $why = $TAINT."y";
    $j = "x" | $why;
    test 146,     tainted $j;
}

# test target of substitution (regression bug)
{
    my $why = $TAINT."y";
    $why =~ s/y/z/;
    test 147,     tainted $why;

    my $z = "[z]";
    $why =~ s/$z/zee/;
    test 148,     tainted $why;

    $why =~ s/e/'-'.$$/ge;
    test 149,     tainted $why;
}

# test shmread
{
    unless ($ipcsysv) {
	print "ok 150 # skipped: no IPC::SysV\n";
	last;
    }
    if ($Config{'extensions'} =~ /\bIPC\/SysV\b/ && $Config{d_shm}) {
	no strict 'subs';
	my $sent = "foobar";
	my $rcvd;
	my $size = 2000;
	my $id = shmget(IPC_PRIVATE, $size, S_IRWXU);

	if (defined $id) {
	    if (shmwrite($id, $sent, 0, 60)) {
		if (shmread($id, $rcvd, 0, 60)) {
		    substr($rcvd, index($rcvd, "\0")) = '';
		} else {
		    warn "# shmread failed: $!\n";
		}
	    } else {
		warn "# shmwrite failed: $!\n";
	    }
	    shmctl($id, IPC_RMID, 0) or warn "# shmctl failed: $!\n";
	} else {
	    warn "# shmget failed: $!\n";
	}

	if ($rcvd eq $sent) {
	    test 150, tainted $rcvd;
	} else {
	    print "ok 150 # Skipped: SysV shared memory operation failed\n";
	}
    } else {
	print "ok 150 # Skipped: SysV shared memory is not available\n";
    }
}

# test msgrcv
{
    unless ($ipcsysv) {
	print "ok 151 # skipped: no IPC::SysV\n";
	last;
    }
    if ($Config{'extensions'} =~ /\bIPC\/SysV\b/ && $Config{d_msg}) {
	no strict 'subs';
	my $id = msgget(IPC_PRIVATE, IPC_CREAT | S_IRWXU);

	my $sent      = "message";
	my $type_sent = 1234;
	my $rcvd;
	my $type_rcvd;

	if (defined $id) {
	    if (msgsnd($id, pack("l! a*", $type_sent, $sent), 0)) {
		if (msgrcv($id, $rcvd, 60, 0, 0)) {
		    ($type_rcvd, $rcvd) = unpack("l! a*", $rcvd);
		} else {
		    warn "# msgrcv failed\n";
		}
	    } else {
		warn "# msgsnd failed\n";
	    }
	    msgctl($id, IPC_RMID, 0) or warn "# msgctl failed: $!\n";
	} else {
	    warn "# msgget failed\n";
	}

	if ($rcvd eq $sent && $type_sent == $type_rcvd) {
	    test 151, tainted $rcvd;
	} else {
	    print "ok 151 # Skipped: SysV message queue operation failed\n";
	}
    } else {
	print "ok 151 # Skipped: SysV message queues are not available\n";
    }
}

{
    # bug id 20001004.006

    open IN, "./TEST" or warn "$0: cannot read ./TEST: $!" ;
    local $/;
    my $a = <IN>;
    my $b = <IN>;
    print "not " unless tainted($a) && tainted($b) && !defined($b);
    print "ok 152\n";
    close IN;
}

{
    # bug id 20001004.007

    open IN, "./TEST" or warn "$0: cannot read ./TEST: $!" ;
    my $a = <IN>;

    my $c = { a => 42,
	      b => $a };
    print "not " unless !tainted($c->{a}) && tainted($c->{b});
    print "ok 153\n";

    my $d = { a => $a,
	      b => 42 };
    print "not " unless tainted($d->{a}) && !tainted($d->{b});
    print "ok 154\n";

    my $e = { a => 42,
	      b => { c => $a, d => 42 } };
    print "not " unless !tainted($e->{a}) &&
	                !tainted($e->{b}) &&
	                 tainted($e->{b}->{c}) &&
	                !tainted($e->{b}->{d});
    print "ok 155\n";

    close IN;
}

