#!/bin/sh

if ! sysctl -n kern.features.compat_freebsd_32bit >/dev/null 2>&1; then
	echo "1..0 # Skipped: Kernel not built with COMPAT_FREEBSD32"
	exit 0
elif ! sysctl -n kern.supported_archs | grep -q '\<armv7\>'; then
	echo "1..0 # Skipped: 32-bit ARM not supported on this hardware"
	exit 0
fi
