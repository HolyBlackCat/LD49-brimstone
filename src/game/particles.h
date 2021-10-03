#pragma once

#include "main.h"

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

    void Render(ivec2 camera_pos) const;

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

    void AddPlayerDeathFlame(fvec2 pos, fvec2 vel)
    {
        float c = frand <= 0.5;
        bool b = frand <= 1 < 0.25;
        Add(adjust(Particle{},
            pos = pos, vel = vel,
            color = b ? fvec3() : fvec3(1, c, 0),
            end_color = b ? fvec3() : fvec3(1, c + 0.5, 0),
            max_time = 40 <= irand <= 60,
            drag = 0.01,
        ));
    }

    void AddMapFlame(fvec2 pos, fvec2 vel)
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

    void AddSmallMapFlame(fvec2 pos, fvec2 vel)
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

    void AddBlueFlame(fvec2 pos, fvec2 vel)
    {
        float c = frand <= 0.5;
        Add(adjust(Particle{},
            pos = pos, vel = vel,
            color = fvec3(0, c, 1),
            end_color = fvec3(c * 0.5, 0.5 + c * 0.5, 1),
            max_time = 60 <= irand <= 90,
            drag = 0.01,
        ));
    }

    void AddSmallBlueFlame(fvec2 pos, fvec2 vel)
    {
        int max_time = 40 <= irand <= 60;
        float c = frand <= 1;
        Add(adjust(Particle{},
            pos = pos, vel = vel,
            color = fvec3(0, c, 1),
            end_color = fvec3(c * 0.5, 0.5 + c * 0.5, 1),
            time = max_time / 2,
            max_time = max_time,
            drag = 0.005,
        ));
    }

    void AddLongLivedBlueFlame(fvec2 pos, fvec2 vel)
    {
        float c = frand <= 0.5;
        Add(adjust(Particle{},
            pos = pos, vel = vel,
            color = fvec3(0, c, 1),
            end_color = fvec3(c * 0.5, 0.5 + c * 0.5, 1),
            max_time = 90 <= irand <= 180,
            drag = 0.01,
        ));
    }

    void AddMenuFlame(fvec2 pos, fvec2 vel)
    {
        float c = frand <= 0.5;
        Add(adjust(Particle{},
            pos = pos, vel = vel,
            color = fvec3(0, c, 1),
            end_color = fvec3(c * 0.5, 0.5 + c * 0.5, 1),
            max_time = 20 <= irand <= 30,
            drag = 0.01,
        ));
    }
};
