#!/bin/sh
grep -h '/\*proto\*/' *.c |awk '{print $0 ";"}' >devfs_proto.h
