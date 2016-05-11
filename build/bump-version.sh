#!/bin/sh +v

# Start from the build directory, where the version file is located
if [ -f build/version ]; then
    cd build
fi

if [ \! -f version ]; then
    echo "Can't find version file"
    exit 1
fi

# Update the build number in the 'version' file.
# Separate number from additional alpha/beta/etc marker
MARKER=`cat version | sed 's/[0-9.]//g'`
# Bump the number
VN=`cat version | sed 's/[^0-9.]//g'`
# Reassemble and write back out
VN=$(($VN + 1))
rm -f version.old
mv version version.old
chmod +w version.old
echo $VN$MARKER > version
VS="$(($VN/1000000)).$(( ($VN/1000)%1000 )).$(( $VN%1000 ))$MARKER"
cd ..

ANNOUNCE=`date +"%b %d, %Y:"`" libarchive $VS released"

echo $ANNOUNCE

# Add a version notice to NEWS
mv NEWS NEWS.bak
chmod +w NEWS.bak
echo $ANNOUNCE >> NEWS
echo >> NEWS
cat NEWS.bak >> NEWS
