#!/bin/sh -eux

# Copyright (c) 2022 Yubico AB. All rights reserved.
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file.
# SPDX-License-Identifier: BSD-2-Clause

BASE_URL="https://builds.sr.ht"
MANIFEST="$(mktemp)"
LOGFILE="$(mktemp)"
trap '[ -f "${LOGFILE}" ] && cat -- "${LOGFILE}"' EXIT

# construct the sourcehut build manifest
cat > "${MANIFEST}" <<- EOF
image: ${IMAGE}
packages:
  - cmake
  - llvm
  - pcsc-lite
EOF

case "${IMAGE}" in
	freebsd*)
cat >> "${MANIFEST}" <<- EOF
  - libcbor
  - pkgconf
EOF
	;;
esac

cat >> "${MANIFEST}" <<- EOF
sources:
  - ${GITHUB_SERVER_URL}/${GITHUB_REPOSITORY}#$(git rev-parse HEAD)
tasks:
  - build: |
      if [ "\$(uname)" = "OpenBSD" ]; then
        SUDO="doas -u root"
      else
        SUDO=sudo
      fi
      SCAN="/usr/local/bin/scan-build --use-cc=/usr/bin/cc --status-bugs"
      cd libfido2
      for T in Debug Release; do
        mkdir build-\$T
        (cd build-\$T && \${SCAN} cmake -DCMAKE_BUILD_TYPE=\$T ..)
        \${SCAN} make -j"\$(sysctl -n hw.ncpu)" -C build-\$T
        make -C build-\$T regress
        \${SUDO} make -C build-\$T install
      done
EOF

q() {
	curl \
		--silent \
		--oauth2-bearer "${SOURCEHUT_TOKEN}" \
		--header "Content-Type: application/json" \
		--data @- -- \
		"${BASE_URL}/query" \
	| tee -a -- "${LOGFILE}"
}

submit_job() {
	local manifest="$1"
	jq \
		--compact-output --null-input \
		'{ query: $body, variables: { var: $var } }' \
		--arg body 'mutation($var: String!) { submit(manifest: $var) { id } }' \
		--rawfile var "${manifest}" \
	| q \
	| jq --exit-status --raw-output '.data.submit.id'
}

job_status() {
	local id="$1"
	jq \
		--compact-output --null-input \
		'{ query: $body, variables: { var: $var } }' \
		--arg body 'query($var: Int!) { job(id: $var) { status } }' \
		--argjson var "${id}" \
	| q \
	| jq --exit-status --raw-output '.data.job.status'
}

JOB_ID="$(submit_job "${MANIFEST}")" || exit 1
[ -z "${JOB_ID}" ] && exit 1
echo "Job '${JOB_ID}' running at ${BASE_URL}/~yubico-libfido2/job/${JOB_ID}"

while true; do
	JOB_STATUS="$(job_status "${JOB_ID}")" || exit 1
	case "${JOB_STATUS}" in
		SUCCESS) exit 0;;
		FAILED) exit 1;;
		PENDING|QUEUED|RUNNING) ;;
		*) exit 1;;
	esac
	sleep 60
done
