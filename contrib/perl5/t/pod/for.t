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

This is a test

=for theloveofpete
You shouldn't see this
or this
or this

=for text
pod2text should see this
and this
and this

and everything should see this!

=begin text

Similarly, this line ...

and this one ...

as well this one,

should all be in pod2text output

=end text

Tweedley-deedley-dee, Im as happy as can be!
Tweedley-deedley-dum, cuz youre my honey sugar plum!

=begin atthebeginning

But I expect to see neither hide ...

nor tail ...

of this text

=end atthebeginning

The rest of this should show up in everything.

