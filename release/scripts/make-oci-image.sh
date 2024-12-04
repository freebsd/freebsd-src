#! /bin/sh

# Build an Open Container Initiative (OCI) container image

curdir=$1; shift
rev=$1; shift
branch=$1; shift
arch=$1; shift
image=$1; shift

major=${rev%.*}
minor=${rev#*.}

abi=FreeBSD:${major}:${arch}

echo "Building OCI freebsd${major}-${image} image for ${abi}"

. ${curdir}/tools/oci-image-${image}.conf

init_workdir() {
	local abi=$1; shift
	local workdir=$(mktemp -d -t oci-images)

	mkdir ${workdir}/repos
	cat > ${workdir}/repos/base.conf <<EOF
FreeBSD-base: {
  url: "file:///usr/obj/usr/src/repo/${abi}/latest"
  signature_type: "none"
  fingerprints: "none"
}
EOF
	cp /etc/pkg/FreeBSD.conf ${workdir}/repos
	echo ${workdir}
}

install_packages() {
	local abi=$1; shift
	local workdir=$1; shift
	local rootdir=$1; shift
	if [ ! -d ${rootdir}/usr/share/keys/pkg/trusted ]; then
		mkdir -p ${rootdir}/usr/share/keys/pkg/trusted
	fi
	cp /usr/share/keys/pkg/trusted/* ${rootdir}/usr/share/keys/pkg/trusted
	# We install the packages and then remove repository metadata (keeping the
	# metadata for what was installed). This trims more than 40Mb from the
	# resulting image.
	env IGNORE_OSVERSION=yes ABI=${abi} pkg --rootdir ${rootdir} --repo-conf-dir ${workdir}/repos \
		install -yq "$@" || exit $?
	rm -rf ${rootdir}/var/db/pkg/repos
}

workdir=$(init_workdir ${abi})
if [ -n "${OCI_BASE_IMAGE}" ]; then
	base_image=freebsd${major}-${OCI_BASE_IMAGE}
else
	base_image=scratch
fi

c=$(buildah from --arch ${arch} ${base_image})
m=$(buildah mount $c)
oci_image_build
buildah unmount $c
buildah commit --rm $c freebsd${major}-${image}:latest
