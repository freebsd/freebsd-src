#!/usr/local/bin/perl

use Config;
use File::Basename qw(&basename &dirname);
use Cwd;

# List explicitly here the variables you want Configure to
# generate.  Metaconfig only looks for shell variables, so you
# have to mention them as if they were shell variables, not
# %Config entries.  Thus you write
#  $startperl
# to ensure Configure will look for $Config{startperl}.
# Wanted:  $archlibexp

# This forces PL files to create target in same directory as PL file.
# This is so that make depend always knows where to find PL derivatives.
$origdir = cwd;
chdir dirname($0);
$file = basename($0, '.PL');
$file .= '.com' if $^O eq 'VMS';

open OUT,">$file" or die "Can't create $file: $!";

print "Extracting $file (with variable substitutions)\n";

# In this section, perl variables will be expanded during extraction.
# You can use $Config{...} to use Configure variables.

print OUT <<"!GROK!THIS!";
$Config{startperl}
    eval 'exec $Config{perlpath} -S \$0 \${1+"\$@"}'
    if \$running_under_some_shell;
!GROK!THIS!

# In the following, perl variables are not expanded during extraction.

print OUT <<'!NO!SUBS!';

use strict;
use warning;
no warning qw(once);

use Config;

require ByteLoader;

foreach my $infile (@ARGV)
{
    if ($infile =~ /\.p[ml]$/)
    {
	my $outfile = $infile . "c";

	open(OUT,"> $outfile") || die "Can't open $outfile: $!";

	if ($infile =~ /\.pl$/)
	{
	    print OUT "$Config{startperl}\n";
	    print OUT "    eval 'exec $Config{perlpath} -S \$0 \${1+\"\$@\"}'\n";
	    print OUT "    if \$running_under_some_shell;\n\n";
	}

	print OUT "use ByteLoader $ByteLoader::VERSION;\n";

	close(OUT);

	print "$^X -MO=Bytecode $infile >> $outfile\n";

	system("$^X -MO=Bytecode $infile >> $outfile");
    }
    else
    {
	warn "Don't know how to byte compile $infile";
    }
}
!NO!SUBS!

close OUT or die "Can't close $file: $!";
chmod 0755, $file or die "Can't reset permissions for $file: $!\n";
exec("$Config{'eunicefix'} $file") if $Config{'eunicefix'} ne ':';
chdir $origdir;
