#!/bin/sh
################################################################################
#
# lockgraphs.sh by Michele Dallachiesa -- 2008-05-07 -- v0.1
#
# $FreeBSD: src/share/examples/witness/lockgraphs.sh,v 1.1.2.1.6.1 2010/12/21 17:09:25 kensmith Exp $
#
################################################################################

sysctl debug.witness.graphs | awk '
BEGIN {
  print "digraph lockgraphs {"
  }

NR > 1 && $0 ~ /"Giant"/ {
  gsub(","," -> ");
  print $0 ";"
}

END { 
  print "}"
  }'

#eof
