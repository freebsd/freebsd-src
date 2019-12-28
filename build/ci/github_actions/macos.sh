#!/bin/sh
if [ "$1" = "prepare" ]
then
	set -x -e
	brew update > /dev/null
	for pkg in autoconf automake libtool pkg-config cmake xz lz4 zstd
	do
		brew list $pkg > /dev/null && brew upgrade $pkg || brew install $pkg
	done
fi
