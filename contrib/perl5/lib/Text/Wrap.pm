package Text::Wrap;

require Exporter;

@ISA = qw(Exporter);
@EXPORT = qw(wrap fill);
@EXPORT_OK = qw($columns $break $huge);

$VERSION = 2001.0131;

use vars qw($VERSION $columns $debug $break $huge);
use strict;

BEGIN	{
	$columns = 76;  # <= screen width
	$debug = 0;
	$break = '\s';
	$huge = 'wrap'; # alternatively: 'die' or 'overflow'
}

use Text::Tabs qw(expand unexpand);

sub wrap
{
	my ($ip, $xp, @t) = @_;

	my $r = "";
	my $tail = pop(@t);
	my $t = expand(join("", (map { /\s+\Z/ ? ( $_ ) : ($_, ' ') } @t), $tail));
	my $lead = $ip;
	my $ll = $columns - length(expand($ip)) - 1;
	my $nll = $columns - length(expand($xp)) - 1;
	my $nl = "";
	my $remainder = "";

	pos($t) = 0;
	while ($t !~ /\G\s*\Z/gc) {
		if ($t =~ /\G([^\n]{0,$ll})($break|\Z(?!\n))/xmgc) {
			$r .= unexpand($nl . $lead . $1);
			$remainder = $2;
		} elsif ($huge eq 'wrap' && $t =~ /\G([^\n]{$ll})/gc) {
			$r .= unexpand($nl . $lead . $1);
			$remainder = "\n";
		} elsif ($huge eq 'overflow' && $t =~ /\G([^\n]*?)($break|\Z(?!\n))/xmgc) {
			$r .= unexpand($nl . $lead . $1);
			$remainder = $2;
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

	print "Finish up with '$lead'\n" if $debug;

	$r .= $lead . substr($t, pos($t), length($t)-pos($t))
		if pos($t) ne length($t);

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

	my $ps = ($ip eq $xp) ? "\n\n" : "\n";
	return join ($ps, @para);
}

1;
__END__

=head1 NAME

Text::Wrap - line wrapping to form simple paragraphs

=head1 SYNOPSIS 

B<Example 1>

	use Text::Wrap

	$initial_tab = "\t";	# Tab before first line
	$subsequent_tab = "";	# All other lines flush left

	print wrap($initial_tab, $subsequent_tab, @text);
	print fill($initial_tab, $subsequent_tab, @text);

	@lines = wrap($initial_tab, $subsequent_tab, @text);

	@paragraphs = fill($initial_tab, $subsequent_tab, @text);

B<Example 2>

	use Text::Wrap qw(wrap $columns $huge);

	$columns = 132;		# Wrap at 132 characters
	$huge = 'die';
	$huge = 'wrap';
	$huge = 'overflow';

B<Example 3>
	
	use Text::Wrap

	$Text::Wrap::columns = 72;
	print wrap('', '', @text);

=head1 DESCRIPTION

Text::Wrap::wrap() is a very simple paragraph formatter.  It formats a
single paragraph at a time by breaking lines at word boundries.
Indentation is controlled for the first line (C<$initial_tab>) and
all subsquent lines (C<$subsequent_tab>) independently.  Please note: 
C<$initial_tab> and C<$subsequent_tab> are the literal strings that will
be used: it is unlikley you would want to pass in a number.

Lines are wrapped at C<$Text::Wrap::columns> columns.  C<$Text::Wrap::columns>
should be set to the full width of your output device.  In fact,
every resulting line will have length of no more than C<$columns - 1>.  

Beginner note: In example 2, above C<$columns> is imported into
the local namespace, and set locally.  In example 3,
C<$Text::Wrap::columns> is set in its own namespace without importing it.

When words that are longer than C<$columns> are encountered, they
are broken up.  C<wrap()> adds a C<"\n"> at column C<$columns>.
This behavior can be overridden by setting C<$huge> to
'die' or to 'overflow'.  When set to 'die', large words will cause
C<die()> to be called.  When set to 'overflow', large words will be
left intact.  

Text::Wrap::fill() is a simple multi-paragraph formatter.  It formats
each paragraph separately and then joins them together when it's done.  It
will destory any whitespace in the original text.  It breaks text into
paragraphs by looking for whitespace after a newline.  In other respects
it acts like wrap().

When called in list context, C<wrap()> will return a list of lines and 
C<fill()> will return a list of paragraphs.

Historical notes: Older versions of C<wrap()> and C<fill()> always 
returned strings.  Also, 'die' used to be the default value of
C<$huge>.  Now, 'wrap' is the default value.

=head1 EXAMPLE

	print wrap("\t","","This is a bit of text that forms 
		a normal book-style paragraph");

=head1 AUTHOR

David Muir Sharnoff <muir@idiom.com> with help from Tim Pierce and
many many others.  

