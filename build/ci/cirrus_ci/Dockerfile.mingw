FROM cirrusci/windowsservercore:2019

RUN choco install -y --no-progress --installargs 'ADD_CMAKE_TO_PATH=User' cmake
RUN choco install -y --no-progress mingw
RUN curl -o zlib-1.2.11.tar.gz https://www.zlib.net/zlib-1.2.11.tar.gz
RUN tar -x -f zlib-1.2.11.tar.gz
RUN cd zlib-1.2.11 && cmake -G "MinGW Makefiles" -D CMAKE_BUILD_TYPE="Release" . && mingw32-make && mingw32-make install
RUN del /f /q /s zlib-1.2.11 zlib-1.2.11.tar.gz
