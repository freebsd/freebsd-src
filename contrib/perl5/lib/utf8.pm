package utf8;

if (ord('A') != 193) { # make things more pragmatic for EBCDIC folk

$utf8::hint_bits = 0x00800000;

sub import {
    $^H |= $utf8::hint_bits;
    $enc{caller()} = $_[1] if $_[1];
}

sub unimport {
    $^H &= ~$utf8::hint_bits;
}

sub AUTOLOAD {
    require "utf8_heavy.pl";
    goto &$AUTOLOAD if defined &$AUTOLOAD;
    Carp::croak("Undefined subroutine $AUTOLOAD called");
}

}

1;
__END__

=head1 NAME

utf8 - Perl pragma to enable/disable UTF-8 in source code

=head1 SYNOPSIS

    use utf8;
    no utf8;

=head1 DESCRIPTION

WARNING: The implementation of Unicode support in Perl is incomplete.
See L<perlunicode> for the exact details.

The C<use utf8> pragma tells the Perl parser to allow UTF-8 in the
program text in the current lexical scope.  The C<no utf8> pragma
tells Perl to switch back to treating the source text as literal
bytes in the current lexical scope.

This pragma is primarily a compatibility device.  Perl versions
earlier than 5.6 allowed arbitrary bytes in source code, whereas
in future we would like to standardize on the UTF-8 encoding for
source text.  Until UTF-8 becomes the default format for source
text, this pragma should be used to recognize UTF-8 in the source.
When UTF-8 becomes the standard source format, this pragma will
effectively become a no-op.  This pragma already is a no-op on
EBCDIC platforms (where it is alright to code perl in EBCDIC 
rather than UTF-8).

Enabling the C<utf8> pragma has the following effects:

=over

=item *

Bytes in the source text that have their high-bit set will be treated
as being part of a literal UTF-8 character.  This includes most literals
such as identifiers, string constants, constant regular expression patterns
and package names.

=item *

In the absence of inputs marked as UTF-8, regular expressions within the
scope of this pragma will default to using character semantics instead
of byte semantics.

    @bytes_or_chars = split //, $data;	# may split to bytes if data
					# $data isn't UTF-8
    {
	use utf8;			# force char semantics
	@chars = split //, $data;	# splits characters
    }

=head1 SEE ALSO

L<perlunicode>, L<bytes>

=cut
