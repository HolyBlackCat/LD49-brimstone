#include "game/main.h"
#include "game/map.h"
#include "game/particles.h"
#include "game/sky.h"
#include "game/sounds.h"

struct Controls
{
    std::vector<Input::Button> buttons_left = {Input::left, Input::a};
    std::vector<Input::Button> buttons_right = {Input::right, Input::d};
    std::vector<Input::Button> buttons_jump = {Input::z, Input::up, Input::w, Input::space};
    std::vector<Input::Button> buttons_restart = {Input::r};

    [[nodiscard]] bool ButtonActive(std::vector<Input::Button> Controls::*list, bool (Input::Button::*method)() const) const
    {
        return std::any_of((this->*list).begin(), (this->*list).end(), std::mem_fn(method));
    }

    [[nodiscard]] bool LeftDown      () const {return ButtonActive(&Controls::buttons_left   , &Input::Button::down   );}
    [[nodiscard]] bool RightDown     () const {return ButtonActive(&Controls::buttons_right  , &Input::Button::down   );}
    [[nodiscard]] bool JumpPressed   () const {return ButtonActive(&Controls::buttons_jump   , &Input::Button::pressed);}
    [[nodiscard]] bool JumpDown      () const {return ButtonActive(&Controls::buttons_jump   , &Input::Button::down   );}
    [[nodiscard]] bool RestartPressed() const {return ButtonActive(&Controls::buttons_restart, &Input::Button::pressed);}
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

struct Lamp
{
    ivec2 pos{};
    fvec2 vel{};
    fvec2 vel_lag{};

    bool lit = false;
    ivec2 flame_pos{};
};

struct Gate
{
    ivec2 pos{};
    fvec2 vel{};
    fvec2 vel_lag{};

    int ready_timer = 0;

    int finish_timer = 0;
};

struct SpikeBlock
{
    ivec2 pos{};
    float vel = 0;
    float vel_lag = 0;

    int height = 1;

    bool ground = true, prev_ground = true;

    std::shared_ptr<std::vector<SpikeBlock *>> same_x_spikes;

    [[nodiscard]] bool PixelTouches(ivec2 pixel_pos) const
    {
        return (pixel_pos >= pos with(y -= (height-1) * tile_size) - tile_size/2).all() && (pixel_pos < pos + tile_size/2).all();
    }

