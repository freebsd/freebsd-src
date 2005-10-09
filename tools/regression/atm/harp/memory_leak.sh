#!/bin/sh
#
# $FreeBSD$
#
# Perform memory leak test
#

while [ 1 ] ; do
  ./atm_udp.ng 1 127.0.0.1 5001 127.0.0.1 5002
  sleep 2
  ./atm_udp.ng flush
  vmstat -m
done
