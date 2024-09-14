#!/bin/sh
if [ "$1" = "prepare" ]
then
	set -x -e
	#Uncommenting these adds a full minute to the CI time
	#brew update > /dev/null
	#brew upgrade > /dev/null

	# This does an upgrade if the package is already installed
	brew install \
		autoconf \
		automake \
		libtool \
		pkg-config \
		cmake \
		xz \
		lz4 \
		zstd \
		libxml2 \
		openssl
fi
