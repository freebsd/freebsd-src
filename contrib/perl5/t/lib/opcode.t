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

use Opcode qw(
	opcodes opdesc opmask verify_opset
	opset opset_to_ops opset_to_hex invert_opset
	opmask_add full_opset empty_opset define_optag
);

use strict;

my $t = 1;
my $last_test; # initalised at end
print "1..$last_test\n";

my($s1, $s2, $s3);
my(@o1, @o2, @o3);

# --- opset_to_ops and opset

my @empty_l = opset_to_ops(empty_opset);
print @empty_l == 0 ?   "ok $t\n" : "not ok $t\n"; $t++;

my @full_l1  = opset_to_ops(full_opset);
print @full_l1 == opcodes() ? "ok $t\n" : "not ok $t\n"; $t++;
my @full_l2 = @full_l1;	# = opcodes();	# XXX to be fixed
print "@full_l1" eq "@full_l2" ? "ok $t\n" : "not ok $t\n"; $t++;

@empty_l = opset_to_ops(opset(':none'));
print @empty_l == 0 ?   "ok $t\n" : "not ok $t\n"; $t++;

my @full_l3 = opset_to_ops(opset(':all'));
print  @full_l1  ==  @full_l3  ? "ok $t\n" : "not ok $t\n"; $t++;
print "@full_l1" eq "@full_l3" ? "ok $t\n" : "not ok $t\n"; $t++;

die $t unless $t == 7;
$s1 = opset(      'padsv');
$s2 = opset($s1,  'padav');
$s3 = opset($s2, '!padav');
print $s1 eq $s2 ? "not ok $t\n" : "ok $t\n"; ++$t;
print $s1 eq $s3 ? "ok $t\n" : "not ok $t\n"; ++$t;

# --- define_optag

print eval { opset(':_tst_') } ? "not ok $t\n" : "ok $t\n"; ++$t;
define_optag(":_tst_", opset(qw(padsv padav padhv)));
print eval { opset(':_tst_') } ? "ok $t\n" : "not ok $t\n"; ++$t;

# --- opdesc and opcodes

die $t unless $t == 11;
print opdesc("gv") eq "glob value" ? "ok $t\n" : "not ok $t\n"; $t++;
my @desc = opdesc(':_tst_','stub');
print "@desc" eq "private variable private array private hash stub"
				    ? "ok $t\n" : "not ok $t\n#@desc\n"; $t++;
print opcodes() ? "ok $t\n" : "not ok $t\n"; $t++;
print "ok $t\n"; ++$t;

# --- invert_opset

$s1 = opset(qw(fileno padsv padav));
@o2 = opset_to_ops(invert_opset($s1));
print @o2 == opcodes-3 ? "ok $t\n" : "not ok $t\n"; $t++;

# --- opmask

die $t unless $t == 16;
print opmask() eq empty_opset() ? "ok $t\n" : "not ok $t\n"; $t++;	# work
print length opmask() == int((opcodes()+7)/8) ? "ok $t\n" : "not ok $t\n"; $t++;

# --- verify_opset

print verify_opset($s1) && !verify_opset(42) ? "ok $t\n":"not ok $t\n"; $t++;

# --- opmask_add

opmask_add(opset(qw(fileno)));	# add to global op_mask
print eval 'fileno STDOUT' ? "not ok $t\n" : "ok $t\n";	$t++; # fail
print $@ =~ /fileno trapped/ ? "ok $t\n" : "not ok $t\n# $@\n"; $t++;

# --- check use of bit vector ops on opsets

$s1 = opset('padsv');
$s2 = opset('padav');
$s3 = opset('padsv', 'padav', 'padhv');

# Non-negated
print (($s1 | $s2) eq opset($s1,$s2) ? "ok $t\n":"not ok $t\n"); $t++;
print (($s2 & $s3) eq opset($s2)     ? "ok $t\n":"not ok $t\n"); $t++;
print (($s2 ^ $s3) eq opset('padsv','padhv') ? "ok $t\n":"not ok $t\n"); $t++;

# Negated, e.g., with possible extra bits in last byte beyond last op bit.
# The extra bits mean we can't just say ~mask eq invert_opset(mask).

@o1 = opset_to_ops(           ~ $s3);
@o2 = opset_to_ops(invert_opset $s3);
print "@o1" eq "@o2" ? "ok $t\n":"not ok $t\n"; $t++;

# --- finally, check some opname assertions

foreach(@full_l1) { die "bad opname: $_" if /\W/ or /^\d/ }

print "ok $last_test\n";
BEGIN { $last_test = 25 }
