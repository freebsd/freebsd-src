#!./perl

# test added 29th April 1999 by Paul Johnson (pjcj@transeda.com)
# updated    28th May   1999 by Paul Johnson

my $File;

BEGIN
{
  $File = __FILE__;
  if (-d 't')
  {
    chdir 't';
    $File =~ s/^t\W+//;                                 # Remove first directory
  }
  @INC = '../lib';
  require strict; import strict;
}

use Test;

BEGIN { plan tests => 12 }

use IO::File;

sub lineno
{
  my ($f) = @_;
  my $l;
  $l .= "$. ";
  $l .= $f->input_line_number;
  $l .= " $.";                     # check $. before and after input_line_number
  $l;
}

my $t;

open (F, $File) or die $!;
my $io = IO::File->new($File) or die $!;

<F> for (1 .. 10);
ok(lineno($io), "10 0 10");

$io->getline for (1 .. 5);
ok(lineno($io), "5 5 5");

<F>;
ok(lineno($io), "11 5 11");

$io->getline;
ok(lineno($io), "6 6 6");

$t = tell F;                                        # tell F; provokes a warning
ok(lineno($io), "11 6 11");

<F>;
ok(lineno($io), "12 6 12");

select F;
ok(lineno($io), "12 6 12");

<F> for (1 .. 10);
ok(lineno($io), "22 6 22");

$io->getline for (1 .. 5);
ok(lineno($io), "11 11 11");

$t = tell F;
# We used to have problems here before local $. worked.
# input_line_number() used to use select and tell.  When we did the
# same, that mechanism broke.  It should work now.
ok(lineno($io), "22 11 22");

{
  local $.;
  $io->getline for (1 .. 5);
  ok(lineno($io), "16 16 16");
}

ok(lineno($io), "22 16 22");
