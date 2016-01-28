#!/bin/bash

case $1 in
    v*-dts) ;;
    '')
	echo >&2 "No version given"
	exit 1
	;;
    *)
	echo >&2 "Unexpected version: $1"
	exit 1
	;;
esac

v=$1

set -ex

git merge --no-edit "${v}-raw"
git clean -fdqx
make -k -j12 -s
git tag -s -m "Tagging ${v}" -u 695A46C6 "${v}"
