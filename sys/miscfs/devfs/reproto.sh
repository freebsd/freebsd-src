#!/bin/sh
echo "/* THIS FILE PRODUCED AUTOMATICALLY */" >devfs_proto.h
grep -h '/\*proto\*/' *.c |awk '{print $0 ";"}' >>devfs_proto.h
echo "/* THIS FILE PRODUCED AUTOMATICALLY */" >>devfs_proto.h
echo "/* DO NOT EDIT (see reproto.sh) */" >>devfs_proto.h

