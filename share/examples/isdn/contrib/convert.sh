#!/bin/sh
#
# convert recorded message to WAV format, optionally send it via mail
#
# by:		Stefan Herrmann <stefan@asterix.webaffairs.net>
# Date:	Fr  22 Mai 1998 14:18:40 CEST
#
# $FreeBSD$

CAT=/bin/cat
RM=/bin/rm
SOX=/usr/local/bin/sox
ALAW2ULAW=/usr/local/bin/alaw2ulaw
MAIL=/usr/bin/mail
GZIP=/usr/bin/gzip
ZIP=/usr/local/bin/zip
UUENCODE=/usr/bin/uuencode

inputfile=""
outfilename=""
mailto=""
iF=0
oF=0
mF=0

set -- `getopt i:o:m: $*`

if test $? != 0
then
        echo 'Usage: r2w -i <input file> -o <outfile name>.wav -m <email address>'
        exit 1
fi

for i
do
        case "$i"
        in
                -i)
                        inputfile=$2
                        iF=1
                        shift
				shift
                        ;;
                -o)
                        outfilename=$2
                        oF=1
                        shift
				shift
                        ;;
                -m)
                        mailto=$2
                        mF=1
                        shift
				shift
                        ;;
                --)
                        shift
                        break
                        ;;
        esac
done

if [ $iF -eq 0 -o $oF -eq 0 ]
then
        echo 'Usage: r2w -i <input file> -o <outfile name>.wav -m <email address>'
        exit 1
fi

if [ $iF -eq 1 -a $oF -eq 1 ]
then
	echo
	echo "converting $inputfile to $outfilename.wav ..."

	$CAT $inputfile | $ALAW2ULAW | $SOX -t raw -U -b -r 8000 - -t .wav $outfilename.wav
fi

if [ $iF -eq 1 -a $oF -eq 1 -a $mF -eq 1 ]
then
	echo "... and sending it via email to $mailto ..."
	$UUENCODE $outfilename.wav message.wav | $MAIL -s"new message $outfilename" $mailto && $RM $outfilename.wav
	# only usefull when sending over the internet
	#$GZIP -c $outfilename.wav | $UUENCODE message.zip | $MAIL -s"Nachricht vom ISDN Anrufbeantworter" $mailto && $RM $outfilename.wav
fi

echo "done."
echo
