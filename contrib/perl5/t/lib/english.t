#!./perl

print "1..16\n";

BEGIN { @INC = '../lib' }
use English;
use Config;
my $threads = $Config{'usethreads'} || 0;

print $PID == $$ ? "ok 1\n" : "not ok 1\n";

$_ = 1;
print $ARG == $_  || $threads ? "ok 2\n" : "not ok 2\n";

sub foo {
    print $ARG[0] == $_[0] || $threads ? "ok 3\n" : "not ok 3\n";
}
&foo(1);

if ($threads) {
    $_ = "ok 4\nok 5\nok 6\n";
} else {
    $ARG = "ok 4\nok 5\nok 6\n";
}
/ok 5\n/;
print $PREMATCH, $MATCH, $POSTMATCH;

$OFS = " ";
$ORS = "\n";
print 'ok',7;
undef $OUTPUT_FIELD_SEPARATOR;

if ($threads) { $" = "\n" } else { $LIST_SEPARATOR = "\n" };
@foo = ("ok 8", "ok 9");
print "@foo";
undef $OUTPUT_RECORD_SEPARATOR;

eval 'NO SUCH FUNCTION';
print "ok 10\n" if $EVAL_ERROR =~ /method/ || $threads;

print $UID == $< ? "ok 11\n" : "not ok 11\n";
print $GID == $( ? "ok 12\n" : "not ok 12\n";
print $EUID == $> ? "ok 13\n" : "not ok 13\n";
print $EGID == $) ? "ok 14\n" : "not ok 14\n";

print $PROGRAM_NAME == $0 ? "ok 15\n" : "not ok 15\n";
print $BASETIME == $^T ? "ok 16\n" : "not ok 16\n";
