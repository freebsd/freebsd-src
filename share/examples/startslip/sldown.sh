#!/bin/sh
/sbin/ifconfig $1 $2
/sbin/route delete default
