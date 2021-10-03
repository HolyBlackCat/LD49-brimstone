#include "game/main.h"
#include "game/particles.h"
#include "game/progress.h"
#include "game/sky.h"
#include "game/sounds.h"

static const std::string version =
#include "../version.txt"
;

namespace States
{
    STRUCT( Menu EXTENDS GameState )
    {
        MEMBERS()

        struct Controls
        {
            std::vector<Input::Button> buttons_up = {Input::up, Input::w};
            std::vector<Input::Button> buttons_down = {Input::down, Input::s};
            std::vector<Input::Button> buttons_ok = {Input::z, Input::space, Input::enter};

            [[nodiscard]] bool ButtonActive(std::vector<Input::Button> Controls::*list, bool (Input::Button::*method)() const) const
            {
                return std::any_of((this->*list).begin(), (this->*list).end(), std::mem_fn(method));
            }

            [[nodiscard]] bool UpPressed  () const {return ButtonActive(&Controls::buttons_up  , &Input::Button::pressed);}
            [[nodiscard]] bool DownPressed() const {return ButtonActive(&Controls::buttons_down, &Input::Button::pressed);}
            [[nodiscard]] bool OkPressed  () const {return ButtonActive(&Controls::buttons_ok  , &Input::Button::pressed);}
        };

        struct Entry
        {
            ivec2 pos{};
            Graphics::Text text;
            ivec2 size{};
            std::function<void(std::string &)> func;

            Entry(ivec2 new_pos, Graphics::Text new_text, std::function<void(std::string &)> new_func)
                : pos(new_pos), text(std::move(new_text)), func(std::move(new_func))
            {
                size = text.ComputeStats().size;
            }
        };

        Sky sky;
        ParticleController par;

        Controls con;

        std::vector<Entry> entries;

        int active_entry = 0;

        int fade_in_timer = 0;
        int fade_out_timer = 0;

        std::function<void(std::string &)> queued_func;

        SavedProgress saved_progress;

        Graphics::Text title_text;
        ivec2 title_size{};
        ivec2 title_pos = ivec2(0,-4);

        void Init() override
        {
            { // Load the saved progress.
                try
                {
                    saved_progress = Refl::FromString<SavedProgress>(Stream::Input(SavedProgress::path));
                }
                catch (...) {}
            }

            { // Title.
                title_text = Graphics::Text(Fonts::main, "BRIMSTONE");
                title_size = title_text.ComputeStats().size;
            }

            { // Menu entries.
                constexpr int
                    button_spacing = 24;

                int y = 48;

                if (saved_progress.level > 1)
                {
                    entries.emplace_back(ivec2(0, y), Graphics::Text(Fonts::main, "Continue"), [&](std::string &next_state)
                    {
                        next_state = FMT("Game{{camera_starts_above=true,initial_sky_pos={},level_index={}}}", sky.time, saved_progress.level);
                    });
                    y += button_spacing;
                }

                entries.emplace_back(ivec2(0, y), Graphics::Text(Fonts::main, "New game"), [&](std::string &next_state)
                {
                    next_state = FMT("Game{{camera_starts_above=true,initial_sky_pos={}}}", sky.time);
                });
                y += button_spacing;
                entries.emplace_back(ivec2(0, y), Graphics::Text(Fonts::main, "Quit"), [](std::string &next_state)
                {
                    next_state = "0";
                });
                y += button_spacing;
            }
        }

        void Tick(std::string &next_state) override
        {
            fade_in_timer++;
            if (queued_func)
                fade_out_timer++;

            sky.Move();
            par.Tick();

            // Change active entry.
            if (!queued_func)
            {
                if (auto control = con.DownPressed() - con.UpPressed())
                {
                    active_entry += control;
                    active_entry = mod_ex(active_entry, int(entries.size()));
                    Sounds::click({});
                }
            }

            Entry &selected = entries[active_entry];

            { // Triggeg the active entry.
                if (!queued_func && con.OkPressed())
                {
                    queued_func = selected.func;
                    Sounds::click_confirm({});
                }
            }

            { // Particle effects for the active entry.
                if (!queued_func)
                {
                    for (int s : {-1, 1})
                        par.AddMenuFlame(selected.pos with(y += 2, x += (selected.size.x / 2 + 16) * s) + fvec2(frand.abs() <= 1.5, frand.abs() <= 1.5), fvec2::dir(mrand.angle(), frand <= 0.4) with(y -= 0.3));
                }
            }

            { // Particle effects for the title.
                if (!queued_func)
                {
                    for (int i = 0; i < 4; i++)
                        par.AddMapFlame(title_pos + fvec2(frand.abs() <= title_size.x/2, frand.abs() <= title_size.y/2), fvec2::dir(mrand.angle(), frand <= 0.4) with(y -= 0.2));
                }
            }

            if (fade_out_timer > 60)
                queued_func(next_state);
        }

        void Render() const override
        {
            Graphics::SetClearColor(fvec3(0));
            Graphics::Clear();

            r.BindShader();

            sky.Render();
            par.Render(ivec2(0));

            float text_alpha = clamp_min(1 - fade_out_timer / 30.);

            // Title.
            r.itext(title_pos, title_text).color(fvec3(0)).alpha(text_alpha);

            // Buttons.
            for (const Entry &e : entries)
                r.itext(e.pos, e.text).color(fvec3(1)).alpha(text_alpha);

            // Author info.
            r.itext(ivec2(0, screen_size.y/2), Graphics::Text(Fonts::main, "v" + version + ", Oct 2021, by HolyBlackCat for LD49")).color(fvec3(46,45,108)/255).alpha(1).align(ivec2(0,1));

            { // Vignette.
                static const Graphics::TextureAtlas::Region vignette = texture_atlas.Get("vignette.png");
                r.iquad(ivec2(0), vignette).center().alpha(0.5);
            }

            // Fade-in.
            r.iquad(ivec2(0), screen_size).color(fvec3(0)).alpha(smoothstep(clamp(1 - fade_in_timer / 60.))).center();

            r.Finish();
        }
    };
}
