#!/bin/sh

gitver=$(git describe 2>/dev/null | tr - .)
filever=$(cat VERSION)

version=${gitver}
: ${version:=$filever}

echo "$version"
