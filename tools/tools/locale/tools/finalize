#!/bin/sh
#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright 2015 John Marino <draco@marino.st>
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#
#
# This is a helper script for the Makefile in the parent directory.
# When the localization definitions are generated in the draft area,
# this script will copy base ones that others symlink to, and rearrange
# the generate makefile to pull the LOCALES first.
#

set -e

usage ()
{
	echo "finalize <type>' to package standard localization"
	echo "type must be one of { monetdef, msgdef, numericdef, timedef, colldef, ctypedef }"
	exit 1
}

[ $# -ne 1 ] && usage
[ $1 = "monetdef" -o $1 = "msgdef" -o $1 = "colldef" -o \
  $1 = "numericdef" -o $1 = "timedef" -o $1 = "ctypedef" ] || usage

self=$(realpath $0)
base=${BASEDIR:-$(dirname ${self})}
: ${ETCDIR:=${base}/../etc}
: ${TOOLSDIR:=${base}}
: ${OUTBASEDIR:=${base}/../${1}}
: ${OLD_DIR:=${OUTBASEDIR}.draft}
: ${NEW_DIR:=${OUTBASEDIR}}
old=${OLD_DIR}
new=${NEW_DIR}
: ${TMPDIR:=/tmp}
TEMP=${TMPDIR}/${1}.locales
TEMP2=${TMPDIR}/${1}.hashes
TEMP3=${TMPDIR}/${1}.symlinks
TEMP4=${TMPDIR}/${1}.mapped
FULLMAP=${TMPDIR}/utf8-map
FULLEXTRACT=${TMPDIR}/extracted-names
AWKCMD="/## PLACEHOLDER/ { \
	  while ( getline line < \"${TEMP}\" ) {print line} } \
	/## SYMPAIRS/ { \
	  while ( getline line < \"${TEMP3}\" ) {print line} } \
	/## LOCALES_MAPPED/ { \
	  while ( getline line < \"${TEMP4}\" ) {print line} } \
	!/## / { print \$0 }"

# Rename the sources with 3 components name into the POSIX version of the name using @modifier
mkdir -p $old $new
cd $old
pwd
for i in *_*_*.*.src; do
	if [ "$i" = "*_*_*.*.src" ]; then
		break
	fi
	oldname=${i%.*}
	nname=`echo $oldname | awk '{ split($0, a, "_"); print a[1]"_"a[3]"@"a[2];} '`
	mv -f ${oldname}.src ${nname}.src
	sed -i '' -e "s/${oldname}/${nname}/g" Makefile
done

# For variable without @modifier ambiguity do not keep the @modifier
for i in *@*.src; do
	if [ "$i" = "*@*.src" ]; then
		break
	fi
	oldname=${i%.*}
	shortname=${oldname%@*}
	if [ $(ls ${shortname}@* | wc -l) -eq 1 ] ; then
		mv -f $i ${shortname}.src
		sed -i '' -e "s/${oldname}/${shortname}/g" Makefile
	fi
done

# Rename the modifiers into non abbreviated version
for i in *@Latn.src; do
	if [ "$i" = "*@Latn.src" ]; then
		break
	fi
	mv -f ${i} ${i%@*}@latin.src
	sed -i '' -e "s/${i%.*}/${i%@*}@latin/g" Makefile
done

for i in *@Cyrl.src; do
	if [ "$i" = "*@Cyrl.src" ]; then
		break
	fi
	mv -f ${i} ${i%@*}@cyrillic.src
	sed -i '' -e "s/${i%.*}/${i%@*}@cyrillic/g" Makefile
done

# On locales with multiple modifiers rename the "default" version without the @modifier
default_locales="sr_RS@cyrillic"
for i in ${default_locales}; do
	localename=${i%@*}
	mod=${i#*@}
	for l in ${localename}.*@${mod}.src; do
		if [ "$l" = "${localename}.*@${mod}.src" ]; then
			break
		fi
		mv -f ${l} ${l%@*}.src
		sed -i '' -e "s/${l%.*}/${l%@*}/g" Makefile
	done
done
cd -

grep '^LOCALES+' ${old}/Makefile > ${TEMP}

if [ $1 = "ctypedef" ]
then
	keep=$(cat ${TEMP} | awk '{ print $2 ".src" }')
	(cd ${old} && md5 -r ${keep} | sort) > ${TEMP2}
	keep=$(awk '{ if ($1 != last1) print $2; last1 = $1; }' ${TEMP2})
	for original in ${keep}
	do
		cp ${old}/${original} ${new}/
	done
	awk '{ if ($1 == last1) { print "SYMPAIRS+=\t" last2 " " $2 } \
	else {last1 = $1; last2 = $2}}' ${TEMP2} > ${TEMP3}
	rm -f ${TEMP2}
	/usr/bin/sed -E -e 's/[ ]+/ /g' \
		${UNIDIR}/posix/UTF-8.cm \
		> ${ETCDIR}/final-maps/map.UTF-8

elif [ $1 = "colldef" ]
then
	awk -v tmp4=${TEMP4} '$1 == "SAME+=" && $0 !~ /legacy/ {
		orig=$2
		dest=$3
		gsub(/.*\./, "", orig)
		gsub(/.*\./, "", dest)
		if (orig != dest )
			print "LOCALES_MAPPED+=\t"$2 " "$3 > tmp4
		}' ${old}/Makefile

	for line in $(awk '{ print $3 }' ${TEMP4}); do
		sed -i '' "/^SAME.*$line$/d" ${old}/Makefile
	done
	echo "" >> ${TEMP4}

	keep=$(cat ${TEMP} | awk '{ print $2 }')
	for original in ${keep}
	do
		cp ${old}/${original}.src ${new}/
	done
else  # below is everything but ctypedef

	keep=$(cat ${TEMP} | awk '{ print $2 }')
	for original in ${keep}
	do
		cp ${old}/${original}.src ${new}/
	done

fi

grep -v '^LOCALES+' ${old}/Makefile | awk "${AWKCMD}" > ${new}/Makefile

rm -f ${TEMP} ${TEMP3} ${TEMP4}
