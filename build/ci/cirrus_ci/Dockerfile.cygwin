FROM cirrusci/windowsservercore:2019

RUN choco install -y --no-progress cygwin
RUN C:\tools\cygwin\cygwinsetup.exe -q -P make,autoconf,automake,cmake,gcc-core,binutils,libtool,pkg-config,bison,sharutils,zlib-devel,libbz2-devel,liblzma-devel,liblz4-devel,libiconv-devel,libxml2-devel,libzstd-devel,libssl-devel
