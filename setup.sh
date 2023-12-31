#!/bin/bash

# clangd setup
mkdir -p build
pushd build
cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=True -G Ninja ..
popd
ln -s build/compile_commands.json

# Hook install
./hooks/install.sh
