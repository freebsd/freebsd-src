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

This is a test to see if I can do not only C<$self> and C<method()>, but
also C<< $self->method() >> and C<< $self->{FIELDNAME} >> and
C<< $Foo <=> $Bar >> without resorting to escape sequences. If 
I want to refer to the right-shift operator I can do something
like C<<< $x >> 3 >>> or even C<<<< $y >> 5 >>>>.

Now for the grand finale of C<< $self->method()->{FIELDNAME} = {FOO=>BAR} >>.
And I also want to make sure that newlines work like this
C<<<
$self->{FOOBAR} >> 3 and [$b => $a]->[$a <=> $b]
>>>

Of course I should still be able to do all this I<with> escape sequences
too: C<$self-E<gt>method()> and C<$self-E<gt>{FIELDNAME}> and C<{FOO=E<gt>BAR}>.

Dont forget C<$self-E<gt>method()-E<gt>{FIELDNAME} = {FOO=E<gt>BAR}>.

And make sure that C<0> works too!

Now, if I use << or >> as my delimiters, then I have to use whitespace.
So things like C<<$self->method()>> and C<<$self->{FIELDNAME}>> wont end
up doing what you might expect since the first > will still terminate
the first < seen.

Lets make sure these work for empty ones too, like C<<  >> and C<< >> >>
(just to be obnoxious)

=cut
