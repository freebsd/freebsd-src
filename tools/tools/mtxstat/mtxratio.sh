# $FreeBSD: src/tools/tools/mtxstat/mtxratio.sh,v 1.1.28.1 2009/04/15 03:14:26 kensmith Exp $
sysctl debug.mutex.prof.stats | awk '$1 ~ /[0-9]+/ { if ($3 != 0) { hld_prc = $5 / $3 * 100; lck_prc = $6 / $3 * 100 } else { hld_prc = 0; lck_prc = 0 } print $1 " " $2 " " $3 " " $4 " " $5 " " hld_prc " " $6 " " lck_prc " " substr($0, index($0, $7)); next } { print }'
