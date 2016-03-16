#
# Copyright 2013, Juniper Networks, Inc.
# All rights reserved.
# This SOFTWARE is licensed under the LICENSE provided in the
# ../Copyright file. By downloading, installing, copying, or otherwise
# using the SOFTWARE, you agree to be bound by the terms of that
# LICENSE.


if [ ! -f configure ]; then
    vers=`autoreconf --version | head -1`
    echo "Using" $vers

    mkdir -p m4

    autoreconf --install

    if [ ! -f configure ]; then
	echo "Failed to create configure script"
	exit 1
    fi
fi

echo "Creating build directory ..."
mkdir build

echo "Setup is complete.  To build libslax:"

echo "    1) Type 'cd build ; ../configure' to configure libslax"
echo "    2) Type 'make' to build libslax"
echo "    3) Type 'make install' to install libslax"

exit 0
