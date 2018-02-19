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

#include "../utils/Utils.h"
#include "./RenderUtils.h"
#include "../Configuration.h"
#include "RenderData.h"


/**
Used for managing all the rendering logic of the application.

Manages creation, destruction and use of all the resources necessary
to render to the screen using the Vulkan API.
*/
class Renderer
{
private:
	using WindowPtr = std::unique_ptr<GLFWwindow, GLFWWindowDestroyer>;

public:
	/**
	Default Constructor.
	Since we tie a lot of the resources to this instance and we need to
	manage them only from here the copy and move functionalities are not allowed.
	*/
	[[gsl::suppress(26439)]] Renderer();
	Renderer(const Renderer&) = delete;
	Renderer& operator=(const Renderer&) = delete;
	Renderer(Renderer&&) = delete;
	Renderer& operator=(Renderer&&) = delete;
	~Renderer();

	/* --------------------------------------------------------------------------------------------------------- */
	/* ---------------------------------------- PUBLIC FUNCTION MEMBERS ---------------------------------------- */
	/* --------------------------------------------------------------------------------------------------------- */

	/**
	Updates the uniform buffer for the object being rendered

	@TODO: Reformat this function out of the renderer.

	@see m_uniform_buffer
	*/
	auto updateRotateTestUniformBuffer() ->void;

	/**
	Sets up the beggining of a frame. Setting up the recording of a
	one time command buffer to submit to the rendering queue.
	*/
	auto beginFrame() -> void;

	/**
	Finishes the rendering stages and submits all the necessary information
	to the graphics card for rendering.
	*/
	auto endFrame() -> void;

	/**
	Returns true when we should close the application as far as the render manager
	is concerned. Currently it deals with the user closing the window itself.

	@return true if the window has been closed or we should end the application.
	*/
	auto shouldClose() const noexcept -> bool;

private:

	/* ---------------------------------------------------------------------------------------------------------- */
	/* ---------------------------------------- PRIVATE FUNCTION MEMBERS ---------------------------------------- */
	/* ---------------------------------------------------------------------------------------------------------- */

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
	auto physicalDeviceSuitability(
		const VkPhysicalDevice& device
	) const noexcept->std::tuple<bool, int>;

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
	auto findQueueFamilies(
		const VkPhysicalDevice& physical_device,
		PrintOptions print_options
	) const->QueueFamilyIndices;

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
	[[gsl::suppress(bounds.3)]]
	auto checkDeviceExtensionSupport(
		const VkPhysicalDevice& device
	) const -> bool;

	/**
	Checks and retrieves information about the swap chain capabilities of a physical device.
	Fills up said information in the SwapChainSupportDetails struct

	@see SwapChainSupportDetails
	@param The physical device to check for swap chain support
	@return A SwapChainSupportDetails filled with swap chain support information
	*/
	auto querySwapChainSupport(
		const VkPhysicalDevice& device
	) const->SwapChainSupportDetails;

	/**
	Checks and returns the best available surface chain format from the provided ones
	prioritizing the ones that fit us best.

	@param A vector with the surface formats to check
	@return The best surface format for our application
	*/
	auto pickSurfaceChainFormat(
		const std::vector<VkSurfaceFormatKHR>& available_formats
	) const->VkSurfaceFormatKHR;

	/**
	Checks and returns the best available present mode from the provided ones
	prioritizing the ones that fit us best.

	@param A vector with the present modes to check
	@return The best present mode for our application
	*/
	auto pickSurfacePresentMode(
		const std::vector<VkPresentModeKHR>& available_modes
	)  const noexcept->VkPresentModeKHR;

	/**
	Checks and returns the best available extent for the images in the
	swap chain based on its capabilities.

	@param The capabilities of the surface
	@return The best possible extent (width, height) for our application
	*/
	auto pickSwapExtent(
		const VkSurfaceCapabilitiesKHR& capabilities
	)  const noexcept->VkExtent2D;

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
	auto createSwapChainImageViews() -> void;

	/**
	Creates the render pass that will be used to then create
	the graphics pipeline.

	@see m_render_pass
	*/
	auto createRenderPass() -> void;

	/**
	Creates the descriptor set layout that we will set in
	the pipeline.

	@see m_descriptor_set_layout;
	*/
	auto createDescriptorSetLayout() -> void;

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
	Helper function that creates a vulkan image in a general way

