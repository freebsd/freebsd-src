#!/bin/sh

# $FreeBSD: src/sys/dev/drm/drm-preprocess.sh,v 1.3.8.1 2008/11/25 02:59:29 kensmith Exp $

cvs up -CPd *.[ch]

for i in `ls *.[ch]`; do
	mv $i $i.cvs
done

cp /usr/src/drm/bsd-core/*.[ch] .
rm -f i810*.[ch]
rm via*.[ch]

(cd /usr/src/drm/bsd-core/ && make drm_pciids.h)

# Replace drm_pciids.h with one with a $FreeBSD: src/sys/dev/drm/drm-preprocess.sh,v 1.3.8.1 2008/11/25 02:59:29 kensmith Exp $
line=`grep \\\$FreeBSD drm_pciids.h.cvs`
rm -f drm_pciids.h
echo "/*" >> drm_pciids.h
echo "$line" >> drm_pciids.h
echo " */" >> drm_pciids.h
cat /usr/src/drm/bsd-core/drm_pciids.h >> drm_pciids.h

for i in `ls *.[ch]`; do
	mv $i $i.orig
	perl drm-subprocess.pl < $i.orig > $i
done

for orig in `ls *.[ch].cvs`; do
	real=`echo "$orig" | sed s/.cvs//`
	line=`grep __FBSDID $orig | sed s/\\\\\$/\\\\\\\\\$/g`
	perl -pi -e "s|__FBSDID.*|$line|g" $real
done

rm *.cvs
