#! /bin/sh -
exec groff -Tps -l -C ${1+"$@"}
