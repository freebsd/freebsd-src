@ECHO OFF
SET ZLIB_VERSION=1.2.11
IF NOT "%BE%"=="cygwin-gcc"  (
  IF NOT "%BE%"=="mingw-gcc" (
    IF NOT "%BE%"=="msvc" (
      ECHO Environment variable BE must be cygwin-gcc, mingw-gcc or msvc
      EXIT /b 1
    )
  )
)

IF "%1%"=="prepare" (
  IF "%BE%"=="cygwin-gcc" (
    @ECHO ON
    choco install -y --no-progress cygwin || EXIT /b 1
    C:\tools\cygwin\cygwinsetup.exe -q -P make,autoconf,automake,cmake,gcc-core,binutils,libtool,pkg-config,bison,sharutils,zlib-devel,libbz2-devel,liblzma-devel,liblz4-devel,libiconv-devel,libxml2-devel,libzstd-devel,libssl-devel || EXIT /b 1
    @EXIT /b 0
  ) ELSE IF "%BE%"=="mingw-gcc" (
    @ECHO ON
    choco install -y --no-progress mingw || EXIT /b 1
    choco install -y --no-progress --installargs 'ADD_CMAKE_TO_PATH=System' cmake || EXIT /b 1
    @EXIT /b 0
  ) ELSE IF "%BE%"=="msvc" (
    @ECHO ON
    choco install -y --no-progress visualstudio2017community || EXIT /b 1
    choco install -y --no-progress visualstudio2017-workload-vctools || EXIT /b 1
    choco install -y --no-progress --installargs 'ADD_CMAKE_TO_PATH=System' cmake || EXIT /b 1
  )
) ELSE IF "%1"=="deplibs" (
  IF "%BE%"=="cygwin-gcc" (
    ECHO Skipping on this platform
    EXIT /b 0
  )
  IF NOT EXIST build_ci\libs (
    MKDIR build_ci\libs
  )
  CD build_ci\libs
  IF NOT EXIST zlib-%ZLIB_VERSION%.tar.gz (
    curl -o zlib-%ZLIB_VERSION%.tar.gz https://www.zlib.net/zlib-%ZLIB_VERSION%.tar.gz
  )
  IF NOT EXIST zlib-%ZLIB_VERSION% (
    tar -x -z -f zlib-%ZLIB_VERSION%.tar.gz
  )
  SET PATH=%PATH%;C:\Program Files\cmake\bin;C:\ProgramData\chocolatey\lib\mingw\tools\install\mingw64\bin
  CD zlib-%ZLIB_VERSION%
  IF "%BE%"=="mingw-gcc" (
    cmake -G "MinGW Makefiles" -D CMAKE_BUILD_TYPE="Release" . || EXIT /b 1
    mingw32-make || EXIT /b 1
    mingw32-make test || EXIT /b 1
    mingw32-make install || EXIT /b 1
  ) ELSE IF "%BE%"=="msvc" (
    cmake -G "Visual Studio 15 2017" . || EXIT /b 1
    cmake --build . --target ALL_BUILD --config Release || EXIT /b 1
    cmake --build . --target RUN_TESTS --config Release || EXIT /b 1
    cmake --build . --target INSTALL --config Release || EXIT /b 1
  )
) ELSE IF "%1%"=="configure" (
  IF "%BE%"=="cygwin-gcc" (
    SET BS=cmake
    SET CONFIGURE_ARGS=-DENABLE_ACL=OFF
    C:\tools\cygwin\bin\bash.exe --login -c "cd '%cd%'; ./build/ci/build.sh -a configure" || EXIT /b 1
  ) ELSE IF "%BE%"=="mingw-gcc" (
    SET PATH=%PATH%;C:\Program Files\cmake\bin;C:\ProgramData\chocolatey\lib\mingw\tools\install\mingw64\bin
    MKDIR build_ci\cmake
    CD build_ci\cmake
    cmake -G "MinGW Makefiles" ..\.. || EXIT /b 1
  ) ELSE IF "%BE%"=="msvc" (
    SET PATH=%PATH%;C:\Program Files\cmake\bin;C:\ProgramData\chocolatey\lib\mingw\tools\install\mingw64\bin
    MKDIR build_ci\cmake
    CD build_ci\cmake
    cmake -G "Visual Studio 15 2017" -D CMAKE_BUILD_TYPE="Release" ..\.. || EXIT /b 1
  )
) ELSE IF "%1%"=="build" (
  IF "%BE%"=="cygwin-gcc" (
    SET BS=cmake
    C:\tools\cygwin\bin\bash.exe --login -c "cd '%cd%'; ./build/ci/build.sh -a build"
  ) ELSE IF "%BE%"=="mingw-gcc" (
    SET PATH=%PATH%;C:\Program Files\cmake\bin;C:\ProgramData\chocolatey\lib\mingw\tools\install\mingw64\bin
    CD build_ci\cmake
    mingw32-make || EXIT /b 1
  ) ELSE IF "%BE%"=="msvc" (
    SET PATH=%PATH%;C:\Program Files\cmake\bin;C:\ProgramData\chocolatey\lib\mingw\tools\install\mingw64\bin
    CD build_ci\cmake
    cmake --build . --target ALL_BUILD --config Release
  )
) ELSE IF "%1%"=="test" (
  IF "%BE%"=="cygwin-gcc" (
    ECHO "Skipping tests on this platform"
    EXIT /b 0
    REM SET BS=cmake
    REM SET SKIP_TEST_SPARSE=1
    REM C:\tools\cygwin\bin\bash.exe --login -c "cd '%cd%'; ./build/ci/build.sh -a test"
  ) ELSE IF "%BE%"=="mingw-gcc" (
    SET PATH=%PATH%;C:\Program Files\cmake\bin;C:\ProgramData\chocolatey\lib\mingw\tools\install\mingw64\bin
    COPY "C:\Program Files (x86)\zlib\bin\libzlib.dll" build_ci\cmake\bin\
    CD build_ci\cmake
    SET SKIP_TEST_SPARSE=1
    mingw32-make test
  ) ELSE IF "%BE%"=="msvc" (
    SET PATH=%PATH%;C:\Program Files\cmake\bin;C:\ProgramData\chocolatey\lib\mingw\tools\install\mingw64\bin
    ECHO "Skipping tests on this platform"
    EXIT /b 0
    REM CD build_ci\cmake
    REM cmake --build . --target RUN_TESTS --config Release
  )
) ELSE IF "%1%"=="install" (
  IF "%BE%"=="cygwin-gcc" (
    SET BS=cmake
    C:\tools\cygwin\bin\bash.exe --login -c "cd '%cd%'; ./build/ci/build.sh -a install"
  ) ELSE IF "%BE%"=="mingw-gcc" (
    SET PATH=%PATH%;C:\Program Files\cmake\bin;C:\ProgramData\chocolatey\lib\mingw\tools\install\mingw64\bin
    CD build_ci\cmake
    mingw32-make install DESTDIR=%cd%\destdir
  ) ELSE IF "%BE%"=="msvc" (
    SET PATH=%PATH%;C:\Program Files\cmake\bin;C:\ProgramData\chocolatey\lib\mingw\tools\install\mingw64\bin
    cmake --build . --target INSTALL --config Release
  )
) ELSE (
  ECHO "Usage: %0% prepare|deplibs|configure|build|test|install"
  @EXIT /b 0
)
@EXIT /b 0
