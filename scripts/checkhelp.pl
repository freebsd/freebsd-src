#!/usr/bin/perl
# checkhelp.pl - finds configuration options that have no
#                corresponding section in the help file
#
# made by Meelis Roos (mroos@tartu.cyber.ee)

# read the help file
@options=split /\n/, `grep '^CONFIG' Documentation/Configure.help`;
die "Can't read Documentation/Configure.help\n" if $#options == -1;

#read all the files
foreach $file (@ARGV)
{
	open (FILE, $file) || die "Can't open $file: $!\n";
	while (<FILE>) {
		# repeat until no CONFIG_* are left
		while (/^\s*(bool|tristate|dep_tristate|string|int|hex).*' *(.*)'.*(CONFIG_\w*)/) {
			$what=$3;
			$name=$2;
			s/$3//;
			@found = grep (/$what$/, @options);
			if ($#found == -1) {
				next if $nohelp{$what};
				print "$name\n$what\n  No help for $what\n\n";
				$nohelp{$what}=1;
			}
		}
	}
	close (FILE);
}
