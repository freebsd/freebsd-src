#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
}

my @expect;
my $data = "";
my @data = ();
my $test = 1;

sub ok { print "not " unless shift; print "ok ",$test++,"\n"; }

package Implement;

BEGIN { *ok = \*main::ok }

sub compare {
    return unless @expect;
    return ok(0) unless(@_ == @expect);

    my $i;
    for($i = 0 ; $i < @_ ; $i++) {
	next if $_[$i] eq $expect[$i];
	return ok(0);
    }

    ok(1);
}

sub TIEHANDLE {
    compare(TIEHANDLE => @_);
    my ($class,@val) = @_;
    return bless \@val,$class;
}

sub PRINT {
    compare(PRINT => @_);
    1;
}

sub PRINTF {
    compare(PRINTF => @_);
    2;
}

sub READLINE {
    compare(READLINE => @_);
    wantarray ? @data : shift @data;
}

sub GETC {
    compare(GETC => @_);
    substr($data,0,1);
}

sub READ {
    compare(READ => @_);
    substr($_[1],$_[3] || 0) = substr($data,0,$_[2]);
    3;
}

sub WRITE {
    compare(WRITE => @_);
    $data = substr($_[1],$_[3] || 0, $_[2]);
    length($data);
}

sub CLOSE {
    compare(CLOSE => @_);
    
    5;
}

package main;

use Symbol;

print "1..33\n";

my $fh = gensym;

@expect = (TIEHANDLE => 'Implement');
my $ob = tie *$fh,'Implement';
ok(ref($ob) eq 'Implement');
ok(tied(*$fh) == $ob);

@expect = (PRINT => $ob,"some","text");
$r = print $fh @expect[2,3];
ok($r == 1);

@expect = (PRINTF => $ob,"%s","text");
$r = printf $fh @expect[2,3];
ok($r == 2);

$text = (@data = ("the line\n"))[0];
@expect = (READLINE => $ob);
$ln = <$fh>;
ok($ln eq $text);

@expect = ();
@in = @data = qw(a line at a time);
@line = <$fh>;
@expect = @in;
Implement::compare(@line);

@expect = (GETC => $ob);
$data = "abc";
$ch = getc $fh;
ok($ch eq "a");

$buf = "xyz";
@expect = (READ => $ob, $buf, 3);
$data = "abc";
$r = read $fh,$buf,3;
ok($r == 3);
ok($buf eq "abc");


$buf = "xyzasd";
@expect = (READ => $ob, $buf, 3,3);
$data = "abc";
$r = sysread $fh,$buf,3,3;
ok($r == 3);
ok($buf eq "xyzabc");

$buf = "qwerty";
@expect = (WRITE => $ob, $buf, 4,1);
$data = "";
$r = syswrite $fh,$buf,4,1;
ok($r == 4);
ok($data eq "wert");

$buf = "qwerty";
@expect = (WRITE => $ob, $buf, 4);
$data = "";
$r = syswrite $fh,$buf,4;
ok($r == 4);
ok($data eq "qwer");

$buf = "qwerty";
@expect = (WRITE => $ob, $buf, 6);
$data = "";
$r = syswrite $fh,$buf;
ok($r == 6);
ok($data eq "qwerty");

@expect = (CLOSE => $ob);
$r = close $fh;
ok($r == 5);

# Does aliasing work with tied FHs?
*ALIAS = *$fh;
@expect = (PRINT => $ob,"some","text");
$r = print ALIAS @expect[2,3];
ok($r == 1);

{
    use warnings;
    # Special case of aliasing STDERR, which used
    # to dump core when warnings were enabled
    *STDERR = *$fh;
    @expect = (PRINT => $ob,"some","text");
    $r = print STDERR @expect[2,3];
    ok($r == 1);
}
