#!/bin/sh
set -eux

BUILD_DIR=build

echo "Select build type:"
echo "  1) release"
echo "  2) debug"
echo "  3) debugoptimized"
printf "Enter choice [1-3] (default: 1): "

read -r choice || choice=""

case "$choice" in
2)
	BUILD_TYPE=debug
	;;
3)
	BUILD_TYPE=debugoptimized
	;;
"" | 1)
	BUILD_TYPE=release
	;;
*)
	echo "Invalid choice: $choice"
	exit 1
	;;
esac

echo "Configuring build type: $BUILD_TYPE"

meson setup "$BUILD_DIR" \
	--native-file clang.ini \
	--buildtype="$BUILD_TYPE" \
	--wipe
