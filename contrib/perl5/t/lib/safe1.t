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
}

# Tests Todo:
#	'main' as root

package test;	# test from somewhere other than main

use vars qw($bar);

use Opcode 1.00, qw(opdesc opset opset_to_ops opset_to_hex
	opmask_add full_opset empty_opset opcodes opmask define_optag);

use Safe 1.00;

my $last_test; # initalised at end
print "1..$last_test\n";

my $t = 1;
my $cpt;
# create and destroy some automatic Safe compartments first
$cpt = new Safe or die;
$cpt = new Safe or die;
$cpt = new Safe or die;

$cpt = new Safe "Root" or die;

foreach(1..3) {
	$foo = 42;

	$cpt->share(qw($foo));

	print ${$cpt->varglob('foo')}       == 42 ? "ok $t\n" : "not ok $t\n"; $t++;

	${$cpt->varglob('foo')} = 9;

	print $foo == 9	? "ok $t\n" : "not ok $t\n"; $t++;

	print $cpt->reval('$foo')       == 9	? "ok $t\n" : "not ok $t\n"; $t++;
	# check 'main' has been changed:
	print $cpt->reval('$::foo')     == 9	? "ok $t\n" : "not ok $t\n"; $t++;
	print $cpt->reval('$main::foo') == 9	? "ok $t\n" : "not ok $t\n"; $t++;
	# check we can't see our test package:
	print $cpt->reval('$test::foo')     	? "not ok $t\n" : "ok $t\n"; $t++;
	print $cpt->reval('${"test::foo"}')		? "not ok $t\n" : "ok $t\n"; $t++;

	$cpt->erase;	# erase the compartment, e.g., delete all variables

	print $cpt->reval('$foo') ? "not ok $t\n" : "ok $t\n"; $t++;

	# Note that we *must* use $cpt->varglob here because if we used
	# $Root::foo etc we would still see the original values!
	# This seems to be because the compiler has created an extra ref.

	print ${$cpt->varglob('foo')} ? "not ok $t\n" : "ok $t\n"; $t++;
}

print "ok $last_test\n";
BEGIN { $last_test = 28 }
