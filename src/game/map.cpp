#include "map.h"

#include "game/main.h"

constexpr int cor_stage_len = 15;

static std::vector<TileInfo> tile_info_vec = []{
    std::vector<TileInfo> ret = {
        {.tile = Tile::air , .solid = false, .corruptable = false, .vis = TileFlavors::Invis{}},
        {.tile = Tile::wall, .solid = true , .corruptable = false, .vis = TileFlavors::Random{.index = 0, .count = 4}},
        {.tile = Tile::dirt, .solid = true , .corruptable = true , .vis = TileFlavors::Merged{.index = 1}},
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

bool TileFlavors::Merged::ShouldMergeWith(Tile a, Tile b)
{
    if (a == Tile::dirt && b == Tile::wall)
        return true;
    return a == b;
}

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

    for (auto pos : vector_range(layer_mid.size()))
    {
        Cell &cell = cells.safe_nonthrowing_at(pos);

        int index = layer_mid.safe_throwing_at(pos);
        if (index < 0 || index >= int(Tile::_count))
            Program::Error(source.GetExceptionPrefix() + FMT("Invalid tile index {} at {}.", index, pos));

        cell.tile = Tile(index);
        cell.random = irand <= 255;
    }
}

void Map::Tick()
{
    time++;
}

void Map::Render(ivec2 camera_pos) const
{
    static const Graphics::TextureAtlas::Region tiles_tex = texture_atlas.Get("tiles.png");

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
                r.iquad(tile_pixel_pos, tiles_tex.region(ivec2(cell.random % flavor.count, flavor.index) * tile_size, ivec2(tile_size)));
            },
            [&](const TileFlavors::Merged &flavor)
            {
                int rand_index = std::array{0,0,0,0,1,1,2,3}[cell.random % 8];

                for (ivec2 sub_pos : vector_range(ivec2(2)))
                {
                    ivec2 offset = sub_pos * 2 - 1;
                    bool merge_h = flavor.ShouldMergeWith(cell.tile, cells.clamped_at(tile_pos + offset with(y = 0)).tile);
                    bool merge_v = flavor.ShouldMergeWith(cell.tile, cells.clamped_at(tile_pos + offset with(x = 0)).tile);
                    bool merge_hv = merge_h && merge_v && flavor.ShouldMergeWith(cell.tile, cells.clamped_at(tile_pos + offset).tile);

                    ivec2 variant;
                    if      (merge_hv) variant = ivec2(rand_index, 0);
                    else if (merge_h && merge_v) variant = ivec2(3, 1);
                    else if (merge_h ) variant = ivec2(1, 1);
                    else if (merge_v ) variant = ivec2(0, 1);
                    else               variant = ivec2(2, 1);

                    ivec2 pixel_sub_pos = sub_pos * tile_size / 2;

                    r.iquad(tile_pixel_pos + pixel_sub_pos, tiles_tex.region((ivec2(0, flavor.index) + variant) * tile_size + pixel_sub_pos, ivec2(tile_size / 2)));
                }
            },
        }, tile_info.vis);
    }
}

void Map::RenderCorruption(ivec2 camera_pos) const
{
    static const Graphics::TextureAtlas::Region corruption_tex = texture_atlas.Get("corruption.png");

    ivec2 a = div_ex(camera_pos - screen_size/2, tile_size);
    ivec2 b = div_ex(camera_pos + screen_size/2, tile_size);

    for (ivec2 tile_pos : a <= vector_range <= b)
    {
        ivec2 tile_pixel_pos = tile_pos * tile_size - camera_pos;

        const Cell &cell = cells.clamped_at(tile_pos);

        if (cell.corruption_stage == 0)
            continue; // Not corrupted.

        int visual_stage = clamp_min(clamp_max((time - cell.corruption_time) / cor_stage_len, Cell::num_corruption_stages-1) - (Cell::num_corruption_stages - cell.corruption_stage));
        r.iquad(tile_pixel_pos, corruption_tex.region(ivec2(visual_stage * tile_size, 0), ivec2(tile_size)));
    }
}

bool Map::PixelIsSolid(ivec2 pos) const
{
    ivec2 tile_pos = div_ex(pos, tile_size);
    if (!cells.pos_in_range(tile_pos))
        return false;
    return GetTileInfo(cells.safe_nonthrowing_at(tile_pos).tile).solid;
}
