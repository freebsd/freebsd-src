#!/bin/bash

for e in /sys/bus/coresight/devices/etm*/; do
       echo "Strobing period for $e is $((`cat $e/strobe_period`))"
       echo "Strobing window for $e is $((`cat $e/strobe_window`))"
done
