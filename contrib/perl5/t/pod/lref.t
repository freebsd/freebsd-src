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

Try out I<LOTS> of different ways of specifying references:

Reference the L<manpage/section>

Reference the L<manpage / section>

Reference the L<manpage/ section>

Reference the L<manpage /section>

Reference the L<"manpage/section">

Reference the L<"manpage"/section>

Reference the L<manpage/"section">

Reference the L<manpage/
section>

Reference the L<manpage
/section>

Now try it using the new "|" stuff ...

Reference the L<thistext|manpage/section>

Reference the L<thistext | manpage / section>

Reference the L<thistext| manpage/ section>

Reference the L<thistext |manpage /section>

Reference the L<thistext|
"manpage/section">

Reference the L<thistext
|"manpage"/section>

Reference the L<thistext|manpage/"section">

Reference the L<thistext|
manpage/
section>

Reference the L<thistext
|manpage
/section>

