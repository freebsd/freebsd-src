# $Id: ndcedit.awk,v 1.8 1998/07/31 08:47:16 phk Exp $
NR == 3 {
	print "#"
	print "# This file is generated automatically, do not edit it here!"
	print "# Please change src/usr.sbin/ndc/ndcedit.awk instead"
	print "#"
	print ""

	print "# If there is a global system configuration file, suck it in."
	print "if [ -f /etc/defaults/rc.conf ]; then"
	print "\t. /etc/defaults/rc.conf"
	print "elif [ -f /etc/rc.conf ]; then"
	print "\t. /etc/rc.conf"
	print "fi\n"
}
{
	if ($1 == "named") {
		printf "\t\t# $named_flags is imported from /etc/rc.conf\n"
		printf "\t\tif [ \"X${named_enable}\" = X\"YES\" ]; then\n"
		printf "\t\t\t${named_program} ${named_flags} && {\n"
		getline
		printf "\t%s\n", $0
		getline
		printf "\t%s\n", $0
		getline
		printf "\t%s\n", $0
		printf "\t\tfi\n"
	} else if (/PS=`/) {
		printf "\tif [ -f /proc/$PID/status ]; then\n"
		printf "\t\tPS=`cat /proc/$PID/status 2>/dev/null | grep named`\n"
		printf "\telse\n"
		gsub("\t", "\t\t", $0);
		print;
		printf "\tfi\n"
	} else {
	        if (/PATH=/) {
			gsub(":/usr/ucb:", ":", $0);
		} 
		print;
	}
}
