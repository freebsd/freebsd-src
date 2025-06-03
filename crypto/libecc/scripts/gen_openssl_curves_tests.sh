#/*
# *  Copyright (C) 2017 - This file is part of libecc project
# *
# *  Authors:
# *      Ryad BENADJILA <ryadbenadjila@gmail.com>
# *      Arnaud EBALARD <arnaud.ebalard@ssi.gouv.fr>
# *      Jean-Pierre FLORI <jean-pierre.flori@ssi.gouv.fr>
# *
# *  Contributors:
# *      Nicolas VIVET <nicolas.vivet@ssi.gouv.fr>
# *      Karim KHALFALLAH <karim.khalfallah@ssi.gouv.fr>
# *
# *  This software is licensed under a dual BSD and GPL v2 license.
# *  See LICENSE file at the root folder of the project.
# */
#!/bin/sh

CURVES=`openssl ecparam -list_curves | grep prime | cut -d':' -f1 | tr '\n' ' '`

# Find a suitable python command if none has been provided
if [ -z "$PYTHON" ]
then
        echo "Looking for suitable python and deps"
        # Try to guess which python we want to use depending on the installed
        # packages. We need Pyscard, Crypto, and IntelHex
        for i in python python3 python2; do
                if [ -x "`which $i`" ]; then
                        echo "Found and using python=$i"
                        PYTHON=$i
                        break
                fi
        done
else
        echo "Using user provided python=$PYTHON"
fi

if [ -z "$PYTHON" ]; then
        echo "Failed to find working python cmd!" >&2
        exit
fi

# Get the expand_libecc python script path
BASEDIR=$(dirname "$0")
EXPAND_LIBECC=$BASEDIR/expand_libecc.py

for curve in $CURVES
do
	echo "Adding $curve"
	openssl ecparam -param_enc explicit -outform DER -name $curve -out "$curve".der
	$PYTHON $EXPAND_LIBECC --name="$curve" --ECfile="$curve".der --add-test-vectors=2
	rm "$curve".der
done
