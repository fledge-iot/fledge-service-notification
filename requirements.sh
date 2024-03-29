#!/usr/bin/env bash

##--------------------------------------------------------------------
## Copyright (c) 2019 Dianomic Systems
##
## Licensed under the Apache License, Version 2.0 (the "License");
## you may not use this file except in compliance with the License.
## You may obtain a copy of the License at
##
##     http://www.apache.org/licenses/LICENSE-2.0
##
## Unless required by applicable law or agreed to in writing, software
## distributed under the License is distributed on an "AS IS" BASIS,
## WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
## See the License for the specific language governing permissions and
## limitations under the License.
##--------------------------------------------------------------------

##
## Author: Massimiliano Pinto
##


set -e

fledge_location=$(pwd)
os_name=$(grep -o '^NAME=.*' /etc/os-release | cut -f2 -d\" | sed 's/"//g')
os_version=$(grep -o '^VERSION_ID=.*' /etc/os-release | cut -f2 -d\" | sed 's/"//g')
echo "Platform is ${os_name}, Version: ${os_version}"

if [[ ( $os_name == *"Red Hat"* || $os_name == *"CentOS"* ) ]]; then
	if [[ $os_name == *"Red Hat"* ]]; then
		sudo yum-config-manager --enable 'Red Hat Enterprise Linux Server 7 RHSCL (RPMs)'
		sudo yum install -y @development
	else
		sudo yum groupinstall "Development tools" -y
		# Install centos-release-scl only if OS is CentOS 7
		if [[ $os_version == *"7"* ]]; 
		then
			sudo yum install -y centos-release-scl
			# A gcc version newer than 4.9.0 is needed to properly use <regex>
			# the installation of these packages will not overwrite the previous compiler
			# the new one will be available using the command 'source scl_source enable devtoolset-7'
			# the previous gcc will be enabled again after a log-off/log-in.
			sudo yum-config-manager --enable rhel-server-rhscl-7-rpms
			sudo yum install -y devtoolset-7
		fi
	fi
	sudo yum install -y boost-devel
	sudo yum install -y glib2-devel
	sudo yum install -y rsyslog
	sudo yum install -y openssl-devel
	sudo yum install -y wget
	sudo yum install -y zlib-devel
	sudo yum install -y git
	sudo yum install -y cmake
	sudo yum install -y libuuid-devel
	sudo yum install -y yum-utils
	
elif apt --version 2>/dev/null; then
	sudo apt install -y avahi-daemon curl
	sudo apt install -y cmake g++ make build-essential autoconf automake uuid-dev
	sudo apt install -y libtool libboost-dev libboost-system-dev libboost-thread-dev libpq-dev libssl-dev libz-dev
	sudo apt install -y pkg-config
else
	echo "Requirements cannot be automatically installed, please refer README.rst to install requirements manually"
fi
