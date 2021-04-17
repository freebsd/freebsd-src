#!/bin/sh

error()
{
    echo "Build test failed"
    exit 1
}

for i in *; do
    if [ -d $i ]; then
	cd $i
	make clean
	make -j8 || error
	make clean
	cd ..
    fi
done

echo "Build test succeeded"
