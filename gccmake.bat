: Where to find gcc (and upx), change this to your need
set SAVE_PATH=%PATH%
set PATH=C:\mingw\bin;%PATH%
set CC=gcc
set AR=ar
set JC=gcj
set JAR=jar
: Recommended C-flags for Win32. Works for all gcc-derived compilers.
set CFLAGS=-O -Wall -Wwrite-strings -Wextra -Werror -mno-cygwin -mms-bitfields -pipe -DSTRICT
: For gcj < 4.3, remove the options -fsource/-ftarget
set JFLAGS=-Wall -Werror -femit-class-file -fsyntax-only -foutput-class-dir=. -fsource=1.3 -ftarget=1.3

%CC% %CFLAGS% -c -o busllib.o busllib.c
%AR% -cr busl.a busllib.o
%CC% %CFLAGS% -mconsole -s -Wl,--large-address-aware -o busl.exe busl.c busllib.o
%CC% %CFLAGS% -mconsole -s -Wl,--large-address-aware -o buslcxx.exe busl.cxx busllib.o -lstdc++
%CC% %CFLAGS% -mwindows -s -Wl,--large-address-aware -o buslw.exe buslw.c busllib.o
%CC% %CFLAGS% -shared -s -Wl,--large-address-aware,--kill-at -DBUSL_EXPORT=__declspec(dllexport) -o busl.dll busl.def busljni.c busllib.c
dlltool -l busl.dll.a -d buslgnu.def -D busl.dll --kill-at
%CC% %CFLAGS% -s -Wl,--large-address-aware -mconsole -o buslx.exe busl.c -L. -lbusl

upx --best busl*.exe

%JC% %JFLAGS% Busl.java
%JAR% -cfm Busl.jar MANIFEST.MF org/tigris/busl/Busl.class

set PATH=%SAVE_PATH%
