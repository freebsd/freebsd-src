#!./perl
BEGIN {
   chdir 't' if -d 't';
   unshift @INC, './pod', '../lib';
   require "testpchk.pl";
   import TestPodChecker;
}

my %options = map { $_ => 1 } @ARGV;  ## convert cmdline to options-hash
my $passed  = testpodchecker \%options, $0;
exit( ($passed == 1) ? 0 : -1 )  unless $ENV{HARNESS_ACTIVE};

### Deliberately throw in some blank but non-empty lines
                                        
### The above line should contain spaces


__END__


=head1 NAME

poderrors.t - test Pod::Checker on some pod syntax errors

=unknown1 this is an unknown command with two N<unknownA>
and D<unknownB> interior sequences.

This is some paragraph text with some unknown interior sequences,
such as Q<unknown2>,
A<unknown3>,
and Y<unknown4 V<unknown5>>.

Now try some unterminated sequences like
I<hello mudda!
B<hello fadda!

Here I am at C<camp granada!

Camps is very,
entertaining.
And they say we'll have some fun if it stops raining!

Okay, now use a non-empty blank line to terminate a paragraph and make
sure we get a warning.
	                                     	
The above blank line contains tabs and spaces only

=head1 Additional tests

=head2 item without over

=item oops

=head2 back without over

=back

=head2 over without back

=over 4

=item oops

=head2 end without begin

=end

=head2 begin and begin

=begin html

=begin text

=end

=end

=head2 Nested sequences of the same type

C<code I<italic C<code again!>>>

=head2 Garbled entities

E<alea iacta est>
E<C<auml>>
E<abcI<bla>>

=head2 Unresolved internal links

L</"begin or begin">
L<"end with begin">
L</OoPs>

=head2 Some links with problems

L<abc
def>
L<>
L<"Warnings"> this one is ok

=head2 Warnings

L<passwd(5)>
L<   some text|page/"section"   >

=over 4

=item bla

=back 200

=begin html

What?

=end xml

=over 4

=back

see these unescaped < and > in the text?

=cut

