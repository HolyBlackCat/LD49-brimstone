#pragma once
#include "gameutils/tiled_map.h"

inline constexpr int tile_size = 12;

enum class Tile
{
    air,
    wall,
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
}
using TileDrawMethod = std::variant<TileFlavors::Invis, TileFlavors::Random>;

struct TileInfo
{
    Tile tile = Tile::air;
    bool solid = false;
    TileDrawMethod vis;
};
[[nodiscard]] const TileInfo GetTileInfo(Tile tile);

struct Cell
{
    Tile tile;
    std::uint8_t random = 0;
};

class Map
{
  public:
    Array2D<Cell> cells;
    Tiled::PointLayer points;

    Map() {}
    Map(Stream::Input source);

    void Render(ivec2 camera_pos) const;

    [[nodiscard]] bool PixelIsSolid(ivec2 pos) const;
};
