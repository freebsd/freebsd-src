#! @PERL@
#
# Generate a man page from sections of a Texinfo manual.
#
# Copyright 2004 The Free Software Foundation,
#                Derek R. Price,
#                & Ximbiot <http://ximbiot.com>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or (at your option)
# any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software Foundation,
# Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.



# Need Perl 5.005 or greater for re 'eval'.
require 5.005;

# The usual.
use strict;
use IO::File;



###
### GLOBALS
###
my $texi_num = 0; # Keep track of how many texinfo files have been encountered.
my @parent;       # This needs to be global to be used inside of a regex later.
my $nk;           # Ditto.
my $ret;          # The RE match Type, used in debug prints.
my $debug = 0;    # Debug mode?



###
### FUNCTIONS
###
sub debug_print
{
	print @_ if $debug;
}



sub keyword_mode
{
	my ($keyword, $file) = @_;

	return "\\fR"
		if $keyword =~ /^(|r|t)$/;
	return "\\fB"
		if $keyword =~ /^(strong|sc|code|file|samp)$/;
	return "\\fI"
		if $keyword =~ /^(emph|var|dfn)$/;
	die "no handler for keyword \`$keyword', found at line $. of file \`$file'\n";
}



# Return replacement for \@$keyword{$content}.
sub do_keyword
{
	my ($file, $parent, $keyword, $content) = @_;

	return "see node \`$content\\(aq in the CVS manual"
		if $keyword =~ /^(p?x)?ref$/;
	return "\\fP\\fP$content"
		if $keyword =~ /^splitrcskeyword$/;

	my $endmode = keyword_mode $parent;
	my $startmode = keyword_mode $keyword, $file;

	return "$startmode$content$endmode";
}



