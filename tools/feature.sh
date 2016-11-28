#!/bin/sh
# This will generate the list of feature flags for implemented symbols.

echo '/* DO NOT EDIT. This is auto-generated from feature.sh */'
nm ../regressions/ck_pr/validate/ck_pr_cas|cut -d ' ' -f 3|sed s/ck_pr/ck_f_pr/|awk '/^ck_f_pr/ {print "#define " toupper($1);}'|sort
