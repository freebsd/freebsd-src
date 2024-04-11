@ECHO OFF
SET ZLIB_VERSION=1.3
SET BZIP2_VERSION=1ea1ac188ad4b9cb662e3f8314673c63df95a589
SET XZ_VERSION=5.4.4
IF NOT "%BE%"=="mingw-gcc" (
  IF NOT "%BE%"=="msvc" (
    ECHO Environment variable BE must be mingw-gcc or msvc
    EXIT /b 1
  )
)

REM v1.5.6 has a bug with the CMake files & MSVC
REM https://github.com/facebook/zstd/issues/3999
REM Fall back to 1.5.5 for MSVC until fixed
IF "%BE%"=="msvc" (
  SET ZSTD_VERSION=1.5.5
) ELSE (
  SET ZSTD_VERSION=1.5.6
)

SET ORIGPATH=%PATH%
IF "%BE%"=="mingw-gcc" (
  SET MINGWPATH=C:\WINDOWS\system32;C:\WINDOWS;C:\WINDOWS\System32\Wbem;C:\WINDOWS\System32\WindowsPowerShell\v1.0\;C:\Program Files\cmake\bin;C:\ProgramData\mingw64\mingw64\bin
)

IF "%1"=="deplibs" (
  IF NOT EXIST build_ci\libs (
    MKDIR build_ci\libs
  )
  CD build_ci\libs
  IF NOT EXIST zlib-%ZLIB_VERSION%.zip (
    ECHO Downloading https://github.com/libarchive/zlib/archive/v%ZLIB_VERSION%.zip
    curl -L -o zlib-%ZLIB_VERSION%.zip https://github.com/libarchive/zlib/archive/v%ZLIB_VERSION%.zip || EXIT /b 1
  )
  IF NOT EXIST zlib-%ZLIB_VERSION% (
    ECHO Unpacking zlib-%ZLIB_VERSION%.zip
    C:\windows\system32\tar.exe -x -f zlib-%ZLIB_VERSION%.zip || EXIT /b 1
  )
  IF NOT EXIST bzip2-%BZIP2_VERSION%.zip (
    echo Downloading https://github.com/libarchive/bzip2/archive/%BZIP2_VERSION%.zip
    curl -L -o bzip2-%BZIP2_VERSION%.zip https://github.com/libarchive/bzip2/archive/%BZIP2_VERSION%.zip || EXIT /b 1
  )
  IF NOT EXIST bzip2-%BZIP2_VERSION% (
    echo Unpacking bzip2-%BZIP2_VERSION%.zip
    C:\windows\system32\tar.exe -x -f bzip2-%BZIP2_VERSION%.zip || EXIT /b 1
  )
  IF NOT EXIST xz-%XZ_VERSION%.zip (
    echo Downloading https://github.com/libarchive/xz/archive/%XZ_VERSION%.zip
    curl -L -o xz-%XZ_VERSION%.zip https://github.com/libarchive/xz/archive/v%XZ_VERSION%.zip || EXIT /b 1
  )
  IF NOT EXIST xz-%XZ_VERSION% (
    echo Unpacking xz-%XZ_VERSION%.zip
    C:\windows\system32\tar.exe -x -f xz-%XZ_VERSION%.zip || EXIT /b 1
  )
  IF NOT EXIST zstd-%ZSTD_VERSION%.zip (
    echo Downloading https://github.com/facebook/zstd/archive/refs/tags/v%ZSTD_VERSION%.zip
    curl -L -o zstd-%ZSTD_VERSION%.zip https://github.com/facebook/zstd/archive/refs/tags/v%ZSTD_VERSION%.zip || EXIT /b 1
  )
  IF NOT EXIST zstd-%ZSTD_VERSION% (
    echo Unpacking zstd-%ZSTD_VERSION%.zip
    C:\windows\system32\tar.exe -x -f zstd-%ZSTD_VERSION%.zip || EXIT /b 1
  )
  CD zlib-%ZLIB_VERSION%
  IF "%BE%"=="mingw-gcc" (
    SET PATH=%MINGWPATH%
    cmake -G "MinGW Makefiles" -D CMAKE_BUILD_TYPE="Release" . || EXIT /b 1
    mingw32-make || EXIT /b 1
    mingw32-make test || EXIT /b 1
    mingw32-make install || EXIT /b 1
  ) ELSE IF "%BE%"=="msvc" (
    cmake -G "Visual Studio 17 2022" . || EXIT /b 1
    cmake --build . --target ALL_BUILD --config Release || EXIT /b 1
    cmake --build . --target RUN_TESTS --config Release || EXIT /b 1
    cmake --build . --target INSTALL --config Release || EXIT /b 1
  )
  CD ..
  CD bzip2-%BZIP2_VERSION%
  IF "%BE%"=="mingw-gcc" (
    SET PATH=%MINGWPATH%
    cmake -G "MinGW Makefiles" -D CMAKE_BUILD_TYPE="Release" -D ENABLE_LIB_ONLY=ON -D ENABLE_SHARED_LIB=OFF -D ENABLE_STATIC_LIB=ON . || EXIT /b 1
    mingw32-make || EXIT /b 1
    REM mingw32-make test || EXIT /b 1
    mingw32-make install || EXIT /b 1
  ) ELSE IF "%BE%"=="msvc" (
    cmake -G "Visual Studio 17 2022" -D CMAKE_BUILD_TYPE="Release" -D ENABLE_LIB_ONLY=ON -D ENABLE_SHARED_LIB=OFF -D ENABLE_STATIC_LIB=ON . || EXIT /b 1
    cmake --build . --target ALL_BUILD --config Release || EXIT /b 1
    REM cmake --build . --target RUN_TESTS --config Release || EXIT /b 1
    cmake --build . --target INSTALL --config Release || EXIT /b 1
  )
  CD ..
  CD xz-%XZ_VERSION%
  IF "%BE%"=="mingw-gcc" (
    SET PATH=%MINGWPATH%
    cmake -G "MinGW Makefiles" -D CMAKE_BUILD_TYPE="Release" . || EXIT /b 1
    mingw32-make || EXIT /b 1
    mingw32-make install || EXIT /b 1
  ) ELSE IF "%BE%"=="msvc" (
    cmake -G "Visual Studio 17 2022" -D CMAKE_BUILD_TYPE="Release" . || EXIT /b 1
    cmake --build . --target ALL_BUILD --config Release || EXIT /b 1
    cmake --build . --target INSTALL --config Release || EXIT /b 1
  )
  CD ..
  CD zstd-%ZSTD_VERSION%\build\cmake
  IF "%BE%"=="mingw-gcc" (
    SET PATH=%MINGWPATH%
    cmake -G "MinGW Makefiles" -D CMAKE_BUILD_TYPE="Release" . || EXIT /b 1
    mingw32-make || EXIT /b 1
    mingw32-make install || EXIT /b 1
  ) ELSE IF "%BE%"=="msvc" (
    cmake -G "Visual Studio 17 2022" -D CMAKE_BUILD_TYPE="Release" . || EXIT /b 1
    cmake --build . --target ALL_BUILD --config Release || EXIT /b 1
    cmake --build . --target INSTALL --config Release || EXIT /b 1
  )
) ELSE IF "%1%"=="configure" (
  IF "%BE%"=="mingw-gcc" (
    SET PATH=%MINGWPATH%
    MKDIR build_ci\cmake
    CD build_ci\cmake
    cmake -G "MinGW Makefiles" -D ZLIB_LIBRARY="C:/Program Files (x86)/zlib/lib/libzlibstatic.a" -D ZLIB_INCLUDE_DIR="C:/Program Files (x86)/zlib/include" -D BZIP2_LIBRARIES="C:/Program Files (x86)/bzip2/lib/libbz2_static.a" -D BZIP2_INCLUDE_DIR="C:/Program Files (x86)/bzip2/include" -D LIBLZMA_LIBRARY="C:/Program Files (x86)/xz/lib/liblzma.a" -D LIBLZMA_INCLUDE_DIR="C:/Program Files (x86)/xz/include" -D ZSTD_LIBRARY="C:/Program Files (x86)/zstd/lib/libzstd.a" -D ZSTD_INCLUDE_DIR="C:/Program Files (x86)/zstd/include" ..\.. || EXIT /b 1
  ) ELSE IF "%BE%"=="msvc" (
    MKDIR build_ci\cmake
    CD build_ci\cmake
    cmake -G "Visual Studio 17 2022" -D CMAKE_BUILD_TYPE="Release" -D ZLIB_LIBRARY="C:/Program Files (x86)/zlib/lib/zlibstatic.lib" -D ZLIB_INCLUDE_DIR="C:/Program Files (x86)/zlib/include" -D BZIP2_LIBRARIES="C:/Program Files (x86)/bzip2/lib/bz2_static.lib" -D BZIP2_INCLUDE_DIR="C:/Program Files (x86)/bzip2/include" -D LIBLZMA_LIBRARY="C:/Program Files (x86)/xz/lib/liblzma.lib" -D LIBLZMA_INCLUDE_DIR="C:/Program Files (x86)/xz/include" -D ZSTD_LIBRARY="C:/Program Files (x86)/zstd/lib/zstd_static.lib" -D ZSTD_INCLUDE_DIR="C:/Program Files (x86)/zstd/include" ..\.. || EXIT /b 1
  )
) ELSE IF "%1%"=="build" (
  IF "%BE%"=="mingw-gcc" (
    SET PATH=%MINGWPATH%
    CD build_ci\cmake
    mingw32-make VERBOSE=1 || EXIT /b 1
  ) ELSE IF "%BE%"=="msvc" (
    CD build_ci\cmake
    cmake --build . --target ALL_BUILD --config Release || EXIT /b 1
  )
) ELSE IF "%1%"=="test" (
  IF "%BE%"=="mingw-gcc" (
    SET PATH=%MINGWPATH%
    CD build_ci\cmake
    SET SKIP_TEST_SPARSE=1
    mingw32-make test VERBOSE=1 || EXIT /b 1
  ) ELSE IF "%BE%"=="msvc" (
    ECHO "Skipping tests on this platform"
    EXIT /b 0
    REM CD build_ci\cmake
    REM cmake --build . --target RUN_TESTS --config Release || EXIT /b 1
  )
) ELSE IF "%1%"=="install" (
  IF "%BE%"=="mingw-gcc" (
    SET PATH=%MINGWPATH%
    CD build_ci\cmake
    mingw32-make install || EXIT /b 1
  ) ELSE IF "%BE%"=="msvc" (
    CD build_ci\cmake
    cmake --build . --target INSTALL --config Release || EXIT /b 1
  )
) ELSE IF "%1"=="artifact" (
    C:\windows\system32\tar.exe -c -C "C:\Program Files (x86)" --format=zip -f libarchive.zip libarchive
) ELSE (
  ECHO "Usage: %0% deplibs|configure|build|test|install|artifact"
  @EXIT /b 0
)
@EXIT /b 0
