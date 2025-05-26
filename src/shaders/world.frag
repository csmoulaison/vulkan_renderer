#version 450

layout(location = 0) in vec2 frag_texture_coord;

layout(location = 0) out vec4 outColor;

layout(binding = 2) uniform sampler2D texture_sampler;

void main() {
	outColor = texture(texture_sampler, frag_texture_coord);
}
