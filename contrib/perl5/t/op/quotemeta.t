#!./perl

print "1..15\n";

if ($^O eq 'os390') { # An EBCDIC variant.
    $_=join "", map chr($_), 129..233;

    # 105 characters - 52 letters = 53 backslashes
    # 105 characters + 53 backslashes = 158 characters
    $_=quotemeta $_;
    if ( length == 158 ){print "ok 1\n"} else {print "not ok 1\n"}
    # 104 non-backslash characters
    if (tr/\\//cd == 104){print "ok 2\n"} else {print "not ok 2\n"}
} else { # some ASCII descendant, then.
    $_=join "", map chr($_), 32..127;

    # 96 characters - 52 letters - 10 digits - 1 underscore = 33 backslashes
    # 96 characters + 33 backslashes = 129 characters
    $_=quotemeta $_;
    if ( length == 129 ){print "ok 1\n"} else {print "not ok 1\n"}
    # 95 non-backslash characters
    if (tr/\\//cd == 95){print "ok 2\n"} else {print "not ok 2\n"}
}

if (length quotemeta "" == 0){print "ok 3\n"} else {print "not ok 3\n"}

print "aA\UbB\LcC\EdD" eq "aABBccdD" ? "ok 4\n" : "not ok 4 \n";
print "aA\LbB\UcC\EdD" eq "aAbbCCdD" ? "ok 5\n" : "not ok 5 \n";
print "\L\upERL" eq "Perl" ? "ok 6\n" : "not ok 6 \n";
print "\u\LpERL" eq "Perl" ? "ok 7\n" : "not ok 7 \n";
print "\U\lPerl" eq "pERL" ? "ok 8\n" : "not ok 8 \n";
print "\l\UPerl" eq "pERL" ? "ok 9\n" : "not ok 9 \n";
print "\u\LpE\Q#X#\ER\EL" eq "Pe\\#x\\#rL" ? "ok 10\n" : "not ok 10 \n";
print "\l\UPe\Q!x!\Er\El" eq "pE\\!X\\!Rl" ? "ok 11\n" : "not ok 11 \n";
print "\Q\u\LpE.X.R\EL\E." eq "Pe\\.x\\.rL." ? "ok 12\n" : "not ok 12 \n";
print "\Q\l\UPe*x*r\El\E*" eq "pE\\*X\\*Rl*" ? "ok 13\n" : "not ok 13 \n";
print "\U\lPerl\E\E\E\E" eq "pERL" ? "ok 14\n" : "not ok 14 \n";
print "\l\UPerl\E\E\E\E" eq "pERL" ? "ok 15\n" : "not ok 15 \n";
