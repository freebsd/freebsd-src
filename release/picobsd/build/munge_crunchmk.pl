#!/usr/bin/perl -wi

# Post process a crunch1.mk file:
#
# from...
# ftp_make:
#	(cd $(ftp_SRCDIR) && make depend && make $(ftp_OBJS))
#
# to...
# ftp_make:
#	(cd $(ftp_SRCDIR) && make obj && make depend && make $(OPTS) $(ftp_OPTS) $(ftp_OBJS))

use strict;

while (my $line = <>) {
	if ( $line =~ /(.*)make depend && make (\$\((.*?)_OBJS\).*)/ ) {
		my $start = $1;		# The start of the line.
		my $end = $2;		# The end of the line.
		my $prog = $3;		# The parsed out name of the program.

		print $start;
		print 'make obj && make depend && ';
		print 'make $(OPTS) $(' . $prog . '_OPTS) ';
		print $end, "\n";
	} else {
		print $line;
	}
}

#end