###
### MAIN
###
for my $file (@ARGV)
{
	my $fh = new IO::File "< $file"
		or die "Failed to open file \`$file': $!";

	if ($file !~ /\.(texinfo|texi|txi)$/)
	{
		print stderr "Passing \`$file' through unprocessed.\n";
		# Just cat any file that doesn't look like a Texinfo source.
		while (my $line = $fh->getline)
		{
			print $line;
		}
		next;
	}

	print stderr "Processing \`$file'.\n";
	$texi_num++;
	my $gotone = 0;
	my $inblank = 0;
	my $indent = 0;
	my $inexample = 0;
	my $inmenu = 0;
	my $intable = 0;
	my $last_header = "";
	my @table_headers;
	my @table_footers;
	my $table_header = "";
	my $table_footer = "";
	my $last;
	while ($_ = $fh->getline)
	{
		if (!$gotone && /^\@c ----- START MAN $texi_num -----$/)
		{
			$gotone = 1;
			next;
		}

		# Skip ahead until our man section.
		next unless $gotone;

		# If we find the end tag we are done.
		last if /^\@c ----- END MAN $texi_num -----$/;

		# Need to do this everywhere.  i.e., before we print example
		# lines, since literal back slashes can appear there too.
		s/\\/\\\\/g;
		s/^\./\\&./;
		s/([\s])\./$1\\&./;
		s/'/\\(aq/g;
		s/`/\\`/g;
		s/(?<!-)---(?!-)/\\(em/g;
		s/\@bullet({}|\b)/\\(bu/g;
		s/\@dots({}|\b)/\\&.../g;

		# Examples should be indented and otherwise untouched
		if (/^\@example$/)
		{
			$indent += 2;
			print qq{.SP\n.PD 0\n};
			$inexample = 1;
			next;
		}
		if ($inexample)
		{
			if (/^\@end example$/)
			{
				$indent -= 2;
				print qq{\n.PD\n.IP "" $indent\n};
				$inexample = 0;
				next;
			}
			if (/^[ 	]*$/)
			{
				print ".SP\n";
				next;
			}

			# Preserve the newline.
			$_ = qq{.IP "" $indent\n} . $_;
		}

		# Compress blank lines into a single line.  This and its
		# corresponding skip purposely bracket the @menu and comment
		# removal so that blanks on either side of a menu are
		# compressed after the menu is removed.
		if (/^[ 	]*$/)
		{
			$inblank = 1;
			next;
		}

		# Not used
		if (/^\@(ignore|menu)$/)
		{
			$inmenu++;
			next;
		}
		# Delete menu contents.
		if ($inmenu)
		{
			next unless /^\@end (ignore|menu)$/;
			$inmenu--;
			next;
		}

		# Remove comments
		next if /^\@c(omment)?\b/;

		# Ignore includes.
		next if /^\@include\b/;

		# It's okay to ignore this keyword - we're not using any
		# first-line indent commands at all.
		next if s/^\@noindent\s*$//;

		# @need is only significant in printed manuals.
		next if s/^\@need\s+.*$//;

		# If we didn't hit the previous check and $inblank is set, then
		# we just finished with some number of blanks.  Print the man
		# page blank symbol before continuing processing of this line.
		if ($inblank)
		{
			print ".SP\n";
			$inblank = 0;
		}

		# Chapter headers.
		$last_header = $1 if s/^\@node\s+(.*)$/.SH "$1"/;
		if (/^\@appendix\w*\s+(.*)$/)
		{
			my $content = $1;
			$content =~ s/^$last_header(\\\(em|\s+)?//;
			next if $content =~ /^\s*$/;
			s/^\@appendix\w*\s+.*$/.SS "$content"/;
		}

		# Tables are similar to examples, except we need to handle the
		# keywords.
		if (/^\@(itemize|table)(\s+(.*))?$/)
		{
			$indent += 2;
			push @table_headers, $table_header;
			push @table_footers, $table_footer;
			my $content = $3;
			if (/^\@itemize/)
			{
				my $bullet = $content;
				$table_header = qq{.IP "$bullet" $indent\n};
				$table_footer = "";
			}
			else
			{
				my $hi = $indent - 2;
				$table_header = qq{.IP "" $hi\n};
				$table_footer = qq{\n.IP "" $indent};
				if ($content)
				{
					$table_header .= "$content\{";
					$table_footer = "\}$table_footer";
				}
			}
			$intable++;
			next;
		}

		if ($intable)
		{
			if (/^\@end (itemize|table)$/)
			{
				$table_header = pop @table_headers;
				$table_footer = pop @table_footers;
				$indent -= 2;
				$intable--;
				next;
			}
			s/^\@itemx?(\s+(.*))?$/$table_header$2$table_footer/;
			# Fall through so the rest of the table lines are
			# processed normally.
		}

		# Index entries.
		s/^\@cindex\s+(.*)$/.IX "$1"/;

		$_ = "$last$_" if $last;
		undef $last;

		# Trap keywords
		$nk = qr/
				\@(\w+)\{
				(?{ debug_print "$ret MATCHED $&\nPUSHING $1\n";
				    push @parent, $1; })      # Keep track of the last keyword
				                              # keyword we encountered.
				((?>
					[^{}]|(?<=\@)[{}]     # Non-braces...
						|             #    ...or...
					(??{ $nk })           # ...nested keywords...
				)*)                           # ...without backtracking.
				\}
				(?{ debug_print "$ret MATCHED $&\nPOPPING ",
				                pop (@parent), "\n"; })            # Lose track of the current keyword.
			/x;

		$ret = "m//";
		if (/\@\w+\{(?:[^{}]|(?<=\@)[{}]|(??{ $nk }))*$/)
		{
			# If there is an opening keyword on this line without a
			# close bracket, we need to find the close bracket
			# before processing the line.  Set $last to append the
			# next line in the next pass.
			$last = $_;
			next;
		}

		# Okay, the following works somewhat counter-intuitively.  $nk
		# processes the whole line, so @parent gets loaded properly,
		# then, since no closing brackets have been found for the
		# outermost matches, the innermost matches match and get
		# replaced first.
		#
		# For example:
		#
		# Processing the line:
		#
		#   yadda yadda @code{yadda @var{foo} yadda @var{bar} yadda}
		#
		# Happens something like this:
		#
		# 1. Ignores "yadda yadda "
		# 2. Sees "@code{" and pushes "code" onto @parent.
		# 3. Ignores "yadda " (backtracks and ignores "yadda yadda
		#                      @code{yadda "?)
		# 4. Sees "@var{" and pushes "var" onto @parent.
		# 5. Sees "foo}", pops "var", and realizes that "@var{foo}"
		#    matches the overall pattern ($nk).
		# 6. Replaces "@var{foo}" with the result of:
		#
		#      do_keyword $file, $parent[$#parent], $1, $2;
		#
		#    which would be "\Ifoo\B", in this case, because "var"
		#    signals a request for italics, or "\I", and "code" is
		#    still on the stack, which means the previous style was
		#    bold, or "\B".
		#
		# Then the while loop restarts and a similar series of events
		# replaces "@var{bar}" with "\Ibar\B".
		#
		# Then the while loop restarts and a similar series of events
		# replaces "@code{yadda \Ifoo\B yadda \Ibar\B yadda}" with
		# "\Byadda \Ifoo\B yadda \Ibar\B yadda\R".
		#
		$ret = "s///";
		@parent = ("");
		while (s/$nk/do_keyword $file, $parent[$#parent], $1, $2/e)
		{
			# Do nothing except reset our last-replacement
			# tracker - the replacement regex above is handling
			# everything else.
			debug_print "FINAL MATCH $&\n";
			@parent = ("");
		}

		# Finally, unprotect texinfo special characters.
		s/\@://g;
		s/\@([{}])/$1/g;

		# Verify we haven't left commands unprocessed.
		die "Unprocessed command at line $. of file \`$file': "
		    . ($1 ? "$1\n" : "<EOL>\n")
			if /^(?>(?:[^\@]|\@\@)*)\@(\w+|.|$)/;

		# Unprotect @@.
		s/\@\@/\@/g;

		# And print whatever's left.
		print $_;
	}
}