	@param Width of the image
	@param Height of the image
	@param Vulkan Format of the image
	@param Tiling Options for the image
	@param Usage flags that the image will need
	@param Usage type for allocation
	@param Flags necessary for allocation
	@param The image to be populated
	@param The sharing mode for the image (Concurrent or exclusive)
	@param The family indices when sharing mode is concurrent (or nullptr otherwise)
	@param The sample count for the image, 1 sample by default (used on images meant for multisampling)
	*/
	auto createImage(
		uint width,
		uint height,
		VkFormat format,
		VkImageTiling tiling,
		VkImageUsageFlags flags,
		VmaMemoryUsage allocation_usage,
		VmaAllocationCreateFlags allocation_flags,
		AllocatedImage& image,
		VkSharingMode sharing_mode,
		const std::vector<uint>* queue_family_indices,
		short samples = 1
	) -> void;

	/**
	Destroys the image provided and frees its memory

	@param The image to destroy
	*/
	auto destroyImage(AllocatedImage& image) noexcept -> void;

	/**
	Creates the necessary resources for the implementation
	of a depth buffer

	@ m_depth_image
	@ m_depth_image_view
	*/
	auto createDepthResources() -> void;

	/**
	Creates a texture image with the data loaded from a file

	@param the path of th texture
	@return The allocated image with the texture
	*/
	auto createTextureImage(std::string path) -> AllocatedImage;

	/**
	Creates a texture image view into the texture image

	@param The image to create the view from
	@return An image view for the provided image.
	*/
	auto createTextureImageView(AllocatedImage image) -> VkImageView;

	/**
	Loads the scene with the provided object (.obj) and texture paths

	@param The path to the .obj model
	@param The path to the texture for the model
	@see m_scene
	*/
	auto loadScene(std::string object_path, std::string texture_path) -> void;

	/**
	Creates a sampler to sample the textures
	used in the rendering phase.

	@see m_texture_sampler
	*/
	auto createTextureSampler() -> void;

	/**
	Helper function that creates a vulkan buffer in a general way.

	@param The size of the memory to be allocated
	@param The usage flags necessary for the memory we are allocating
	@param The allocation usage that the buffer will have
	@param The flags for the allocator to use during creating of the buffer.
	@param The handle to the buffer to create
	@param The sharing mode of the buffer (VK_SHARING_MODE_(EXCLUSIVE/CONCURRENT))
	@param The family indices of the queues this buffer will be shared between if CONCURRENT, nullptr otherwise
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
		const std::vector<uint>* queue_family_indices
	) -> void;

	/**
	Destroys the buffer provided and frees its memory

	@param The Buffer to destroy
	*/
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
	Creates the uniform buffer that will hold the object data to render.

	@see m_uniform_buffer
	*/
	auto createUniformBuffer() -> void;

	/**
	Creates the descriptor pool that will hold the descriptors sets that will
	be used during rendering.

	@see m_descriptor_pool
	*/
	auto createDescriptorPool() -> void;

	/**
	Creates the descriptor set that will hold the descriptors
	used during rendering.

	@see m_descriptor_set
	*/
	auto createDescriptorSet() -> void;

	/**
	Helper function that finds the appropriate format for a depth attachment.

	@return Format that supports usage as a depth attachment
	*/
	auto findDepthFormat()->VkFormat;

	/**
	Checks if a given format has a stencil component in it.

	@param The format to check
	@return True if it has a stencil component, false otherwise
	*/
	auto hasStencilComponent(VkFormat format) const noexcept -> bool;

	/**
	Helper function that finds the post appropriate format for an image given
	the desired features and candidates.

	@param The possible formats to choose from
	@param The tiling options of the image
	@param The features desired
	@return The most appropriate format
	*/
	auto findSupportedFormat(
		const std::vector<VkFormat>& candidates,
		VkImageTiling tiling,
		VkFormatFeatureFlags features
	)->VkFormat;

	/**
	Helper function that calculates the required memory types given the input properties.

	@param Flags that indicate the types of memories that we can consider
	@param Properties of the memory we need
	@param A reference to a boolean that will indicate if we have found an appropriate type
	@return Appropriate flags of the memory we can use.
	*/
	auto findMemoryType(
		uint type_filter,
		VkMemoryPropertyFlags properties,
		VkBool32 *found = nullptr
	)->uint;

	/**
	Copies the contents from one VkBuffer to another.

	@param The source buffer
	@param The destination buffer
	@param The size of the memory to be copied
	*/
	auto copyBuffer(
		VkBuffer src,
		VkBuffer dst,
		VkDeviceSize size
	) noexcept -> void;

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
	Creates the semaphores and fences necessary for synchronization of
	the rendering phase.

