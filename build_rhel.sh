#!/bin/bash

# Pass any cmake options this way:

# ./build_rhel.sh -DFLEDGE_INSTALL=/some_path/Fledge

# The new gcc 7 is now available using the command 'source scl_source enable devtoolset-7'
# the previous gcc will be enabled again after this script exits
# Commented scl_source  because it does not work in Centos Stream 9
# source scl_source enable devtoolset-7

mkdir build
cd build/
cmake $@ ..
make
