#!/bin/sh
#

# Script generates dtb file ($3) from dts source ($2) in build tree S ($1)
S=$1
dts="$2"
dtb_path=$3

if [ -z "$dts" ]; then
    echo "No DTS specified"
    exit 1
fi

if [ -z "${MACHINE}" ]; then
    MACHINE=$(uname -m)
fi

: "${DTC:=dtc}"
: "${ECHO:=echo}"
: "${CPP:=cpp}"

for d in ${dts}; do
    dtb="${dtb_path}/$(basename "$d" .dts).dtb"
    ${CPP} -P -x assembler-with-cpp -I "$S/dts/include" -I "$S/contrib/device-tree/include" -I "$S/dts/${MACHINE}" -I "$S/contrib/device-tree/src/${MACHINE}" -I "$S/contrib/device-tree/src/" -include "$d" -include "$S/dts/freebsd-compatible.dts" /dev/null |
	${DTC} -@ -O dtb -o "$dtb" -b 0 -p 1024 -i "$S/dts/${MACHINE}" -i "$S/contrib/device-tree/src/${MACHINE}" -i "$S/contrib/device-tree/src/"
done
