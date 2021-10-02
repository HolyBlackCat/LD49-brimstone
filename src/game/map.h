#pragma once
#include "gameutils/tiled_map.h"

inline constexpr int tile_size = 12;

enum class Tile
{
    air,
    wall,
    dirt,
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

        [[nodiscard]] static bool ShouldMergeWith(Tile a, Tile b);
    };
}
using TileDrawMethod = std::variant<TileFlavors::Invis, TileFlavors::Random, TileFlavors::Merged>;

struct TileInfo
{
    Tile tile = Tile::air;
    bool solid = false;
    bool corruptable = false;
    TileDrawMethod vis;
};
[[nodiscard]] const TileInfo GetTileInfo(Tile tile);

struct Cell
{
    Tile tile;
    std::uint8_t random = 0;

    int corruption_stage = 0; // 0..4
    int corruption_time = 0; // Relative to `Map::Time()`.

    static constexpr int num_corruption_stages = 4;
};

class Map
{
    int time = 0;
  public:
    Array2D<Cell> cells;
    Tiled::PointLayer points;

    Map() {}
    Map(Stream::Input source);

    void Tick();
    void Render(ivec2 camera_pos) const;
    void RenderCorruption(ivec2 camera_pos) const;

    [[nodiscard]] int Time() const {return time;}

    [[nodiscard]] bool PixelIsSolid(ivec2 pos) const;
};
