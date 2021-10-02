#include "game/main.h"
#include "game/map.h"
#include "game/sky.h"

namespace Sounds
{
    #define SOUNDS_LIST(X) \
        X( player_jumps, 0.3 ) \
        X( player_lands, 0.3 ) \
        X( player_dies , 0.3 ) \

    #define SOUND_FUNC(name, rand)                                           \
        void name( std::optional<fvec2> pos, float vol = 1, float pitch = 0) \
        {                                                                    \
            auto src = audio_controller.Add(Audio::File<#name>());           \
            if (pos)                                                         \
                src->pos(*pos);                                              \
            else                                                             \
                src->relative();                                             \
            src->volume(vol);                                                \
            src->pitch(pow(2, pitch + (frand.abs() <= rand)));               \
            src->play();                                                     \
        }                                                                    \

    SOUNDS_LIST(SOUND_FUNC)
    #undef SOUND_FUNC
}

struct Controls
{
    Input::Button left = Input::left;
    Input::Button right = Input::right;
    Input::Button jump = Input::z;
};

struct Particle
{
    fvec2 pos{};
    fvec2 vel{};
    fvec2 acc{};
    float drag = 0;
    int max_time = 0;
    int time = 0;
    int tex_index = 0; // If negative, treated as the custom size.
    fvec3 color;
    std::optional<fvec3> end_color;
};

class ParticleController
{
    std::vector<Particle> list;

  public:
    ParticleController() {}

    void Tick()
    {
        for (Particle &par : list)
        {
            par.time++;
            par.vel += par.acc;
            par.vel *= 1 - par.drag;
            par.pos += par.vel;
        }

        std::erase_if(list, [](const Particle &par){return par.time >= par.max_time;});
    }

    void Render(ivec2 camera_pos) const
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

    void Add(const Particle &par)
    {
        list.push_back(par);
    }

    void AddPlayerFlame(fvec2 pos, fvec2 vel)
    {
        float c = frand <= 0.5;
        Add(adjust(Particle{},
            pos = pos, vel = vel,
            color = fvec3(1, c, 0),
            end_color = fvec3(1, c + 0.5, 0),
            max_time = 20 <= irand <= 40,
            drag = 0.01,
        ));
    }

    void AddSmallPlayerFlame(fvec2 pos, fvec2 vel)
    {
        int max_time = 40 <= irand <= 60;
        float c = frand <= 0.5;
        Add(adjust(Particle{},
            pos = pos, vel = vel,
            color = fvec3(1, c, 0),
            end_color = fvec3(1, c + 0.5, 0),
            time = max_time / 2,
            max_time = max_time,
            drag = 0.01,
        ));
    }

    void AddLongLivedPlayerFlame(fvec2 pos, fvec2 vel)
    {
        float c = frand <= 0.5;
        Add(adjust(Particle{},
            pos = pos, vel = vel,
            color = fvec3(1, c, 0),
            end_color = fvec3(1, c + 0.5, 0),
            max_time = 60 <= irand <= 90,
            drag = 0.01,
        ));
    }
};

class SceneSwitch
{
    static constexpr float timer_step = 0.02;
    float timer = 0;

    std::optional<int> next_level;

  public:
    SceneSwitch() {}

    void Tick()
    {
        clamp_var(timer += (next_level ? 1 : -1) * timer_step);
    }

    void Render() const
    {
        constexpr fvec3 color = fvec3(25, 13, 43) / 255;
        float t = clamp(smoothstep(timer) * 1.1);
        r.iquad(-screen_size/2, ivec2(screen_size.x, t * screen_size.y / 2)).color(color);
        r.iquad(screen_size/2, -ivec2(screen_size.x, t * screen_size.y / 2)).color(color);
    }

    // Returns non-null after "exit scene" animation finishes playing.
    [[nodiscard]] std::optional<int> ShouldSwitchToLevel() const
    {
        return next_level && timer >= 1 ? next_level : std::nullopt;
    }

    // Play "exit scene" animation.
    void QueueSwitchToLevel(int level_index)
    {
        if (next_level)
            return; // Already switching, refuse to change target.

        next_level = level_index;
    }

    // Play "enter scene" animation.
    void EnterSceneAnimation()
    {
        timer = 1;
    }
};

struct Player
{
    ivec2 pos{};
    fvec2 vel{}, prev_vel{}, prev2_vel{};
    fvec2 vel_lag{};
    bool left = false;

    bool ground = false, prev_ground = false;

