#pragma once

inline constexpr ivec2 screen_size = ivec2(480, 270);
extern const std::string_view window_name;

extern Interface::Window window;
extern Audio::SourceManager audio_controller;

extern const Graphics::ShaderConfig shader_config;
extern Interface::ImGuiController gui_controller;

namespace Fonts
{
    namespace Files
    {
        extern Graphics::FontFile main;
    }

    extern Graphics::Font main;
}

extern Graphics::TextureAtlas texture_atlas;
extern Graphics::Texture texture_main;

extern GameUtils::AdaptiveViewport adaptive_viewport;
extern Render r;

extern Input::Mouse mouse;

STRUCT( GameState POLYMORPHIC EXTENDS GameUtils::State::Base )
{
    virtual void Render() const = 0;
};

extern Random::Scalar<int> irand;
extern Random::Scalar<float> frand;
extern Random::Misc<float> miscrand;
