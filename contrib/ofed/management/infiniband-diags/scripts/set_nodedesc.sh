#!/bin/sh

if [ -f /etc/sysconfig/network ]; then
. /etc/sysconfig/network
fi

ib_sysfs="/sys/class/infiniband"
newname="$HOSTNAME"


function usage
{
	echo "Usage: `basename $0` [-hv] [<name>]"
	echo "   set the node_desc field of all hca's found in \"$ib_sysfs\""
	echo "   -h this help"
	echo "   -v view all node descriptors"
	echo "   [<name>] set name to name specified."
	echo "      Default is to use the hostname: \"$HOSTNAME\""
	exit 2
}

function viewall
{
   for hca in `ls $ib_sysfs`; do
      if [ -f $ib_sysfs/$hca/node_desc ]; then
         echo -n "$hca: "
         cat $ib_sysfs/$hca/node_desc
      else
         logger -s "Failed to set node_desc for : $hca"
      fi
   done
   exit 0
}

while getopts "hv" flag
do
   case $flag in
      "h") usage;;
      "v") viewall;;
   esac
done

shift $(($OPTIND - 1))

if [ "$1" != "" ]; then
	newname="$1"
fi

for hca in `ls $ib_sysfs`; do
   if [ -f $ib_sysfs/$hca/node_desc ]; then
      echo -n "$newname" >> $ib_sysfs/$hca/node_desc
   else
      logger -s "Failed to set node_desc for : $hca"
   fi
done

exit 0
