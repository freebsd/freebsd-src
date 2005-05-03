#!/bin/sh
#
# This script generates the disk layout for the CD images built by the FreeBSD
# release engineers as dictated by a specified master INDEX file.  Each disc
# contains the master INDEX, it's assigned list of packages, and the
# appropriate tree of category symlinks.
#
# Usage: package-tress.sh <copy method> <INDEX> <package tree> <destination>
#
# $FreeBSD$

# Verify the command line
if [ $# -ne 4 ]; then
    echo "Invalid number of arguments"
    echo "Usage: package-trees.sh <copy method> <INDEX> <tree> <destination>"
    exit 1
fi

COPY=$1 ; shift
INDEX=$1 ; shift
TREE=$1 ; shift
DESTDIR=$1 ; shift

# First, determine the highest disc number.
high_disc=`cut -d '|' -f 14 ${INDEX} | sort -n | tail -1`
echo "Generating trees for ${high_disc} discs"

# Second, initialize the trees for each disc
for disc in `jot $high_disc`; do
    rm -rf ${DESTDIR}/disc${disc}/packages
    mkdir -p ${DESTDIR}/disc${disc}/packages/All
    cp ${INDEX} ${DESTDIR}/disc${disc}/packages/INDEX
done

# Third, run through the INDEX copying each package to its appropriate CD and
# making the appropriate category symlinks
while read line; do
    disc=`echo $line | cut -d '|' -f 14`
    package=`echo $line | cut -d '|' -f 1`
    categories=`echo $line | cut -d '|' -f 7`
    discdir=${DESTDIR}/disc${disc}
    if [ -n "$PKG_VERBOSE" ]; then
	echo "--> Copying $package to Disc $disc"
    fi
    ${COPY} ${TREE}/All/${package}.tbz ${discdir}/packages/All
    for cat in ${categories}; do
        catdir=${discdir}/packages/${cat}
	mkdir -p ${catdir}
	ln -s ../All/${package}.tbz ${catdir}
    done
done < ${INDEX}

# Fourth, output du info for the relative size of the trees.
discs=""
for disc in `jot $high_disc`; do
    discs="${discs} disc${disc}"
done
(cd ${DESTDIR}; du -sh ${discs})
