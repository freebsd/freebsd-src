#! /bin/sh

: '@(#)yearistype.sh	7.4'

case $#-$2 in
	2-even)		case $1 in
				*[24680])			exit 0 ;;
				*)				exit 1 ;;
			esac ;;
	2-nonpres)	case $1 in
				*[02468][048]|*[13579][26])	exit 1 ;;
				*)				exit 0 ;;
			esac ;;
	2-odd)		case $1 in
				*[13579])			exit 0 ;;
				*)				exit 1 ;;
			esac ;;
	2-uspres)	case $1 in
				*[02468][048]|*[13579][26])	exit 0 ;;
				*)				exit 1 ;;
			esac ;;
	2-*)		echo "$0: wild type - $2" >&2
			exit 1 ;;
	*)		echo "$0: usage is $0 year type" >&2
			exit 1 ;;
esac
