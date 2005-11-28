#!/bin/sh

# $FreeBSD$

cp /usr/src/drm/bsd-core/*.[ch] .
rm i810*.[ch]
rm via*.[ch]

# Replace drm_pciids.h with one with a $FreeBSD$
rm -f drm_pciids.h
echo "/*" >> drm_pciids.h
echo " * \$FreeBSD\$" >> drm_pciids.h
echo " */" >> drm_pciids.h
cat /usr/src/drm/bsd-core/drm_pciids.h >> drm_pciids.h

for i in `ls *.[ch]`; do
	mv $i $i.orig
	perl drm-subprocess.pl < $i.orig > $i
done

