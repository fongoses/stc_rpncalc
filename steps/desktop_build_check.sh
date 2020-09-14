#!/bin/bash

set -e

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

# build
cd $SCRIPT_DIR/..
if [ "$1" == "--rebuild" ]; then
    rm -rf build_qt
    mkdir build_qt
else
    mkdir -p build_qt
fi
cd build_qt
cmake .. -GNinja
ninja

# run tests
src/decn/decn_tests

# get coverage
lcov --capture --directory src/decn --output-file coverage.info
genhtml coverage.info --output-directory lcov
