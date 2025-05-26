GLSLC=glslc
SHADER_SRC=../src/shaders/
SHADER_OUT=$BUILD_BIN_DIR/shaders

# Shader compilation
$GLSLC $SHADER_SRC/world.vert -o $SHADER_OUT/world_vertex.spv
if [ $? -ne 0 ]; then
	exit 1
fi
$GLSLC $SHADER_SRC/world.frag -o $SHADER_OUT/world_fragment.spv
if [ $? -ne 0 ]; then
	exit 1
fi
