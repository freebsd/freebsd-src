#!./perl -w
$|=1;
BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    require Config; import Config;
    if ($Config{'extensions'} !~ /\bOpcode\b/ && $Config{'osname'} ne 'VMS') {
        print "1..0\n";
        exit 0;
    }
    # test 30 rather naughtily expects English error messages
    $ENV{'LC_ALL'} = 'C';
}

# Tests Todo:
#	'main' as root

use vars qw($bar);

use Opcode 1.00, qw(opdesc opset opset_to_ops opset_to_hex
	opmask_add full_opset empty_opset opcodes opmask define_optag);

use Safe 1.00;

my $last_test; # initalised at end
print "1..$last_test\n";

# Set up a package namespace of things to be visible to the unsafe code
$Root::foo = "visible";
$bar = "invisible";

# Stop perl from moaning about identifies which are apparently only used once
$Root::foo .= "";

my $cpt;
# create and destroy a couple of automatic Safe compartments first
$cpt = new Safe or die;
$cpt = new Safe or die;

$cpt = new Safe "Root";

$cpt->reval(q{ system("echo not ok 1"); });
if ($@ =~ /^system trapped by operation mask/) {
    print "ok 1\n";
} else {
    print "#$@" if $@;
    print "not ok 1\n";
}

$cpt->reval(q{
    print $foo eq 'visible'		? "ok 2\n" : "not ok 2\n";
    print $main::foo  eq 'visible'	? "ok 3\n" : "not ok 3\n";
    print defined($bar)			? "not ok 4\n" : "ok 4\n";
    print defined($::bar)		? "not ok 5\n" : "ok 5\n";
    print defined($main::bar)		? "not ok 6\n" : "ok 6\n";
});
print $@ ? "not ok 7\n#$@" : "ok 7\n";

$foo = "ok 8\n";
%bar = (key => "ok 9\n");
@baz = (); push(@baz, "o", "10"); $" = 'k ';
$glob = "ok 11\n";
@glob = qw(not ok 16);

sub sayok { print "ok @_\n" }

$cpt->share(qw($foo %bar @baz *glob sayok));
$cpt->share('$"') unless $Config{archname} =~ /-thread$/;

$cpt->reval(q{
    package other;
    sub other_sayok { print "ok @_\n" }
    package main;
    print $foo ? $foo : "not ok 8\n";
    print $bar{key} ? $bar{key} : "not ok 9\n";
    (@baz) ? print "@baz\n" : print "not ok 10\n";
    print $glob;
    other::other_sayok(12);
    $foo =~ s/8/14/;
    $bar{new} = "ok 15\n";
    @glob = qw(ok 16);
});
print $@ ? "not ok 13\n#$@" : "ok 13\n";
$" = ' ';
print $foo, $bar{new}, "@glob\n";

$Root::foo = "not ok 17";
@{$cpt->varglob('bar')} = qw(not ok 18);
${$cpt->varglob('foo')} = "ok 17";
@Root::bar = "ok";
push(@Root::bar, "18"); # Two steps to prevent "Identifier used only once..."

print "$Root::foo\n";
print "@{$cpt->varglob('bar')}\n";

use strict;

print 1 ? "ok 19\n" : "not ok 19\n";
print 1 ? "ok 20\n" : "not ok 20\n";

my $m1 = $cpt->mask;
$cpt->trap("negate");
my $m2 = $cpt->mask;
my @masked = opset_to_ops($m1);
print $m2 eq opset("negate", @masked) ? "ok 21\n" : "not ok 21\n";

print eval { $cpt->mask("a bad mask") } ? "not ok 22\n" : "ok 22\n";

print $cpt->reval("2 + 2") == 4 ? "ok 23\n" : "not ok 23\n";

$cpt->mask(empty_opset);
my $t_scalar = $cpt->reval('print wantarray ? "not ok 24\n" : "ok 24\n"');
print $cpt->reval('@ary=(6,7,8);@ary') == 3 ? "ok 25\n" : "not ok 25\n";
my @t_array  = $cpt->reval('print wantarray ? "ok 26\n" : "not ok 26\n"; (2,3,4)');
print $t_array[2] == 4 ? "ok 27\n" : "not ok 27\n";

my $t_scalar2 = $cpt->reval('die "foo bar"; 1');
print defined $t_scalar2 ? "not ok 28\n" : "ok 28\n";
print $@ =~ /foo bar/ ? "ok 29\n" : "not ok 29\n";

# --- rdo
  
my $t = 30;
$cpt->rdo('/non/existant/file.name');
# The regexp is getting rather baroque.
print $! =~ /No such file|file specification syntax error|A file or directory in the path name does not exist|Invalid argument|Device not configured|file not found/i ? "ok $t\n" : "not ok $t # $!\n"; $t++;
# test #31 is gone.
print 1 ? "ok $t\n" : "not ok $t\n#$@/$!\n"; $t++;
  
#my $rdo_file = "tmp_rdo.tpl";
#if (open X,">$rdo_file") {
#    print X "999\n";
#    close X;
#    $cpt->permit_only('const', 'leaveeval');
#    print  $cpt->rdo($rdo_file) == 999 ? "ok $t\n" : "not ok $t\n"; $t++;
#    unlink $rdo_file;
#}
#else {
#    print "# test $t skipped, can't open file: $!\nok $t\n"; $t++;
#}


print "ok $last_test\n";
BEGIN { $last_test = 32 }
