void vulkan_image_memory_barrier(
	VkCommandBuffer         command_buffer, 
	VkImage                 image, 
	VkImageAspectFlags      aspect_flags,
	VkImageLayout           old_layout, 
	VkImageLayout           new_layout, 
	VkAccessFlags           src_access_flags,
	VkAccessFlags           dst_access_flags,
	VkPipelineStageFlagBits stage_src, 
	VkPipelineStageFlagBits stage_dst) 
{
    VkImageMemoryBarrier image_memory_barrier = 
    {
    	.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
    	.pNext               = 0,
    	.srcAccessMask       = src_access_flags,
    	.dstAccessMask       = dst_access_flags,
    	.oldLayout           = old_layout,
    	.newLayout           = new_layout,
    	.srcQueueFamilyIndex = 0,
    	.dstQueueFamilyIndex = 0,
    	.image               = image,
    	.subresourceRange    = (VkImageSubresourceRange)
    	{
			.aspectMask     = aspect_flags,
			.baseMipLevel   = 0,
			.levelCount     = 1,
			.baseArrayLayer = 0,
			.layerCount     = 1
    	}
	};

    vkCmdPipelineBarrier(command_buffer, stage_src, stage_dst, 0, 0, 0, 0, 0, 1, &image_memory_barrier);
}
