#pragma once
#include "gameutils/tiled_map.h"

class ParticleController;

inline constexpr int tile_size = 12;

enum class Tile
{
    air,
    wall,
    dirt,
    grass,
    spike,
    spike_down,
    _count,
};

namespace TileFlavors
{
    struct Invis {};

    struct Random
    {
        int index = 0;
        int count = 0;
    };

    struct Merged
    {
        int index = 0;
    };

    struct HorMergedWithRandom
    {
        int index = 0;
        int rand_count = 0;
    };

    [[nodiscard]] bool ShouldMergeWith(Tile a, Tile b);
}
using TileDrawMethod = std::variant<TileFlavors::Invis, TileFlavors::Random, TileFlavors::Merged, TileFlavors::HorMergedWithRandom>;

struct TileInfo
{
    Tile tile = Tile::air;
    bool solid = false;
    bool kills = false;
    bool corruptable = false;
    TileDrawMethod vis;
};
[[nodiscard]] const TileInfo GetTileInfo(Tile tile);

struct Cell
{
    Tile tile;
    std::uint8_t random = 0;

    static constexpr int
        num_corruption_stages = 6,
        corruption_stage_len = 15;

    int corruption_stage = 0; // 0..num_corruption_stages
    int corruption_start_time = 0; // Relative to `Map::Time()`.
    int damage = 0;
    int visual_damage = 0;

    [[nodiscard]] int CalcVisualCorruptionStage() const;
};

class Map
{
    int time = 1; // `0` is reserved for "never".
  public:
    Array2D<Cell> cells;
    Tiled::PointLayer points;

    Map() {}
    Map(Stream::Input source);

    void Tick(ParticleController &par);
    void Render(ivec2 camera_pos) const;
    void RenderCorruption(ivec2 camera_pos) const;

    [[nodiscard]] int Time() const {return time;}

    [[nodiscard]] bool PixelIsSolid(ivec2 pos) const;

    void CorrputTile(ivec2 tile_pos, int stage);
};
