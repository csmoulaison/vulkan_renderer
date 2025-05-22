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
    (void)game;

	render_list->clear_color = vec3_new(0, 0, 0);
}
