#include "game/main.h"
#include "game/particles.h"

namespace States
{
    STRUCT( Final EXTENDS GameState )
    {
        MEMBERS()

        struct Label
        {
            ivec2 pos{};
            Graphics::Text text;
            ivec2 size{};

            Label(ivec2 new_pos, Graphics::Text new_text) : pos(new_pos), text(std::move(new_text))
            {
                size = text.ComputeStats().size;
            }
        };

        static constexpr int
            any_key_delay = 360,
            any_key_anim_len = 150;

        int timer = 0;

        std::vector<Label> labels;

        ParticleController par;

        void Init() override
        {
            labels.emplace_back(ivec2(0, -32), Graphics::Text(Fonts::main, "The end"));

            labels.emplace_back(ivec2(0, 0), Graphics::Text(Fonts::main, "Thanks for playing!"));
        }

        void Tick(std::string &next_state) override
        {
            timer++;

            par.Tick();

            for (Label &label : labels)
            {
                for (int i = 0; i < label.size.x / 20; i++)
                {
                    if (irand <= 360 < timer)
                        par.AddBlueFlame(label.pos + fvec2(frand.abs() <= label.size.x/2, frand.abs() <= label.size.y/2), fvec2::dir(mrand.angle(), frand <= 0.3) with(y -= 0.15));
                }
            }

            if (timer >= any_key_delay + 30 && Input::Button().AssignKey())
                next_state = "0";
        }

        void Render() const override
        {
            fvec3 bg_color = fvec3(25, 13, 43) / 255 * clamp_min(1 - timer / 60.);

            Graphics::SetClearColor(bg_color);
            Graphics::Clear();

            r.BindShader();

            // Text outline.
            for (const Label &label : labels)
            {
                for (int i = 0; i < 4; i++)
                    r.itext(label.pos + ivec2::dir4(i), label.text).color(clamp_min(fvec3(0, 0.3, 0.6) * clamp_max(timer / 540.), bg_color));
            }

            // Particles.
            par.Render(ivec2(0));

            // Text.
            for (const Label &label : labels)
                r.itext(label.pos, label.text).color(bg_color);

            { // More custom text.
                r.itext(ivec2(0,48), Graphics::Text(Fonts::main, "Press any key to quit")).color(fvec3(0.5)).alpha(clamp((timer - any_key_delay) / float(any_key_anim_len)));
            }

            r.Finish();
        }
    };
}
