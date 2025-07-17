#!/usr/local/bin/ksh93

BSDDEVS="ad|da|mlxd|myld|aacd|ided|twed"
ls /dev|egrep "^($BSDDEVS)[0-9]+\$" |sed 's/^/\/dev\//'
