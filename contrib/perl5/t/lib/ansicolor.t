BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
}

# Test suite for the Term::ANSIColor Perl module.  Before `make install' is
# performed this script should be runnable with `make test'.  After `make
# install' it should work as `perl test.pl'.

############################################################################
# Ensure module can be loaded
############################################################################

BEGIN { $| = 1; print "1..8\n" }
END   { print "not ok 1\n" unless $loaded }
use Term::ANSIColor qw(:constants color colored);
$loaded = 1;
print "ok 1\n";


############################################################################
# Test suite
############################################################################

# Test simple color attributes.
if (color ('blue on_green', 'bold') eq "\e[34;42;1m") {
    print "ok 2\n";
} else {
    print "not ok 2\n";
}

# Test colored.
if (colored ("testing", 'blue', 'bold') eq "\e[34;1mtesting\e[0m") {
    print "ok 3\n";
} else {
    print "not ok 3\n";
}

# Test the constants.
if (BLUE BOLD "testing" eq "\e[34m\e[1mtesting") {
    print "ok 4\n";
} else {
    print "not ok 4\n";
}

# Test AUTORESET.
$Term::ANSIColor::AUTORESET = 1;
if (BLUE BOLD "testing" eq "\e[34m\e[1mtesting\e[0m\e[0m") {
    print "ok 5\n";
} else {
    print "not ok 5\n";
}

# Test EACHLINE.
$Term::ANSIColor::EACHLINE = "\n";
if (colored ("test\n\ntest", 'bold')
    eq "\e[1mtest\e[0m\n\n\e[1mtest\e[0m") {
    print "ok 6\n";
} else {
    print colored ("test\n\ntest", 'bold'), "\n";
    print "not ok 6\n";
}

# Test EACHLINE with multiple trailing delimiters.
$Term::ANSIColor::EACHLINE = "\r\n";
if (colored ("test\ntest\r\r\n\r\n", 'bold')
    eq "\e[1mtest\ntest\r\e[0m\r\n\r\n") {
    print "ok 7\n";
} else {
    print "not ok 7\n";
}

# Test the array ref form.
$Term::ANSIColor::EACHLINE = "\n";
if (colored (['bold', 'on_green'], "test\n", "\n", "test")
    eq "\e[1;42mtest\e[0m\n\n\e[1;42mtest\e[0m") {
    print "ok 8\n";
} else {
    print colored (['bold', 'on_green'], "test\n", "\n", "test");
    print "not ok 8\n";
}
