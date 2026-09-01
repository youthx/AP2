@echo off
set PATH=C:\msys64\ucrt64\bin;C:\Windows\System32;C:\Windows;%PATH%
cd /d c:\Users\jackw\OneDrive\Desktop\git-projects\aphelion-engine\ap2
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug ^
  -DCMAKE_C_COMPILER=C:/msys64/ucrt64/bin/gcc.exe ^
  -DCMAKE_MAKE_PROGRAM=C:/msys64/ucrt64/bin/ninja.exe ^
  -DCMAKE_C_FLAGS="-B C:/msys64/ucrt64/bin" ^
  -DCMAKE_EXE_LINKER_FLAGS="-B C:/msys64/ucrt64/bin" ^
  -DCMAKE_SHARED_LINKER_FLAGS="-B C:/msys64/ucrt64/bin" ^
  -DCMAKE_STATIC_LINKER_FLAGS="-B C:/msys64/ucrt64/bin"
