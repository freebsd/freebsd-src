#!/bin/sh

if [ ! -d /boot/kernel.GENERIC ]
then
        mv /boot/kernel /boot/kernel.GENERIC
        echo "Moved currently installed kernel to /boot/kernel.GENERIC"
fi

exit 0
