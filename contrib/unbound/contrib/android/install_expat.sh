#!/usr/bin/env bash

echo "Downloading Expat"
if ! curl -L -k -s -o expat-2.2.9.tar.gz https://github.com/libexpat/libexpat/releases/download/R_2_2_9/expat-2.2.9.tar.gz;
then
    echo "Failed to download Expat"
    exit 1
fi

echo "Unpacking Expat"
rm -rf ./expat-2.2.9
if ! tar -xf expat-2.2.9.tar.gz;
then
    echo "Failed to unpack Expat"
    exit 1
fi

cd expat-2.2.9 || exit 1

echo "Configuring Expat"
if ! ./configure --build="$AUTOTOOLS_BUILD" --host="$AUTOTOOLS_HOST" --prefix="$ANDROID_PREFIX"; then
    echo "Error: Failed to configure Expat"
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
