#include "map.h"

#include "game/main.h"

static std::vector<TileInfo> tile_info_vec = []{
    std::vector<TileInfo> ret = {
        {.tile = Tile::air , .solid = false, .vis = TileFlavors::Invis{}},
        {.tile = Tile::wall, .solid = true , .vis = TileFlavors::Random{.index = 0, .count = 4}},
    };
    if (ret.size() != std::size_t(Tile::_count))
        Program::Error("Internal error: The tile info vector size is different from the tile enum size.");

    for (std::size_t i = 0; i < std::size_t(Tile::_count); i++)
    {
        if (Tile(i) != ret[i].tile)
            Program::Error(FMT("Internal error: The tile info at index {} has incorrect enum specified.", i));
    }

    return ret;
}();

const TileInfo GetTileInfo(Tile tile)
{
    if (tile < Tile{} || tile >= Tile::_count)
        Program::Error(FMT("Invalid tile index: {}", int(tile)));

    return tile_info_vec[int(tile)];
}

Map::Map(Stream::Input source)
{
    Json json(source.ReadToMemory().string(), 64);

    auto layer_mid = Tiled::LoadTileLayer(Tiled::FindLayer(json, "mid"));
    points = Tiled::LoadPointLayer(Tiled::FindLayer(json, "obj"));

    cells = decltype(cells)(layer_mid.size());

    for (index_vec2 pos : index_vec2(0) <= vector_range < layer_mid.size())
    {
        Cell &cell = cells.safe_nonthrowing_at(pos);

        int index = layer_mid.safe_throwing_at(pos);
        if (index < 0 || index >= int(Tile::_count))
            Program::Error(source.GetExceptionPrefix() + FMT("Invalid tile index {} at {}.", index, pos));

        cell.tile = Tile(index);
        cell.random = irand <= 255;
    }
}

void Map::Render(ivec2 camera_pos) const
{
    static const Graphics::TextureAtlas::Region tiles_region = texture_atlas.Get("tiles.png");

    ivec2 a = div_ex(camera_pos - screen_size/2, tile_size);
    ivec2 b = div_ex(camera_pos + screen_size/2, tile_size);

    for (ivec2 tile_pos : a <= vector_range <= b)
    {
        ivec2 tile_pixel_pos = tile_pos * tile_size - camera_pos;

        const Cell &cell = cells.clamped_at(tile_pos);
        const TileInfo &tile_info = GetTileInfo(cell.tile);

        std::visit(Meta::overload{
            [](const TileFlavors::Invis &) {},
            [&](const TileFlavors::Random &flavor)
            {
                r.iquad(tile_pixel_pos, tiles_region.region(ivec2(cell.random % flavor.count, flavor.index) * tile_size, ivec2(tile_size)));
            },
        }, tile_info.vis);
    }
}

bool Map::PixelIsSolid(ivec2 pos) const
{
    ivec2 tile_pos = div_ex(pos, tile_size);
    if (!cells.pos_in_range(tile_pos))
        return false;
    return GetTileInfo(cells.safe_nonthrowing_at(tile_pos).tile).solid;
}
