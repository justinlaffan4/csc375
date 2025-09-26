@echo off

if not exist build mkdir build

set cc_opts=/DINTERNAL_BUILD /Od /Zi /Fehw1.exe
set cl_opts=/INCREMENTAL:NO /OPT:REF

pushd build
cl ..\hw1\platform_win.cpp %cc_opts% /link user32.lib opengl32.lib gdi32.lib %cl_opts%
popd build
