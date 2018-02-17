#version 450
#extension GL_ARB_separate_shader_objects : enable

// This is the object data
layout(binding = 0) uniform UniformBufferObject {
	mat4 model;
	mat4 view;
	mat4 proj;
} ubo;

// This is the per vertex input data
layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec3 inColor;


// And this is the per vertex output data
layout(location = 0) out vec3 fragColor;

out gl_PerVertex {
    vec4 gl_Position;
};

void main() {
	/*
	This follows PVM order since GLSL uses row-vector style sothe vector is ordered 
	following the rows. That is, the matrix is transposed. Making these mult operations
	go from right to left.
	*/
    gl_Position =  ubo.proj * ubo.view * ubo.model * vec4(inPosition, 0.0, 1.0);
    fragColor = inColor;
}