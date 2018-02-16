#pragma once
#include <exception>
#include <iostream>
#include <cstdlib>
#include <functional>
#include <memory>
#include <vector>
#include <gsl/gsl>

#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#ifdef max
#undef max
#endif
#ifdef min
#undef min
#endif

/*
We define this in case we want to use VulkanMemoryAllocator's
allocators.

	- https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator

Currently we are using the allocator by default so allocation of
buffers and images without VMA_USE_ALLOCATOR could not be fully supported.
*/
#define VMA_USE_ALLOCATOR

#ifdef VMA_USE_ALLOCATOR
#pragma warning(push)
#include <CppCoreCheck/Warnings.h>
#pragma warning(disable: ALL_CPPCORECHECK_WARNINGS)
#include "vk_mem_alloc.h"
#pragma warning(pop)


struct AllocatedBuffer {
	VkBuffer buffer{};
	VmaAllocation allocation{};
	VmaAllocationInfo allocation_info{};
};

#else

struct AllocatedBuffer {
	VkBuffer buffer{};
	VkDeviceMemory memory{};
};

#endif

#include "../utils/Utils.h"
#include "./RenderUtils.h"

/**
Used for managing all the rendering logic of the application.

Manages creation, destruction and use of all the resources necessary
to render to the screen using the Vulkan API.
*/
class RenderManager
{
private:
	using WindowPtr = std::unique_ptr<GLFWwindow, GLFWWindowDestroyer>;
	using uint = uint32_t;

public:
	/**
	Default Constructor.
	Since we tie a lot of the resources to this instance and we need to
	manage them only from here the copy and move functionalities are not allowed.
	*/
	[[gsl::suppress(26439)]] RenderManager();
	RenderManager(const RenderManager&) = delete;
	RenderManager& operator=(const RenderManager&) = delete;
	RenderManager(RenderManager&&) = delete;
	RenderManager& operator=(RenderManager&&) = delete;
	~RenderManager();

	/* -------------------------------------------------------------------------------------------------- */
	/* ---------------------------------------- FUNCTION MEMBERS ---------------------------------------- */
	/* -------------------------------------------------------------------------------------------------- */

	/**
	Runs a tick of the render manager once it is initializated,
	it is meant to be included in a "game loop" type of loop.
	*/
	auto update() noexcept -> void;

	/**
	Returns true when we should close the application as far as the render manager
	is concerned. Currently it deals with the user closing the window itself.

	@return true if the window has been closed or we should end the application.
	*/
	auto shouldClose() const noexcept -> bool;

private:

	/**
	Initializes the physical window shown by the operating system

	@see m_window
	*/
	auto initWindow() noexcept -> void;

	/**
	Initializes all the Vulkan related components necessary for rendering to
	the screen.
	*/
	auto initVulkan() -> void;

	/**
	Recreates all the necessary members to create a new swap chain, for example
	when resizing the window.
	*/
	auto recreateSwapChain() -> void;

	/**
	Internal function that represents one iteration of the loop that will
	be run within each tick.
	*/
	auto loopIteration() noexcept -> void;

	/**
	Cleans up all the resources that need to be explicitly cleaned up,
	mostly related to the Vulkan API and its resources.
	*/
	auto cleanup() noexcept -> void;

	/**
	Cleans up all the resources related to the swap chain, this is useful when
	recreating the swap chain to be sure we are not leaking memory and resources.
	*/
	auto cleanupSwapChain() noexcept -> void;

	/**
	Creates a Vulkan instance based on the current configuration parameters.

	@see m_instance
	@return A Valid Vulkan Instance
	*/
	auto createInstance()->VkInstance;

	/**
	Prints to standard output all the extension names of all the extensions provided.

	@param a vector with the extensions you would like to print
	*/
	auto printInstanceExtensions(const std::vector<VkExtensionProperties>& extensions)
		const -> void;

