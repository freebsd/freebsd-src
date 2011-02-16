# $FreeBSD: src/tools/tools/mtxstat/mtxratio.sh,v 1.1.34.1 2010/12/21 17:10:29 kensmith Exp $
sysctl debug.mutex.prof.stats | awk '$1 ~ /[0-9]+/ { if ($3 != 0) { hld_prc = $5 / $3 * 100; lck_prc = $6 / $3 * 100 } else { hld_prc = 0; lck_prc = 0 } print $1 " " $2 " " $3 " " $4 " " $5 " " hld_prc " " $6 " " lck_prc " " substr($0, index($0, $7)); next } { print }'
