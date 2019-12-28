@ECHO OFF
SET ZLIB_VERSION=1.2.11
IF NOT "%BE%"=="mingw-gcc" (
  IF NOT "%BE%"=="msvc" (
    ECHO Environment variable BE must be mingw-gcc or msvc
    EXIT /b 1
  )
)

IF "%1"=="deplibs" (
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
  CD zlib-%ZLIB_VERSION%
  IF "%BE%"=="mingw-gcc" (
    SET PATH=C:\Program Files\cmake\bin;C:\ProgramData\chocolatey\lib\mingw\tools\install\mingw64\bin
    cmake -G "MinGW Makefiles" -D CMAKE_BUILD_TYPE="Release" . || EXIT /b 1
    mingw32-make || EXIT /b 1
    mingw32-make test || EXIT /b 1
    mingw32-make install || EXIT /b 1
  ) ELSE IF "%BE%"=="msvc" (
    cmake -G "Visual Studio 16 2019" . || EXIT /b 1
    cmake --build . --target ALL_BUILD --config Release || EXIT /b 1
    cmake --build . --target RUN_TESTS --config Release || EXIT /b 1
    cmake --build . --target INSTALL --config Release || EXIT /b 1
  )
) ELSE IF "%1%"=="configure" (
  IF "%BE%"=="mingw-gcc" (
    SET PATH=C:\Program Files\cmake\bin;C:\ProgramData\chocolatey\lib\mingw\tools\install\mingw64\bin
    MKDIR build_ci\cmake
    CD build_ci\cmake
    cmake -G "MinGW Makefiles" ..\.. || EXIT /b 1
  ) ELSE IF "%BE%"=="msvc" (
    MKDIR build_ci\cmake
    CD build_ci\cmake
    cmake -G "Visual Studio 16 2019" -D CMAKE_BUILD_TYPE="Release" ..\.. || EXIT /b 1
  )
) ELSE IF "%1%"=="build" (
  IF "%BE%"=="mingw-gcc" (
    SET PATH=C:\Program Files\cmake\bin;C:\ProgramData\chocolatey\lib\mingw\tools\install\mingw64\bin
    CD build_ci\cmake
    mingw32-make || EXIT /b 1
  ) ELSE IF "%BE%"=="msvc" (
    CD build_ci\cmake
    cmake --build . --target ALL_BUILD --config Release
  )
) ELSE IF "%1%"=="test" (
  IF "%BE%"=="mingw-gcc" (
    SET PATH=C:\Program Files\cmake\bin;C:\ProgramData\chocolatey\lib\mingw\tools\install\mingw64\bin
    COPY "C:\Program Files (x86)\zlib\bin\libzlib.dll" build_ci\cmake\bin\
    CD build_ci\cmake
    SET SKIP_TEST_SPARSE=1
    mingw32-make test
  ) ELSE IF "%BE%"=="msvc" (
    ECHO "Skipping tests on this platform"
    EXIT /b 0
    REM CD build_ci\cmake
    REM cmake --build . --target RUN_TESTS --config Release
  )
) ELSE IF "%1%"=="install" (
  IF "%BE%"=="mingw-gcc" (
    SET PATH=C:\Program Files\cmake\bin;C:\ProgramData\chocolatey\lib\mingw\tools\install\mingw64\bin
    CD build_ci\cmake
    mingw32-make install DESTDIR=%cd%\destdir
  ) ELSE IF "%BE%"=="msvc" (
    cmake --build . --target INSTALL --config Release
  )
) ELSE (
  ECHO "Usage: %0% deplibs|configure|build|test|install"
  @EXIT /b 0
)
@EXIT /b 0