	/**
	Checks and prints if all the required extensions provided are available within the available ones.

	@param The vector of the names of the required extensions
	@param The vector of the struct with the properties of the available extensions
	@return true if all the required extensions are within the available ones, false otherwise.
	*/
	[[gsl::suppress(bounds.3)]] auto checkInstanceExtensionsNamesAvailable(
		const std::vector<const char*>& required_extensions,
		const std::vector<VkExtensionProperties>& available_extensions)
		const -> bool;

	/**
	Checks and prints if all the required validation layers in the conficuration are available.

	@return true if all the required validation layers are within the available ones, false otherwise.
	*/
	[[gsl::suppress(bounds.3)]] auto checkValidationLayerSupport() const noexcept -> bool;

	/**
	Calculates and returns the necessary Vulkan extensions for this application based
	on configuration.

	@return A vector with the names of the required extensions
	*/
	auto getRequiredExtensions() const noexcept->std::vector<const char*>;

	/**
	Callback function called to receive messages from the validation layers when
	enabled (in debug mode).

	It follows the parameters of the vulkan PFN_vkDebugReportCallbackEXT function

	@see PFN_vkDebugReportCallbackEXT
	@return VK_FALSE to indicate that the Vulkan call should NOT be aborted.
	*/
#pragma warning( push )
#pragma warning( disable : 4229)
	auto static VKAPI_ATTR VKAPI_CALL debugReportCallback(
		VkDebugReportFlagsEXT                       flags,
		VkDebugReportObjectTypeEXT                  object_type,
		uint64_t                                    object,
		size_t                                      location,
		int32_t                                     msg_code,
		const char*                                 layer_prefix,
		const char*                                 msg,
		void*                                       user_data
	)->VkBool32;
#pragma warning( pop )

	/**
	Sets up the callback function to receive debug information from
	the debug report validation layer.

	@see debugReportCallback
	@see m_debug_callback
	*/
	auto setupDebugCallback() -> void;

	/**
	Creates the window surface to which we will render to.

	@see m_surface
	*/
	auto createSurface() -> void;

	/**
	Interface to the vulkan function "vkCreateDebugReportCallbackEXT" to create
	the debug function callback.

	@see vkCreateDebugReportCallbackEXT
	@return the result of vkCreateDebugReportCallbackEXT or VK_ERROR_EXTENSION_NOT_PRESENT if
	we can't find the function pointer.
	*/
	auto createDebugReportCallbackEXT(
		const VkInstance& instance,
		const VkDebugReportCallbackCreateInfoEXT * create_info,
		const VkAllocationCallbacks* allocator,
		VkDebugReportCallbackEXT* callback
	) noexcept->VkResult;


	/**
	Interface to the vulkan function "vkDestroyDebugReportCallbackEXT" to destroy
	the debug function callback.

	@see vkDestroyDebugReportCallbackEXT
	*/
	auto destroyDebugReportCallbackEXT(
		const VkInstance& instance,
		const VkDebugReportCallbackEXT& callback,
		const VkAllocationCallbacks* allocator
	) noexcept -> void;

	/**
	Iterates and picks the most suitable physical device
	available on the system for this application.

	@see m_physical_device
	@see m_physical_device_properties
	@see m_physical_device_features
	*/
	auto pickPhysicalDevice() -> void;

	/**
	Checks if the physical device is suitable for the application
	and gives it a score based on how suitable it is based on configuration.

	@param the physical device to rate
	@return a tuple with a bool representing that is true when the device is valid
	and an int representing the score, the higher the better.
	*/
	auto physicalDeviceSuitability(const VkPhysicalDevice& device) const noexcept->std::tuple<bool, int>;

	/**
	Checks the queue families supported by the provided device and returns the indices for the
	required ones in the struct QueueFamilyIndices (graphics and present queues right now).

	@see QueueFamilyIndices
	@see m_queue_family_indices
	@see PrintOptions
	@param The physical device to retrieve the queue families from
	@param The print options for the function to indicate the output desired to standard output
	@return The struct with the family queue indices required or -1 if they haven't been found
	*/
	auto findQueueFamilies(const VkPhysicalDevice& physical_device, PrintOptions print_options) const->QueueFamilyIndices;

