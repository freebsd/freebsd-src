#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
}

print "1..5\n";

use Text::Wrap qw(wrap $columns);

$columns = 30;

$text = <<'EOT';
Text::Wrap is a very simple paragraph formatter.  It formats a
single paragraph at a time by breaking lines at word boundries.
Indentation is controlled for the first line ($initial_tab) and
all subsquent lines ($subsequent_tab) independently.  $Text::Wrap::columns
should be set to the full width of your output device.
EOT

$text =~ s/\n/ /g;
$_ = wrap "|  ", "|", $text;

#print "$_\n";

print "not " unless /^\|  Text::Wrap is/;  # start is ok
print "ok 1\n";

print "not " if /^.{31,}$/m;  # no line longer than 30 chars
print "ok 2\n";

print "not " unless /^\|\w/m;  # other lines start with 
print "ok 3\n";

print "not " unless /\bsubsquent\b/; # look for a random word
print "ok 4\n";

print "not " unless /\bdevice\./;  # look for last word
print "ok 5\n";
