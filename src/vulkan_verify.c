VkResult vk_verify_macro_result;

#define vk_verify(FUNC) vk_verify_macro_result = FUNC;\
						if(vk_verify_macro_result != VK_SUCCESS)\
						{\
							printf("vk_verify error (%i)\n", vk_verify_macro_result);\
							panic();\
						}
