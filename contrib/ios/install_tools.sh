#!/usr/bin/env bash

# This step should install tools needed for all packages - OpenSSL, Expat and Unbound
# brew update hangs, so we try to skip that step.
#echo "Updating tools"
#brew update 1>/dev/null
echo "Installing tools"
# already installed are: autoconf automake libtool pkg-config
#brew install curl perl 1>/dev/null
HOMEBREW_NO_AUTO_UPDATE=1 brew install curl perl 1>/dev/null
