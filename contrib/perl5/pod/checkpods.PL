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
# From roderick@gate.netThu Sep  5 17:19:30 1996
# Date: Thu, 05 Sep 1996 00:11:22 -0400
# From: Roderick Schertler <roderick@gate.net>
# To: perl5-porters@africa.nicoh.com
# Subject: POD lines with only spaces
#
# There are some places in the documentation where a POD directive is
# ignored because the line before it contains whitespace (and so the
# directive doesn't start a paragraph).  This patch adds a way to check
# for these to the pod Makefile (though it isn't made part of the build
# process, which would be a good idea), and fixes those places where the
# problem currently exists.
#
#  Version 1.00  Original.
#  Version 1.01  Andy Dougherty <doughera@lafcol.lafayette.edu>
#    Trivial modifications to output format for easier auto-parsing
#    Broke it out as a separate function to avoid nasty
#	Make/Shell/Perl quoting problems, and also to make it easier
#	to grow.  Someone will probably want to rewrite in terms of
#	some sort of Pod::Checker module.  Or something.  Consider this
#	a placeholder for the future.
#  Version 1.02  Roderick Schertler <roderick@argon.org>
#	Check for pod directives following any kind of unempty line, not
#	just lines of whitespace.

@directive = qw(head1 head2 item over back cut pod for begin end);
@directive{@directive} = (1) x @directive;

$exit = $last_unempty = 0;
while (<>) {
    chomp;
    if (/^=(\S+)/ && $directive{$1} && $last_unempty) {
	printf "%s: line %5d, no blank line preceeding directive =%s\n",
		$ARGV, $., $1;
	$exit = 1;
    }
    $last_unempty = ($_ ne '');
    if (eof) {
	close(ARGV);
	$last_unempty = 0;
    }
}
exit $exit
!NO!SUBS!

close OUT or die "Can't close $file: $!";
chmod 0755, $file or die "Can't reset permissions for $file: $!\n";
exec("$Config{'eunicefix'} $file") if $Config{'eunicefix'} ne ':';
chdir $origdir;
