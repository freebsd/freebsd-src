BEGIN {
   chdir 't' if -d 't';
   unshift @INC, '../lib';
   unshift @INC, './pod';
   require "testp2pt.pl";
   import TestPodIncPlainText;
}

my %options = map { $_ => 1 } @ARGV;  ## convert cmdline to options-hash
my $passed  = testpodplaintext \%options, $0;
exit( ($passed == 1) ? 0 : -1 )  unless $ENV{HARNESS_ACTIVE};


__END__


=pod

This file tries to demonstrate a simple =include directive
for pods. It is used as follows:

   =include filename

where "filename" is expected to be an absolute pathname, or else
reside be relative to the directory in which the current processed
podfile resides, or be relative to the current directory.

Lets try it out with the file "included.t" shall we.

***THIS TEXT IS IMMEDIATELY BEFORE THE INCLUDE***

=include included.t

***THIS TEXT IS IMMEDIATELY AFTER THE INCLUDE***

So how did we do???
