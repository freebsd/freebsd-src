#!./perl
#
# $Id: soundex.t,v 1.2 1994/03/24 00:30:27 mike Exp $
#
# test module for soundex.pl
#
# $Log: soundex.t,v $
# Revision 1.2  1994/03/24  00:30:27  mike
# Subtle bug (any excuse :-) spotted by Rich Pinder <rpinder@hsc.usc.edu>
# in the way I handles leasing characters which were different but had
# the same soundex code.  This showed up comparing it with Oracle's
# soundex output.
#
# Revision 1.1  1994/03/02  13:03:02  mike
# Initial revision
#
#

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
}

use Text::Soundex;

$test = 0;
print "1..13\n";

while (<DATA>)
{
  chop;
  next if /^\s*;?#/;
  next if /^\s*$/;

  ++$test;
  $bad = 0;

  if (/^eval\s+/)
  {
    ($try = $_) =~ s/^eval\s+//;

    eval ($try);
    if ($@)
    {
      $bad++;
      print "not ok $test\n";
      print "# eval '$try' returned $@";
    }
  }
  elsif (/^\(/)
  {
    ($in, $out) = split (':');

    $try = "\@expect = $out; \@got = &soundex $in;";
    eval ($try);

    if (@expect != @got)
    {
      $bad++;
      print "not ok $test\n";
      print "# expected ", scalar @expect, " results, got ", scalar @got, "\n";
      print "# expected (", join (', ', @expect),
	    ") got (", join (', ', @got), ")\n";
    }
    else
    {
      while (@got)
      {
	$expect = shift @expect;
	$got = shift @got;

	if ($expect ne $got)
	{
	  $bad++;
	  print "not ok $test\n";
	  print "# expected $expect, got $got\n";
	}
      }
    }
  }
  else
  {
    ($in, $out) = split (':');

    $try = "\$expect = $out; \$got = &soundex ($in);";
    eval ($try);

    if ($expect ne $got)
    {
      $bad++;
      print "not ok $test\n";
      print "# expected $expect, got $got\n";
    }
  }

  print "ok $test\n" unless $bad;
}

__END__
#
# 1..6
#
# Knuth's test cases, scalar in, scalar out
#
'Euler':'E460'
'Gauss':'G200'
'Hilbert':'H416'
'Knuth':'K530'
'Lloyd':'L300'
'Lukasiewicz':'L222'
#
# 7..8
#
# check default bad code
#
'2 + 2 = 4':undef
undef:undef
#
# 9
#
# check array in, array out
#
('Ellery', 'Ghosh', 'Heilbronn', 'Kant', 'Ladd', 'Lissajous'):('E460', 'G200', 'H416', 'K530', 'L300', 'L222')
#
# 10
#
# check array with explicit undef
#
('Mike', undef, 'Stok'):('M200', undef, 'S320')
#
# 11..12
#
# check setting $Text::Soundex::noCode
#
eval $soundex_nocode = 'Z000';
('Mike', undef, 'Stok'):('M200', 'Z000', 'S320')
#
# 13
#
# a subtle difference between me & oracle, spotted by Rich Pinder
# <rpinder@hsc.usc.edu>
#
CZARKOWSKA:C622
