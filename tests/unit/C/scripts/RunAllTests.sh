#!/bin/sh
#set -e
#
# This is the shell script wrapper for running C unit tests
#
exitstate=0

jobs="-j4"
if [ "$1" = "-j*" ]; then
  jobs="$1"
fi

COVERAGE=0
if [ "$1" = "coverage" ]; then
  COVERAGE=1
fi

# Set here location of Fledge source code:
# if FLEDGE_ROOT is not set then use Fledge includes and Fledge libs

FLEDGE_SRC="${FLEDGE_ROOT}"
# NOTE: Fledge libraries come from FLEDGE_SRC/cmake_build/C/lib

# If not set ...
if [ "${FLEDGE_SRC}" = "" ]; then
	# Set path with Fledge includes and Fledge libs:
	FLEDGE_INCLUDE_DIRS="/usr/include/fledge"
	FLEDGE_LIB_DIRS="/usr/lib/fledge"
fi

# Go back to all tests path
cd ..

if [ ! -d results ] ; then
        mkdir results
fi

cmakefile=`find . -name CMakeLists.txt`
for f in $cmakefile; do	
	dir=`dirname $f`
	echo Testing $dir
	(
		cd $dir;
		rm -rf build;
		mkdir build;
		cd build;
		echo Building Tests...;
		cmake -DFLEDGE_SRC="${FLEDGE_SRC}" -DFLEDGE_INCLUDE="${FLEDGE_INCLUDE_DIRS}" -DFLEDGE_LIB="${FLEDGE_LIB_DIRS}" ..;
		rc=$?
		if [ $rc != 0 ]; then
			echo cmake failed for $dir;
			exit 1
		fi
		make ${jobs};
		rc=$?
		if [ $rc != 0 ]; then
			echo make failed for $dir;
			exit 1
		fi
		if [ $COVERAGE -eq 0 ]; then
			echo Running tests...;
			./RunTests --gtest_output=xml > /tmp/results;
			rc=$?
			if [ $rc != 0 ]; then
				exit $rc
			fi
		fi

		if [ $COVERAGE -eq 1 ]; then
			echo Generating coverage reports...;
			file=$(basename $f)
			grep -q CoverageHtml ../${file}
			[ $? -eq 0 ] && (echo Running "make CoverageHtml" && make CoverageHtml) || echo "CoverageHtml target not found, skipping..."
		fi

	) >/dev/null
	rc=$?
	if [ $rc != 0 ]; then
		echo Tests for $dir failed
		cat /tmp/results
		exitstate=1
	else
		echo All tests in $dir passed
	fi
	file=`echo $dir | sed -e 's#./##' -e 's#/#_#g'`
	[ -f $dir/build/test_detail.xml ] && mv $dir/build/test_detail.xml results/${file}.xml
done
exit $exitstate
