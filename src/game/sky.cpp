#include "sky.h"

#include "game/main.h"

void Sky::Move()
{
    time++;
}

void Sky::Render() const
{
    static const Graphics::TextureAtlas::Region
        bg_tex = texture_atlas.Get("sky_bg.png"),
        clouds_tex = texture_atlas.Get("sky_clouds.png");

    static const std::vector<float> visual_distances = {600, 350, 100};
    constexpr float speed_factor = 100;

    r.iquad(ivec2(), bg_tex).center();

    int layer_index = 0;
    for (float layer_dist : visual_distances)
    {
        int offset = mod_ex(int(time / layer_dist * speed_factor), screen_size.x);

        auto layer_region = clouds_tex.region(ivec2(0, screen_size.y / 2 * layer_index), screen_size with(y /= 2));
        r.iquad(-screen_size/2 + ivec2(offset, 0), layer_region);
        r.iquad(-screen_size/2 + ivec2(offset - screen_size.x, 0), layer_region);

        r.iquad(screen_size with(y = 0, x /= -2, x += offset), layer_region).flip_x().flip_y();
        r.iquad(screen_size with(y = 0, x /= -2, x += offset - screen_size.x), layer_region).flip_x().flip_y();

        layer_index++;
    }
}
