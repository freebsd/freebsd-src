:
if [ -f bindist.tgz.aa ] ; then
	cat bindist.tgz.?? | zcat | ( cd / ; cpio -H tar -idumV )
fi
