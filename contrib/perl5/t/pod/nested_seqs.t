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

The statement: C<This is dog kind's I<finest> hour!> is a parody of a
quotation from Winston Churchill.

=cut

