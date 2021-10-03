#pragma once

#include "game/main.h"

namespace Sounds
{
    #define SOUNDS_LIST(X) \
        X( player_jumps   , 0.3 ) \
        X( player_lands   , 0.3 ) \
        X( player_dies    , 0.3 ) \
        X( block_explodes , 0.6 ) \
        X( lamp_ignites   , 0.3 ) \
        X( lamp_fire_moves, 0.2 ) \
        X( gate_ignites   , 0.2 ) \
        X( gate_entered   , 0.2 ) \
        X( spike_lands    , 0.2 ) \
        X( click          , 0.2 ) \
        X( click_confirm  , 0.2 ) \

    #define SOUND_FUNC(name, rand)                                           \
        inline void name( std::optional<fvec2> pos, float vol = 1, float pitch = 0) \
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