	@see m_image_available_semaphores
	@see m_render_finished_semaphores
	@see m_command_buffer_fences
	*/
	auto createSemaphoresAndFences() -> void;


	/**
	Handles the event of resizing the window to set up the appropriate
	rendering parameters accordingly.

	@param Reference to the window that has been resized
	@param New width of the window
	@param New height of the window
	*/
	auto static onWindowsResized(
		GLFWwindow * window,
		int width,
		int heigth) -> void;

	/**
	Creates a single use command buffer and starts recording to it

	@param the type of commands that will be used (graphics also allows transfer)
	@return The command buffer we are recording to
	*/
	auto beginSingleTimeCommands(
		CommandType command_type = CommandType::graphics
	) noexcept->WrappedCommandBuffer;

	/**
	End recording to a particulaar command buffer and submits it to the queue

	@param The command buffer to submit to the queue
	*/
	auto endSingleTimeCommands(
		WrappedCommandBuffer& command_buffer
	) noexcept ->void;

	/**
	Helper function that changes the image layout to a new one

	@param The image to change the layout to
	@param The format of the image
	@param The old layout of the image
	@param The new layout for the image
	*/
	auto changeImageLayout(
		VkImage& image,
		VkFormat format,
		VkImageLayout old_layout,
		VkImageLayout new_layout)->void;

	/**
	Helper function that copies a buffer with image data into
	a vulkan image structure.

	@param The buffer to read the data from
	@param The image to write de data into
	@param Width of the image
	@param Height of the image
	*/
	auto copyBufferToImage(
		VkBuffer buffer,
		VkImage image,
		uint width,
		uint heigth) noexcept -> void;

	/**
	Helped function that creates an image view to the
	provided image.

	@param The image to create a view from
	@param The format of the view
	@param The aspect mast to create the image view regarding its use
	@return An image view into the provided image
	*/
	auto createImageView(
		VkImage image,
		VkFormat format,
		VkImageAspectFlags aspect_flags
	)->VkImageView;

	/**
	Helper function that calculates the required flags to
	set up an especific amount of samples.

	@param The number of samples required (power of 2 up to 64)
	@return The required vulkan flag for the number of samples
	*/
	auto getSampleBits(short samples)->VkSampleCountFlagBits;

	/**
	Creates a render target for multisampling. This includes, the image,
	its memory and a view to it.

	@param Width of the render target
	@param Height of the render target
	@param Desired format of the render target
	@param The usage flags for the render target
	@param The aspect flags for the image view of the render target
	@return The render target struct with all the relevant info
	*/
	auto createMultisampleRenderTarget(
		uint width,
		uint height,
		VkFormat format,
		VkImageUsageFlags usage,
		VkImageAspectFlags aspect_mask)->WrappedRenderTarget;


	/* ---------------------------------------------------------------------------------------------- */
	/* ---------------------------------------- DATA MEMBERS ---------------------------------------- */
	/* ---------------------------------------------------------------------------------------------- */

	RenderConfiguration config{};

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

	WrappedRenderTarget m_render_target{};

	WrappedRenderTarget m_depth_target{};

	VkFormat m_depth_format{};

	VkSwapchainKHR m_swap_chain{};

	uint m_current_swapchain_buffer{};

	std::vector<VkImage> m_swap_chain_images{};

	VkFormat m_swap_chain_image_format{};

	VkExtent2D m_swap_chain_extent{};

	std::vector<VkImageView> m_swap_chain_image_views{};

	VkRenderPass m_render_pass{};

	VkPipelineLayout m_pipeline_layout{};

	SimpleObjScene m_scene{};

	VkDescriptorSetLayout m_descriptor_set_layout{};

	VkPipeline m_pipeline{};

	std::vector<VkFramebuffer> m_swap_chain_framebuffers{};

	VkCommandPool m_graphics_command_pool{};

	VkCommandPool m_transfer_command_pool{};

	AllocatedBuffer m_vertex_buffer{};

	AllocatedBuffer m_index_buffer{};

	AllocatedBuffer m_uniform_buffer{};

	AllocatedImage m_depth_image{};

	VkImageView m_depth_image_view{};

	VkSampler m_texture_sampler{};

	VkDescriptorPool m_descriptor_pool{};

	VkDescriptorSet m_descriptor_set{};

	std::vector<VkCommandBuffer> m_command_buffers{};

	uint m_current_command_buffer{};

	std::vector<bool> m_command_buffer_submitted{};

	std::vector<VkSemaphore> m_image_available_semaphores{};

	std::vector<VkSemaphore> m_render_finished_semaphores{};

	std::vector<VkFence> m_command_buffer_fences{};

};

