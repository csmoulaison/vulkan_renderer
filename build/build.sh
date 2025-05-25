BIN=../bin

CC=gcc
EXE=out
SRC=../src/xcb_main.c
INCLUDE=../src/
LIBS="-lX11 -lX11-xcb -lm -lxcb -lxcb-xfixes -lxcb-keysyms -lvulkan"
FLAGS="-g -O3 -Wall"

GLSLC=glslc
SHADER_SRC=../src/shaders/
SHADER_OUT=$BIN/shaders

# Shader compilation
$GLSLC $SHADER_SRC/world.vert -o $SHADER_OUT/world_vertex.spv
if [ $? -ne 0 ]; then
	exit 1
fi
$GLSLC $SHADER_SRC/world.frag -o $SHADER_OUT/world_fragment.spv
if [ $? -ne 0 ]; then
	exit 1
fi

# Executable compilation
printf "Compiling executable...\n"

$CC -o $BIN/$EXE $SRC -I $INCLUDE $FLAGS $LIBS

if [ $? -eq 0 ]; then
    printf "Compilation was \033[0;32m\033[1msuccessful\033[0m.\n"
    exit 0
else
	exit 1
fi
