#!/bin/sh

old_unit=$1
new_unit=$2

if [ $old_unit != -1 ]; then
	ifconfig sl$old_unit delete down
	if [ $new_unit == -1 ]; then
		route delete default
	fi
fi

if [ $new_unit != -1 ]; then
	ifconfig sl$new_unit <address1> <address2>
	if [ $old_unit == -1 ]; then
		route add default <address2>
	fi
fi
