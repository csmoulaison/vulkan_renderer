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
	alignas(64) struct
	{
		alignas(16) Mat4 view;
		alignas(16) Vec4 projection;
		alignas(16) Vec3 clear_color;
	} global;

	alignas(64) struct
	{
		alignas(16) Mat4 models[INSTANCES_COUNT];
	} instance;
} VulkanHostMappedData;

typedef struct
{
	Vec3 position;
	Vec3 color;
} VulkanMeshVertex;

typedef struct
{
	void*    vertex_memory;
	void*    index_memory;

	// In bytes
	uint32_t vertex_data_stride;

	uint32_t vertices_len;
	uint32_t indices_len;

	uint32_t vertex_buffer_offset;
	uint32_t index_buffer_offset;
} VulkanMeshData;

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

	VulkanMeshData        mesh_datas[1];
	VulkanMemoryBuffer    mesh_data_memory_buffer;

	// CONSIDER - Ought this be part of VulkanMeshData?
	VulkanAllocatedImage  texture_images[1];

	VulkanMemoryBuffer    host_mapped_buffer;
	// CONSIDER - Does this need to be void*? Why not just do the struct?
	VulkanHostMappedData* host_mapped_data;

	// Used in swapchain initialization.
	// 
	// TODO - Localize to create swapchain function. Surely anything that breaks should be
	// included in swapchain creation?
	float                 device_max_sampler_anisotropy;
	VkSampleCountFlagBits device_framebuffer_sample_counts;
} VulkanRenderer;
