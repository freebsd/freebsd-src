package Text::Wrap;

require Exporter;

@ISA = qw(Exporter);
@EXPORT = qw(wrap fill);
@EXPORT_OK = qw($columns $break $huge);

$VERSION = 98.112902;

use vars qw($VERSION $columns $debug $break $huge);
use strict;

BEGIN	{
	$columns = 76;  # <= screen width
	$debug = 0;
	$break = '\s';
	$huge = 'wrap'; # alternatively: 'die'
}

use Text::Tabs qw(expand unexpand);

sub wrap
{
	my ($ip, $xp, @t) = @_;

	my $r = "";
	my $t = expand(join(" ",@t));
	my $lead = $ip;
	my $ll = $columns - length(expand($ip)) - 1;
	my $nll = $columns - length(expand($xp)) - 1;
	my $nl = "";
	my $remainder = "";

	while ($t !~ /^\s*$/) {
		if ($t =~ s/^([^\n]{0,$ll})($break|\Z(?!\n))//xm) {
			$r .= unexpand($nl . $lead . $1);
			$remainder = $2;
		} elsif ($huge eq 'wrap' && $t =~ s/^([^\n]{$ll})//) {
			$r .= unexpand($nl . $lead . $1);
			$remainder = "\n";
		} elsif ($huge eq 'die') {
			die "couldn't wrap '$t'";
		} else {
			die "This shouldn't happen";
		}
			
		$lead = $xp;
		$ll = $nll;
		$nl = "\n";
	}
	$r .= $remainder;

	print "-----------$r---------\n" if $debug;

	print "Finish up with '$lead', '$t'\n" if $debug;

	$r .= $lead . $t if $t ne "";

	print "-----------$r---------\n" if $debug;;
	return $r;
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
	print fill($initial_tab, $subsequent_tab, @text);

	use Text::Wrap qw(wrap $columns $huge);

	$columns = 132;
	$huge = 'die';
	$huge = 'wrap';

=head1 DESCRIPTION

Text::Wrap::wrap() is a very simple paragraph formatter.  It formats a
single paragraph at a time by breaking lines at word boundaries.
Indentation is controlled for the first line ($initial_tab) and
all subsequent lines ($subsequent_tab) independently.  

Lines are wrapped at $Text::Wrap::columns columns.  
$Text::Wrap::columns should be set to the full width of your output device.

When words that are longer than $columns are encountered, they
are broken up.  Previous versions of wrap() die()ed instead.
To restore the old (dying) behavior, set $Text::Wrap::huge to
'die'.

Text::Wrap::fill() is a simple multi-paragraph formatter.  It formats
each paragraph separately and then joins them together when it's done.  It
will destroy any whitespace in the original text.  It breaks text into
paragraphs by looking for whitespace after a newline.  In other respects
it acts like wrap().

=head1 EXAMPLE

	print wrap("\t","","This is a bit of text that forms 
		a normal book-style paragraph");

=head1 AUTHOR

David Muir Sharnoff <muir@idiom.com> with help from Tim Pierce and
many many others.  

