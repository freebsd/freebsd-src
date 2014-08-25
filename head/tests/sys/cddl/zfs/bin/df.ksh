#! /usr/local/bin/ksh93 -p

newoptions=""

while getopts F:lnhmk c
do
        case $c in
                F)
                        newoptions="$newoptions -t $OPTARG"
                        ;;
                l)
                        ;;
                *)
			newoptions="$newoptions -$c"
                        ;;
	esac
done
shift $(($OPTIND - 1))

/bin/df $newoptions $*
