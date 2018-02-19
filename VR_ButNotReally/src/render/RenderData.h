#pragma once
#include <vector>
#include <array>
#include <gsl/gsl>


#pragma warning(push)
#include <CppCoreCheck/Warnings.h>
#pragma warning(disable: ALL_CPPCORECHECK_WARNINGS)
#include <glm/glm.hpp>
#pragma warning(pop)

#include <vulkan/vulkan.h>



struct Vertex {
	glm::vec3 pos{};
	glm::vec3 color{};
	glm::vec2 tex_coord{};

	auto static getBindingDescription() noexcept ->VkVertexInputBindingDescription {

		auto binding_description = VkVertexInputBindingDescription{};

		binding_description.binding = 0;
		binding_description.stride = sizeof(Vertex);
		binding_description.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

		return binding_description;
	}

	auto static getAttributeDescriptions() noexcept->std::array<VkVertexInputAttributeDescription, 3> {

		auto attribute_descriptions = std::array<VkVertexInputAttributeDescription, 3>{};

		attribute_descriptions[0].binding = 0;
		attribute_descriptions[0].location = 0;
		attribute_descriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
		attribute_descriptions[0].offset = offsetof(Vertex, pos);

		attribute_descriptions[1].binding = 0;
		attribute_descriptions[1].location = 1;
		attribute_descriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
		attribute_descriptions[1].offset = offsetof(Vertex, color);

		attribute_descriptions[2].binding = 0;
		attribute_descriptions[2].location = 2;
		attribute_descriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
		attribute_descriptions[2].offset = offsetof(Vertex, tex_coord);

		return attribute_descriptions;
	}
};


/*
MVP model.

http://www.opengl-tutorial.org/es/beginners-tutorials/tutorial-3-matrices/
https://solarianprogrammer.com/2013/05/22/opengl-101-matrices-projection-view-model/
*/
struct UniformBufferObject {
	glm::mat4 model{};
	glm::mat4 view{};
	glm::mat4 proj{};
};


#pragma warning(push)
#include <CppCoreCheck/Warnings.h>
#pragma warning(disable: 26426)
const auto vertices = std::vector<Vertex>{
	{ { -0.5f, -0.5f, 0.0f },{ 1.0f, 0.0f, 0.0f }, { 1.0f, 0.0f} },
	{ {  0.5f, -0.5f, 0.0f },{ 0.0f, 1.0f, 0.0f }, { 0.0f, 0.0f } },
	{ {  0.5f,  0.5f, 0.0f },{ 0.0f, 0.0f, 1.0f }, { 0.0f, 1.0f } },
	{ { -0.5f,  0.5f, 0.0f },{ 1.0f, 1.0f, 1.0f }, { 1.0f, 1.0f } },

	{ { -0.5f, -0.5f, -0.5f },{ 1.0f, 0.0f, 0.0f },{ 1.0f, 0.0f } },
	{ {  0.5f, -0.5f, -0.5f },{ 0.0f, 1.0f, 0.0f },{ 0.0f, 0.0f } },
	{ {  0.5f,  0.5f, -0.5f },{ 0.0f, 0.0f, 1.0f },{ 0.0f, 1.0f } },
	{ { -0.5f,  0.5f, -0.5f },{ 1.0f, 1.0f, 1.0f },{ 1.0f, 1.0f } },
};

const auto indices = std::vector<uint16_t>{
	0, 1, 2, 2, 3, 0,
	4, 5, 6, 6, 7, 4
};
#pragma warning(pop)


