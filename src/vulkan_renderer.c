typedef struct
{
	VkBuffer       buffer;
	VkDeviceMemory memory;
} VulkanMemoryBuffer;

typedef struct
{
	VkImage         image;
	VkImageView     view;
	VkDeviceMemory  memory;
} VulkanAllocatedImage;

typedef struct
{
	VkPipeline            pipeline;
	VkPipelineLayout      layout;

	VkDescriptorSetLayout descriptor_set_layout; // CONSIDER - not used outside of pipeline creation
	VkDescriptorPool      descriptor_pool;       // < also not used outside of pipeline creation
	VkDescriptorSet       descriptor_set;
} VulkanPipeline;

typedef struct
{
	alignas(16) mat4 view;
	alignas(16) mat4 projection;
	alignas(16) Vec3 clear_color;
} VulkanHostMappedGlobal;

typedef struct
{
	alignas(16) mat4 models[INSTANCES_COUNT];
} VulkanHostMappedInstance;

typedef struct
{
	alignas(64) VulkanHostMappedGlobal   global;
	alignas(64) VulkanHostMappedInstance instance;
} VulkanHostMappedData;

typedef struct
{
	Vec3 position;
	Vec2 texture_uv;
} VulkanMeshVertex;

// NOW - this might be good as is, but remember that its been renamed and changed to only include
// data which is used at loop time, as opposed to that needed during initialization.
// 
// This is the final result of loading a mesh, in other words. The only thing we need to do is make
// sure all of this data is actually used at loop time, and if not, only store it transiently during
// initialization (as part of VulkanMeshData, perhaps).
typedef struct
{
	uint32_t vertices_len;
	uint32_t indices_len;

	// TODO - Will be used for when multiple meshes.
	uint32_t vertex_buffer_offset;
	uint32_t index_buffer_offset;
} VulkanAllocatedMesh;

typedef struct 
{
	VkInstance            instance;

	VkDevice              device;
	VkPhysicalDevice      physical_device;

	VkQueue               graphics_queue;
	VkQueue               present_queue;

	VkSurfaceKHR          surface;
	VkSurfaceFormatKHR    surface_format;

	VkSwapchainKHR        swapchain;
	VkExtent2D            swapchain_extent;
	VkImageView           swapchain_image_views[SWAPCHAIN_IMAGES_COUNT];
	VkImage               swapchain_images     [SWAPCHAIN_IMAGES_COUNT];

	VkSemaphore           image_available_semaphore;
	VkSemaphore           render_finished_semaphore;

	VkCommandPool         command_pool;
	VkCommandBuffer       main_command_buffer;

	VulkanAllocatedImage  render_image;
	VulkanAllocatedImage  depth_image;

	VulkanPipeline        pipelines[PIPELINES_COUNT];
	VkSampler             texture_sampler;

	VulkanAllocatedMesh   allocated_meshes[1];
	VulkanMemoryBuffer    mesh_data_memory_buffer;

	// CONSIDER - Ought this be part of VulkanAllocatedMesh?
	VulkanAllocatedImage  texture_images[1];

	VulkanMemoryBuffer    host_mapped_buffer;
	// CONSIDER - Does this need to be void*? Why not just do the struct?
	void*                 host_mapped_data;

	// Used in swapchain initialization.
	// 
	// TODO - Localize to create swapchain function. Surely anything that breaks should be
	// included in swapchain creation?
	float                 device_max_sampler_anisotropy;
	VkSampleCountFlagBits device_framebuffer_sample_counts;
} VulkanRenderer;
