#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "panic.c"
#include "linalg.c"
#include "random.c"

#define STATIC_MESHES_LEN 2

#include "program.c"


#define VK_USE_PLATFORM_XCB_KHR
#include <vulkan/vulkan.h>

// Taken from Xlib keysym defs which can be found at the following link:
// https://www.cl.cam.ac.uk/~mgk25/ucs/keysymdef.h
#define XCB_ESCAPE 0xff1b
#define XCB_W 0x0077
#define XCB_A 0x0061
#define XCB_S 0x0073
#define XCB_D 0x0064

#include <xcb/xcb.h>
#include <xcb/xfixes.h>
#include <xcb/xinput.h>
#include <xcb/xcb_keysyms.h>
#include <vulkan/vulkan_xcb.h>

#include "render_list.c"
#include "vulkan.c"
#include "renderer.c"
#include "game.c"

#define MEMORY_POOL_BYTES 1073741824

typedef struct
{
	bool                running;
	float               time_since_start;
	struct timespec     time_prev;
	
	xcb_connection_t*   connection;
	xcb_screen_t*       screen;
	xcb_window_t        window;
	uint32_t            window_w;
	uint32_t            window_h;

	xcb_key_symbols_t*  keysyms;
	bool 				mouse_just_warped;
	bool				mouse_moved_yet;

	void*               memory_pool;
	size_t 				memory_pool_bytes;

	InputContext        input;
	RenderList          render_list;
	Renderer            renderer;
} XcbContext;

VkResult xcb_create_surface_callback(VulkanContext* vulkan, void* context)
{
	XcbContext* xcb = (XcbContext*)context;
	
	VkXcbSurfaceCreateInfoKHR info = {};
	info.sType      = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR;
	info.pNext      = 0;
	info.flags      = 0;
	info.connection = xcb->connection;
	info.window     = xcb->window;

	return vkCreateXcbSurfaceKHR(vulkan->instance, &info, 0, &vulkan->surface);
}

void print_mat(float* m)
{
	for(uint8_t i = 0; i < 16; i++)
	{
		printf("%.2f, ", m[i]);
	}
	printf("\n");
}

