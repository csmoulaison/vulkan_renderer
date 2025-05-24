#define VK_DEBUG 1

#define PIPELINES_COUNT   1
#define MESHES_COUNT      1
#define SWAP_IMAGES_COUNT 4
#define INSTANCES_COUNT   4

VkResult vk_verify_macro_result;
#define vk_verify(FUNC) vk_verify_macro_result = FUNC;\
						if(vk_verify_macro_result != VK_SUCCESS)\
{\
							printf("vk_verify error (%i)\n", vk_verify_macro_result);\
							panic();\
						}\

typedef struct
{
	VkDeviceMemory*      memory;
	VkMemoryRequirements requirements;
	uint32_t             type_mask;
} VulkanAllocationConfig;

typedef struct
{
	VkImage         image;
	VkImageView     image_view;
	VkDeviceMemory  device_memory;
} VulkanAllocatedImage;

typedef struct
{
	VulkanAllocatedImage  image;
	VkExtent2D            extent;
	VkFormat              format;
	VkSampleCountFlagBits sample_count;
	VkUsageMask           usage_mask;
} VulkanAllocatedImageConfig;

typedef struct
{
	VkPipeline            pipeline;
	VkPipelineLayout      pipeline_layout;

	VkDescriptorSetLayout descriptor_set_layout; // CONSIDER - not used outside of pipeline creation
	VkDescriptorPool      descriptor_pool;       // < also not used outside of pipeline creation
	VkDescriptorSet       descriptor_set;
} VulkanPipelineState;

typedef struct 
{
} VulkanMeshData;

typedef struct
{
	VkBuffer       buffer;
	VkDeviceMemory memory;
} VulkanMemoryBuffer;

typedef struct 
{
	VkInstance           instance;

	VkDevice             device;
	VkPhysicalDevice     physical_device;

	VkQueue              graphics_queue;
	VkQueue              present_queue;

	VkSurfaceKHR         surface;
	VkSurfaceFormatKHR   surface_format;

	VkSwapchainKHR       swapchain;
	VkExtent2D           swapchain_extent;
	VkImageView          swapchain_image_views[SWAP_IMAGES_COUNT];
	VkImage              swapchain_images     [SWAP_IMAGES_COUNT];

	VkSemaphore          image_available_semaphore;
	VkSemaphore          semaphore_render_finished;

	VkCommandPool        command_pool;
	VkCommandBuffer      command_buffer;

	VulkanAllocatedImage render_image;
	VulkanAllocatedImage depth_image;

	VulkanPipelineState  pipelines[PIPELINES_COUNT];

	VulkanMeshData       mesh_datas[MESHES_COUNT];
	VulkanMemoryBuffer   mesh_data_buffer;

	// CONSIDER - Ought this be part of VulkanMeshData?
	VulkanAllocatedImage texture_images[1];

	VulkanMemoryBuffer   ubo_device_buffer;
	// CONSIDER - Does this need to be void*? Why not just do the struct?
	void*                ubo_mapped_memory;

	// Used in swapchain initialization.
	float                 device_max_sampler_anisotropy;
	VkSampleCountFlagBits device_framebuffer_sample_counts;
} VulkanRenderer;

typedef struct
{
} VulkanInitContext;

typedef struct
{
	struct
	{
		alignas(16) Mat4 view;
		alignas(16) Vec4 projection;

		alignas(16) Vec3 clear_color;
	} global;

	struct
	{
		alignas(16) Mat4 models[INSTANCES_COUNT];
	} meshes;
} VulkanUbo;

typedef struct
{
	VkResult(*create_surface_callback)(VulkanRenderer* renderer, void* context);
	void*   context;
	char**  window_extensions;
	uint8_t window_extensions_len;
} VulkanPlatform;

void vulkan_allocate_memory(
	VulkanRenderer*        renderer, 
	VulkanAllocationConfig config)
{
	VkPhysicalDeviceMemoryProperties properties;
	vkGetPhysicalDeviceMemoryProperties(renderer->physical_device, &properties);

	uint32_t suitable_type_index = UINT32_MAX;
	for(uint32_t type_index = 0; type_index < properties.memoryTypeCount; type_index++)
	{
		if((config.requirements.memoryTypeBits & (1 << type_index))
			&& (properties.memoryTypes[type_index].propertyFlags & config.type_mask) == config.type_mask)
		{
			suitable_type_index = type_index;
			break;
		}
	}	
	if(suitable_type_index == UINT32_MAX)
	{
		panic();
	}

	VkMemoryAllocateInfo allocate_info = 
	{
		.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.pNext           = 0,
		.allocationSize  = config.requirements.size,
		.memoryTypeIndex = suitable_type_index
	};
	vk_verify(vkAllocateMemory(renderer->device, &allocate_info, 0, config.memory));
}

