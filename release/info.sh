#!/bin/sh
ls $1.* | wc | awk '{ print "Pieces = ",$1 }'
