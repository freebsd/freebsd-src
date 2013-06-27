#!/bin/sh

if [ -d /boot/kernel.GENERIC ]
then
        rmdir /boot/kernel
        mv /boot/kernel.GENERIC /boot/kernel
fi

exit 0
