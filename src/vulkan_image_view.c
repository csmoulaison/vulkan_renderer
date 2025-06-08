void vulkan_create_image_view(
	VulkanContext*     vulkan,
	VkImage*           image,
	VkImageView*       image_view,
	VkFormat           format,
	VkImageAspectFlags aspect_flags)
{
	VkImageViewCreateInfo image_view_create_info = 
	{
		.sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.image            = *image,
		.viewType         = VK_IMAGE_VIEW_TYPE_2D,
		.format           = format,
		.subresourceRange =
		{
			.aspectMask     = aspect_flags,
			.baseMipLevel   = 0,
			.levelCount     = 1,
			.baseArrayLayer = 0,
			.layerCount     = 1
		}
	};
	vk_verify(vkCreateImageView(vulkan->device, &image_view_create_info, 0, image_view));
}
