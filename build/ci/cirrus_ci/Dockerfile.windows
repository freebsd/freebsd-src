FROM cirrusci/windowsservercore:2019

RUN choco install -y --no-progress mingw
RUN choco install -y --no-progress --installargs 'ADD_CMAKE_TO_PATH=User' cmake
RUN choco install -y --no-progress visualstudio2017community
RUN choco install -y --no-progress visualstudio2017-workload-vctools
RUN curl -o zlib-1.2.11.tar.gz https://www.zlib.net/zlib-1.2.11.tar.gz
RUN tar -x -f zlib-1.2.11.tar.gz
RUN cd zlib-1.2.11 && cmake -G "Visual Studio 15 2017" . && cmake --build . --target ALL_BUILD --config Release && cmake --build . --target INSTALL --config Release
RUN del /f /q /s zlib-1.2.11 zlib-1.2.11.tar.gz
RUN choco install -y --no-progress cygwin
RUN C:\tools\cygwin\cygwinsetup.exe -q -P make,autoconf,automake,cmake,gcc-core,binutils,libtool,pkg-config,bison,sharutils,zlib-devel,libbz2-devel,liblzma-devel,liblz4-devel,libiconv-devel,libxml2-devel,libzstd-devel,libssl-devel
