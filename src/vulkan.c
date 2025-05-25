#define VK_DEBUG 1

#define PIPELINES_COUNT        1
#define MESHES_COUNT           1
#define SWAPCHAIN_IMAGES_COUNT 4
#define INSTANCES_COUNT        4

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_BMP
#include "stb/stb_image.h"

#include "vulkan_verify.c"
#include "vulkan_renderer.c"
#include "vulkan_allocate.c"
#include "vulkan_transient_commands.c"
#include "vulkan_image_memory_barrier.c"
#include "vulkan_image_view.c"
#include "vulkan_pipeline.c"

typedef struct
{
	VkResult(*create_surface_callback)(VulkanRenderer* renderer, void* context);
	void*   context;
	char**  window_extensions;
	uint8_t window_extensions_len;
} VulkanPlatform;

void vulkan_initialize_swapchain(VulkanRenderer* renderer, bool recreate)
{
	// This function is being called in one of two situations:
	// 1. During program initialization.
	// 2. The platform surface has changed and swapchain related information is no longer valid.
	if(recreate)
	{
		vkDeviceWaitIdle(renderer->device);

		for(uint8_t image_index = 0; image_index < SWAPCHAIN_IMAGES_COUNT; image_index++)
		{
			vkDestroyImageView(renderer->device, renderer->swapchain_image_views[image_index], 0);
		}

		vkDestroySwapchainKHR(renderer->device, renderer->swapchain, 0);

		vkDestroyImage(renderer->device, renderer->render_image.image, 0);
		vkDestroyImageView(renderer->device, renderer->render_image.view, 0);
		vkFreeMemory(renderer->device, renderer->render_image.memory, 0);

		vkDestroySemaphore(renderer->device, renderer->image_available_semaphore, 0);
		vkDestroySemaphore(renderer->device, renderer->render_finished_semaphore, 0);
	}

	// Query surface capabilities to give us the following info:
	// - The transform of the surface (believe the position on screen, roughly speaking?)
	// - Swapchain width and height
	// - Swapchain image count
	VkSurfaceCapabilitiesKHR surface_capabilities;
	vk_verify(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
		renderer->physical_device, 
		renderer->surface, 
		&surface_capabilities));

	VkSurfaceTransformFlagBitsKHR surface_pre_transform = surface_capabilities.currentTransform;

	renderer->swapchain_extent.width  = surface_capabilities.maxImageExtent.width;
	renderer->swapchain_extent.height = surface_capabilities.maxImageExtent.height;

	// CONSIDER - Not sure exactly what this logic is for. Look at it a little closer.
	uint32_t swapchain_image_count = surface_capabilities.minImageCount + 1;
	if(surface_capabilities.maxImageCount > 0 && swapchain_image_count > surface_capabilities.maxImageCount) 
	{	
		swapchain_image_count = surface_capabilities.maxImageCount;
	}

	// Choose the best surface format.
	uint32_t formats_len;
	vkGetPhysicalDeviceSurfaceFormatsKHR(renderer->physical_device, renderer->surface, &formats_len, 0);
	if(formats_len == 0)
	{
		panic();
	}

	VkSurfaceFormatKHR formats[formats_len];
	vkGetPhysicalDeviceSurfaceFormatsKHR(renderer->physical_device, renderer->surface, &formats_len, formats);

	renderer->surface_format = formats[0];
	for(uint32_t format_index = 0; format_index < formats_len; format_index++)
	{
		if(formats[format_index].format == VK_FORMAT_B8G8R8A8_SRGB 
			&& formats[format_index].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
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
			present_mode = modes[mode_index];
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
	};

	vk_verify(vkCreateSwapchainKHR(renderer->device, &swapchain_create_info, 0, &renderer->swapchain));

	// Get references to the swapchain images.
	uint32_t swapchain_images_count = SWAPCHAIN_IMAGES_COUNT;
	vk_verify(vkGetSwapchainImagesKHR(
		renderer->device, 
		renderer->swapchain, 
		&swapchain_images_count, 
		0));
	vk_verify(vkGetSwapchainImagesKHR(
		renderer->device, 
		renderer->swapchain, 
		&swapchain_images_count, 
		renderer->swapchain_images));

	// Allocate resources for render and depth images.
	//
	// CONSIDER - The following description is a bit lacking in understanding.
	// The render image is used in the rendering pipeline, and is transferred to the swapchain.
	vulkan_allocate_image(
		renderer,
		&renderer->render_image,
		renderer->swapchain_extent,
		renderer->surface_format.format,
		renderer->device_framebuffer_sample_counts,
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);

	vulkan_allocate_image(
		renderer, 
		&renderer->depth_image,
		renderer->swapchain_extent,
		VK_FORMAT_D32_SFLOAT,
		renderer->device_framebuffer_sample_counts,
		VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);

	vulkan_create_image_view(
		renderer,
		&renderer->render_image.image,
		&renderer->render_image.view,
		renderer->surface_format.format,
		VK_IMAGE_ASPECT_COLOR_BIT);

	vulkan_create_image_view(
		renderer,
		&renderer->depth_image.image,
		&renderer->depth_image.view,
		VK_FORMAT_D32_SFLOAT,
		VK_IMAGE_ASPECT_DEPTH_BIT);

	for(uint32_t image_index = 0; image_index < SWAPCHAIN_IMAGES_COUNT; image_index++)
	{
		vulkan_create_image_view(
			renderer,
			&renderer->swapchain_images[image_index],
			&renderer->swapchain_image_views[image_index],
			renderer->surface_format.format,
			VK_IMAGE_ASPECT_COLOR_BIT);
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
	};

	vk_verify(vkCreateSemaphore(renderer->device, &semaphore_create_info, 0, &renderer->image_available_semaphore));
	vk_verify(vkCreateSemaphore(renderer->device, &semaphore_create_info, 0, &renderer->render_finished_semaphore));
}

