#!/bin/bash

# Pass any cmake options this way:

# ./build_rhel.sh -DFLEDGE_INSTALL=/some_path/Fledge

# The new gcc 7 is now available using the command 'source scl_source enable devtoolset-7'
# the previous gcc will be enabled again after this script exits
os_version=$(grep -o '^VERSION_ID=.*' /etc/os-release | cut -f2 -d\" | sed 's/"//g')
# Use scl_source only if OS is RedHat/CentOS 7
if [[ $os_version == *"7"* ]]
then
    source scl_source enable devtoolset-7
fi

mkdir build
cd build/
cmake $@ ..
make
