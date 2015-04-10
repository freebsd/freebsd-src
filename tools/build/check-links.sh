#!/bin/sh
# $FreeBSD$

mime=$(file -L --mime-type $1)
case $mime in
*application/x-executable);;
*application/x-sharedlib);;
*) echo "Not an elf file" >&2 ; exit 1;;
esac

# Check for useful libs
list_libs=""
for lib in $(readelf -d $1 | awk '$2 ~ /\(?NEEDED\)?/ { sub(/\[/,"",$NF); sub(/\]/,"",$NF); print $NF }'); do
        echo -n "checking if $lib is needed: "
        libpath=$(ldd $1 | awk -v lib=$lib '$1 == lib { print $3 }')
        list_libs="$list_libs $libpath"
        foundone=0
        for fct in $(nm -D $libpath | awk '$2 == "R" || $2 == "D" || $2 == "T" || $2 == "W" || $2 == "B" { print $3 }'); do
                nm -D $1 | awk -v s=$fct '$1 == "U" && $2 == s { found=1 ; exit } END { if (found != 1) { exit 1 } }' && foundone=1 && break
        done
        if [ $foundone -eq 1 ]; then
                echo -n "yes... "
                nm -D $1 | awk -v s=$fct '$1 == "U" && $2 == s { print $2 ; exit }'
        else
                echo "no"
        fi
done

for sym in $(nm -D $1 | awk '$1 == "U" { print $2 }'); do
        found=0
        for l in ${list_libs} ; do
                nm -D $l | awk -v s=$sym '($2 == "R" || $2 == "D" || $2 == "T" || $2 == "W" || $2 == "B") && $3 == s { found=1 ; exit } END { if (found != 1) { exit 1 } }' && found=1 && break
        done
        if [ $found -eq 0 ]; then
                echo "Unresolved symbol $sym"
        fi
done
