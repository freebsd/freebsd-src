package charnames;
use bytes ();		# for $bytes::hint_bits
use warnings();
$charnames::hint_bits = 0x20000;

my $txt;

# This is not optimized in any way yet
sub charnames {
  $name = shift;
  $txt = do "unicode/Name.pl" unless $txt;
  my @off;
  if ($^H{charnames_full} and $txt =~ /\t\t$name$/m) {
    @off = ($-[0], $+[0]);
  }
  unless (@off) {
    if ($^H{charnames_short} and $name =~ /^(.*?):(.*)/s) {
      my ($script, $cname) = ($1,$2);
      my $case = ( $cname =~ /[[:upper:]]/ ? "CAPITAL" : "SMALL");
      if ($txt =~ m/\t\t\U$script\E (?:$case )?LETTER \U$cname$/m) {
	@off = ($-[0], $+[0]);
      }
    }
  }
  unless (@off) {
    my $case = ( $name =~ /[[:upper:]]/ ? "CAPITAL" : "SMALL");
    for ( @{$^H{charnames_scripts}} ) {
      (@off = ($-[0], $+[0])), last 
	if $txt =~ m/\t\t$_ (?:$case )?LETTER \U$name$/m;
    }
  }
  die "Unknown charname '$name'" unless @off;

  my $hexlen = 4; # Unicode guarantees 4-, 5-, or 6-digit format
  $hexlen++ while
      $hexlen < 6 && substr($txt, $off[0] - $hexlen - 1, 1) =~ /[0-9a-f]/;
  my $ord = hex substr $txt, $off[0] - $hexlen, $hexlen;
  if ($^H & $bytes::hint_bits) {	# "use bytes" in effect?
    use bytes;
    return chr $ord if $ord <= 255;
    my $hex = sprintf '%X=0%o', $ord, $ord;
    my $fname = substr $txt, $off[0] + 2, $off[1] - $off[0] - 2;
    die "Character 0x$hex with name '$fname' is above 0xFF";
  }
  return chr $ord;
}

sub import {
  shift;
  die "`use charnames' needs explicit imports list" unless @_;
  $^H |= $charnames::hint_bits;
  $^H{charnames} = \&charnames ;
  my %h;
  @h{@_} = (1) x @_;
  $^H{charnames_full} = delete $h{':full'};
  $^H{charnames_short} = delete $h{':short'};
  $^H{charnames_scripts} = [map uc, keys %h];
  if (warnings::enabled('utf8') && @{$^H{charnames_scripts}}) {
	$txt = do "unicode/Name.pl" unless $txt;
    for (@{$^H{charnames_scripts}}) {
        warnings::warn('utf8',  "No such script: '$_'") unless
	    $txt =~ m/\t\t$_ (?:CAPITAL |SMALL )?LETTER /;
	}
  }
}


1;
__END__

=head1 NAME

charnames - define character names for C<\N{named}> string literal escape.

=head1 SYNOPSIS

  use charnames ':full';
  print "\N{GREEK SMALL LETTER SIGMA} is called sigma.\n";

  use charnames ':short';
  print "\N{greek:Sigma} is an upper-case sigma.\n";

  use charnames qw(cyrillic greek);
  print "\N{sigma} is Greek sigma, and \N{be} is Cyrillic b.\n";

=head1 DESCRIPTION

Pragma C<use charnames> supports arguments C<:full>, C<:short> and
script names.  If C<:full> is present, for expansion of
C<\N{CHARNAME}}> string C<CHARNAME> is first looked in the list of
standard Unicode names of chars.  If C<:short> is present, and
C<CHARNAME> has the form C<SCRIPT:CNAME>, then C<CNAME> is looked up
as a letter in script C<SCRIPT>.  If pragma C<use charnames> is used
with script name arguments, then for C<\N{CHARNAME}}> the name
C<CHARNAME> is looked up as a letter in the given scripts (in the
specified order).

For lookup of C<CHARNAME> inside a given script C<SCRIPTNAME>
this pragma looks for the names

  SCRIPTNAME CAPITAL LETTER CHARNAME
  SCRIPTNAME SMALL LETTER CHARNAME
  SCRIPTNAME LETTER CHARNAME

in the table of standard Unicode names.  If C<CHARNAME> is lowercase,
then the C<CAPITAL> variant is ignored, otherwise the C<SMALL> variant is
ignored.

=head1 CUSTOM TRANSLATORS

The mechanism of translation of C<\N{...}> escapes is general and not
hardwired into F<charnames.pm>.  A module can install custom
translations (inside the scope which C<use>s the module) with the
following magic incantation:

    use charnames ();		# for $charnames::hint_bits
    sub import {
	shift;
	$^H |= $charnames::hint_bits;
	$^H{charnames} = \&translator;
    }

Here translator() is a subroutine which takes C<CHARNAME> as an
argument, and returns text to insert into the string instead of the
C<\N{CHARNAME}> escape.  Since the text to insert should be different
in C<bytes> mode and out of it, the function should check the current
state of C<bytes>-flag as in:

    use bytes ();			# for $bytes::hint_bits
    sub translator {
	if ($^H & $bytes::hint_bits) {
	    return bytes_translator(@_);
	}
	else {
	    return utf8_translator(@_);
	}
    }

=head1 BUGS

Since evaluation of the translation function happens in a middle of
compilation (of a string literal), the translation function should not
do any C<eval>s or C<require>s.  This restriction should be lifted in
a future version of Perl.

=cut
