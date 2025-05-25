VkCommandBuffer vulkan_start_transient_commands(
	VulkanRenderer* renderer)
{
	VkCommandBufferAllocateInfo allocate_info = 
	{
		.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.pNext              = 0,
		.commandPool        = renderer->command_pool,
		.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount = 1
	};

	VkCommandBuffer command_buffer;
	vkAllocateCommandBuffers(renderer->device, &allocate_info, &command_buffer);

	VkCommandBufferBeginInfo begin_info = 
	{
		.sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.pNext            = 0,
		.flags            = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
		.pInheritanceInfo = 0
	};
	vkBeginCommandBuffer(command_buffer, &begin_info);

	return command_buffer;
}

void vulkan_end_transient_commands(
	VulkanRenderer* renderer,
	VkCommandBuffer command_buffer,
	VkQueue         submission_queue)
{
	vkEndCommandBuffer(command_buffer);

	VkSubmitInfo submit_info = 
	{
		.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.pNext                = 0,
		.waitSemaphoreCount   = 0,
		.pWaitSemaphores      = 0,
		.pWaitDstStageMask    = 0,
		.commandBufferCount   = 1,
		.pCommandBuffers      = &command_buffer,
		.signalSemaphoreCount = 0,
		.pSignalSemaphores    = 0
	};

	// CONSIDER  - We are using the graphics queue for this presently, but might
	// we want to use a transfer queue for this?
	// I'm not sure if there's any possible performance boost, and I'd rather not
	// do it just for the sake of it.
	// 
	// High level on how this might be done at the following link:
	// https://vulkan-tutorial.com/Vertex_buffers/Staging_buffer
	vkQueueSubmit(submission_queue, 1, &submit_info, 0);
	vkQueueWaitIdle(submission_queue);

	vkFreeCommandBuffers(renderer->device, renderer->command_pool, 1, &command_buffer);
}
