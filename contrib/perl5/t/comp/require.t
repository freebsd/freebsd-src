#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '.';
    push @INC, '../lib';
}

# don't make this lexical
$i = 1;
# Tests 21 .. 23 work only with non broken UTF16-as-code implementations,
# i.e. not EBCDIC Perls.
my $Is_EBCDIC = ord('A') == 193 ? 1 : 0;
if ($Is_EBCDIC) {
   print "1..20\n";
}
else {
   print "1..23\n";
}

sub do_require {
    %INC = ();
    write_file('bleah.pm',@_);
    eval { require "bleah.pm" };
    my @a; # magic guard for scope violations (must be first lexical in file)
}

sub write_file {
    my $f = shift;
    open(REQ,">$f") or die "Can't write '$f': $!";
    binmode REQ;
    use bytes;
    print REQ @_;
    close REQ;
}

eval {require 5.005};
print "# $@\nnot " if $@;
print "ok ",$i++,"\n";

eval { require 5.005 };
print "# $@\nnot " if $@;
print "ok ",$i++,"\n";

eval { require 5.005; };
print "# $@\nnot " if $@;
print "ok ",$i++,"\n";

eval {
    require 5.005
};
print "# $@\nnot " if $@;
print "ok ",$i++,"\n";

# new style version numbers

eval { require v5.5.630; };
print "# $@\nnot " if $@;
print "ok ",$i++,"\n";

eval { require 10.0.2; };
print "# $@\nnot " unless $@ =~ /^Perl v10\.0\.2 required/;
print "ok ",$i++,"\n";

eval q{ use v5.5.630; };
print "# $@\nnot " if $@;
print "ok ",$i++,"\n";

eval q{ use 10.0.2; };
print "# $@\nnot " unless $@ =~ /^Perl v10\.0\.2 required/;
print "ok ",$i++,"\n";

my $ver = 5.005_63;
eval { require $ver; };
print "# $@\nnot " if $@;
print "ok ",$i++,"\n";

# check inaccurate fp
$ver = 10.2;
eval { require $ver; };
print "# $@\nnot " unless $@ =~ /^Perl v10\.200\.0 required/;
print "ok ",$i++,"\n";

$ver = 10.000_02;
eval { require $ver; };
print "# $@\nnot " unless $@ =~ /^Perl v10\.0\.20 required/;
print "ok ",$i++,"\n";

print "not " unless 5.5.1 gt v5.5;
print "ok ",$i++,"\n";

{
    use utf8;
    print "not " unless v5.5.640 eq "\x{5}\x{5}\x{280}";
    print "ok ",$i++,"\n";

    print "not " unless v7.15 eq "\x{7}\x{f}";
    print "ok ",$i++,"\n";

    print "not "
      unless v1.20.300.4000.50000.600000 eq "\x{1}\x{14}\x{12c}\x{fa0}\x{c350}\x{927c0}";
    print "ok ",$i++,"\n";
}

# interaction with pod (see the eof)
write_file('bleah.pm', "print 'ok $i\n'; 1;\n");
require "bleah.pm";
$i++;

# run-time failure in require
do_require "0;\n";
print "# $@\nnot " unless $@ =~ /did not return a true/;
print "ok ",$i++,"\n";

# compile-time failure in require
do_require "1)\n";
# bison says 'parse error' instead of 'syntax error',
# various yaccs may or may not capitalize 'syntax'.
print "# $@\nnot " unless $@ =~ /(syntax|parse) error/mi;
print "ok ",$i++,"\n";

# successful require
do_require "1";
print "# $@\nnot " if $@;
print "ok ",$i++,"\n";

# do FILE shouldn't see any outside lexicals
my $x = "ok $i\n";
write_file("bleah.do", <<EOT);
\$x = "not ok $i\\n";
EOT
do "bleah.do";
dofile();
sub dofile { do "bleah.do"; };
print $x;

exit if $Is_EBCDIC;

# UTF-encoded things
my $utf8 = chr(0xFEFF);

$i++; do_require(qq(${utf8}print "ok $i\n"; 1;\n));

sub bytes_to_utf16 {
    my $utf16 = pack("$_[0]*", unpack("C*", $_[1]));
    return @_ == 3 && $_[2] ? pack("$_[0]", 0xFEFF) . $utf16 : $utf16;
}

$i++; do_require(bytes_to_utf16('n', qq(print "ok $i\\n"; 1;\n), 1)); # BE
$i++; do_require(bytes_to_utf16('v', qq(print "ok $i\\n"; 1;\n), 1)); # LE

END { 1 while unlink 'bleah.pm'; 1 while unlink 'bleah.do'; }

# ***interaction with pod (don't put any thing after here)***

=pod
