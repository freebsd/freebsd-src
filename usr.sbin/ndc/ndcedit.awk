# $Id: ndcedit.awk,v 1.1.2.2 1996/11/12 09:11:33 phk Exp $
NR == 3 {
	print "#"
	print "# This file is generated automatically, do not edit it here!"
	print "# Please change src/usr.sbin/ndc/ndcedit.awk instead"
	print "#"
	print ""

	print "# If there is a global system configuration file, suck it in."
	print "if [ -f /etc/rc.conf ]; then"
	print "\t. /etc/rc.conf"
	print "fi\n"
}
{
	if ($1 == "named") {
		printf "\t\t# $namedflags is imported from /etc/rc.conf\n"
		printf "\t\tif [ \"X${named_flags}\" != X\"NO\" ]; then\n"
		printf "\t\t\tnamed ${named_flags} && {\n"
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
