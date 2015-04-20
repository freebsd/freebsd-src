#!/bin/sh
# $FreeBSD$

ret=0
CHECK_UNRESOLVED=1
while getopts "U" flag; do
	case "${flag}" in
		U) CHECK_UNRESOLVED=0 ;;
	esac
done
shift $((OPTIND-1))

mime=$(file -L --mime-type $1)
case $mime in
*application/x-executable);;
*application/x-sharedlib);;
*) echo "Not an elf file" >&2 ; exit 1;;
esac

# Gather all symbols from the target
unresolved_symbols=$(nm -D -u --format=posix "$1" | awk '$2 == "U" {print $1}' | tr '\n' ' ')
ldd_libs=$(ldd $1 | awk '{print $1 ":" $3}')

libkey() {
	libkey="lib_symbols_$1"
	patterns=[.+,-]
	replacement=_
	while :; do
		case " ${libkey} " in
			*${patterns}*)
				libkey="${libkey%%${patterns}*}${replacement}${libkey#*${patterns}}"
				;;
			*)
				break
				;;
		esac
	done
	return 0
}

# Check for useful libs
list_libs=
resolved_symbols=
for lib in $(readelf -d $1 | awk '$2 ~ /\(?NEEDED\)?/ { sub(/\[/,"",$NF); sub(/\]/,"",$NF); print $NF }'); do
	echo -n "checking if $lib is needed: "
	for libpair in ${ldd_libs}; do
		case "${libpair}" in
			${lib}:*) libpath="${libpair#*:}" && break ;;
		esac
	done
	list_libs="$list_libs $lib"
	foundone=
	lib_symbols="$(nm -D --defined-only --format=posix "${libpath}" | awk '$2 ~ /R|D|T|W|B|V/ {print $1}' | tr '\n' ' ')"
	if [ ${CHECK_UNRESOLVED} -eq 1 ]; then
		# Save the global symbols for this lib
		libkey "${lib}"
		setvar "${libkey}"  "${lib_symbols}"
	fi
	for fct in ${lib_symbols}; do
		case " ${unresolved_symbols} " in
			*\ ${fct}\ *) foundone="${fct}" && break ;;
		esac
	done
	if [ -n "${foundone}" ]; then
		echo "yes... ${foundone}"
	else
		echo "no"
		ret=1
	fi
done

if [ ${CHECK_UNRESOLVED} -eq 1 ]; then
	for sym in ${unresolved_symbols}; do
		found=0
		for lib in ${list_libs}; do
			libkey "${lib}"
			eval "lib_symbols=\"\${${libkey}}\""
			# lib_symbols now contains symbols for the lib.
			case " ${lib_symbols} " in
				*\ ${sym}\ *) found=1 && break ;;
			esac
		done
		if [ $found -eq 0 ]; then
			echo "Unresolved symbol $sym"
			ret=1
		fi
	done
fi

exit ${ret}
