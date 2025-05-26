export BUILD_BIN_DIR=../bin

CC=gcc
EXE=vulkan_renderer
SRC=../src/xcb_main.c
INCLUDE=../src/
LIBS="-lX11 -lX11-xcb -lm -lxcb -lxcb-xfixes -lxcb-keysyms -lvulkan"
FLAGS="-g -Wall"

sh compile_shaders.sh

# Executable compilation
printf "Compiling executable...\n"

$CC -o $BUILD_BIN_DIR/$EXE $SRC -I $INCLUDE $FLAGS $LIBS

if [ $? -eq 0 ]; then
    printf "Compilation was \033[0;32m\033[1msuccessful\033[0m.\n"
    exit 0
else
	exit 1
fi
