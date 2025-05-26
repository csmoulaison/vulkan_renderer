#version 450

layout(location = 0) in vec3 in_pos;
layout(location = 1) in vec2 in_texture_coord;

layout(location = 0) out vec2 frag_texture_coord;

layout(binding = 0) uniform ubo_global {
	mat4 view;
	mat4 projection;
	vec3 clear_color;
} global;

layout(binding = 1) uniform ubo_inst {
	mat4 model;
} inst;

void main() {
	gl_Position = global.projection * global.view * inst.model * vec4(in_pos, 1.0);
    frag_texture_coord = in_texture_coord;
}
