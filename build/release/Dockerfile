FROM ubuntu:20.04
ENV DEBIAN_FRONTEND=noninteractive
RUN ln -sf /usr/share/zoneinfo/Europe/Berlin /etc/localtime
RUN apt-get update && apt-get install -y build-essential autoconf automake libtool pkg-config cmake zlib1g-dev libssl-dev libacl1-dev libbz2-dev liblzma-dev liblz4-dev libzstd-dev lzop groff ghostscript bsdmainutils zip
ADD . $HOME/libarchive/
ADD "http://git.savannah.gnu.org/gitweb/?p=config.git;a=blob_plain;f=config.guess;hb=HEAD" $HOME/libarchive/build/autoconf/config.guess
ADD "http://git.savannah.gnu.org/gitweb/?p=config.git;a=blob_plain;f=config.sub;hb=HEAD" $HOME/libarchive/build/autoconf/config.sub
WORKDIR $HOME/libarchive
RUN /bin/sh build/clean.sh
RUN /bin/sh build/autogen.sh
ENV SKIP_OPEN_FD_ERR_TEST=1 SKIP_TEST_SPARSE=1
RUN ./configure
RUN make V=1 distcheck
