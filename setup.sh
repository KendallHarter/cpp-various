#!/bin/bash

# clangd setup
mkdir -p build
pushd build
cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=True -G Ninja ..
popd

# Hook install
./hooks/install.sh