void vulkan_allocate_buffer(
	VulkanRenderer* renderer,

void vulkan_initialize_swapchain(VulkanRenderer* renderer, bool recreate)
{
	// This function is being called in one of two situations:
	// 1. During program initialization.
	// 2. The platform surface has changed and swapchain related information is no longer valid.
	if(recreate)
	{
		vkDeviceWaitIdle(renderer->device);

		for(uint8_t image_index; image_index < renderer->swapchain_images_len; image_index++)
		{
			vkDestroyImageView(renderer->device, renderer->swapchain_image_views[image_index], 0);
		}

		vkDestroySwapchainKHR(renderer->device, renderer->swapchain, 0);

		vkDestroyImage(renderer->device, renderer->render_image, 0);
		vkDestroyImageView(renderer->device, renderer->render_image_view, 0);
		vkFreeMemory(renderer->device, vk->render_image_memory, 0);

		vkDestroySemaphore(renderer->device, renderer->semaphore_image_available, 0);
		vkDestroySemaphore(renderer->device, renderer->semaphore_render_finished, 0);
	}

	// Query surface capabilities to give us the following info:
	// - The transform of the surface (believe the position on screen, roughly speaking?)
	// - Swapchain width and height
	// - Swapchain image count
	VkSurfaceCapabilitiesKHR surface_capabilities;
	vk_verify(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
		renderer->physical_device, 
		renderer->surface, 
		&surface_capabilities);

	VkSurfaceTransformFlagBitsKHR surface_pre_transform;

	renderer->swapchain_extent.width = surface_capabilities.maxImageExtent.width;
	renderer->swapchain_extent.width = surface_capabilities.maxImageExtent.height;

	// CONSIDER - Not sure exactly what this logic is for. Look at it a little closer.
	uint32_t swapchain_image_count = abilities.minImageCount + 1;
	if(abilities.maxImageCount > 0 && image_count > abilities.maxImageCount) 
	{	
		swapchain_image_count = abilities.maxImageCount;
	}

	// Choose the best surface format.
	uint32_t formats_len
	vkGetPhysicalSurfaceFormatsKHR(renderer->physical_device, renderer->surface, &formats_len, 0);
	if(formats_len == 0)
	{
		panic();
	}

	VkSurfaceFormatKHR formats[formats_len];
	vkGetPhysicalDeviceSurfaceFormatsKHR(renderer->physical_device, renderer->surface, &formats_len, formats);

	vulkan->surface_format = formats[0];
	for(uint32_t format_index = 0; format_index < formats_len; format_index++)
	{
		if(formats[format_index].format == VK_FORMAT_B8G8R8A8_SRGB 
			&& formats[format_index].colorSpace == VK_COLOR_SPACE_SRGB_NONELINEAR_KHR)
		{
			renderer->surface_format = formats[format_index];
			break;
		}
	}

	// Choose presentation mode, defaulting to VK_PRESENT_MODE_FIFO_KHR, which is guaranteed to
	// be supported by the spec.
	VkPresentModeKHR present_mode = VK_PRESENT_MODE_FIFO_KHR;

	uint32_t modes_len;
	vk_verify(vkGetPhysicalDeviceSurfacePresentModesKHR(renderer->physical_device, renderer->surface, &modes_len, 0));

	VkPresentModeKHR modes[modes_len];
	vk_verify(vkGetPhysicalDeviceSurfacePresentModesKHR(renderer->physical_device, renderer->surface, &modes_len, modes));

	for(uint32_t mode_index = 0; mode_index < modes_len; mode_index++)
	{
		if(modes[mode_index] == VK_PRESENT_MODE_MAILBOX_KHR)
		{
			present_mode = modes[i];
			break;
		}
	}

#if VK_IMMEDIATE
	present_mode = CK_PRESENT_MODE_IMMEDIATE_KHR;
#endif

	// Create the swapchain.
	VkSwapchainCreateInfoKHR swapchain_create_info = 
	{
		.sType                 = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
		.pNext                 = 0,
		.flags                 = 0, 
		.surface               = renderer->surface,
		.minImageCount         = swapchain_image_count, 
		.imageFormat           = renderer->surface_format.format,
		.imageColorSpace       = renderer->surface_format.colorSpace,
		.imageExtent           = renderer->swapchain_extent,
		.imageArrayLayers      = 1,
		.imageUsage            = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
		.imageSharingMode      = VK_SHARING_MODE_EXCLUSIVE,
		.queueFamilyIndexCount = 0,
		.pQueueFamilyIndices   = 0,
		.preTransform          = surface_pre_transform,
		.compositeAlpha        = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
		.presentMode           = present_mode,
		.clipped               = VK_TRUE,
		.oldSwapchain          = VK_NULL_HANDLE
	}

	vk_verify(vkCreateSwapchainKHR(renderer->device, &swapchain_create_info, 0, &renderer->swapchain));

	// Get references to the swapchain images.
	vk_verify(vkGetSwapchainImagesKHR(
		renderer->device, 
		renderer->swapchain, 
		&renderer->swapchain_images_len, 
		0));
	vk_verify(vkGetSwapchainImagesKHR(
		renderer->device, 
		renderer->swapchain, 
		&renderer->swapchain_images_len, 
		renderer->swapchain_images));

	// Allocate resources for render and depth images.
	//
	// CONSIDER - The following description is a bit lacking in understanding.
	// The render image is used in the rendering pipeline, and is transferred to the swapchain.
	vulkan_allocate_image(
		renderer, 
		(VulkanAllocatedImageConfig)
		{
			.image        = &renderer->render_image,
			.extent       = renderer->swapchain_extent,
			.format       = renderer->surface_format.format,
			.sample_count = renderer->render_sample_count,
			.usage_mask   = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT
		});

	vulkan_allocate_image(
		renderer, 
		(VulkanAllocatedImageConfig)
		{
			.image        = &renderer->depth_image,
			.extent       = renderer->swapchain_extent,
			.format       = DEPTH_ATTACHMENT_FORMAT,
			.sample_count = renderer->render_sample_count,
			.usage_mask   = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
		});

	for(uint32_t image_index = 0; image_index < renderer->swapchain_images_len; image_index++)
	{
		vulkan_create_image_view(
			renderer,
			(VkImageViewConfig)
			{
				.view        = &renderer->swapchain_image_views[image_index],
				.image       = renderer->swapchain_images[image_index],
				.format      = renderer->surface_format.format,
				.aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT
			});
	}

	// Create synchonization primitives.
	//
	// This needs to happen every time the swapchain is recreated because otherwise semaphores
	// could be in a now invalid state(terminology?).
	VkSemaphoreCreateInfo semaphore_create_info =
	{
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
		.pNext = 0,
		.flags = 0
	}

	vk_verify(vkCreateSemaphore(renderer->device, &semaphore_create_info, 0, &renderer->semaphore_image_available));
	vk_verify(vkCreateSemaphore(renderer->device, &semaphore_create_info, 0, &renderer->semaphore_render_finished));
}

// A broad overview of the initialization of our Vulkan backend.
// 1. Create VkInstance, dependant on:
//      Verify api version as supported by the implementation.
//      Define application info.
//      Define instance extensions. (platform-window specific extensions, and debug extensions)
//      Define layers.              (either none or optionally the Khronos validation layers)
// 2. 
// 3. Get the platform specific surface using a callback function passed from the platform.
void vulkan_initialize_renderer(VulkanRenderer* renderer, VulkanPlatform* platform)
{
	VulkanInitContext init_context;
	(void)init_context;

	// Verify that our desired Vulkan version is supported by the implementation.
	uint32_t desired_api_version = VK_API_VERSION_1_3;
	uint32_t instance_api_version;
	vk_verify(vkEnumerateInstanceVersion(&instance_api_version));
	if(instance_api_version < desired_api_version)
	{
		panic();
	}

	// Define instance extensions and layers, padding both arrays by 1 for possible debug extension.
	uint32_t instance_extensions_len = platform->window_extensions_len;
	const char* instance_extensions[instance_extensions_len + 1];
	for(uint8_t i = 0; i < platform->window_extensions_len; i++)
	{
		instance_extensions[i] = platform->window_extensions[i];
	}

	uint32_t layers_len = 0;
	const char* layers[layers_len + 1];

#if VK_DEBUG
	instance_extensions[instance_extensions_len] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
	instance_extensions_len++;

	// LATER - Do we have a macro for the validation layer string?
	layers[layers_len] = "VK_LAYER_KHRONOS_validation";
	layers_len++;
#endif

	// Create the instance.
	VkInstanceCreateInfo instance_create_info =
	{
		.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
		.pNext                   = 0,
		.flags                   = 0,
		.pApplicationInfo        = &(VkApplicationInfo)
		{
			.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO,
	    		.pNext              = 0,
	    		.pApplicationName   = PROGRAM_NAME,
	    		.applicationVersion = 1,
	    		.pEngineName        = 0,
	    		.engineVersion      = 0,
	    		.apiVersion         = desired_api_version
		},
		.enabledLayerCount       = layers_len,
		.ppEnabledLayerNames     = layers,
		.enabledExtensionCount   = instance_extensions_len,
		.ppEnabledExtensionNames = instance_extensions
	};
	vk_verify(vkCreateInstance(&instance_create_info, 0, &renderer->instance));

	// Get surface from the platform specific callback.
	vk_verify(platform->create_surface_callback(renderer, platform->context));

	// Query all physical devices.
	uint32_t physical_devices_len;
	vk_verify(vkEnumeratePhysicalDevices(renderer->instance, &physical_devices_len, 0));

	VkPhysicalDevice physical_devices[physical_devices_len];
	vk_verify(vkEnumeratePhysicalDevices(renderer->instance, &physical_devices_len, physical_devices));

	// Pick the best device for our needs.
	typedef struct
	{
		VkPhysicalDevice   handle;
		uint8_t            score;
		VkSampleCountFlags framebuffer_color_sample_counts;
		uint32_t           graphics_family_index;
		uint32_t           present_family_index;
		float              max_sampler_anisotropy;
	} PhysicalDeviceCandidate;

	PhysicalDeviceCandidate best_physical_device = {};

#define REQUIRED_EXTENSIONS_COUNT 2
	const char* required_extensions[REQUIRED_EXTENSIONS_COUNT] = 
	{
		VK_KHR_SWAPCHAIN_EXTENSION_NAME,
		VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME
	};

	for(uint32_t device_index = 0; device_index < physical_devices_len; device_index++)
	{
		PhysicalDeviceCandidate candidate = {};
		candidate.handle = physical_devices[device_index];

		// Criteria: queue families
		// - MUST have a graphics and present queue family.
		// - +4 for those being the same index.
		uint32_t queue_families_len;
		vkGetPhysicalDeviceQueueFamilyProperties(candidate.handle, &queue_families_len, 0);
		VkQueueFamilyProperties queue_family_properties[queue_families_len];
		vkGetPhysicalDeviceQueueFamilyProperties(
			candidate.handle, 
			&queue_families_len, 
			queue_family_properties);

		candidate.graphics_family_index = UINT32_MAX;
		candidate.present_family_index  = UINT32_MAX;
		for(uint32_t queue_index = 0; queue_index < queue_families_len; queue_index++)
		{
			if(queue_family_properties[queue_index].queueFlags & VK_QUEUE_GRAPHICS_BIT)
			{
				candidate.graphics_family_index = queue_index;
			}

			VkBool32 present_support;
			vkGetPhysicalDeviceSurfaceSupportKHR(candidate.handle, queue_index, renderer->surface, &present_support);
			if(present_support)
			{
				candidate.present_family_index = queue_index;
			}

			if(candidate.graphics_family_index == queue_index && candidate.present_family_index == queue_index)
			{
				candidate.score += 4;
				break;
			}
		}
		if(candidate.graphics_family_index == UINT32_MAX || candidate.present_family_index == UINT32_MAX)
		{
			continue;
		}

		// Criteria: extensions
		// - MUST have KHR_SWAPCHAIN and KHR_DYNAMIC_RENDERING extensions
		uint32_t extensions_len;
		vkEnumerateDeviceExtensionProperties(candidate.handle, 0, &extensions_len, 0);
		VkExtensionProperties extensions[extensions_len];
		vkEnumerateDeviceExtensionProperties(candidate.handle, 0, &extensions_len, extensions);

		bool found_extensions[REQUIRED_EXTENSIONS_COUNT] = {};
		for(uint32_t extension_index = 0; extension_index < extensions_len; extension_index++)
		{
			for(uint8_t required_index = 0; required_index < REQUIRED_EXTENSIONS_COUNT; required_index++)
			{
				if(strcmp(extensions[extension_index].extensionName, required_extensions[required_index]) == 0)
				{
					found_extensions[required_index] = true;
				}
			}
		}
		bool all_extensions_found = true;
		for(uint8_t required_index = 0; required_index < REQUIRED_EXTENSIONS_COUNT; required_index++)
		{
			if(!found_extensions[required_index])
			{
				all_extensions_found = false;
			}
		}
		if(!all_extensions_found)
		{
			continue;
		}

		// Criteria: device features
		// - Features MUST include samplerAnisotropy.
		VkPhysicalDeviceFeatures device_features;
		vkGetPhysicalDeviceFeatures(candidate.handle, &device_features);
		if(device_features.samplerAnisotropy != VK_TRUE)
		{
			continue;
		}

		// Criteria: properties
		// - +1 points for each framebufferColorSampleCounts flag above VK_SAMPLE_COUNT_1_BIT
		VkPhysicalDeviceProperties properties;
		vkGetPhysicalDeviceProperties(candidate.handle, &properties);

		candidate.framebuffer_color_sample_counts = properties.limits.framebufferColorSampleCounts;
		VkSampleCountFlags sample_count_options[7] = 
		{
			VK_SAMPLE_COUNT_64_BIT,
			VK_SAMPLE_COUNT_32_BIT, 
			VK_SAMPLE_COUNT_16_BIT, 
			VK_SAMPLE_COUNT_8_BIT, 
			VK_SAMPLE_COUNT_4_BIT, 
			VK_SAMPLE_COUNT_2_BIT
		};
		for(uint8_t samples_index = 0; samples_index < 7; samples_index++)
		{
			if(candidate.framebuffer_color_sample_counts & sample_count_options[samples_index])
			{
				candidate.score += 7 - samples_index;
				break;
			}
		}

		// While we are at it, store our max_sampler_anisotropy value.
		// CONSIDER - Include this in device score?
		candidate.max_sampler_anisotropy = properties.limits.maxSamplerAnisotropy;

		if(candidate.score > best_physical_device.score)
		{
			best_physical_device = candidate;
		}
	}
	if(best_physical_device.score == 0)
	{
		panic();
	}

	renderer->physical_device = best_physical_device.handle;
	init_context.device_max_sampler_anisotropy    = best_physical_device.max_sampler_anisotropy;
	init_context.device_framebuffer_sample_counts = best_physical_device.framebuffer_color_sample_counts;

	// Create logical device queues.
	uint32_t queue_family_indices[2] = 
	{ 
		best_physical_device.graphics_family_index,
		best_physical_device.present_family_index
	};

	uint8_t queue_create_infos_len = 2;
	// Present and graphics could be in the same queue.
	if(best_physical_device.graphics_family_index == best_physical_device.present_family_index)
	{
		queue_create_infos_len = 1;
	}

	VkDeviceQueueCreateInfo queue_create_infos[queue_create_infos_len];

	float queue_priority = 1;
	for(uint8_t queue_index = 0; queue_index < queue_create_infos_len; queue_index++)
	{
		queue_create_infos[queue_index] = (VkDeviceQueueCreateInfo)
		{
			.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
			.pNext            = 0,
			.flags            = 0,
			.queueFamilyIndex = queue_family_indices[queue_index],
			.queueCount       = 1,
			.pQueuePriorities = &queue_priority,
		};
	}

	// Get physical device features for device creation.
	VkPhysicalDeviceFeatures2 device_features_2 =
	{
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
		.pNext = &(VkPhysicalDeviceDynamicRenderingFeatures)
		{
			.sType            = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES,
			.pNext            = 0,
			.dynamicRendering = VK_TRUE
		},
		.features = {}
	};
	vkGetPhysicalDeviceFeatures2(renderer->physical_device, &device_features_2);

	VkDeviceCreateInfo device_create_info =
	{
		.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
		.pNext                   = &device_features_2,
		.flags                   = 0,
		.queueCreateInfoCount    = queue_create_infos_len,
		.pQueueCreateInfos       = queue_create_infos,
		.enabledLayerCount       = 0, // Deprecated
		.ppEnabledLayerNames     = 0, // Deprecated
		.enabledExtensionCount   = REQUIRED_EXTENSIONS_COUNT,
		.ppEnabledExtensionNames = required_extensions,
		.pEnabledFeatures        = 0
	};
	vk_verify(vkCreateDevice(renderer->physical_device, &device_create_info, 0, &renderer->device));

	vulkan_initialize_swapchain(renderer, false);
}

void vulkan_loop(VulkanRenderer* renderer, RenderList* render_list)
{
}