	/**
	Creates the vulkan logical device we are going to use and retrieves the graphics and present
	queues from it.

	@see m_device
	@see m_graphics_queue
	@see m_present_queue
	*/
	auto createLogicalDevice() -> void;

	/**
	Creates the allocator used to reserve memory in vulkan
	
	@see m_vma_allocator
	*/
	auto createAllocator() noexcept ->void;
	
	/**
	Checks if the physical device provided supports all the extensions required by our configuration

	@param The physical device to check for extension support
	@return true if all the extensions are supported, false otherwise
	*/
	[[gsl::suppress(bounds.3)]] auto checkDeviceExtensionSupport(const VkPhysicalDevice& device) const -> bool;

	/**
	Checks and retrieves information about the swap chain capabilities of a physical device.
	Fills up said information in the SwapChainSupportDetails struct

	@see SwapChainSupportDetails
	@param The physical device to check for swap chain support
	@return A SwapChainSupportDetails filled with swap chain support information
	*/
	auto querySwapChainSupport(const VkPhysicalDevice& device) const->SwapChainSupportDetails;

	/**
	Checks and returns the best available surface chain format from the provided ones
	prioritizing the ones that fit us best.

	@param A vector with the surface formats to check
	@return The best surface format for our application
	*/
	auto pickSurfaceChainFormat(const std::vector<VkSurfaceFormatKHR>& available_formats) const->VkSurfaceFormatKHR;

	/**
	Checks and returns the best available present mode from the provided ones
	prioritizing the ones that fit us best.

	@param A vector with the present modes to check
	@return The best present mode for our application
	*/
	auto pickSurfacePresentMode(const std::vector<VkPresentModeKHR>& available_modes)  const noexcept->VkPresentModeKHR;

