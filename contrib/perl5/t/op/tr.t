# tr.t

print "1..4\n";

$_ = "abcdefghijklmnopqrstuvwxyz";

tr/a-z/A-Z/;

print "not " unless $_ eq "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
print "ok 1\n";

tr/A-Z/a-z/;

print "not " unless $_ eq "abcdefghijklmnopqrstuvwxyz";
print "ok 2\n";

tr/b-y/B-Y/;

print "not " unless $_ eq "aBCDEFGHIJKLMNOPQRSTUVWXYz";
print "ok 3\n";

# In EBCDIC 'I' is \xc9 and 'J' is \0xd1, 'i' is \x89 and 'j' is \x91.
# Yes, discontinuities.  Regardless, the \xca in the below should stay
# untouched (and not became \x8a).

$_ = "I\xcaJ";

tr/I-J/i-j/;

print "not " unless $_ eq "i\xcaj";
print "ok 4\n";

#
