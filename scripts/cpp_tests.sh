#!/bin/bash
set -euxo pipefail

BUILD_REGISTRY=ON
BUILD_TYPE=RelWithDebInfo
BUILD_DIR=build-cpp-tests/
if [[ "$(uname)" == "Darwin" ]]; then
	NUM_PROCS=$(sysctl -n hw.ncpu)
else
	NUM_PROCS=$(nproc)
fi

cmake -S . -B ${BUILD_DIR} \
	-DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
	-DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build ${BUILD_DIR} \
	--config ${BUILD_TYPE} \
	--target mlc_backtrace_static \
	-j "${NUM_PROCS}"
