:
if [ -f bin_tgz.aa ] ; then
	cat bin_tgz.?? | zcat | ( cd / ; cpio -H tar -idumV )
fi
