#!./perl

#####################################################################
#
# Test for process id return value from open
# Ronald Schmidt (The Software Path) RonaldWS@software-path.com
#
#####################################################################

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    if ($^O eq 'dos') {
        print "1..0 # Skip: no multitasking\n";
        exit 0;
    }
}

use Config;
$| = 1;
$SIG{PIPE} = 'IGNORE';

print "1..10\n";

$perl = qq[$^X "-I../lib"];

#
# commands run 4 perl programs.  Two of these programs write a
# short message to STDOUT and exit.  Two of these programs
# read from STDIN.  One reader never exits and must be killed.
# the other reader reads one line, waits a few seconds and then
# exits to test the waitpid function.
#
$cmd1 = qq/$perl -e "\$|=1; print qq[first process\\n]; sleep 30;"/;
$cmd2 = qq/$perl -e "\$|=1; print qq[second process\\n]; sleep 30;"/;
$cmd3 = qq/$perl -e "print <>;"/; # hangs waiting for end of STDIN
$cmd4 = qq/$perl -e "print scalar <>;"/;

#warn "#$cmd1\n#$cmd2\n#$cmd3\n#$cmd4\n";

# start the processes
$pid1 = open(FH1, "$cmd1 |") or print "not ";
print "ok 1\n";
$pid2 = open(FH2, "$cmd2 |") or print "not ";
print "ok 2\n";
$pid3 = open(FH3, "| $cmd3") or print "not ";
print "ok 3\n";
$pid4 = open(FH4, "| $cmd4") or print "not ";
print "ok 4\n";

print "# pids were $pid1, $pid2, $pid3, $pid4\n";

my $killsig = 'HUP';
$killsig = 1 unless $Config{sig_name} =~ /\bHUP\b/;

# get message from first process and kill it
chomp($from_pid1 = scalar(<FH1>));
print "# child1 returned [$from_pid1]\nnot "
    unless $from_pid1 eq 'first process';
print "ok 5\n";
$kill_cnt = kill $killsig, $pid1;
print "not " unless $kill_cnt == 1;
print "ok 6\n";

# get message from second process and kill second process and reader process
chomp($from_pid2 = scalar(<FH2>));
print "# child2 returned [$from_pid2]\nnot "
    unless $from_pid2 eq 'second process';
print "ok 7\n";
$kill_cnt = kill $killsig, $pid2, $pid3;
print "not " unless $kill_cnt == 2;
print "ok 8\n";

# send one expected line of text to child process and then wait for it
select(FH4); $| = 1; select(STDOUT);

print FH4 "ok 9\n";
print "# waiting for process $pid4 to exit\n";
$reap_pid = waitpid $pid4, 0;
print "# reaped pid $reap_pid != $pid4\nnot "
    unless $reap_pid == $pid4;         
print "ok 10\n";
