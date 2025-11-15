#! /bin/sh

# Build an Open Container Initiative (OCI) container image

curdir=$1; shift
rev=$1; shift
branch=$1; shift
arch=$1; shift
image=$1; shift
output=$1; shift

major=${rev%.*}
minor=${rev#*.}

abi=FreeBSD:${major}:${arch}
ver=${rev}-${branch}-${arch}

echo "Building OCI freebsd${major}-${image} image for ${abi}"

. ${curdir}/tools/oci-image-${image}.conf

init_repo() {
	local workdir=$1; shift
	local abi=$1; shift
	local srcdir=$(realpath ${curdir}/..)

	mkdir -p ${workdir}/repos
	cat > ${workdir}/repos/base.conf <<EOF
FreeBSD-base: {
  url: "file:///usr/obj${srcdir}/repo/${abi}/latest"
  signature_type: "none"
  fingerprints: "none"
}
EOF
}

# Install packages using pkg(8) into a container with rootfs at $3
install_packages() {
	local abi=$1; shift
	local workdir=$1; shift
	local rootdir=${workdir}/rootfs

	# Make sure we have the keys needed for verifying package integrity if
	# not already added by a parent image.
	if [ ! -d ${rootdir}/usr/share/keys/pkg/trusted ]; then
		mkdir -p ${rootdir}/usr/share/keys/pkg/trusted
	fi
	for i in ${curdir}/../share/keys/pkg/trusted/pkg.*; do
		if [ ! -f ${rootdir}/usr/share/keys/pkg/trusted/$(basename $i) ]; then
			cp $i ${rootdir}/usr/share/keys/pkg/trusted
		fi
	done

	# We install the packages and then remove repository metadata (keeping the
	# metadata for what was installed). This trims more than 40Mb from the
	# resulting image.
	env IGNORE_OSVERSION=yes ABI=${abi} pkg --rootdir ${rootdir} --repo-conf-dir ${workdir}/repos \
		install -yq -g "$@" || exit $?
	rm -rf ${rootdir}/var/db/pkg/repos
}

set_cmd() {
	local workdir=$1; shift
	oci_cmd="$@"
}

# Convert FreeBSD architecture to OCI-style. See
# https://github.com/containerd/platforms/blob/main/platforms.go for details
normalize_arch() {
	local arch=$1; shift
	case ${arch} in
		i386)
		       arch=386
		       ;;
		aarch64)
		       arch=arm64
		       ;;
		amd64) ;;
		riscv64) ;;
		*)
			echo "Architecture ${arch} not supported for container images"
			;;
	esac
	echo ${arch}
}

create_container() {
	local workdir=$1; shift
	local base_workdir=$1; shift
	oci_cmd=
	if [ -d ${workdir}/rootfs ]; then
		chflags -R 0 ${workdir}/rootfs
		rm -rf ${workdir}/rootfs
	fi
	mkdir -p ${workdir}/rootfs
	if [ "${base_workdir}" != "" ]; then
		tar -C ${workdir}/rootfs -xf ${base_workdir}/rootfs.tar.gz
	fi
}

commit_container() {
	local workdir=$1; shift
	local image=$1; shift
	local output=$1; shift

	# Note: the diff_id (needed for image config) is the hash of the
	# uncompressed tar.
	#
	# For compatibility with Podman, we must disable sparse-file
	# handling. See https://github.com/containers/podman/issues/25270 for
	# more details.
	tar -C ${workdir}/rootfs --strip-components 1 --no-read-sparse -cf ${workdir}/rootfs.tar .
	local diff_id=$(sha256 -q < ${workdir}/rootfs.tar)
	gzip -f ${workdir}/rootfs.tar
	local create_time=$(date -u +%Y-%m-%dT%TZ)
	local root_hash=$(sha256 -q < ${workdir}/rootfs.tar.gz)
	local root_size=$(stat -f %z ${workdir}/rootfs.tar.gz)

	oci_arch=$(normalize_arch ${arch})

	config=
	if [ -n "${oci_cmd}" ]; then
		config=",\"config\":{\"cmd\":[\"${oci_cmd}\"]}"
	fi
	echo "{\"created\":\"${create_time}\",\"architecture\":\"${oci_arch}\",\"os\":\"freebsd\"${config},\"rootfs\":{\"type\":\"layers\",\"diff_ids\":[\"sha256:${diff_id}\"]},\"history\":[{\"created\":\"${create_time}\",\"created_by\":\"make-oci-image.sh\"}]}" > ${workdir}/config.json
	local config_hash=$(sha256 -q < ${workdir}/config.json)
	local config_size=$(stat -f %z ${workdir}/config.json)

	echo "{\"schemaVersion\":2,\"mediaType\":\"application/vnd.oci.image.manifest.v1+json\",\"config\":{\"mediaType\":\"application/vnd.oci.image.config.v1+json\",\"digest\":\"sha256:${config_hash}\",\"size\":${config_size}},\"layers\":[{\"mediaType\":\"application/vnd.oci.image.layer.v1.tar+gzip\",\"digest\":\"sha256:${root_hash}\",\"size\":${root_size}}],\"annotations\":{}}" > ${workdir}/manifest.json
	local manifest_hash=$(sha256 -q < ${workdir}/manifest.json)
	local manifest_size=$(stat -f %z ${workdir}/manifest.json)

	mkdir -p ${workdir}/oci/blobs/sha256
	echo "{\"imageLayoutVersion\": \"1.0.0\"}" > ${workdir}/oci/oci-layout
	echo "{\"schemaVersion\":2,\"manifests\":[{\"mediaType\":\"application/vnd.oci.image.manifest.v1+json\",\"digest\":\"sha256:${manifest_hash}\",\"size\":${manifest_size},\"annotations\":{\"org.opencontainers.image.ref.name\":\"freebsd-${image}:${ver}\"}}]}" > ${workdir}/oci/index.json
	ln ${workdir}/rootfs.tar.gz ${workdir}/oci/blobs/sha256/${root_hash}
	ln ${workdir}/config.json ${workdir}/oci/blobs/sha256/${config_hash}
	ln ${workdir}/manifest.json ${workdir}/oci/blobs/sha256/${manifest_hash}

	tar -C ${workdir}/oci --xz --strip-components 1 --no-read-sparse -a -cf ${output} .
}

# Prefix with "container-image-" so that we can create a unique work area under
# ${.OBJDIR}. We can assume that make has set our working directory to
# ${.OBJDIR}.
workdir=${PWD}/container-image-${image}
init_repo ${workdir} ${abi}

if [ -n "${OCI_BASE_IMAGE}" ]; then
	base_workdir=${PWD}/container-image-${OCI_BASE_IMAGE}
else
	base_workdir=
fi

create_container ${workdir} ${base_workdir}
oci_image_build
commit_container ${workdir} ${image} ${output}
