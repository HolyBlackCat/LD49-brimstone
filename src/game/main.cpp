#include "main.h"

const std::string_view window_name = "LD49";

Interface::Window window(std::string(window_name), screen_size * 2, Interface::windowed, adjust_(Interface::WindowSettings{}, min_size = screen_size));
static Graphics::DummyVertexArray dummy_vao = nullptr;

static Audio::Context audio_context = nullptr;
Audio::SourceManager audio_controller;

const Graphics::ShaderConfig shader_config = Graphics::ShaderConfig::Core();
Interface::ImGuiController gui_controller(Poly::derived<Interface::ImGuiController::GraphicsBackend_Modern>, adjust_(Interface::ImGuiController::Config{}, shader_header = shader_config.common_header));

namespace Fonts
{
    namespace Files
    {
        Graphics::FontFile main("assets/CatIV15.ttf", 15);
    }

    Graphics::Font main;
}

Graphics::TextureAtlas texture_atlas = []{
    Graphics::TextureAtlas ret(ivec2(2048), "assets/_images", "assets/atlas.png", "assets/atlas.refl", {{"/font_storage", ivec2(256)}});
    auto font_region = ret.Get("/font_storage");

    Unicode::CharSet glyph_ranges;
    glyph_ranges.Add(Unicode::Ranges::Basic_Latin);

    Graphics::MakeFontAtlas(ret.GetImage(), font_region.pos, font_region.size, {
        {Fonts::main, Fonts::Files::main, glyph_ranges, Graphics::FontFile::hinting_mode_light},
    });
    return ret;
}();

Graphics::Texture texture_main = Graphics::Texture(nullptr).Wrap(Graphics::clamp).Interpolation(Graphics::nearest).SetData(texture_atlas.GetImage());

Graphics::Texture framebuffer_texture_map = Graphics::Texture(nullptr).Wrap(Graphics::clamp).Interpolation(Graphics::nearest).SetData(screen_size);
Graphics::FrameBuffer framebuffer_map(framebuffer_texture_map);

GameUtils::AdaptiveViewport adaptive_viewport(shader_config, screen_size);
Render r = adjust_(Render(0x2000, shader_config), SetTexture(texture_main), SetMatrix(adaptive_viewport.GetDetails().MatrixCentered()));

Input::Mouse mouse;

static auto random_generator = Random::RandomDeviceSeedSeq().MakeRng<Random::DefaultGenerator>();
Random::Scalar<int> irand(random_generator);
Random::Scalar<float> frand(random_generator);
Random::Misc<float> mrand(random_generator);

struct ProgramState : Program::DefaultBasicState
{
    GameUtils::State::Manager<GameState> state_manager;
    GameUtils::FpsCounter fps_counter;

    Metronome metronome = Metronome(60);

    void Resize()
    {
        Graphics::Viewport(window.Size());
        adaptive_viewport.Update();
        mouse.SetMatrix(adaptive_viewport.GetDetails().MouseMatrixCentered());
    }

    Metronome *GetTickMetronome() override
    {
        return &metronome;
    }

    int GetFpsCap() override
    {
        return 60 * NeedFpsCap();
    }

    void EndFrame() override
    {
        fps_counter.Update();
        window.SetTitle(STR((window_name), " TPS:", (fps_counter.Tps()), " FPS:", (fps_counter.Fps()), " AUDIO:", (audio_controller.ActiveSources())));

        if (!state_manager)
            Program::Exit();
    }

    void Tick() override
    {
        // window.ProcessEvents();
        window.ProcessEvents({gui_controller.EventHook()});

        if (window.ExitRequested())
            Program::Exit();
        if (window.Resized())
            Resize();

        gui_controller.PreTick();
        state_manager.Tick();
        audio_controller.Tick();

        Audio::CheckErrors();
    }

    void Render() override
    {
        gui_controller.PreRender();
        adaptive_viewport.BeginFrame();
        state_manager.Call(&GameState::Render);
        adaptive_viewport.FinishFrame();
        gui_controller.PostRender();
        Graphics::CheckErrors();

        window.SwapBuffers();
    }


    void Init()
    {
        // Load audio.
        Audio::LoadMentionedFiles(Audio::LoadFromPrefixWithExt("assets/audio/"), Audio::mono, Audio::wav);

        ImGui::StyleColorsDark();

        Graphics::Blending::Enable();
        Graphics::Blending::FuncNormalPre();

        state_manager.SetState("Game{}");
    }
};

IMP_MAIN(,)
{
    ProgramState state;
    state.Init();
    state.Resize();
    state.RunMainLoop();
    return 0;
}
