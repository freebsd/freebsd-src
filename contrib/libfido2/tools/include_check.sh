#!/bin/sh -u

# Copyright (c) 2019 Yubico AB. All rights reserved.
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file.
# SPDX-License-Identifier: BSD-2-Clause

SKIP='(webauthn.h)'

check() {
	try="cc $CFLAGS -Isrc -xc -c - -o /dev/null 2>&1"
	git ls-files "$1" | grep '.*\.h$' | while read -r header; do
		if echo "$header" | grep -Eq "$SKIP"; then
			echo "Skipping $header"
		else
			body="#include \"$header\""
			echo "echo $body | $try"
			echo "$body" | eval "$try"
		fi
	done
}

check examples
check fuzz
check openbsd-compat
CFLAGS="${CFLAGS} -D_FIDO_INTERNAL" check src
check src/fido.h
check src/fido
check tools
