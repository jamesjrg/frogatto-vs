#ifndef LEVEL_SOLID_MAP_HPP_INCLUDED
#define LEVEL_SOLID_MAP_HPP_INCLUDED

#include <bitset>
#include <map>
#include <vector>

static const int TileSize = 32;

typedef std::pair<int,int> tile_pos;
typedef std::bitset<TileSize*TileSize> tile_bitmap;

struct tile_solid_info {
	tile_solid_info() : all_solid(false), friction(0), traction(0), damage(-1)
	{}
	tile_bitmap bitmap;
	bool all_solid;
	int friction;
	int traction;
	int damage;
};

class level_solid_map {
public:
	level_solid_map();
	level_solid_map(const level_solid_map& m);
	level_solid_map& operator=(const level_solid_map& m);
	~level_solid_map();
	tile_solid_info& insert_or_find(const tile_pos& pos);
	const tile_solid_info* find(const tile_pos& pos) const;
	void erase(const tile_pos& pos);
	void clear();

	void merge(const level_solid_map& m, int xoffset, int yoffset);
private:

	tile_solid_info** insert_raw(const tile_pos& pos);

	struct row {
		std::vector<tile_solid_info*> positive_cells, negative_cells;
	};

	std::vector<row> positive_rows_, negative_rows_;
};

#endif
