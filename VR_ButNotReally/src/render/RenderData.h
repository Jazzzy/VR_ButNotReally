#pragma once
#include <vector>
#include <array>
#include <glm/glm.hpp>
#include <vulkan/vulkan.h>



struct Vertex {
	glm::vec2 pos;
	glm::vec3 color;

	auto static getBindingDescription() ->VkVertexInputBindingDescription {
	
		auto binding_description = VkVertexInputBindingDescription{};

		binding_description.binding = 0;
		binding_description.stride = sizeof(Vertex);
		binding_description.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

		return binding_description;
	}

	auto static getAttributeDescriptions() -> std::array<VkVertexInputAttributeDescription,2> {
	
		auto attribute_descriptions = std::array<VkVertexInputAttributeDescription, 2>{};

		attribute_descriptions[0].binding = 0;
		attribute_descriptions[0].location = 0;
		attribute_descriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
		attribute_descriptions[0].offset = offsetof(Vertex, pos);

		attribute_descriptions[1].binding = 0;
		attribute_descriptions[1].location = 1;
		attribute_descriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
		attribute_descriptions[1].offset = offsetof(Vertex, color);

		return attribute_descriptions;
	}
};

const auto vertices = std::vector<Vertex>{
	{ {  0.0f,-0.5f },{ 1.0f, 0.8f, 0.0f } },
	{ {  0.5f, 0.5f },{ 0.0f, 1.0f, 0.0f } },
	{ { -0.5f, 0.5f },{ 0.3f, 0.0f, 1.0f } },
};

