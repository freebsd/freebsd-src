package Text::Wrap;

use vars qw(@ISA @EXPORT @EXPORT_OK $VERSION $columns $debug);
use strict;
use Exporter;

$VERSION = "97.02";
@ISA = qw(Exporter);
@EXPORT = qw(wrap);
@EXPORT_OK = qw($columns $tabstop fill);

use Text::Tabs qw(expand unexpand $tabstop);


BEGIN	{
    $columns = 76;  # <= screen width
    $debug = 0;
}

sub wrap
{
    my ($ip, $xp, @t) = @_;

    my @rv;
    my $t = expand(join(" ",@t));

    my $lead = $ip;
    my $ll = $columns - length(expand($lead)) - 1;
    my $nl = "";

    $t =~ s/^\s+//;
    while(length($t) > $ll) {
	# remove up to a line length of things that
	# aren't new lines and tabs.
	if ($t =~ s/^([^\n]{0,$ll})(\s|\Z(?!\n))//) {
	    my ($l,$r) = ($1,$2);
	    $l =~ s/\s+$//;
	    print "WRAP  $lead$l..($r)\n" if $debug;
	    push @rv, unexpand($lead . $l), "\n";
		
	} elsif ($t =~ s/^([^\n]{$ll})//) {
	    print "SPLIT $lead$1..\n" if $debug;
	    push @rv, unexpand($lead . $1),"\n";
	}
	# recompute the leader
	$lead = $xp;
	$ll = $columns - length(expand($lead)) - 1;
	$t =~ s/^\s+//;
    } 
    print "TAIL  $lead$t\n" if $debug;
    push @rv, $lead.$t if $t ne "";
    return join '', @rv;
}


sub fill 
{
	my ($ip, $xp, @raw) = @_;
	my @para;
	my $pp;

	for $pp (split(/\n\s+/, join("\n",@raw))) {
		$pp =~ s/\s+/ /g;
		my $x = wrap($ip, $xp, $pp);
		push(@para, $x);
	}

	# if paragraph_indent is the same as line_indent, 
	# separate paragraphs with blank lines

	return join ($ip eq $xp ? "\n\n" : "\n", @para);
}

1;
__END__

=head1 NAME

Text::Wrap - line wrapping to form simple paragraphs

=head1 SYNOPSIS 

	use Text::Wrap

	print wrap($initial_tab, $subsequent_tab, @text);

	use Text::Wrap qw(wrap $columns $tabstop fill);

	$columns = 132;
	$tabstop = 4;

	print fill($initial_tab, $subsequent_tab, @text);
	print fill("", "", `cat book`);

=head1 DESCRIPTION

Text::Wrap::wrap() is a very simple paragraph formatter.  It formats a
single paragraph at a time by breaking lines at word boundries.
Indentation is controlled for the first line ($initial_tab) and
all subsquent lines ($subsequent_tab) independently.  $Text::Wrap::columns
should be set to the full width of your output device.

Text::Wrap::fill() is a simple multi-paragraph formatter.  It formats
each paragraph separately and then joins them together when it's done.  It
will destory any whitespace in the original text.  It breaks text into
paragraphs by looking for whitespace after a newline.  In other respects
it acts like wrap().

=head1 EXAMPLE

	print wrap("\t","","This is a bit of text that forms 
		a normal book-style paragraph");

=head1 BUGS

It's not clear what the correct behavior should be when Wrap() is
presented with a word that is longer than a line.  The previous 
behavior was to die.  Now the word is now split at line-length.

=head1 AUTHOR

David Muir Sharnoff <muir@idiom.com> with help from Tim Pierce and
others. Updated by Jacqui Caren.

=cut