    int walk_timer = 0;

    int death_timer = 0;

    void Kill()
    {
        if (death_timer > 0)
            return; // Already dead.

        death_timer = 1;
        Sounds::player_dies(pos);
    }

    [[nodiscard]] bool SolidAtOffset(Map &map, ivec2 offset)
    {
        for (int point_y : {-5, 4})
        for (int point_x : {-4, 3})
        {
            ivec2 point = pos + offset + ivec2(point_x, point_y);

            if (map.PixelIsSolid(point))
                return true;
        }

        return false;
    }
};

namespace States
{
    STRUCT( Game EXTENDS GameState )
    {
        MEMBERS(
            DECL(int INIT=1 ATTR Refl::Optional) level_index
        )

        SIMPLE_STRUCT( Atlas
            DECL(Graphics::TextureAtlas::Region)
                player
        )

        Atlas atlas;

        Map map;
        Sky sky;
        Player p;
        Controls con;
        ParticleController par;
        SceneSwitch scene_switch;

        ivec2 camera_pos{};


        void Init() override
        {
            texture_atlas.InitRegions(atlas, ".png");
            map = Map(FMT("assets/maps/{}.json", level_index));
            camera_pos = map.cells.size() * tile_size / 2;

            p.pos = ivec2(map.points.GetSinglePoint("player") / tile_size) * tile_size + tile_size/2;
        }

        void Tick(std::string &next_state) override
        {
            (void)next_state;

            constexpr float
                gravity = 0.1,
                p_extra_gravity_to_stop_jump = 0.25,
                p_walk_speed = 1.5,
                p_walk_acc = 0.4,
                p_walk_dec = 0.4,
                p_walk_dec_air = 0.3,
                p_jump_speed = 3.5,
                p_vel_limit_y_up = 2.5,
                p_vel_limit_y_down = 3.5,
                p_vel_lag_decr = 0.05,
                p_min_y_vel_for_landing_effect = 1.5,
                listener_dist = screen_size.x * 2.5;

            constexpr int
                p_death_anim_len = 20;

            { // Switch between levels (must be first).
                if (auto next_level = scene_switch.ShouldSwitchToLevel())
                {
                    *this = Game();
                    level_index = *next_level;
                    Init();
                    scene_switch.EnterSceneAnimation();
                }
            }

            sky.Move();
            par.Tick();
            scene_switch.Tick();

            { // Player.
                p.prev2_vel = p.prev_vel;
                p.prev_vel = p.vel;

                p.prev_ground = p.ground;
                p.ground = p.SolidAtOffset(map, ivec2(0,1));

                auto CreateJumpEffect = [&](bool landing)
                {
                    for (int i = 0; i < 10; i++)
                        par.AddSmallPlayerFlame(p.pos + fvec2(frand.abs() <= 3, 5), fvec2(frand.abs() <= 1, (-0.8 <= frand <= 0.16) * (landing ? -1 : 1)));
                    if (landing)
                        Sounds::player_lands(p.pos);
                    else
                        Sounds::player_jumps(p.pos);
                };

                { // Flame trail.
                    if (p.death_timer == 0)
                        par.AddPlayerFlame(p.pos + fvec2(frand.abs() <= 4, frand.abs() <= 4), p.vel * 0.4 + fvec2(frand.abs() < 0.1, -(0.1 < frand < 1)));
                }

                { // Controls.
                    { // Walking.
                        int hc = con.right.down() - con.left.down();

                        if (hc)
                        {
                            clamp_var_abs(p.vel.x += p_walk_acc * hc, p_walk_speed);
                            p.left = hc < 0;
                            p.walk_timer++;
                        }
                        else
                        {
                            p.walk_timer = 0;
                            float dec = p.ground ? p_walk_dec : p_walk_dec_air;
                            if (abs(p.vel.x) <= dec)
                                p.vel.x = 0;
                            else
                                p.vel.x -= sign(p.vel.x) * dec;
                        }
                    }

                    { // Jumping.
                        if (p.ground && con.jump.pressed())
                        {
                            CreateJumpEffect(false);
                            p.vel.y = -p_jump_speed;
                        }

                        if (!p.ground && p.vel.y < 0 && !con.jump.down())
                            p.vel.y += p_extra_gravity_to_stop_jump;

                        if (p.ground && !p.prev_ground && p.prev2_vel.y >= p_min_y_vel_for_landing_effect)
                            CreateJumpEffect(true);
                    }
                }

                { // Gravity.
                    p.vel.y += gravity;
                }

                { // Update position.
                    fvec2 clamped_vel = p.vel;
                    clamp_var(clamped_vel.y, -p_vel_limit_y_up, p_vel_limit_y_down);

                    fvec2 eff_vel = clamped_vel + p.vel_lag;
                    ivec2 int_vel = iround(eff_vel);
                    p.vel_lag = eff_vel - int_vel;

                    for (int i : {0, 1})
                    {
                        if (abs(p.vel_lag[i]) > p_vel_lag_decr)
                            p.vel_lag[i] -= sign(p.vel_lag[i]) * p_vel_lag_decr;
                        else
                            p.vel_lag[i] = 0;
                    }

                    while (int_vel)
                    {
                        for (int i : {0, 1})
                        {
                            if (int_vel[i] == 0)
                                continue;

                            int s = sign(int_vel[i]);

                            ivec2 offset{};
                            offset[i] = s;

                            if (!p.SolidAtOffset(map, offset))
                            {
                                p.pos[i] += s;
                                int_vel[i] -= s;
                            }
                            else
                            {
                                int_vel[i] = 0;
                                if (p.vel[i] * s > 0)
                                    p.vel[i] = 0;
                                if (p.vel_lag[i] * s > 0)
                                    p.vel_lag[i] = 0;
                            }
                        }
                    }
                }

                { // Death conditions.
                    { // Falling off from the map.
                        constexpr int margin = tile_size / 2;

                        // Check all borders except the top one.
                        if ((p.pos >= map.cells.size() * tile_size + margin).any() || p.pos.x < -margin)
                            p.Kill();
                    }

                    if (mouse.left.pressed())
                        p.Kill();
                }

                { // Death effects
                    if (p.death_timer)
                        p.death_timer++;

                    // Particles.
                    if (p.death_timer > 0 && p.death_timer <= p_death_anim_len)
                    {
                        for (int i = 0; i < 10; i++)
                            par.AddLongLivedPlayerFlame(p.pos + fvec2(frand.abs() <= 4, frand.abs() <= 4), fvec2::dir(mrand.angle(), frand <= mix(p.death_timer / float(p_death_anim_len), 2, 1)));
                    }

                    // Reload level.
                    if (p.death_timer > 60)
                        scene_switch.QueueSwitchToLevel(level_index);
                }
            }

            { // Camera.
                Audio::ListenerPosition(fvec2(camera_pos).to_vec3(-listener_dist));
                Audio::ListenerOrientation(fvec3(0,0,1), fvec3(0,-1,0));
                Audio::Source::DefaultRefDistance(listener_dist);
            }
        }

        void Render() const override
        {
            constexpr int player_tex_size = 12;

            Graphics::SetClearColor(fvec3(0));
            Graphics::Clear();
            r.BindShader();

            sky.Render();

            { // Render the map.
                r.Finish();

                // Render the map itself to a separate texture.
                framebuffer_map.Bind();
                Graphics::SetClearColor(fvec4(0));
                Graphics::Clear();
                map.Render(camera_pos);
                r.Finish();

                // Render that texture to the main framebuffer, with outline.
                adaptive_viewport.GetFrameBuffer().Bind();
                r.SetTexture(framebuffer_texture_map);
                for (int i = 0; i < 4; i++)
                    r.iquad(ivec2::dir4(i), screen_size).tex(ivec2(0)).color(fvec3(0)).mix(0).center().flip_y().alpha(0.3);
                r.iquad(ivec2(0), screen_size).tex(ivec2(0)).center().flip_y();
                r.Finish();

                r.SetTexture(texture_main);
            }


            float p_alpha = clamp_min(1 - p.death_timer / 15.);

            { // Player (before particles).
                int frame = p.walk_timer == 0 ? 0 : p.walk_timer / 5 % 2 + 1;
                r.iquad(p.pos - camera_pos, atlas.player.region(ivec2(0, player_tex_size * frame), ivec2(player_tex_size))).center().flip_x(p.left).color(fvec3(1, frand <= 1, 0)).mix(0).alpha(p_alpha);
            }

            par.Render(camera_pos);

            { // Player (after particles).
                r.iquad(p.pos - camera_pos + ivec2(p.left ? -1 : 0, 0), atlas.player.region(ivec2(player_tex_size, 0), ivec2(player_tex_size))).center().alpha(p_alpha);
            }

            scene_switch.Render();

            r.Finish();
        }
    };
}
