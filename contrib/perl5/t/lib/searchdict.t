#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
}

print "1..4\n";

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

my $pos = look *DICT, "Ababa";
chomp($word = <DICT>);
print "not " if $pos < 0 || $word ne "Ababa";
print "ok 1\n";

if (ord('a') > ord('A') ) {  # ASCII

    $pos = look *DICT, "foo";
    chomp($word = <DICT>);

    print "not " if $pos != length($DICT);  # will search to end of file
    print "ok 2\n";

    my $pos = look *DICT, "abash";
    chomp($word = <DICT>);
    print "not " if $pos < 0 || $word ne "abash";
    print "ok 3\n";

}
else { # EBCDIC systems e.g. os390

    $pos = look *DICT, "FOO";
    chomp($word = <DICT>);

    print "not " if $pos != length($DICT);  # will search to end of file
    print "ok 2\n";

    my $pos = look *DICT, "Abba";
    chomp($word = <DICT>);
    print "not " if $pos < 0 || $word ne "Abba";
    print "ok 3\n";
}

$pos = look *DICT, "aarhus", 1, 1;
chomp($word = <DICT>);

print "not " if $pos < 0 || $word ne "Aarhus";
print "ok 4\n";

close DICT or die "cannot close";
unlink "dict-$$";
