#include "game/main.h"
#include "game/map.h"
#include "game/sky.h"

struct Player
{
    ivec2 pos{};
    fvec2 vel{}, prev_vel{}, prev2_vel{};
    fvec2 vel_lag{};
    bool left = false;

    bool ground = false, prev_ground = false;

    [[nodiscard]] bool SolidAtOffset(Map &map, ivec2 offset)
    {
        for (int point_y : {-5, 4})
        for (int point_x : {-4, 3})
        {
            ivec2 point(point_x, point_y);

            if (map.PixelIsSolid(pos + offset + point))
                return true;
        }

        return false;
    }
};

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
    int tex_index = 0;
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
        static const Graphics::TextureAtlas::Region tex = texture_atlas.Get("particles.png");

        constexpr int pixel_size = 8;
        constexpr int tex_frames = 5;

        for (std::size_t i = list.size(); i-- > 0;)
        {
            const Particle &par = list[i];

            if (((par.pos - camera_pos).abs() > screen_size/2 + pixel_size/2).any())
                continue; // Not visible.

            int frame = clamp(par.time * tex_frames / par.max_time, 0, tex_frames-1);

            r.iquad(iround(par.pos) - camera_pos, tex.region(ivec2(frame, par.tex_index) * pixel_size, ivec2(pixel_size))).center().color(!par.end_color ? par.color : mix(par.time / float(par.max_time), par.color, *par.end_color)).mix(0);
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
};

namespace States
{
    STRUCT( Game EXTENDS GameState )
    {
        MEMBERS()

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

        ivec2 camera_pos{};


        void Init() override
        {
            texture_atlas.InitRegions(atlas, ".png");
            map = Map("assets/maps/1.json");
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
                p_min_y_vel_for_landing_effect = 1.5;

            sky.Move();
            par.Tick();

            { // Player.
                p.prev2_vel = p.prev_vel;
                p.prev_vel = p.vel;

                p.prev_ground = p.ground;
                p.ground = p.SolidAtOffset(map, ivec2(0,1));

                auto CreateJumpEffect = [&](bool landing)
                {
                    for (int i = 0; i < 10; i++)
                        par.AddSmallPlayerFlame(p.pos + fvec2(frand.abs() <= 3, 5), fvec2(frand.abs() <= 1, (-0.8 <= frand <= 0.16) * (landing ? -1 : 1)));
                };

                { // Flame trail
                    par.AddPlayerFlame(p.pos + fvec2(frand.abs() <= 4, frand.abs() <= 4), p.vel * 0.4 + fvec2(frand.abs() < 0.1, -(0.1 < frand < 1)));
                }

                { // Controls.
                    { // Walking.
                        int hc = con.right.down() - con.left.down();

                        if (hc)
                        {
                            clamp_var_abs(p.vel.x += p_walk_acc * hc, p_walk_speed);
                            p.left = hc < 0;
                        }
                        else
                        {
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
            }
        }

        void Render() const override
        {
            constexpr int player_tex_size = 12;

            Graphics::SetClearColor(fvec3(25, 13, 43) / 255);
            Graphics::Clear();
            r.BindShader();

            sky.Render();
            map.Render(camera_pos);

            { // Player (before particles).
                r.iquad(p.pos - camera_pos, atlas.player.region(ivec2(), ivec2(player_tex_size))).center().flip_x(p.left).color(fvec3(1, frand <= 1, 0)).mix(0);
            }

            par.Render(camera_pos);

            { // Player (after particles).
                r.iquad(p.pos - camera_pos + ivec2(p.left ? -1 : 0, 0), atlas.player.region(ivec2(player_tex_size, 0), ivec2(player_tex_size))).center();
            }

            r.Finish();
        }
    };
}
