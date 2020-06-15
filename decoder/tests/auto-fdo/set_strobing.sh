#!/bin/bash

WINDOW=$1
PERIOD=$2

if [[ -z $WINDOW ]] || [[ -z $PERIOD ]]; then
       echo "Window or Period not specified!"
       echo "Example usage: ./set_strobing.sh <WINDOW VALUE> <PERIOD VALUE>"
       echo "Example usage: ./set_strobing.sh 5000 10000"
       exit -1
fi


if [[ $EUID != 0 ]]; then
    echo "Please run as root"
    exit -1
fi

for e in /sys/bus/coresight/devices/etm*/; do
       printf "%x" $WINDOW | tee $e/strobe_window > /dev/null
       printf "%x" $PERIOD | tee $e/strobe_period > /dev/null
       echo "Strobing period for $e set to $((`cat $e/strobe_period`))"
       echo "Strobing window for $e set to $((`cat $e/strobe_window`))"
done

## Shows the user a simple usage example
echo ">> Done! <<"
echo "You can now run perf to trace your application, for example:"
echo "perf record -e cs_etm/@tmc_etr0/u -- <your app>"
