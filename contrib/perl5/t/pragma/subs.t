#!./perl 

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    $ENV{PERL5LIB} = '../lib';
}

$| = 1;
undef $/;
my @prgs = split "\n########\n", <DATA>;
print "1..", scalar @prgs, "\n";

my $Is_VMS = $^O eq 'VMS';
my $Is_MSWin32 = $^O eq 'MSWin32';
my $tmpfile = "tmp0000";
my $i = 0 ;
1 while -f ++$tmpfile;
END {  if ($tmpfile) { 1 while unlink $tmpfile} }

for (@prgs){
    my $switch = "";
    my @temps = () ;
    if (s/^\s*-\w+//){
        $switch = $&;
    }
    my($prog,$expected) = split(/\nEXPECT\n/, $_);
    if ( $prog =~ /--FILE--/) {
        my(@files) = split(/\n--FILE--\s*([^\s\n]*)\s*\n/, $prog) ;
	shift @files ;
	die "Internal error test $i didn't split into pairs, got " . 
		scalar(@files) . "[" . join("%%%%", @files) ."]\n"
	    if @files % 2 ;
	while (@files > 2) {
	    my $filename = shift @files ;
	    my $code = shift @files ;
    	    push @temps, $filename ;
	    open F, ">$filename" or die "Cannot open $filename: $!\n" ;
	    print F $code ;
	    close F ;
	}
	shift @files ;
	$prog = shift @files ;
    }
    open TEST, ">$tmpfile";
    print TEST $prog,"\n";
    close TEST;
    my $results = $Is_VMS ?
                  `./perl $switch $tmpfile 2>&1` :
		  $Is_MSWin32 ?
                  `.\\perl -I../lib $switch $tmpfile 2>&1` :
                  `./perl $switch $tmpfile 2>&1`;
    my $status = $?;
    $results =~ s/\n+$//;
    # allow expected output to be written as if $prog is on STDIN
    $results =~ s/tmp\d+/-/g;
    $results =~ s/\n%[A-Z]+-[SIWEF]-.*$// if $Is_VMS;  # clip off DCL status msg
# bison says 'parse error' instead of 'syntax error',
# various yaccs may or may not capitalize 'syntax'.
    $results =~ s/^(syntax|parse) error/syntax error/mig;
    $expected =~ s/\n+$//;
    my $prefix = ($results =~ s/^PREFIX\n//) ;
    if ( $results =~ s/^SKIPPED\n//) {
	print "$results\n" ;
    }
    elsif (($prefix and $results !~ /^\Q$expected/) or
	   (!$prefix and $results ne $expected)){
        print STDERR "PROG: $switch\n$prog\n";
        print STDERR "EXPECTED:\n$expected\n";
        print STDERR "GOT:\n$results\n";
        print "not ";
    }
    print "ok ", ++$i, "\n";
    foreach (@temps) 
	{ unlink $_ if $_ } 
}

__END__

# Error - not predeclaring a sub
Fred 1,2 ;
sub Fred {}
EXPECT
Number found where operator expected at - line 3, near "Fred 1"
	(Do you need to predeclare Fred?)
syntax error at - line 3, near "Fred 1"
Execution of - aborted due to compilation errors.
########

# Error - not predeclaring a sub in time
Fred 1,2 ;
use subs qw( Fred ) ;
sub Fred {}
EXPECT
Number found where operator expected at - line 3, near "Fred 1"
	(Do you need to predeclare Fred?)
syntax error at - line 3, near "Fred 1"
BEGIN not safe after errors--compilation aborted at - line 4.
########

# AOK
use subs qw( Fred) ;
Fred 1,2 ;
sub Fred { print $_[0] + $_[1], "\n" }
EXPECT
3
########

# override a built-in function
use subs qw( open ) ;
open 1,2 ;
sub open { print $_[0] + $_[1], "\n" }
EXPECT
3
########

# override a built-in function, call after definition
use subs qw( open ) ;
sub open { print $_[0] + $_[1], "\n" }
open 1,2 ;
EXPECT
3
########

# override a built-in function, call with ()
use subs qw( open ) ;
open (1,2) ;
sub open { print $_[0] + $_[1], "\n" }
EXPECT
3
########

# override a built-in function, call with () after definition
use subs qw( open ) ;
sub open { print $_[0] + $_[1], "\n" }
open (1,2) ;
EXPECT
3
########

--FILE-- abc
Fred 1,2 ;
1;
--FILE--
use subs qw( Fred ) ;
require "./abc" ;
sub Fred { print $_[0] + $_[1], "\n" }
EXPECT
3
########

# check that it isn't affected by block scope
{
    use subs qw( Fred ) ;
}
Fred 1, 2;
sub Fred { print $_[0] + $_[1], "\n" }
EXPECT
3
