#!/bin/sh
#
# $FreeBSD$
#

# Bail if things fail and be verbose about what we are doing
set -ex

# Arguments are as follows: file destdir chunksize description
FILE=$1; shift
DEST=$1; shift
CHUNK_SIZE=$1; shift
DESCR=$1; shift

# Make sure we can read the file.
[ -r ${FILE} ]

# Create clean working area to stick file chunks and list in
rm -rf ${DEST} || true
mkdir -p ${DEST}

# Split the file into pieces
prefix=`basename $FILE`
dd if=${FILE} bs=16k iseek=1 | split -b ${CHUNK_SIZE}k - ${DEST}/${prefix}.

# Create a special file for the first 16k that gets stuck on the boot
# floppy
files=`ls ${DEST}/${prefix}.*`
first=`echo "${files}" | head -1`
bootchunk="${DEST}/${prefix}.boot"
dd if=${FILE} of=${bootchunk} bs=16k count=1

# Create the split index file
echo `basename ${bootchunk}` "\"Boot floppy\"" > ${DEST}/${prefix}.split
i=1
for file in ${files}; do
	echo `basename ${file}` "\"${DESCR} floppy ${i}\"" >> ${DEST}/${prefix}.split
	i=$(($i + 1))
done
