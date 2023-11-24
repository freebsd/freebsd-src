#!/usr/bin/env bash

# This step should install tools needed for all packages - OpenSSL, Expat and Unbound
echo "Updating tools"
sudo apt-get -qq update
sudo apt-get -qq install --no-install-recommends curl tar zip unzip perl openjdk-8-jdk autoconf automake libtool pkg-config
