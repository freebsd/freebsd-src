#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
}

print "1..3\n";

$DICT = <<EOT;
Aarhus
Aaron
Ababa
aback
abaft
abandon
abandoned
abandoning
abandonment
abandons
abase
abased
abasement
abasements
abases
abash
abashed
abashes
abashing
abasing
abate
abated
abatement
abatements
abater
abates
abating
Abba
EOT

use Search::Dict;

open(DICT, "+>dict-$$") or die "Can't create dict-$$: $!";
binmode DICT;			# To make length expected one.
print DICT $DICT;

my $pos = look *DICT, "abash";
chomp($word = <DICT>);
print "not " if $pos < 0 || $word ne "abash";
print "ok 1\n";

$pos = look *DICT, "foo";
chomp($word = <DICT>);

print "not " if $pos != length($DICT);  # will search to end of file
print "ok 2\n";

$pos = look *DICT, "aarhus", 1, 1;
chomp($word = <DICT>);

print "not " if $pos < 0 || $word ne "Aarhus";
print "ok 3\n";

close DICT or die "cannot close";
unlink "dict-$$";
