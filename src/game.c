#include "input.c"

typedef struct
{
	double time_since_initialize;
} GameMemory;

void game_initialize(void* mem, uint32_t mem_bytes)
{
	srand(time(NULL));

    GameMemory* game = (GameMemory*)mem;

    game->time_since_initialize = 0;
}

void game_loop(
    void*                memory,
    size_t               memory_bytes,
    float                dt,
    uint32_t             window_w,
    uint32_t             window_h,
    InputContext*        input,
    RenderList*          render_list)
{
    GameMemory* game = (GameMemory*)memory;

    game->time_since_initialize += dt;

	render_list->clear_color     = vec3_new(0, 0,  0);
	render_list->camera_position = vec3_new(0, 0, -3);
	render_list->camera_target   = vec3_new(0, 0,  0);
	//render_list->cube_transform  = ???; // NOW
}
