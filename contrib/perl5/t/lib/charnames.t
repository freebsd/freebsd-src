#!./perl

BEGIN {
    unless(grep /blib/, @INC) {
	chdir 't' if -d 't';
	unshift @INC, '../lib' if -d '../lib';
    }
}

$| = 1;
print "1..12\n";

use charnames ':full';

print "not " unless "Here\N{EXCLAMATION MARK}?" eq "Here\041?";
print "ok 1\n";

{
  use bytes;			# UTEST can switch utf8 on

  print "# \$res=$res \$\@='$@'\nnot "
    if $res = eval <<'EOE'
use charnames ":full";
"Here: \N{CYRILLIC SMALL LETTER BE}!";
1
EOE
      or $@ !~ /above 0xFF/;
  print "ok 2\n";
  # print "# \$res=$res \$\@='$@'\n";

  print "# \$res=$res \$\@='$@'\nnot "
    if $res = eval <<'EOE'
use charnames 'cyrillic';
"Here: \N{Be}!";
1
EOE
      or $@ !~ /CYRILLIC CAPITAL LETTER BE.*above 0xFF/;
  print "ok 3\n";
}

# If octal representation of unicode char is \0xyzt, then the utf8 is \3xy\2zt
$encoded_be = "\320\261";
$encoded_alpha = "\316\261";
$encoded_bet = "\327\221";
{
  use charnames ':full';

  print "not " unless "\N{CYRILLIC SMALL LETTER BE}" eq $encoded_be;
  print "ok 4\n";

  use charnames qw(cyrillic greek :short);

  print "not " unless "\N{be},\N{alpha},\N{hebrew:bet}" 
    eq "$encoded_be,$encoded_alpha,$encoded_bet";
  print "ok 5\n";
}

{
    use charnames ':full';
    print "not " unless "\x{263a}" eq "\N{WHITE SMILING FACE}";
    print "ok 6\n";
    print "not " unless length("\x{263a}") == 1;
    print "ok 7\n";
    print "not " unless length("\N{WHITE SMILING FACE}") == 1;
    print "ok 8\n";
    print "not " unless sprintf("%vx", "\x{263a}") eq "263a";
    print "ok 9\n";
    print "not " unless sprintf("%vx", "\N{WHITE SMILING FACE}") eq "263a";
    print "ok 10\n";
    print "not " unless sprintf("%vx", "\xFF\N{WHITE SMILING FACE}") eq "ff.263a";
    print "ok 11\n";
    print "not " unless sprintf("%vx", "\x{ff}\N{WHITE SMILING FACE}") eq "ff.263a";
    print "ok 12\n";
}
