#include "particles.h"

void ParticleController::Render(ivec2 camera_pos) const
{
    static const Graphics::TextureAtlas::Region
        tex = texture_atlas.Get("particles.png"),
        tex_custom_size = texture_atlas.Get("particle_large_circle.png");

    constexpr int pixel_size = 8;
    constexpr int tex_frames = 5;

    for (std::size_t i = list.size(); i-- > 0;)
    {
        const Particle &par = list[i];

        bool has_custom_size = par.tex_index < 0;
        float custom_size = !has_custom_size ? 0 : -par.tex_index * (1 - par.time / float(par.max_time));

        if (((par.pos - camera_pos).abs() > screen_size/2 + (has_custom_size ? custom_size : pixel_size)/2).any())
            continue; // Not visible.

        int frame = clamp(par.time * tex_frames / par.max_time, 0, tex_frames-1);

        auto quad = r.iquad(iround(par.pos) - camera_pos, !has_custom_size ? tex.region(ivec2(frame, par.tex_index) * pixel_size, ivec2(pixel_size)) : tex_custom_size)
            .center()
            .color(!par.end_color ? par.color : mix(par.time / float(par.max_time), par.color, *par.end_color))
            .mix(0);

        if (has_custom_size)
            quad.scale(custom_size / tex_custom_size.size.x);
    }
}
