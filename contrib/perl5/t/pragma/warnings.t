#!./perl 

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    $ENV{PERL5LIB} = '../lib';
    require Config; import Config;
}

$| = 1;

my $Is_VMS     = $^O eq 'VMS';
my $Is_MSWin32 = $^O eq 'MSWin32';
my $tmpfile = "tmp0000";
my $i = 0 ;
1 while -f ++$tmpfile;
END {  if ($tmpfile) { 1 while unlink $tmpfile} }

my @prgs = () ;
my @w_files = () ;

if (@ARGV)
  { print "ARGV = [@ARGV]\n" ; @w_files = map { s#^#./pragma/warn/#; $_ } @ARGV }
else
  { @w_files = sort glob("pragma/warn/*") }

foreach (@w_files) {

    next if /(~|\.orig|,v)$/;

    open F, "<$_" or die "Cannot open $_: $!\n" ;
    while (<F>) {
	last if /^__END__/ ;
    }

    {
        local $/ = undef;
        @prgs = (@prgs, split "\n########\n", <F>) ;
    }
    close F ;
}

undef $/;

print "1..", scalar @prgs, "\n";
 
 
for (@prgs){
    my $switch = "";
    my @temps = () ;
    if (s/^\s*-\w+//){
        $switch = $&;
        $switch =~ s/(-\S*[A-Z]\S*)/"$1"/ if $Is_VMS; # protect uc switches
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
                  `./perl "-I../lib" $switch $tmpfile 2>&1` :
		  $Is_MSWin32 ?
                  `.\\perl -I../lib $switch $tmpfile 2>&1` :
                  `./perl -I../lib $switch $tmpfile 2>&1`;
    my $status = $?;
    $results =~ s/\n+$//;
    # allow expected output to be written as if $prog is on STDIN
    $results =~ s/tmp\d+/-/g;
    $results =~ s/\n%[A-Z]+-[SIWEF]-.*$// if $Is_VMS;  # clip off DCL status msg
# bison says 'parse error' instead of 'syntax error',
# various yaccs may or may not capitalize 'syntax'.
    $results =~ s/^(syntax|parse) error/syntax error/mig;
    # allow all tests to run when there are leaks
    $results =~ s/Scalars leaked: \d+\n//g;
    $expected =~ s/\n+$//;
    my $prefix = ($results =~ s#^PREFIX(\n|$)##) ;
    # any special options? (OPTIONS foo bar zap)
    my $option_regex = 0;
    if ($expected =~ s/^OPTIONS? (.+)\n//) {
	foreach my $option (split(' ', $1)) {
	    if ($option eq 'regex') { # allow regular expressions
		$option_regex = 1;
	    } else {
		die "$0: Unknown OPTION '$option'\n";
	    }
	}
    }
    if ( $results =~ s/^SKIPPED\n//) {
	print "$results\n" ;
    }
    elsif (($prefix  && (( $option_regex && $results !~ /^$expected/) ||
			 (!$option_regex && $results !~ /^\Q$expected/))) or
	   (!$prefix && (( $option_regex && $results !~ /^$expected/) ||
			 (!$option_regex && $results ne $expected)))) {
        print STDERR "PROG: $switch\n$prog\n";
        print STDERR "EXPECTED:\n$expected\n";
        print STDERR "GOT:\n$results\n";
        print "not ";
    }
    print "ok ", ++$i, "\n";
    foreach (@temps) 
	{ unlink $_ if $_ } 
}