    [[nodiscard]] bool SolidAtOffset(const Map &map, ivec2 offset) const
    {
        // Only the first point is used for spike-spike collisions.
        const std::array<ivec2, 2> hitbox_points = {ivec2(0,5), ivec2(0,-8 - (height - 1) * tile_size)}; // Note `-8`, this lets spikes stick to the ceiling.

        return std::any_of(hitbox_points.begin(), hitbox_points.end(), [&](ivec2 point){return map.PixelIsSolid(pos + point + offset);})
            || std::any_of(same_x_spikes->begin(), same_x_spikes->end(), [&](const SpikeBlock *other){return other != this && other->PixelTouches(pos + hitbox_points.front() + offset);});
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

    static constexpr ivec2 hitbox_a = ivec2(-4, -5), hitbox_b = ivec2(3, 4);

    [[nodiscard]] bool SolidAtOffset(Map &map, ivec2 offset)
    {
        for (int point_y : {hitbox_a.y, hitbox_b.y})
        for (int point_x : {hitbox_a.x, hitbox_b.x})
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
                player,
                lamp,
                gate,
                spike,
                vignette
        )

        Atlas atlas;

        Map map;
        Sky sky;
        Player p;
        Controls con;
        ParticleController par;
        SceneSwitch scene_switch;

        ivec2 camera_pos{};

        std::vector<Lamp> lamps;
        Gate gate;

        std::vector<SpikeBlock> spike_blocks;

        [[nodiscard]] static std::string GetLevelFileName(int index)
        {
            return FMT("assets/maps/{}.json", index);
        }

        void Init() override
        {
            texture_atlas.InitRegions(atlas, ".png");
            map = Map(GetLevelFileName(level_index));
            camera_pos = map.cells.size() * tile_size / 2;

            // Player.
            p.pos = ivec2(map.points.GetSinglePoint("player"));

            // Gate.
            gate.pos = ivec2(map.points.GetSinglePoint("gate"));

            // Lamps.
            for (fvec2 lamp_pos : map.points.GetPointList("lamp"))
            {
                Lamp &new_lamp = lamps.emplace_back();
                new_lamp.pos = ivec2(lamp_pos / tile_size) * tile_size + tile_size/2 + ivec2(0, -7);
            }

            // Spikes.
            {
                map.points.ForEachPointWithNamePrefix("spike", [&](std::string_view suffix, fvec2 pos)
                {
                    SpikeBlock &new_spike = spike_blocks.emplace_back();
                    new_spike.pos = ivec2(pos / tile_size) * tile_size + tile_size/2;
                    if (!suffix.empty())
                        new_spike.height = Refl::FromString<int>(suffix);
                });

                // Sort spike blocks by Y. This ensures consistent fall pattern for stacks of spike blocks.
                std::sort(spike_blocks.begin(), spike_blocks.end(), [](const SpikeBlock &a, const SpikeBlock &b){return a.pos.y < b.pos.y;}); // Note `<`. This ensures gaps between falling blocks.

                // Group spike blocks by X coord.
                std::map<int, std::shared_ptr<std::vector<SpikeBlock *>>> spike_map;
                for (SpikeBlock &spike : spike_blocks)
                {
                    auto it = spike_map.find(spike.pos.x);
                    if (it == spike_map.end())
                        it = spike_map.try_emplace(spike.pos.x, std::make_shared<std::vector<SpikeBlock *>>()).first;

                    it->second->push_back(&spike);
                    spike.same_x_spikes = it->second;
                }
            }
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
                p_death_anim_len = 20,
                p_spike_hitbox_shrink_x = 2,
                lamp_hitbox_half_x = 6,
                lamp_hitbox_half_y_up = 2,
                lamp_hitbox_half_y_down = 10,
                gate_anim_delay = 30,
                gate_anim_len = 90,
                gate_hitbox_half = 6,
                gate_finish_anim_len = 120,
                gate_transition_delay = 60;

            constexpr ivec2
                gate_center_offset(0,-6);

            { // Switch between levels (must be first).
                if (auto next_level = scene_switch.ShouldSwitchToLevel())
                {
                    if (!std::filesystem::exists(GetLevelFileName(*next_level)))
                    {
                        next_state = "Final{}";
                        return;
                    }

                    *this = Game();
                    level_index = *next_level;
                    Init();
                    scene_switch.EnterSceneAnimation();
                }
            }

            sky.Move();
            map.Tick(par);
            par.Tick();
            scene_switch.Tick();

            { // Restart level using a hotkey.
                if (con.RestartPressed())
                    scene_switch.QueueSwitchToLevel(level_index);
            }

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

                { // Controls.
                    bool can_control = p.death_timer == 0 && gate.finish_timer == 0;

                    { // Walking.
                        int hc = can_control ? con.RightDown() - con.LeftDown() : 0;

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
                        if (p.ground &&can_control && con.JumpPressed())
                        {
                            CreateJumpEffect(false);
                            p.vel.y = -p_jump_speed;
                        }

                        if (!p.ground && p.vel.y < 0 && !con.JumpDown())
                            p.vel.y += p_extra_gravity_to_stop_jump;

                        if (p.ground && !p.prev_ground && p.prev2_vel.y >= p_min_y_vel_for_landing_effect)
                            CreateJumpEffect(true);
                    }
                }

                { // Spreading corruption.
                    if (p.death_timer == 0)
                    {
                        for (int point_y : {p.hitbox_a.y-1, p.hitbox_b.y+1})
                        for (int point_x : {p.hitbox_a.x-1, p.hitbox_b.x+1})
                            map.CorrputTile(div_ex(p.pos + ivec2(point_x, point_y), tile_size), Cell::num_corruption_stages);
                    }
                }

                { // Gravity.
                    p.vel.y += gravity;
                }

                { // Update position.
                    if (p.death_timer == 0 && gate.finish_timer == 0)
                    {
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

                { // Flame trail.
                    if (p.death_timer == 0 && gate.finish_timer == 0)
                        par.AddPlayerFlame(p.pos + fvec2(frand.abs() <= 4, frand.abs() <= 4), p.vel * 0.4 + fvec2(frand.abs() < 0.1, -(0.1 < frand < 1)));
                }

                { // Death conditions.
                    if (p.death_timer == 0 && gate.finish_timer == 0)
                    {
                        { // Falling off from the map.
                            constexpr int margin = tile_size / 2;

                            // Check all borders except the top one.
                            if ((p.pos >= map.cells.size() * tile_size + margin).any() || p.pos.x < -margin)
                                p.Kill();
                        }

                        { // Touching a spike tile.
                            for (int point_y : {p.hitbox_a.y, p.hitbox_b.y})
                            for (int point_x : {p.hitbox_a.x, p.hitbox_b.x})
                            {
                                ivec2 tile_pos = div_ex(p.pos + ivec2(point_x, point_y), tile_size);
                                if (map.cells.pos_in_range(tile_pos) && GetTileInfo(map.cells.unsafe_at(tile_pos).tile).kills)
                                    p.Kill();
                            }
                        }

                        { // Touching a spike object.
                            for (const SpikeBlock &spike : spike_blocks)
                            {
                                for (int point_y : {p.hitbox_a.y, p.hitbox_b.y})
                                for (int point_x : {p.hitbox_a.x+p_spike_hitbox_shrink_x, p.hitbox_b.x-p_spike_hitbox_shrink_x})
                                {
                                    if (spike.PixelTouches(p.pos + ivec2(point_x, point_y)))
                                        p.Kill();
                                }
                            }
                        }
                    }
                }

                { // Death effects.
                    if (p.death_timer)
                        p.death_timer++;

                    // Particles.
                    if (p.death_timer > 0 && p.death_timer <= p_death_anim_len)
                    {
                        for (int i = 0; i < 10; i++)
                            par.AddPlayerDeathFlame(p.pos + fvec2(frand.abs() <= 4, frand.abs() <= 4), fvec2::dir(mrand.angle(), frand <= mix(p.death_timer / float(p_death_anim_len), 3, 0)));
                    }

                    // Reload level.
                    if (p.death_timer > 60)
                        scene_switch.QueueSwitchToLevel(level_index);
                }
            }

            { // Spike blocks.
                for (SpikeBlock &spike : spike_blocks)
                {
                    spike.prev_ground = spike.ground;
                    spike.ground = spike.SolidAtOffset(map, ivec2(0, 1));

                    { // Landing sound.
                        if (spike.ground && !spike.prev_ground)
                            Sounds::spike_lands(spike.pos);
                    }

                    spike.vel += gravity;

                    { // Update position.
                        float clamped_vel = spike.vel;
                        clamp_var(clamped_vel, -p_vel_limit_y_up, p_vel_limit_y_down);

                        float eff_vel = clamped_vel + spike.vel_lag;
                        int int_vel = iround(eff_vel);
                        spike.vel_lag = eff_vel - int_vel;

                        if (abs(spike.vel_lag) > p_vel_lag_decr)
                            spike.vel_lag -= sign(spike.vel_lag) * p_vel_lag_decr;
                        else
                            spike.vel_lag = 0;

                        while (int_vel)
                        {
                            int s = sign(int_vel);

                            ivec2 offset{};
                            offset.y = s;

                            if (!spike.SolidAtOffset(map, offset))
                            {
                                spike.pos.y += s;
                                int_vel -= s;
                            }
                            else
                            {
                                int_vel = 0;
                                if (spike.vel * s > 0)
                                    spike.vel = 0;
                                if (spike.vel_lag * s > 0)
                                    spike.vel_lag = 0;
                            }
                        }
                    }
                }
            }

            { // Lamps.
                constexpr float
                    gate_flame_circle_rad = 5,
                    gate_flame_circle_speed = 0.01;

                int lamp_index = 0;
                for (Lamp &lamp : lamps)
                {
                    { // Start burning if touching player.
                        if (!lamp.lit)
                        {
                            fvec2 delta = lamp.pos - p.pos;
                            if (abs(delta.x) <= lamp_hitbox_half_x && delta.y >= -lamp_hitbox_half_y_down && delta.y <= lamp_hitbox_half_y_up)
                            {
                                lamp.lit = true;
                                lamp.flame_pos = lamp.pos;
                                Sounds::lamp_ignites(lamp.pos);

                                if (std::all_of(lamps.begin(), lamps.end(), std::mem_fn(&Lamp::lit)))
                                    gate.ready_timer = 1;

                                for (int i = 0; i < 10; i++)
                                    par.AddBlueFlame(lamp.pos + fvec2(frand.abs() <= 1.5, frand.abs() <= 1.5), fvec2::dir(mrand.angle(), frand <= 0.7) with(y -= 0.3));
                            }
                        }
                    }

                    { // Flame particles.
                        if (gate.finish_timer == 0 && lamp.lit && map.Time() % 2 == 0)
                        {
                            float t = clamp((gate.ready_timer - gate_anim_delay) / float(gate_anim_len));

                            fvec2 target = (gate.pos + gate_center_offset) with(x -= 0.5) + (lamps.size() <= 1 ? fvec2(0) : fvec2::dir(lamp_index / float(lamps.size()) * f_pi * 2 + map.Time() * gate_flame_circle_speed, gate_flame_circle_rad));

                            fvec2 final_pos = mix(smoothstep(t), lamp.flame_pos, target);

                            par.AddSmallBlueFlame(final_pos + fvec2(frand.abs() <= 1.5, frand.abs() <= 1.5) * (1 - t * 0.5), fvec2(frand.abs() <= 0.1, -0.6 <= frand <= 0) * (1 - t * 0.6));

                        }
                    }

                    lamp.vel.y += gravity;

                    { // Update position.
                        constexpr ivec2 hitpoint_offset(0, 12);

                        fvec2 clamped_vel = lamp.vel;
                        clamp_var(clamped_vel.y, -p_vel_limit_y_up, p_vel_limit_y_down);

                        fvec2 eff_vel = clamped_vel + lamp.vel_lag;
                        ivec2 int_vel = iround(eff_vel);
                        lamp.vel_lag = eff_vel - int_vel;

                        for (int i : {0, 1})
                        {
                            if (abs(lamp.vel_lag[i]) > p_vel_lag_decr)
                                lamp.vel_lag[i] -= sign(lamp.vel_lag[i]) * p_vel_lag_decr;
                            else
                                lamp.vel_lag[i] = 0;
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

                                if (!map.PixelIsSolid(lamp.pos + hitpoint_offset + offset))
                                {
                                    lamp.pos[i] += s;
                                    int_vel[i] -= s;
                                }
                                else
                                {
                                    int_vel[i] = 0;
                                    if (lamp.vel[i] * s > 0)
                                        lamp.vel[i] = 0;
                                    if (lamp.vel_lag[i] * s > 0)
                                        lamp.vel_lag[i] = 0;
                                }
                            }
                        }
                    }

                    lamp_index++;
                }
            }

            { // Gate.
                if (gate.ready_timer > 0)
                    gate.ready_timer++;
                if (gate.finish_timer > 0)
                    gate.finish_timer++;

                { // Player interactions.
                    if (gate.finish_timer == 0 && gate.ready_timer > gate_anim_delay + gate_anim_len && ((p.pos - gate.pos).abs() <= gate_hitbox_half).all())
                    {
                        gate.finish_timer = 1;
                        Sounds::gate_entered({});
                    }
                }

                { // Effects.
                    if (gate.ready_timer == gate_anim_delay)
                        Sounds::lamp_fire_moves(gate.pos);

                    if (gate.ready_timer == gate_anim_delay + gate_anim_len)
                    {
                        Sounds::gate_ignites(gate.pos + gate_center_offset);
                        for (int i = 0; i < 30; i++)
                        {
                            fvec2 dir = fvec2::dir(mrand.angle());
                            par.AddBlueFlame(gate.pos + gate_center_offset + dir * (frand <= 5), dir * (frand <= 0.3) + fvec2(0, -0.15));
                        }
                    }

                    if (gate.finish_timer > 0 && gate.finish_timer < gate_finish_anim_len && gate.finish_timer % 3 == 0)
                    {
                        for (int i = 0; i < 5; i++)
                        {
                            fvec2 dir = fvec2::dir(mrand.angle()) * clamp_min(1 - gate.finish_timer / gate_finish_anim_len);
                            par.AddLongLivedBlueFlame(gate.pos + gate_center_offset + dir * (frand <= 8), dir * (frand <= 0.3));
                        }
                    }
                }

                { // Level transition.
                    if (gate.finish_timer == gate_transition_delay)
                        scene_switch.QueueSwitchToLevel(level_index + 1);
                }

                { // Update position.
                    constexpr ivec2 hitpoint_offset(0, 6);

                    fvec2 clamped_vel = gate.vel;
                    clamp_var(clamped_vel.y, -p_vel_limit_y_up, p_vel_limit_y_down);

                    fvec2 eff_vel = clamped_vel + gate.vel_lag;
                    ivec2 int_vel = iround(eff_vel);
                    gate.vel_lag = eff_vel - int_vel;

                    for (int i : {0, 1})
                    {
                        if (abs(gate.vel_lag[i]) > p_vel_lag_decr)
                            gate.vel_lag[i] -= sign(gate.vel_lag[i]) * p_vel_lag_decr;
                        else
                            gate.vel_lag[i] = 0;
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

                            if (!map.PixelIsSolid(gate.pos + hitpoint_offset + offset))
                            {
                                gate.pos[i] += s;
                                int_vel[i] -= s;
                            }
                            else
                            {
                                int_vel[i] = 0;
                                if (gate.vel[i] * s > 0)
                                    gate.vel[i] = 0;
                                if (gate.vel_lag[i] * s > 0)
                                    gate.vel_lag[i] = 0;
                            }
                        }
                    }
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
            constexpr int
                player_tex_size = 12;

            constexpr float
                outline_alpha = 0.3;


            Graphics::SetClearColor(fvec3(0));
            Graphics::Clear();
            r.BindShader();

            sky.Render();

            { // Map.
                r.Finish();

                // Render the map itself to a separate texture.
                framebuffer_map.Bind();
                Graphics::SetClearColor(fvec4(0));
                Graphics::Clear();
                map.Render(camera_pos);
                r.Finish();
                Graphics::Blending::Func(Graphics::Blending::one, Graphics::Blending::one_minus_src_a, Graphics::Blending::zero, Graphics::Blending::one);
                map.RenderCorruption(camera_pos);
                r.Finish();
                Graphics::Blending::FuncNormalPre();

                { // Objects that should have outline.
                    { // Lamps.
                        for (const Lamp &lamp : lamps)
                        {
                            r.iquad(lamp.pos - camera_pos, atlas.lamp).center();
                        }
                    }

                    { // Gate.
                        r.iquad(gate.pos with(y -= 6) - camera_pos, atlas.gate).center();
                    }
                }
                r.Finish();

                // Render that texture to the main framebuffer, with outline.
                adaptive_viewport.GetFrameBuffer().Bind();
                r.SetTexture(framebuffer_texture_map);
                for (int i = 0; i < 4; i++)
                    r.iquad(ivec2::dir4(i), screen_size).tex(ivec2(0)).color(fvec3(0)).mix(0).center().flip_y().alpha(outline_alpha);
                r.iquad(ivec2(0), screen_size).tex(ivec2(0)).center().flip_y();
                r.Finish();

                r.SetTexture(texture_main);
            }

            float p_alpha = clamp_min(1 - p.death_timer / 15. - gate.finish_timer / 15);

            { // Player (before particles).
                int frame = p.walk_timer == 0 ? 0 : p.walk_timer / 5 % 2 + 1;
                r.iquad(p.pos - camera_pos, atlas.player.region(ivec2(0, player_tex_size * frame), ivec2(player_tex_size))).center().flip_x(p.left).color(fvec3(1, frand <= 1, 0)).mix(0).alpha(p_alpha);
            }

            par.Render(camera_pos);

            { // Player (after particles).
                r.iquad(p.pos - camera_pos + ivec2(p.left ? -1 : 0, 0), atlas.player.region(ivec2(player_tex_size, 0), ivec2(player_tex_size))).center().alpha(p_alpha);
            }

            { // Spike blocks.
                for (const SpikeBlock &spike : spike_blocks)
                {
                    for (int i = 0; i < spike.height; i++)
                        r.iquad(spike.pos with(y -= i * tile_size) - camera_pos, atlas.spike.region(ivec2(0, tile_size * (spike.height <= 1 ? 0 : i == 0 ? 3 : i == spike.height-1 ? 1 : 2)), ivec2(tile_size))).center();
                }
            }

            { // Vignette.
                r.iquad(ivec2(0), atlas.vignette).center().alpha(0.5);
            }

            // Scene transition.
            scene_switch.Render();

            r.Finish();
        }
    };
}
