# $Id: ndcedit.awk,v 1.1 1996/08/29 21:46:46 peter Exp $
NR == 3 {
	print "#"
	print "# This file is generated automatically, do not edit it here!"
	print "# Please change src/usr.sbin/ndc/ndcedit.awk instead"
	print "#"
	print ""

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
	        if (/PATH=/) {
			gsub(":/usr/ucb:", ":", $0);
		      	if (!/export/) {
				$0=$0"\nexport PATH";
			}
		} 
		print;
	}
}
