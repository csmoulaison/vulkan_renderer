#define VK_DEBUG 1

#define PIPELINES_COUNT        1
#define MESHES_COUNT           1
#define SWAPCHAIN_IMAGES_COUNT 4

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_BMP
#include "stb/stb_image.h"

// TODO - Load models ourselves, presumably once we decide on what format(s) we are using.
#define TINYOBJ_LOADER_C_IMPLEMENTATION
#include "tinyobj_loader_c/tinyobj_loader_c.h"

#include "vulkan_verify.c"
#include "vulkan_context.c"
#include "vulkan_allocate.c"
#include "vulkan_transient_commands.c"
#include "vulkan_image_memory_barrier.c"
#include "vulkan_image_view.c"
#include "vulkan_mesh.c"
#include "vulkan_pipeline.c"

typedef struct
{
	VkResult(*create_surface_callback)(VulkanContext* ctx, void* context);
	void*   context;
	char**  window_extensions;
	uint8_t window_extensions_len;
} VulkanPlatform;

void vulkan_initialize_swapchain(VulkanContext* ctx, bool recreate)
{
	// This function is being called in one of two situations:
	// 1. During program initialization.
	// 2. The platform surface has changed and swapchain related information is no longer valid.
	if(recreate)
	{
		vkDeviceWaitIdle(ctx->device);

		for(uint8_t image_index = 0; image_index < SWAPCHAIN_IMAGES_COUNT; image_index++)
		{
			vkDestroyImageView(ctx->device, ctx->swapchain_image_views[image_index], 0);
		}

		vkDestroySwapchainKHR(ctx->device, ctx->swapchain, 0);

		vkDestroyImage(ctx->device, ctx->render_image.image, 0);
		vkDestroyImageView(ctx->device, ctx->render_image.view, 0);
		vkFreeMemory(ctx->device, ctx->render_image.memory, 0);

		vkDestroySemaphore(ctx->device, ctx->image_available_semaphore, 0);
		vkDestroySemaphore(ctx->device, ctx->render_finished_semaphore, 0);
	}

	// Query surface capabilities to give us the following info:
	// - The transform of the surface (believe the position on screen, roughly speaking?)
	// - Swapchain width and height
	// - Swapchain image count
	VkSurfaceCapabilitiesKHR surface_capabilities;
	vk_verify(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
		ctx->physical_device, 
		ctx->surface, 
		&surface_capabilities));

	VkSurfaceTransformFlagBitsKHR surface_pre_transform = surface_capabilities.currentTransform;

	ctx->swapchain_extent.width  = surface_capabilities.maxImageExtent.width;
	ctx->swapchain_extent.height = surface_capabilities.maxImageExtent.height;

	// CONSIDER - Not sure exactly what this logic is for. Look at it a little closer.
	uint32_t swapchain_image_count = surface_capabilities.minImageCount + 1;
	if(surface_capabilities.maxImageCount > 0 && swapchain_image_count > surface_capabilities.maxImageCount) 
	{	
		swapchain_image_count = surface_capabilities.maxImageCount;
	}

	// Choose the best surface format.
	uint32_t formats_len;
	vkGetPhysicalDeviceSurfaceFormatsKHR(ctx->physical_device, ctx->surface, &formats_len, 0);
	if(formats_len == 0)
	{
		panic();
	}

	VkSurfaceFormatKHR formats[formats_len];
	vkGetPhysicalDeviceSurfaceFormatsKHR(ctx->physical_device, ctx->surface, &formats_len, formats);

	ctx->surface_format = formats[0];
	for(uint32_t format_index = 0; format_index < formats_len; format_index++)
	{
		if(formats[format_index].format == VK_FORMAT_B8G8R8A8_SRGB 
			&& formats[format_index].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
		{
			ctx->surface_format = formats[format_index];
			break;
		}
	}

	// Choose presentation mode, defaulting to VK_PRESENT_MODE_FIFO_KHR, which is guaranteed to
	// be supported by the spec.
	VkPresentModeKHR present_mode = VK_PRESENT_MODE_FIFO_KHR;

	uint32_t modes_len;
	vk_verify(vkGetPhysicalDeviceSurfacePresentModesKHR(ctx->physical_device, ctx->surface, &modes_len, 0));

	VkPresentModeKHR modes[modes_len];
	vk_verify(vkGetPhysicalDeviceSurfacePresentModesKHR(ctx->physical_device, ctx->surface, &modes_len, modes));

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
		.surface               = ctx->surface,
		.minImageCount         = swapchain_image_count, 
		.imageFormat           = ctx->surface_format.format,
		.imageColorSpace       = ctx->surface_format.colorSpace,
		.imageExtent           = ctx->swapchain_extent,
		.imageArrayLayers      = 1,
		.imageUsage            = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
		.imageSharingMode      = VK_SHARING_MODE_EXCLUSIVE,
		.queueFamilyIndexCount = 0,
		.pQueueFamilyIndices   = 0,
		.preTransform          = surface_pre_transform,
		.compositeAlpha        = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
		.presentMode           = present_mode,
		.clipped               = VK_TRUE,
		.oldSwapchain          = 0
	};

	vk_verify(vkCreateSwapchainKHR(ctx->device, &swapchain_create_info, 0, &ctx->swapchain));

	// Get references to the swapchain images.
	uint32_t swapchain_images_count = SWAPCHAIN_IMAGES_COUNT;
	vk_verify(vkGetSwapchainImagesKHR(
		ctx->device, 
		ctx->swapchain, 
		&swapchain_images_count, 
		0));
	vk_verify(vkGetSwapchainImagesKHR(
		ctx->device, 
		ctx->swapchain, 
		&swapchain_images_count, 
		ctx->swapchain_images));

	// Allocate resources for render and depth images.
	//
	// CONSIDER - The following description is a bit lacking in understanding.
	// The render image is used in the rendering pipeline, and is transferred to the swapchain.
	vulkan_allocate_image(
		ctx,
		&ctx->render_image,
		ctx->swapchain_extent,
		ctx->surface_format.format,
		ctx->device_framebuffer_sample_counts,
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);

	vulkan_allocate_image(
		ctx, 
		&ctx->depth_image,
		ctx->swapchain_extent,
		VK_FORMAT_D32_SFLOAT,
		ctx->device_framebuffer_sample_counts,
		VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);

	vulkan_create_image_view(
		ctx,
		&ctx->render_image.image,
		&ctx->render_image.view,
		ctx->surface_format.format,
		VK_IMAGE_ASPECT_COLOR_BIT);

	vulkan_create_image_view(
		ctx,
		&ctx->depth_image.image,
		&ctx->depth_image.view,
		VK_FORMAT_D32_SFLOAT,
		VK_IMAGE_ASPECT_DEPTH_BIT);

	for(uint32_t image_index = 0; image_index < SWAPCHAIN_IMAGES_COUNT; image_index++)
	{
		vulkan_create_image_view(
			ctx,
			&ctx->swapchain_images[image_index],
			&ctx->swapchain_image_views[image_index],
			ctx->surface_format.format,
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

	vk_verify(vkCreateSemaphore(ctx->device, &semaphore_create_info, 0, &ctx->image_available_semaphore));
	vk_verify(vkCreateSemaphore(ctx->device, &semaphore_create_info, 0, &ctx->render_finished_semaphore));
}

void vulkan_initialize(VulkanContext* ctx, VulkanPlatform* platform)
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
	vk_verify(vkCreateInstance(&instance_create_info, 0, &ctx->instance));

	// Get surface from the platform specific callback.
	vk_verify(platform->create_surface_callback(ctx, platform->context));

	// Query all physical devices.
	uint32_t physical_devices_len;
	vk_verify(vkEnumeratePhysicalDevices(ctx->instance, &physical_devices_len, 0));

	VkPhysicalDevice physical_devices[physical_devices_len];
	vk_verify(vkEnumeratePhysicalDevices(ctx->instance, &physical_devices_len, physical_devices));

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
			vkGetPhysicalDeviceSurfaceSupportKHR(candidate.handle, queue_index, ctx->surface, &present_support);
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

	ctx->physical_device = best_physical_device.handle;
	ctx->device_max_sampler_anisotropy    = best_physical_device.max_sampler_anisotropy;
	ctx->device_framebuffer_sample_counts = best_physical_device.framebuffer_color_sample_counts;

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
	vkGetPhysicalDeviceFeatures2(ctx->physical_device, &device_features_2);

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
	vk_verify(vkCreateDevice(ctx->physical_device, &device_create_info, 0, &ctx->device));

	vkGetDeviceQueue(ctx->device, best_physical_device.graphics_family_index, 0, &ctx->graphics_queue);
	vkGetDeviceQueue(ctx->device, best_physical_device.present_family_index, 0, &ctx->present_queue);

	// Initially initialize swapchain.
	vulkan_initialize_swapchain(ctx, false);

	// Allocate host mapped memory buffer.
	VkDeviceSize host_mapped_memory_size = sizeof(VulkanHostMappedData);

	vulkan_allocate_memory_buffer(
		ctx,
		&ctx->host_mapped_buffer,
		host_mapped_memory_size,
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	vkMapMemory(
		ctx->device, 
		ctx->host_mapped_buffer.memory, 
		0, 
		host_mapped_memory_size, 
		0, 
		(void*)&ctx->host_mapped_data);

	// Create command pool and allocate main command buffer
	VkCommandPoolCreateInfo command_pool_create_info = 
	{
		.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.pNext            = 0,
		.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
		// NOTE - This is kind of icky because this variable was set so long ago and appears proximal there.
		.queueFamilyIndex = best_physical_device.graphics_family_index
	};
	vk_verify(vkCreateCommandPool(ctx->device, &command_pool_create_info, 0, &ctx->command_pool));

	VkCommandBufferAllocateInfo command_buffer_allocate_info = 
	{
		.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.pNext              = 0,
		.commandPool        = ctx->command_pool,
		.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount = 1
	};
	vk_verify(vkAllocateCommandBuffers(ctx->device, &command_buffer_allocate_info, &ctx->main_command_buffer));

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
		.maxAnisotropy           = ctx->device_max_sampler_anisotropy,
		.compareEnable           = VK_FALSE,
		.compareOp               = VK_COMPARE_OP_ALWAYS,
		.minLod                  = 0.0f,
		.maxLod                  = 0.0f,
		.borderColor             = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
		.unnormalizedCoordinates = VK_FALSE
	};
	vk_verify(vkCreateSampler(ctx->device, &sampler_create_info, 0, &ctx->texture_sampler));

	// Allocate texture image.
	// Load image from disk.
	int32_t texture_w;
	int32_t texture_h;
	int32_t texture_channels;
	stbi_uc* image_pixels = stbi_load("assets/viking_room.bmp", &texture_w, &texture_h, &texture_channels, STBI_rgb_alpha);
	if(!image_pixels)
	{
		printf("Failed to load image file.\n");
		panic();
	}

	// Allocate staging buffer.
	VulkanMemoryBuffer staging_memory_buffer;
	VkDeviceSize staging_buffer_size = texture_w * texture_h * 4;

	vulkan_allocate_memory_buffer(
		ctx,
		&staging_memory_buffer,
		staging_buffer_size,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	void* mapped_buffer_data;
	vkMapMemory(ctx->device, staging_memory_buffer.memory, 0, staging_buffer_size, 0, &mapped_buffer_data);
	memcpy(mapped_buffer_data, image_pixels, (size_t)staging_buffer_size);
	vkUnmapMemory(ctx->device, staging_memory_buffer.memory);

	stbi_image_free(image_pixels);

	// Allocate image memory.
	// TODO - Currently this assumes only one texture image.
	vulkan_allocate_image(
		ctx,
		&ctx->texture_images[0],
		(VkExtent2D){ texture_w, texture_h },
		VK_FORMAT_R8G8B8A8_SRGB,
		VK_SAMPLE_COUNT_1_BIT,
		VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);

	// Transfer image from staging buffer to 
	VkCommandBuffer transient_command_buffer = vulkan_start_transient_commands(ctx);
	{
		vulkan_image_memory_barrier(
			transient_command_buffer,
			ctx->texture_images[0].image,
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
			ctx->texture_images[0].image,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1,
			&region);

		vulkan_image_memory_barrier(
			transient_command_buffer,
			ctx->texture_images[0].image,
			VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			0,
			VK_ACCESS_TRANSFER_WRITE_BIT,
			VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
			VK_PIPELINE_STAGE_TRANSFER_BIT);
	}
	vulkan_end_transient_commands(ctx, transient_command_buffer, ctx->graphics_queue);

	vkDestroyBuffer(ctx->device, staging_memory_buffer.buffer, 0);
	vkFreeMemory(ctx->device, staging_memory_buffer.memory, 0);

	// Create texture image view
	vulkan_create_image_view(
		ctx,
		&ctx->texture_images[0].image,
		&ctx->texture_images[0].view,
		VK_FORMAT_R8G8B8A8_SRGB,
		VK_IMAGE_ASPECT_COLOR_BIT);

	// Create graphics pipeline for meshes.
	// TODO - Create second pipeline for IMGUI.

	VulkanDescriptorSetConfig descriptor_set_configs[3] =
	{
		{
			.type                  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			.shader_stage_flags    = VK_SHADER_STAGE_VERTEX_BIT,
			.offset_in_host_memory = offsetof(VulkanHostMappedData, global),
			.range_in_host_memory  = sizeof(VulkanHostMappedGlobal)
		},
		{
			.type                  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
			.shader_stage_flags    = VK_SHADER_STAGE_VERTEX_BIT,
			.offset_in_host_memory = offsetof(VulkanHostMappedData, instance),
			.range_in_host_memory  = sizeof(VulkanHostMappedInstance)
		},
		{
			.type                  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			.shader_stage_flags    = VK_SHADER_STAGE_FRAGMENT_BIT,
			.offset_in_host_memory = 0,
			.range_in_host_memory  = 0
		}
	};

	VulkanVertexInputAttributeConfig vertex_input_attribute_configs[3] =
	{
		{
			.format                = VK_FORMAT_R32G32B32_SFLOAT,
			.offset_in_vertex_data = offsetof(VulkanMeshVertex, position)
		},
		{
			.format                = VK_FORMAT_R32G32_SFLOAT,
			.offset_in_vertex_data = offsetof(VulkanMeshVertex, texture_uv)
		}
	};

	// Create graphics pipeline.
	// TODO - This is dependant on only having one pipeline.
	vulkan_create_graphics_pipeline(
		ctx,
		&ctx->pipelines[0],
		"shaders/world_vertex.spv",
		"shaders/world_fragment.spv",
		descriptor_set_configs,
		3,
		vertex_input_attribute_configs,
		2,
		sizeof(VulkanMeshVertex));

	uint8_t meshes_len = 2;
	staging_buffer_size = 0;
	size_t mesh_vertex_buffer_sizes[meshes_len];
	size_t mesh_index_buffer_sizes [meshes_len];
	VulkanMeshData mesh_datas[meshes_len];

	for(uint8_t mesh_index = 0; mesh_index < meshes_len; mesh_index++)
	{
		VulkanMeshData* data = &mesh_datas[mesh_index];
		vulkan_load_mesh(data, "assets/viking_room.obj");

		VulkanAllocatedMesh* mesh = &ctx->allocated_meshes[mesh_index];
		mesh->vertices_len = data->vertices_len;
		mesh->indices_len  = data->indices_len;

		mesh_vertex_buffer_sizes[mesh_index] = MESH_VERTEX_STRIDE * mesh->vertices_len;
		mesh_index_buffer_sizes[mesh_index]  = sizeof(uint32_t)   * mesh->indices_len;

		mesh->vertex_buffer_offset = staging_buffer_size;
		mesh->index_buffer_offset = staging_buffer_size + mesh_vertex_buffer_sizes[mesh_index];
		
		staging_buffer_size += mesh_vertex_buffer_sizes[mesh_index] + mesh_index_buffer_sizes[mesh_index];
	}

	vulkan_allocate_memory_buffer(
		ctx,
		&staging_memory_buffer,
		staging_buffer_size, 
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	vkMapMemory(ctx->device, staging_memory_buffer.memory, 0, staging_buffer_size, 0, &mapped_buffer_data);
	{
		size_t total_offset = 0;
		for(uint8_t mesh_index = 0; mesh_index < meshes_len; mesh_index++)
		{
			VulkanAllocatedMesh* mesh = &ctx->allocated_meshes[mesh_index];
			VulkanMeshData*      data = &mesh_datas[mesh_index];

			memcpy(mapped_buffer_data + mesh->vertex_buffer_offset, data->vertices, mesh_vertex_buffer_sizes[mesh_index]);
			memcpy(mapped_buffer_data + mesh->index_buffer_offset,  data->indices,  mesh_index_buffer_sizes[mesh_index]);
			total_offset += mesh_vertex_buffer_sizes[mesh_index] + mesh_index_buffer_sizes[mesh_index];
		}
	}
	vkUnmapMemory(ctx->device, staging_memory_buffer.memory);

	vulkan_allocate_memory_buffer(
		ctx,
		&ctx->mesh_data_memory_buffer,
		staging_buffer_size, 
		VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	transient_command_buffer = vulkan_start_transient_commands(ctx);
	{
		VkBufferCopy buffer_copy = {};
		buffer_copy.size = staging_buffer_size;
		vkCmdCopyBuffer(transient_command_buffer, staging_memory_buffer.buffer, ctx->mesh_data_memory_buffer.buffer, 1, &buffer_copy);
	}
	vulkan_end_transient_commands(ctx, transient_command_buffer, ctx->graphics_queue);

	vkDestroyBuffer(ctx->device, staging_memory_buffer.buffer, 0);
	vkFreeMemory(ctx->device, staging_memory_buffer.memory, 0);
}

void vulkan_loop(VulkanContext* ctx, RenderList* render_list)
{
	// Translate game memory to uniform buffer object memory.
	VulkanHostMappedData mem = {};
	{
		mem.global.clear_color = render_list->clear_color;

		glm_lookat(render_list->camera_position.data, render_list->camera_target.data, vec3_new(0, 1, 0).data, mem.global.view);
		glm_perspective(radians(75), (float)ctx->swapchain_extent.width / (float)ctx->swapchain_extent.height, 0.1, 100, mem.global.projection);
		mem.global.projection[1][1] *= -1;

		mat4 mesh_transforms[render_list->static_meshes_len];
		for(uint8_t mesh_index = 0; mesh_index < render_list->static_meshes_len; mesh_index++)
		{
		 	mat4* transform  = &mesh_transforms[mesh_index];
		 	StaticMesh* mesh = &render_list->static_meshes[mesh_index];

		    glm_mat4_identity(*transform);
		    glm_translate(*transform, mesh->position.data);
		    glm_mat4_mul(*transform, mesh->orientation, *transform);
		}
		memcpy(mem.instance.models, mesh_transforms, sizeof(mem.instance.models));
	}
	memcpy(ctx->host_mapped_data, &mem, sizeof(mem));

	uint32_t image_index;
	VkResult res = vkAcquireNextImageKHR(
		ctx->device, 
		ctx->swapchain, 
		UINT64_MAX, 
		ctx->image_available_semaphore, 
		0, 
		&image_index);
	if(res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR)
	{
		vulkan_initialize_swapchain(ctx, true);
		return;
	}

	VkCommandBufferBeginInfo command_buffer_begin_info = 
	{
		.sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.pNext            = 0,
		.flags            = 0,
		.pInheritanceInfo = 0
	};

	vkBeginCommandBuffer(ctx->main_command_buffer, &command_buffer_begin_info);
	{
		// Render image transfer
		vulkan_image_memory_barrier(
			ctx->main_command_buffer, 
			ctx->swapchain_images[image_index], 
			VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			0,
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

		// Depth image transfer
		vulkan_image_memory_barrier(
			ctx->main_command_buffer, 
			ctx->depth_image.image, 
			VK_IMAGE_ASPECT_DEPTH_BIT,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
			0,
			VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
			VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
			VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT);

		VkRenderingInfo render_info = 
		{
			.sType                = VK_STRUCTURE_TYPE_RENDERING_INFO,
			.renderArea           = (VkRect2D){{0, 0}, ctx->swapchain_extent},
			.layerCount           = 1,
			.colorAttachmentCount = 1,
			.pColorAttachments    = &(VkRenderingAttachmentInfo)
			{
				.sType                   = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
				.pNext                   = 0,
				.imageView               = ctx->render_image.view,
				.imageLayout             = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				.resolveMode             = VK_RESOLVE_MODE_AVERAGE_BIT,
				.resolveImageView        = ctx->swapchain_image_views[image_index],
				.resolveImageLayout      = VK_IMAGE_LAYOUT_GENERAL,
				.loadOp                  = VK_ATTACHMENT_LOAD_OP_CLEAR,
				.storeOp                 = VK_ATTACHMENT_STORE_OP_STORE,
				.clearValue.color        = (VkClearColorValue)
				{{
					render_list->clear_color.r, 
					render_list->clear_color.g, 
					render_list->clear_color.b, 
					1.0f
				}},
			},
			.pDepthAttachment     = &(VkRenderingAttachmentInfo)
			{
				.sType                   = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
				.pNext                   = 0,
				.imageView               = ctx->depth_image.view,
				.imageLayout             = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
				.resolveMode             = VK_RESOLVE_MODE_NONE,
				.resolveImageView        = 0,
				.resolveImageLayout      = 0,
				.loadOp                  = VK_ATTACHMENT_LOAD_OP_CLEAR,
				.storeOp                 = VK_ATTACHMENT_STORE_OP_STORE,
				.clearValue.depthStencil = (VkClearDepthStencilValue)
				{
					1.0f,
					0
				}
			},
			.pStencilAttachment   = 0
		};

		vkCmdBeginRendering(ctx->main_command_buffer, &render_info);
		{
			VkViewport viewport = 
			{
				.x        = 0,
				.y        = 0,
				.width    = (float)ctx->swapchain_extent.width,
				.height   = (float)ctx->swapchain_extent.height,
				.minDepth = 0,
				.maxDepth = 1
			};
			vkCmdSetViewport(ctx->main_command_buffer, 0, 1, &viewport);

			VkRect2D scissor = 
			{
				.offset = (VkOffset2D){0, 0},
				.extent = ctx->swapchain_extent
			};
			vkCmdSetScissor(ctx->main_command_buffer, 0, 1, &scissor);

			// Render world
			// TODO - This only involves one pipeline, of course.
			vkCmdBindPipeline(ctx->main_command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, ctx->pipelines[0].pipeline);

			VkDeviceSize offsets[] = {ctx->allocated_meshes[0].vertex_buffer_offset};
			vkCmdBindVertexBuffers(
				ctx->main_command_buffer, 
				0, 
				1, 
				&ctx->mesh_data_memory_buffer.buffer,
				offsets);

			vkCmdBindIndexBuffer(
				ctx->main_command_buffer, 
				ctx->mesh_data_memory_buffer.buffer, 
				ctx->allocated_meshes[0].index_buffer_offset, 
				VK_INDEX_TYPE_UINT32);

			for(uint16_t instance = 0; instance < STATIC_MESHES_LEN; instance++) 
			{
				uint32_t dynamic_offset = instance * sizeof(mat4);

				vkCmdBindDescriptorSets(
					ctx->main_command_buffer, 
					VK_PIPELINE_BIND_POINT_GRAPHICS, 
					ctx->pipelines[0].layout, 
					0, 
					1, 
					&ctx->pipelines[0].descriptor_set,
					1,
					&dynamic_offset);
				vkCmdDrawIndexed(ctx->main_command_buffer, ctx->allocated_meshes[0].indices_len, 1, 0, 0, 0);
			}
		}
		vkCmdEndRendering(ctx->main_command_buffer);

		vulkan_image_memory_barrier(
			ctx->main_command_buffer, 
			ctx->swapchain_images[image_index], 
			VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			0,
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
	}
	vkEndCommandBuffer(ctx->main_command_buffer);

	// We wait to submit until that images is available from before. We did all
	// this prior stuff in the meantime, in theory.
	VkSubmitInfo submit_info = 
	{
		.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.pNext                = 0,
		.waitSemaphoreCount   = 1,
		.pWaitSemaphores      = &ctx->image_available_semaphore,
		.pWaitDstStageMask    = &(VkPipelineStageFlags){ VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT },
		.commandBufferCount   = 1,
		.pCommandBuffers      = &ctx->main_command_buffer,
		.signalSemaphoreCount = 1,
		.pSignalSemaphores    = &ctx->render_finished_semaphore
	};
	vkQueueSubmit(ctx->graphics_queue, 1, &submit_info, 0);

	VkPresentInfoKHR present_info = 
	{
		.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
		.pNext              = 0,
		.waitSemaphoreCount = 1,
		.pWaitSemaphores    = &ctx->render_finished_semaphore,
		.swapchainCount     = 1,
		.pSwapchains        = &ctx->swapchain,
		.pImageIndices      = &image_index,
		.pResults           = 0
	};

	res = vkQueuePresentKHR(ctx->present_queue, &present_info); 
	if(res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR)
	{
		vulkan_initialize_swapchain(ctx, true);
		return;
	}

	vkDeviceWaitIdle(ctx->device);
}
