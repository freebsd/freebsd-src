#!/bin/sh
# search /etc/skeykeys for the skey string for this user OR user specified
# in 1st parameter

PATH=/bin:/usr/bin

test -f /etc/skeykeys && {
	WHO=${1-`id | sed 's/^[^(]*(\([^)]*\).*/\1/'`}
	awk '/^'${WHO}'[ 	]/ { print $2-1, $3 }' /etc/skeykeys
}
