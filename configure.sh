#!/bin/sh
set -eux

BUILD_DIR=build

meson setup "$BUILD_DIR" \
	--native-file clang.ini \
	--wipe