	/**
	Checks and returns the best available extent for the images in the
	swap chain based on its capabilities.

	@param The capabilities of the surface
	@return The best possible extent (width, height) for our application
	*/
	auto pickSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities)  const noexcept->VkExtent2D;

	/**
	Creates the vulkan swap chain required for rendering.
	Prioritizes a triple buffering implementation if possible.

	@see m_swap_chain
	*/
	auto createSwapChain() -> void;

	/**
	Creates an image view for each image in the swap chain so
	we can use them as color targets later.

	@see m_swap_chain_image_views
	*/
	auto createImageViews() -> void;

	/**
	Creates the render pass that will be used to then create
	the graphics pipeline.

	@see m_render_pass
	*/
	auto createRenderPass() -> void;

	/**
	Creates the graphics pipeline (or pipelines) that will be used to render
	our scene.

	@see m_pipeline_layout
	@see m_pipeline
	*/
	auto createGraphicsPipeline() -> void;

	/**
	Creates a shader module based on the code provided and wraps it up
	in the VkShaderModule struct

	@see VkShaderModule
	@param An array of characters with the code for the shader
	@return The shader module created based on the shader code
	*/
	auto createShaderModule(const std::vector<char>& code) const->VkShaderModule;

	/**
	Creates the framebuffersto draw to during render time

	@see m_swap_chain_framebuffers
	*/
	auto createFramebuffers() ->  void;

	/**
	Creates the command pool that contains the command buffers to
	draw to during render time.

	@see m_graphics_command_pool
	*/
	auto createGraphicsCommandPool() ->  void;

	/**
	Creates the command pool that contains the command buffers to
	execute transfer memory commands.

	@see m_transfer_command_pool
	*/
	auto createTransferCommandPool() ->  void;

	/**
	Helper function that creates a vulkan buffer in a general way.

	@param The size of the memory to be allocated
	@param The usage flags necessary for the memory we are allocating
	@param The required properties for the memory we are allocating
	@param The sharing mode of the buffer (VK_SHARING_MODE_(EXCLUSIVE/CONCURRENT))
	@param The family indices of the queues this buffer will be shared between if CONCURRENT, nullptr otherwise
	@param The buffer handle we will populate
	@param The device memory handle we will populate
	*/
	auto createBuffer(
		VkDeviceSize size,
		VkBufferUsageFlags usage,
#ifdef VMA_USE_ALLOCATOR
		VmaMemoryUsage allocation_usage,
		VmaAllocationCreateFlags allocation_flags,
#else
		VkMemoryPropertyFlags properties,
#endif
		AllocatedBuffer& allocated_buffer,
		VkSharingMode sharing_mode,
		const std::vector<uint>* queue_family_indices) -> void;

	auto destroyBuffer(
		AllocatedBuffer& allocated_buffer
	) noexcept -> void;

	/**
	Creates the vertex buffer that will hold the vertices to render.

	@see m_vertex_buffer
	*/
	auto createVertexBuffer() -> void;

	/**
	Creates the index buffer that will hold the indexes in order to render.

	@see m_index_buffer
	*/
	auto createIndexBuffer() -> void;

	/**
	Calculates the required memory types given the input properties.

	@param Flags that indicate the types of memories that we can consider.
	@param Properties of the memory we need
	@return Appropriate flags of the memory we can use.
	*/
	auto findMemoryType(uint type_filter, VkMemoryPropertyFlags properties)->uint;

	/**
	Copies the contents from one VkBuffer to another.

	@param The source buffer
	@param The destination buffer
	@param The size of the memory to be copied
	*/
	auto copyBuffer(VkBuffer src, VkBuffer dst, VkDeviceSize size) noexcept -> void;

	/**
	Creates the command buffers that contain the commands to
	draw to during render time.

	@see m_command_buffers
	*/
	auto createCommandBuffers() ->  void;

	/**
	Creates the drawing commands into the command buffers

	@see m_command_buffers
	*/
	auto recordCommandBuffers() -> void;

	/**
	Creates the semaphores necessary for synchronization of
	the rendering phase.

	@see m_image_available_semaphore
	@see m_render_finished_semaphore
	*/
	auto createSemaphores() -> void;


	/**
	Renders a frame to the screen. It can be called after
	updating the application state to use the CPU to do that
	while the previous frame is being rendered.
	*/
	[[gsl::suppress(bounds.3)]] auto drawFrame() -> void;

	/**
	Handles the event of resizing the window to set up the appropriate
	rendering parameters accordingly.

	@param Reference to the window that has been resized
	@param New width of the window
	@param New height of the window
	*/
	auto static onWindowsResized(GLFWwindow * window, int width, int heigth) -> void;

	/* ---------------------------------------------------------------------------------------------- */
	/* ---------------------------------------- DATA MEMBERS ---------------------------------------- */
	/* ---------------------------------------------------------------------------------------------- */

#ifdef VMA_USE_ALLOCATOR
	VmaAllocator m_vma_allocator{};
#endif

	WindowPtr m_window{};

	VkInstance m_instance{};

	VkDebugReportCallbackEXT m_debug_callback{};

	VkSurfaceKHR m_surface{};

	QueueFamilyIndices m_queue_family_indices{};

	VkPhysicalDevice m_physical_device = VK_NULL_HANDLE;

	VkPhysicalDeviceProperties m_physical_device_properties{};

	VkPhysicalDeviceFeatures m_physical_device_features{};

	VkDevice m_device{};

	VkQueue m_graphics_queue{};

	VkQueue m_present_queue{};

	VkQueue m_transfer_queue{};

	VkSwapchainKHR m_swap_chain{};

	std::vector<VkImage> m_swap_chain_images{};

	VkFormat m_swap_chain_image_format{};

	VkExtent2D m_swap_chain_extent{};

	std::vector<VkImageView> m_swap_chain_image_views{};

	VkRenderPass m_render_pass{};

	VkPipelineLayout m_pipeline_layout{};

	VkPipeline m_pipeline{};

	std::vector<VkFramebuffer> m_swap_chain_framebuffers{};

	VkCommandPool m_graphics_command_pool{};

	VkCommandPool m_transfer_command_pool{};

	AllocatedBuffer m_vertex_buffer{};

	AllocatedBuffer m_index_buffer{};

	std::vector<VkCommandBuffer> m_command_buffers{};

	VkSemaphore m_image_available_semaphore{};

	VkSemaphore m_render_finished_semaphore{};


};