void vulkan_initialize_renderer(VulkanRenderer* renderer, VulkanPlatform* platform)
{
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

	// TODO - Do we have a macro for the validation layer string?
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
				candidate.framebuffer_color_sample_counts = sample_count_options[samples_index];
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
	renderer->device_max_sampler_anisotropy    = best_physical_device.max_sampler_anisotropy;
	renderer->device_framebuffer_sample_counts = best_physical_device.framebuffer_color_sample_counts;

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

	vkGetDeviceQueue(renderer->device, best_physical_device.graphics_family_index, 0, &renderer->graphics_queue);
	vkGetDeviceQueue(renderer->device, best_physical_device.present_family_index, 0, &renderer->present_queue);

	// Initially initialize swapchain.
	vulkan_initialize_swapchain(renderer, false);

	// Allocate host mapped memory buffer.
	VkDeviceSize host_mapped_memory_size = sizeof(VulkanHostMappedData);

	vulkan_allocate_memory_buffer(
		renderer,
		&renderer->host_mapped_buffer,
		host_mapped_memory_size,
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	vkMapMemory(
		renderer->device, 
		renderer->host_mapped_buffer.memory, 
		0, 
		host_mapped_memory_size, 
		0, 
		(void*)&renderer->host_mapped_data);

	VulkanDescriptorSetConfig descriptor_set_configs[3] =
	{
		{
			.type                  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			.shader_stage_flags    = VK_SHADER_STAGE_VERTEX_BIT,
			.offset_in_host_memory = offsetof(VulkanHostMappedData, global),
			.range_in_host_memory  = sizeof(((VulkanHostMappedData){}).global)
		},
		{
			.type                  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
			.shader_stage_flags    = VK_SHADER_STAGE_VERTEX_BIT,
			.offset_in_host_memory = offsetof(VulkanHostMappedData, instance),
			.range_in_host_memory  = sizeof(((VulkanHostMappedData){}).instance)
		},
		{
			.type                  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			.shader_stage_flags    = VK_SHADER_STAGE_FRAGMENT_BIT,
			.offset_in_host_memory = 0,
			.range_in_host_memory  = 0
		}
	};

	VulkanVertexInputAttributeConfig vertex_input_attribute_configs[2] =
	{
		{
			.format                = VK_FORMAT_R32G32B32_SFLOAT,
			.offset_in_vertex_data = offsetof(VulkanMeshVertex, position)
		},
		{
			.format                = VK_FORMAT_R32G32B32_SFLOAT,
			.offset_in_vertex_data = offsetof(VulkanMeshVertex, color)
		}
	};

	// Create graphics pipeline.
	// TODO - This is dependant on only having one pipeline.
	vulkan_create_graphics_pipeline(
		renderer,
		&renderer->pipelines[0],
		"shaders/world_vertex.spv",
		"shaders/world_fragment.spv",
		descriptor_set_configs,
		2,
		vertex_input_attribute_configs,
		2,
		sizeof(VulkanMeshVertex));

	// TODO - Create second pipeline for IMGUI.

	// Create command pool and allocate main command buffer
	VkCommandPoolCreateInfo command_pool_create_info = 
	{
		.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.pNext            = 0,
		.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
		// NOTE - This is kind of icky.
		.queueFamilyIndex = best_physical_device.graphics_family_index
	};
	vk_verify(vkCreateCommandPool(renderer->device, &command_pool_create_info, 0, &renderer->command_pool));

	VkCommandBufferAllocateInfo command_buffer_allocate_info = 
	{
		.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.pNext              = 0,
		.commandPool        = renderer->command_pool,
		.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount = 1
	};
	vk_verify(vkAllocateCommandBuffers(renderer->device, &command_buffer_allocate_info, &renderer->main_command_buffer));

	// Create texture sampler.
	VkSamplerCreateInfo sampler_create_info = 
	{
		.sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
		.pNext                   = 0,
		.flags                   = 0,
		.magFilter               = VK_FILTER_LINEAR,
		.minFilter               = VK_FILTER_LINEAR,
		.mipmapMode              = VK_SAMPLER_MIPMAP_MODE_LINEAR,
		.addressModeU            = VK_SAMPLER_ADDRESS_MODE_REPEAT,
		.addressModeV            = VK_SAMPLER_ADDRESS_MODE_REPEAT,
		.addressModeW            = VK_SAMPLER_ADDRESS_MODE_REPEAT,
		.mipLodBias              = 0.0f,
		.anisotropyEnable        = VK_TRUE,
		.maxAnisotropy           = renderer->device_max_sampler_anisotropy,
		.compareEnable           = VK_FALSE,
		.compareOp               = VK_COMPARE_OP_ALWAYS,
		.minLod                  = 0.0f,
		.maxLod                  = 0.0f,
		.borderColor             = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
		.unnormalizedCoordinates = VK_FALSE
	};

	vk_verify(vkCreateSampler(renderer->device, &sampler_create_info, 0, &renderer->texture_sampler));

	// Allocate texture image.
	// Load image from disk.
	int32_t texture_w;
	int32_t texture_h;
	int32_t texture_channels;
	stbi_uc* image_pixels = stbi_load("stone.bmp", &texture_w, &texture_h, &texture_channels, STBI_rgb_alpha);
	if(!image_pixels)
	{
		printf("Failed to load image file.\n");
		panic();
	}

	// Allocate staging buffer.
	VulkanMemoryBuffer staging_memory_buffer;
	VkDeviceSize staging_buffer_size = texture_w * texture_h * 4;

	vulkan_allocate_memory_buffer(
		renderer,
		&staging_memory_buffer,
		staging_buffer_size,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	void* mapped_buffer_data;
	vkMapMemory(renderer->device, staging_memory_buffer.memory, 0, staging_buffer_size, 0, &mapped_buffer_data);
	memcpy(mapped_buffer_data, image_pixels, (size_t)staging_buffer_size);
	vkUnmapMemory(renderer->device, staging_memory_buffer.memory);

	stbi_image_free(image_pixels);

	// Allocate image memory.
	// TODO - Currently this assumes only one texture image.
	vulkan_allocate_image(
		renderer,
		&renderer->texture_images[0],
		(VkExtent2D){ texture_w, texture_h },
		VK_FORMAT_R8G8B8A8_SRGB,
		VK_SAMPLE_COUNT_1_BIT,
		VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);

	// Transfer image from staging buffer to 
	VkCommandBuffer transient_command_buffer = vulkan_start_transient_commands(renderer);
	{
		vulkan_image_memory_barrier(
			transient_command_buffer,
			renderer->texture_images[0].image,
			VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			0,
			VK_ACCESS_TRANSFER_WRITE_BIT,
			VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
			VK_PIPELINE_STAGE_TRANSFER_BIT);

		VkBufferImageCopy region = 
		{
			.bufferOffset      = 0,
			.bufferRowLength   = 0,
			.bufferImageHeight = 0,
			.imageSubresource  = (VkImageSubresourceLayers)
			{ 
				.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
				.mipLevel       = 0,
				.baseArrayLayer = 0,
				.layerCount     = 1
			},
			.imageOffset       = (VkOffset3D){0, 0, 0},
			.imageExtent       = (VkExtent3D){texture_w, texture_h, 1}
		};

		vkCmdCopyBufferToImage(
			transient_command_buffer,
			staging_memory_buffer.buffer,
			renderer->texture_images[0].image,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1,
			&region);
	}
	vulkan_end_transient_commands(renderer, transient_command_buffer, renderer->graphics_queue);

	vkDestroyBuffer(renderer->device, staging_memory_buffer.buffer, 0);
	vkFreeMemory(renderer->device, staging_memory_buffer.memory, 0);

	// Create texture image view
	vulkan_create_image_view(
		renderer,
		&renderer->texture_images[0].image,
		&renderer->texture_images[0].view,
		VK_FORMAT_R8G8B8A8_SRGB,
		VK_IMAGE_ASPECT_COLOR_BIT);


	VulkanMeshVertex vertex_hard_data[24] = 
	{
	    { {{{-0.5f,  0.5f, -0.5f}}}, {{{ 1.0f, 0.0f, 0.0f}}} },
	    { {{{ 0.5f,  0.5f, -0.5f}}}, {{{ 0.0f, 1.0f, 0.0f}}} },
	    { {{{ 0.5f, -0.5f, -0.5f}}}, {{{ 0.0f, 0.0f, 1.0f}}} },
	    { {{{-0.5f, -0.5f, -0.5f}}}, {{{ 0.0f, 0.0f, 0.0f}}} }, 

	    { {{{-0.5f, -0.5f,  0.5f}}}, {{{ 1.0f, 0.0f, 0.0f}}} },
	    { {{{ 0.5f, -0.5f,  0.5f}}}, {{{ 0.0f, 1.0f, 0.0f}}} },
	    { {{{ 0.5f,  0.5f,  0.5f}}}, {{{ 0.0f, 0.0f, 1.0f}}} },
	    { {{{-0.5f,  0.5f,  0.5f}}}, {{{ 0.0f, 0.0f, 0.0f}}} },

	    { {{{-0.5f,  0.5f,  0.5f}}}, {{{ 1.0f, 0.0f, 0.0f}}} },
	    { {{{-0.5f,  0.5f, -0.5f}}}, {{{ 0.0f, 1.0f, 0.0f}}} },
	    { {{{-0.5f, -0.5f, -0.5f}}}, {{{ 0.0f, 0.0f, 1.0f}}} },
	    { {{{-0.5f, -0.5f,  0.5f}}}, {{{ 0.0f, 0.0f, 0.0f}}} },

	    { {{{ 0.5f, -0.5f,  0.5f}}}, {{{ 1.0f, 0.0f, 0.0f}}} },
	    { {{{ 0.5f, -0.5f, -0.5f}}}, {{{ 0.0f, 1.0f, 0.0f}}} },
	    { {{{ 0.5f,  0.5f, -0.5f}}}, {{{ 0.0f, 0.0f, 1.0f}}} },
	    { {{{ 0.5f,  0.5f,  0.5f}}}, {{{ 0.0f, 0.0f, 0.0f}}} },

	    { {{{-0.5f, -0.5f, -0.5f}}}, {{{ 1.0f, 0.0f, 0.0f}}} },
	    { {{{ 0.5f, -0.5f, -0.5f}}}, {{{ 0.0f, 1.0f, 0.0f}}} },
	    { {{{ 0.5f, -0.5f,  0.5f}}}, {{{ 0.0f, 0.0f, 1.0f}}} },
	    { {{{-0.5f, -0.5f,  0.5f}}}, {{{ 0.0f, 0.0f, 0.0f}}} },

	    { {{{-0.5f,  0.5f,  0.5f}}}, {{{ 1.0f, 0.0f, 0.0f}}} },
	    { {{{ 0.5f,  0.5f,  0.5f}}}, {{{ 0.0f, 1.0f, 0.0f}}} },
	    { {{{ 0.5f,  0.5f, -0.5f}}}, {{{ 0.0f, 0.0f, 1.0f}}} },
	    { {{{-0.5f,  0.5f, -0.5f}}}, {{{ 0.0f, 0.0f, 0.0f}}} }
	};

	uint16_t index_hard_data[36] = 
	{
		0, 1, 2, 
		2, 3, 0,

		4, 5, 6, 
		6, 7, 4,

		8,  9,  10, 
		10, 11, 8,

		12, 13, 14, 
		14, 15, 12,

		16, 17, 18, 
		18, 19, 16,

		20, 21, 22, 
		22, 23, 20,
	};

	// Allocate device local memory buffer
	// 
	// TODO - This assumes only one mesh.
	uint8_t meshes_len = 1;
	renderer->mesh_datas[0] = (VulkanMeshData)
	{
		.vertex_memory      = (void*)vertex_hard_data,
		.index_memory       = (void*)index_hard_data,
		.vertex_data_stride = sizeof(VulkanMeshVertex),
		.vertices_len       = 24,
		.indices_len        = 36
	};

	size_t mesh_vertex_buffer_sizes[meshes_len];
	size_t mesh_index_buffer_sizes [meshes_len];

	staging_buffer_size = 0;
	for(uint8_t i = 0; i < meshes_len; i++)
	{
		mesh_vertex_buffer_sizes[i]  = renderer->mesh_datas[i].vertex_data_stride * renderer->mesh_datas[i].vertices_len;
		mesh_index_buffer_sizes[i]   = sizeof(uint16_t)                           * renderer->mesh_datas[i].indices_len;

		renderer->mesh_datas[i].vertex_buffer_offset = staging_buffer_size;
		renderer->mesh_datas[i].index_buffer_offset  = staging_buffer_size + mesh_vertex_buffer_sizes[i];
		
		staging_buffer_size += mesh_vertex_buffer_sizes[i] + mesh_index_buffer_sizes[i];
	}

	vulkan_allocate_memory_buffer(
		renderer,
		&staging_memory_buffer,
		staging_buffer_size, 
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	vkMapMemory(renderer->device, staging_memory_buffer.memory, 0, staging_buffer_size, 0, &mapped_buffer_data);
	{
		size_t total_offset = 0;
		for(uint8_t i = 0; i < meshes_len; i++)
		{
			memcpy(mapped_buffer_data + renderer->mesh_datas[i].vertex_buffer_offset, renderer->mesh_datas[i].vertex_memory, mesh_vertex_buffer_sizes[i]);
			memcpy(mapped_buffer_data + renderer->mesh_datas[i].index_buffer_offset,  renderer->mesh_datas[i].index_memory,  mesh_index_buffer_sizes[i]);
			total_offset += mesh_vertex_buffer_sizes[i] + mesh_index_buffer_sizes[i];
		}
	}
	vkUnmapMemory(renderer->device, staging_memory_buffer.memory);

	vulkan_allocate_memory_buffer(
		renderer,
		&renderer->mesh_data_memory_buffer,
		staging_buffer_size, 
		VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	transient_command_buffer = vulkan_start_transient_commands(renderer);
	{
		VkBufferCopy buffer_copy = {};
		buffer_copy.size = staging_buffer_size;
		vkCmdCopyBuffer(transient_command_buffer, staging_memory_buffer.buffer, renderer->mesh_data_memory_buffer.buffer, 1, &buffer_copy);
	}
	vulkan_end_transient_commands(renderer, transient_command_buffer, renderer->graphics_queue);

	vkDestroyBuffer(renderer->device, staging_memory_buffer.buffer, 0);
	vkFreeMemory(renderer->device, staging_memory_buffer.memory, 0);
}

// NOW - Conform to the new code style.
void vulkan_loop(VulkanRenderer* renderer, RenderList* render_list)
{
	// Translate game memory to uniform buffer object memory.
	VulkanHostMappedData mem = {};
	{
		mem.global.clear_color = render_list->clear_color;
	}
	memcpy(renderer->host_mapped_data, &mem, sizeof(mem));

	uint32_t image_index;
	VkResult res = vkAcquireNextImageKHR(
		renderer->device, 
		renderer->swapchain, 
		UINT64_MAX, 
		renderer->image_available_semaphore, 
		VK_NULL_HANDLE, 
		&image_index);
	if(res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR)
	{
		vulkan_initialize_swapchain(renderer, true);
		return;
	}

	VkCommandBufferBeginInfo begin_info = {};
	begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	vkBeginCommandBuffer(renderer->main_command_buffer, &begin_info);
	{
		// Render image transfer
		vulkan_image_memory_barrier(
			renderer->main_command_buffer, 
			renderer->swapchain_images[image_index], 
			VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			0,
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
		// Depth image transfer
		vulkan_image_memory_barrier(
			renderer->main_command_buffer, 
			renderer->depth_image.image, 
			VK_IMAGE_ASPECT_DEPTH_BIT,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
			0,
			VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
			VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
			VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT);

		VkRenderingAttachmentInfo color_attachment = {};
		color_attachment.sType              = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
		color_attachment.loadOp             = VK_ATTACHMENT_LOAD_OP_CLEAR;
		color_attachment.storeOp            = VK_ATTACHMENT_STORE_OP_STORE;
		color_attachment.imageLayout        = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		color_attachment.imageView          = renderer->render_image.view;
		color_attachment.resolveMode        = VK_RESOLVE_MODE_AVERAGE_BIT;
		color_attachment.resolveImageView   = renderer->swapchain_image_views[image_index];
		color_attachment.resolveImageLayout = VK_IMAGE_LAYOUT_GENERAL;
		color_attachment.clearValue.color   = 
			(VkClearColorValue)
			{{
				render_list->clear_color.r, 
				render_list->clear_color.g, 
				render_list->clear_color.b, 
				1.0f
			}};

		VkRenderingAttachmentInfo depth_attachment = {};
		depth_attachment.sType                   = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
		depth_attachment.loadOp                  = VK_ATTACHMENT_LOAD_OP_CLEAR;
		depth_attachment.storeOp                 = VK_ATTACHMENT_STORE_OP_STORE;
		depth_attachment.imageLayout             = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
		depth_attachment.imageView               = renderer->depth_image.view;
		depth_attachment.resolveMode             = VK_RESOLVE_MODE_NONE;
		depth_attachment.clearValue.depthStencil = 
			(VkClearDepthStencilValue)
			{
				1.0f,
				0
			};

		VkRenderingInfo render_info = {};
		render_info.sType                = VK_STRUCTURE_TYPE_RENDERING_INFO;
		render_info.renderArea           = (VkRect2D){{0, 0}, renderer->swapchain_extent};
		render_info.layerCount           = 1;
		render_info.colorAttachmentCount = 1;
		render_info.pColorAttachments    = &color_attachment;
		render_info.pDepthAttachment     = &depth_attachment;
		render_info.pStencilAttachment   = 0;

		vkCmdBeginRendering(renderer->main_command_buffer, &render_info);
		{
			VkViewport viewport = {};
			viewport.x        = 0;
			viewport.y        = 0;
			viewport.width    = (float)renderer->swapchain_extent.width;
			viewport.height   = (float)renderer->swapchain_extent.height;
			viewport.minDepth = 0;
			viewport.maxDepth = 1;
			vkCmdSetViewport(renderer->main_command_buffer, 0, 1, &viewport);

			VkRect2D scissor = {};
			scissor.offset = (VkOffset2D){0, 0};
			scissor.extent = renderer->swapchain_extent;
			vkCmdSetScissor(renderer->main_command_buffer, 0, 1, &scissor);

			// Render world
			// TODO - This only involves one pipeline, of course.
			vkCmdBindPipeline(
				renderer->main_command_buffer, 
				VK_PIPELINE_BIND_POINT_GRAPHICS, 
				renderer->pipelines[0].pipeline);

			VkDeviceSize offsets[] = {renderer->mesh_datas[0].vertex_buffer_offset};
			vkCmdBindVertexBuffers(
				renderer->main_command_buffer, 
				0, 
				1, 
				&renderer->mesh_data_memory_buffer.buffer,
				offsets);
			vkCmdBindIndexBuffer(
				renderer->main_command_buffer, 
				renderer->mesh_data_memory_buffer.buffer, 
				renderer->mesh_datas[0].index_buffer_offset, 
				VK_INDEX_TYPE_UINT16);

			for(uint16_t i = 0; i < 1; i++) {
				uint32_t dyn_off = i * sizeof(Mat4);
				vkCmdBindDescriptorSets(
					renderer->main_command_buffer, 
					VK_PIPELINE_BIND_POINT_GRAPHICS, 
					renderer->pipelines[0].layout, 
					0, 
					1, 
					&renderer->pipelines[0].descriptor_set,
					1,
					&dyn_off);

				vkCmdDrawIndexed(renderer->main_command_buffer, renderer->mesh_datas[0].indices_len, 1, 0, 0, 0);
			}
		}
		vkCmdEndRendering(renderer->main_command_buffer);

		vulkan_image_memory_barrier(
			renderer->main_command_buffer, 
			renderer->swapchain_images[image_index], 
			VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			0,
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
	}
	vkEndCommandBuffer(renderer->main_command_buffer);

	VkPipelineStageFlags wait_stages[] = 
	{
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
	};

	// We wait to submit until that images is available from before. We did all
	// this prior stuff in the meantime, in theory.
	VkSubmitInfo submit_info = {};
	submit_info.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit_info.waitSemaphoreCount   = 1;
	submit_info.pWaitSemaphores      = &renderer->image_available_semaphore;
	submit_info.pWaitDstStageMask    = wait_stages;
	submit_info.commandBufferCount   = 1;
	submit_info.pCommandBuffers      = &renderer->main_command_buffer;
	submit_info.pSignalSemaphores    = &renderer->render_finished_semaphore;
	submit_info.signalSemaphoreCount = 1;
	vkQueueSubmit(renderer->graphics_queue, 1, &submit_info, VK_NULL_HANDLE);

	VkPresentInfoKHR present_info = {};
	present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	present_info.waitSemaphoreCount = 1;
	present_info.pWaitSemaphores = &renderer->render_finished_semaphore;
	present_info.swapchainCount = 1;
	present_info.pSwapchains = &renderer->swapchain;
	present_info.pImageIndices = &image_index;

	// CONSIDER - See if tracking the present queue and using that here would speed
	// things up.
	res = vkQueuePresentKHR(renderer->graphics_queue, &present_info); 
	if(res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR)
	{
		vulkan_initialize_swapchain(renderer, true);
		return;
	}

	vkDeviceWaitIdle(renderer->device);
}
