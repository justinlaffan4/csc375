@echo off

if not exist build mkdir build

set cc_opts=/DINTERNAL_BUILD /Od /Zi /Fehw1.exe
set cl_opts=/INCREMENTAL:NO /OPT:REF

pushd build
cl ..\hw1\main.cpp %cc_opts% /link %cl_opts%
popd build
