# $Id: awkedit,v 1.1.1.1 1996/08/29 19:42:59 peter Exp $
NR == 3 {
	print "# If there is a global system configuration file, suck it in."
	print "if [ -f /etc/sysconfig ]; then"
	print "\t. /etc/sysconfig"
	print "fi\n"
}
{
	if ($1 == "named") {
		printf "\t\t# $namedflags is imported from /etc/sysconfig\n"
		printf "\t\tif [ \"X${namedflags}\" != \"XNO\" ]; then\n"
		printf "\t\t\tnamed ${namedflags} && {\n"
		getline
		printf "\t%s\n", $0
		getline
		printf "\t%s\n", $0
		getline
		printf "\t%s\n", $0
		printf "\t\tfi\n"
	} else {
		gsub(":/usr/ucb:", ":", $0);
		print;
	}
}
