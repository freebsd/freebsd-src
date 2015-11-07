#!/bin/sh

UNIDATA=$(grep ^unidata etc/unicode.conf | cut -f 2 -d " ")
UTF8=$(grep ^cldr etc/unicode.conf | cut -f 2 -d " ")/UTF-8.cm
CHARMAPS=etc/charmaps

if [ -z "$1" ]; then
	echo "Usage: $0 <unicode string>"
	exit
fi

UCS=$*
UCS_=$(echo $* | sed -e 's/ /./g')
echo UCS: ${UCS}

echo UTF-8.cm:
grep "${UCS_}" ${UTF8} | sed -e 's/   */	/g'

echo UNIDATA:
grep "${UCS_}" ${UNIDATA}
L=$(grep "${UCS_}" ${UNIDATA})

echo UCC:
grep "${UCS_}" ${UNIDATA} | awk -F\; '{ print $1 }'


echo CHARMAPS:
grep ${UCS_} ${CHARMAPS}/* | sed -e "s|${CHARMAPS}/||g"
grep ${UCC} ${CHARMAPS}/* | sed -e "s|${CHARMAPS}/||g"
