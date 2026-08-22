#!/bin/sh

# Less OSC 8 handler.

echo() { printf %s\\n "$*"; }

# Open a link of the form "file://HOST/PATH" or "file:///PATH"
open_file() {
	case $1 in
	file:///* | file://localhost/*)
		less -- "/${1#file://*/}"
		;;

	file://*/*)
		linkhost=${1#file://}; linkhost=${linkhost%%/*}
		localhost=${HOSTNAME:-$(bash -c 'printf %s "$HOSTNAME"' || uname -n)} 2>/dev/null

		case $localhost in
		*/* | '')
			echo "unable to detect local hostname: $localhost"
			exit 1
			;;
		"$linkhost")
			less -- "/${1#file://*/}"
			;;
		*)
			echo "cannot open remote file: $1"
			exit 1
			;;
		esac
		;;

	*)
		echo "invalid file link: $1"
		exit 1
		;;
	esac
}

# Open a link of the form "man:NAME" or "man:NAME(SECTION)".
open_man() {
	case $1 in
	man:?*\(*\) )
		sect=${1#*\(}; sect=${sect%?}
		name=${1#man:}; name=${name%%\(*}
		man -- ${sect:+"$sect"} "$name"
		;;

	man:?*)
		man -- "${1#man:}"
		;;

	*)
		echo "invalid man link: $1"
		exit 1
		;;
	esac
}

case $1 in
file:*) open_file "$1" ;;
man:*) open_man "$1" ;;
*) echo "unsupported link type: $1"; exit 1 ;;
esac
