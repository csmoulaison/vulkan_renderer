void vulkan_allocate_memory(
	VulkanRenderer*      renderer, 
	VkDeviceMemory*      memory,
	VkMemoryRequirements requirements,
	uint32_t             type_mask)
{
	VkPhysicalDeviceMemoryProperties properties;
	vkGetPhysicalDeviceMemoryProperties(renderer->physical_device, &properties);

	uint32_t suitable_type_index = UINT32_MAX;
	for(uint32_t type_index = 0; type_index < properties.memoryTypeCount; type_index++)
	{
		if((requirements.memoryTypeBits & (1 << type_index))
			&& (properties.memoryTypes[type_index].propertyFlags & type_mask) == type_mask)
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
		.allocationSize  = requirements.size,
		.memoryTypeIndex = suitable_type_index
	};
	vk_verify(vkAllocateMemory(renderer->device, &allocate_info, 0, memory));
}

void vulkan_allocate_memory_buffer(
	VulkanRenderer*       renderer,
	VulkanMemoryBuffer*   memory_buffer,
	VkDeviceSize          size, 
	VkBufferUsageFlags    usage_flags, 
	VkMemoryPropertyFlags properties)
{
	VkBufferCreateInfo buffer_create_info = 
	{
		.sType                 = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.pNext                 = 0,
		.flags                 = 0,
		.size                  = size,
		.usage                 = usage_flags,
		.sharingMode           = VK_SHARING_MODE_EXCLUSIVE,
		.queueFamilyIndexCount = 0,
		.pQueueFamilyIndices   = 0
	};
	vk_verify(vkCreateBuffer(renderer->device, &buffer_create_info, 0, &memory_buffer->buffer));

	VkMemoryRequirements requirements;
	vkGetBufferMemoryRequirements(renderer->device, memory_buffer->buffer, &requirements);

	vulkan_allocate_memory(
		renderer,
		&memory_buffer->memory,
		requirements, 
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	vk_verify(vkBindBufferMemory(renderer->device, memory_buffer->buffer, memory_buffer->memory, 0));
}

void vulkan_allocate_image(
	VulkanRenderer*       renderer,
	VulkanAllocatedImage* allocated_image,
	VkExtent2D            extent,
	VkFormat              format,
	VkSampleCountFlagBits sample_count_flag_bits,
	VkImageUsageFlags     usage_flags)
{
	VkImageCreateInfo image_create_info = 
	{
		.sType 		           = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.pNext                 = 0,
		.flags                 = 0,
		.imageType             = VK_IMAGE_TYPE_2D,
		.format                = format,
		.extent                = (VkExtent3D){ extent.width, extent.height, 1 },
		.mipLevels             = 1,
		.arrayLayers           = 1,
		.samples               = sample_count_flag_bits,
		.tiling                = VK_IMAGE_TILING_OPTIMAL,
		.usage                 = usage_flags,
		.sharingMode           = VK_SHARING_MODE_EXCLUSIVE,
		.pQueueFamilyIndices   = 0,
		.queueFamilyIndexCount = 0,
		.initialLayout         = VK_IMAGE_LAYOUT_UNDEFINED
	};
	vk_verify(vkCreateImage(renderer->device, &image_create_info, 0, &allocated_image->image));

	VkMemoryRequirements requirements = {};
	vkGetImageMemoryRequirements(renderer->device, allocated_image->image, &requirements);

	vulkan_allocate_memory(
		renderer,
		&allocated_image->memory,
		requirements, 
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	vk_verify(vkBindImageMemory(renderer->device, allocated_image->image, allocated_image->memory, 0));
}