int32_t main(int32_t argc, char** argv)
{
	XcbContext xcb;
	
	xcb.connection = xcb_connect(0, 0);
	// TODO - Handle more than 1 screen?
	xcb.screen = xcb_setup_roots_iterator(xcb_get_setup(xcb.connection)).data;

	// Window event registration
	uint32_t mask = XCB_CW_EVENT_MASK;
	uint32_t values[1];
	values[0] = 
		XCB_EVENT_MASK_EXPOSURE | 
		XCB_EVENT_MASK_KEY_PRESS | 
		XCB_EVENT_MASK_KEY_RELEASE | 
		XCB_EVENT_MASK_POINTER_MOTION | 
		XCB_EVENT_MASK_STRUCTURE_NOTIFY;
	
	xcb.window = xcb_generate_id(xcb.connection);
	xcb_create_window(
		xcb.connection,
		XCB_COPY_FROM_PARENT,
		xcb.window,
		xcb.screen->root,
		0, 0,
		480, 480,
		1,
		XCB_WINDOW_CLASS_INPUT_OUTPUT,
		xcb.screen->root_visual,
		mask, values);
	xcb_map_window(xcb.connection, xcb.window);

	//XGrabPointer(x11.display, x11.window, 1, PointerMotionMask, GrabModeAsync, GrabModeAsync, x11.window, None, CurrentTime);
	xcb_grab_pointer(xcb.connection, false, xcb.window, mask, XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC, XCB_NONE, XCB_NONE, XCB_CURRENT_TIME);
	xcb_xfixes_query_version(xcb.connection, 4, 0);
	xcb_xfixes_hide_cursor(xcb.connection, xcb.window);

	xcb_flush(xcb.connection);

	// This needs to be initialized in order for keysym lookups to work
	xcb.keysyms = xcb_key_symbols_alloc(xcb.connection);

	// TODO - can we get the width and height post window manager resize to pass
	// to vk_init? Alternatively, don't worry about it and just handle fullscreen.
	/*xcb_get_geometry_reply_t* geometry = xcb_get_geometry_reply(
		xcb.connection, 
		xcb_get_geometry(xcb.connection, xcb.window), 
		0);
	xcb.window_w = geometry->width;
	xcb.window_h = geometry->height;*/

	// VOLATILE - xcb_platform.window_extensions_len must equal length of window_exts.
	char* window_exts[2] = 
	{
		"VK_KHR_surface", // TODO - find relevant _EXTENSION_NAME macro?
		VK_KHR_XCB_SURFACE_EXTENSION_NAME
	};

	RendererPlatformData xcb_renderer_platform_data;
	xcb_renderer_platform_data.vulkan = (VulkanPlatform)
	{
		.context                 = &xcb,
		.create_surface_callback = xcb_create_surface_callback,
		.window_extensions_len   = 2,
		.window_extensions       = window_exts
	};

	renderer_initialize(&xcb.renderer, &xcb_renderer_platform_data);

	xcb.running = true;

	xcb.input.mouse_x = 0;
	xcb.input.mouse_y = 0;
	xcb.input.mouse_delta_x = 0;
	xcb.input.mouse_delta_y = 0;
	for(uint32_t i = 0; i < INPUT_BUTTONS_LEN; i++) 
	{
		xcb.input.buttons[i].held = 0;
		xcb.input.buttons[i].pressed = 0;
		xcb.input.buttons[i].released = 0;
	}

	xcb.mouse_just_warped = false;
	xcb.mouse_moved_yet = false;

	// TODO - raw memory page allocation
	xcb.memory_pool       = malloc(MEMORY_POOL_BYTES);
	xcb.memory_pool_bytes = MEMORY_POOL_BYTES;

	game_initialize(xcb.memory_pool, xcb.memory_pool_bytes);

    if(clock_gettime(CLOCK_REALTIME, &xcb.time_prev))
    {
        panic();
    }
    xcb.time_since_start = 0;

	while(xcb.running)
	{
    	input_reset_buttons(&xcb.input);
    	xcb.input.mouse_delta_x = 0;
    	xcb.input.mouse_delta_y = 0;
    	
		xcb_generic_event_t* e;
		while((e = xcb_poll_for_event(xcb.connection)))
		{
			switch(e->response_type & ~0x80)
			{
				case XCB_CONFIGURE_NOTIFY:
				{
					xcb_configure_notify_event_t* ev = (xcb_configure_notify_event_t*)e;
					xcb.window_w = ev->width;
					xcb.window_h = ev->height;

					xcb.mouse_moved_yet = 0;
					break;
				}
            	case XCB_MOTION_NOTIFY:
                {
                    // TODO - implement properly
					xcb_motion_notify_event_t* ev = (xcb_motion_notify_event_t*)e;

					if(!xcb.mouse_moved_yet) 
					{
    					xcb.mouse_moved_yet = 1;
    					xcb.input.mouse_delta_x = 0;
    					xcb.input.mouse_delta_y = 0;
        				xcb.input.mouse_x = ev->event_x;
    					xcb.input.mouse_y = ev->event_y;
    					break;
					}
                    
					if(xcb.mouse_just_warped) 
					{
    					xcb.mouse_just_warped = 0;
    					break;
					}
               
					xcb.input.mouse_delta_x = ev->event_x - xcb.input.mouse_x;
					xcb.input.mouse_delta_y = ev->event_y - xcb.input.mouse_y;
					xcb.input.mouse_x = ev->event_x;
					xcb.input.mouse_y = ev->event_y;

					int32_t bounds_x = xcb.window_w / 4;
					int32_t bounds_y = xcb.window_h / 4;
					if(xcb.input.mouse_x < bounds_x ||
    					xcb.input.mouse_x > xcb.window_w - bounds_x ||
    					xcb.input.mouse_y < bounds_y ||
    					xcb.input.mouse_y > xcb.window_h - bounds_y)
					{
    					xcb.mouse_just_warped = 1;
    					xcb.input.mouse_x = xcb.window_w / 2;
    					xcb.input.mouse_y = xcb.window_h / 2;

    					xcb_warp_pointer(
        					xcb.connection,
        					XCB_NONE,
        					xcb.window,
        					0, 0, 0, 0,
        					xcb.window_w / 2, xcb.window_h / 2);
    					xcb_flush(xcb.connection);

					}
					break;
                }
				case XCB_KEY_PRESS:
				{
					xcb_key_press_event_t* k_e = (xcb_key_press_event_t*)e;
					xcb_keysym_t keysym = xcb_key_press_lookup_keysym(xcb.keysyms, k_e, 0);
					switch(keysym)
					{
						case XCB_ESCAPE:
						{
							xcb.running = false;
							break;
						}
                		case XCB_W:
                		{
                    		input_button_press(&xcb.input.move_forward);
        					break;
                		}
                		case XCB_A:
                		{
                    		input_button_press(&xcb.input.move_left);
        					break;
                		}
                		case XCB_S:
                		{
                    		input_button_press(&xcb.input.move_back);
        					break;
                		}
                		case XCB_D:
                		{
                    		input_button_press(&xcb.input.move_right);
        					break;
                		}
                		default:
                    	{
                        	break;
                        }
            		}
            		break;
				}
				case XCB_KEY_RELEASE:
				{
					xcb_key_press_event_t* k_e = (xcb_key_press_event_t*)e;
					xcb_keysym_t keysym = xcb_key_press_lookup_keysym(xcb.keysyms, k_e, 0);
					switch(keysym)
					{
                		case XCB_W:
                		{
                    		input_button_release(&xcb.input.move_forward);
        					break;
                		}
                		case XCB_A:
                		{
                    		input_button_release(&xcb.input.move_left);
        					break;
                		}
                		case XCB_S:
                		{
                    		input_button_release(&xcb.input.move_back);
        					break;
                		}
                		case XCB_D:
                		{
                    		input_button_release(&xcb.input.move_right);
        					break;
                		}
                		default:
                    	{
                        	break;
                        }
            		}
            		break;
				}
				default:
				{
					break;
				}
			}
		}

        struct timespec time_cur;
        if(clock_gettime(CLOCK_REALTIME, &time_cur))
        {
    		panic();
        }
    	float dt = time_cur.tv_sec - xcb.time_prev.tv_sec + 
    	(float)time_cur.tv_nsec / 1000000000 - (float)xcb.time_prev.tv_nsec / 1000000000;
        xcb.time_prev = time_cur;
    	xcb.time_since_start += dt;

		game_loop(
    		xcb.memory_pool,
    		xcb.memory_pool_bytes,
    		dt,
    		xcb.window_w,
    		xcb.window_h,
    		&xcb.input,
    		&xcb.render_list);

		// TODO - Open GL implementation + renderer front end, with the goal of atomizing the functions
		// of both GL and Vulkan sufficiently to develop a robust renderer front-end. The ideal of the
		// split is to conserve all possible performance characteristics of each API while minimizing the
		// redundancy in the two implementations.
		renderer_loop(&xcb.renderer, &xcb.render_list);
	}

	return 0;
}
