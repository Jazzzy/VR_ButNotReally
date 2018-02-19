#pragma once
#include <vector>
#include <array>
#include <gsl/gsl>


#pragma warning(push)
#include <CppCoreCheck/Warnings.h>
#pragma warning(disable: ALL_CPPCORECHECK_WARNINGS)
#include <glm/glm.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>
#pragma warning(pop)

#include <vulkan/vulkan.h>

#include "../utils/Utils.h"
#include "./RenderUtils.h"
#include "../Configuration.h"

#define VMA_USE_ALLOCATOR
#ifdef VMA_USE_ALLOCATOR
#pragma warning(push)
#include <CppCoreCheck/Warnings.h>
#pragma warning(disable: ALL_CPPCORECHECK_WARNINGS)
#include "vk_mem_alloc.h"
#pragma warning(pop)

/**
Our main unsigned in type to accomodate
vulkan's needs
*/
using uint = uint32_t;

/**
Wrapps a Vulkan Buffer with allocation information tied to it
*/
struct AllocatedBuffer {
	VkBuffer buffer{};
	VmaAllocation allocation{};
	VmaAllocationInfo allocation_info{};
};

/**
Wrapps a Vulkan Image with allocation information tied to it
*/
struct AllocatedImage {
	VkImage image{};
	VmaAllocation allocation{};
	VmaAllocationInfo allocation_info{};
};

#else

#pragma message ( "This program is meant to currently use VMA allocation" )
#error Current code needs to use VMA allocation

struct AllocatedBuffer {
	VkBuffer buffer{};
	VkDeviceMemory memory{};
};

struct AllocatedImage {
	VkImage image{};
	VkDeviceMemory memory{};
};

#endif


/**
Wraps a Vulkan Command Buffer with relevant
type and state information tied to it
*/
struct WrappedCommandBuffer {
	VkCommandBuffer buffer{};
	CommandType type{};
	bool recording{ false };
};

/**
Wraps a Vulkan Render Target with all the relevant information to
render to it like the image, memory, view, width, heigth and if it has
been initializated.

We are not using an "AllocatedImage" because allocation of a render target
is done with a custom method because of the special requirements of it being
a render target.
*/
struct WrappedRenderTarget {
	VkImage image{};
	VkDeviceMemory memory{};
	VkImageView view{};
	uint width{};
	uint heigth{};
	bool init{ false };
};

/**
Struct that holds dynamic configuration parameters of the renderer
*/
struct RenderConfiguration {
	short multisampling_samples{ config::initial_multisampling_samples };
};

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

	auto operator==(const Vertex& other) const ->bool {
		return	pos == other.pos &&
				color == other.color &&
				tex_coord == other.tex_coord;
	}
};

namespace std {
	template<> struct hash<Vertex> {
		size_t operator()(Vertex const& vertex) const {
			return(
				(hash<glm::vec3>()(vertex.pos) ^
				(hash<glm::vec3>()(vertex.color) << 1)) >> 1) ^
				(hash<glm::vec2>()(vertex.tex_coord) << 1);
		}
	};
}

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


struct SimpleObjScene {
	std::vector<Vertex> vertices;
	std::vector<uint32_t> indices;
	AllocatedImage m_texture_image{};
	VkImageView m_texture_image_view{};
};

#if 0
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
#endif


