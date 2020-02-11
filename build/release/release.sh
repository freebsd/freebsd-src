#!/bin/sh
ID=$(docker build -q -f build/release/Dockerfile .)
if [ -z "$ID" ]; then
	echo "Failed to build docker image"
	exit 1
else
	docker run $ID sh -c "tar -c -f - libarchive-*" | tar -x -f -
fi
