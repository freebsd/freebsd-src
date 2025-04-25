#!/usr/bin/env bash

LIBEXPAT_FNAME=expat-2.7.0
LIBEXPAT_VERSION_DIR=R_2_7_0

echo "Downloading Expat"
if ! curl -L -k -s -o $LIBEXPAT_FNAME.tar.gz https://github.com/libexpat/libexpat/releases/download/$LIBEXPAT_VERSION_DIR/$LIBEXPAT_FNAME.tar.gz;
then
    echo "Failed to download Expat"
    exit 1
fi

echo "Unpacking Expat"
rm -rf ./$LIBEXPAT_FNAME
if ! tar -xf $LIBEXPAT_FNAME.tar.gz;
then
    echo "Failed to unpack Expat"
    exit 1
fi

cd $LIBEXPAT_FNAME || exit 1

export PKG_CONFIG_PATH="$IOS_PREFIX/lib/pkgconfig"

echo "Configuring Expat"
if ! ./configure --without-tests				\
       --build="$AUTOTOOLS_BUILD" --host="$AUTOTOOLS_HOST"	\
       --prefix="$IOS_PREFIX" ;
then
    echo "Error: Failed to configure Expat"
    cat config.log
    exit 1
fi

# Cleanup warnings, https://github.com/libexpat/libexpat/issues/383
echo "Fixing Makefiles"
(IFS="" find "$PWD" -name 'Makefile' -print | while read -r file
do
    cp -p "$file" "$file.fixed"
    sed 's|-Wduplicated-cond ||g; s|-Wduplicated-branches ||g; s|-Wlogical-op ||g' "$file" > "$file.fixed"
    mv "$file.fixed" "$file"

    cp -p "$file" "$file.fixed"
    sed 's|-Wrestrict ||g; s|-Wjump-misses-init ||g; s|-Wmisleading-indentation ||g' "$file" > "$file.fixed"
    mv "$file.fixed" "$file"
done)

echo "Building Expat"
if ! make; then
    echo "Failed to build Expat"
    exit 1
fi

echo "Installing Expat"
if ! make install; then
    echo "Failed to install Expat"
    exit 1
fi

exit 0
