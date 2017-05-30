#ifndef SLAM_CTOR_CORE_PLAIN_GRID_MAP_H_INCLUDED
#define SLAM_CTOR_CORE_PLAIN_GRID_MAP_H_INCLUDED

#include <cmath>
#include <vector>
#include <memory>
#include <cassert>
#include <algorithm>

#include "grid_map.h"

class PlainGridMap : public GridMap {
public:
  // TODO: cp, mv ctors, dtor
  PlainGridMap(std::shared_ptr<GridCell> prototype,
               const GridMapParams& params = MapValues::gmp)
    : GridMap{prototype, params}, _cells(GridMap::height()) {
    for (auto &row : _cells) {
      row.reserve(GridMap::width());
      for (int i = 0; i < GridMap::width(); i++) {
        row.push_back(prototype->clone());
      }
    }
  }

  const GridCell &operator[](const Coord& c) const override {
    assert(has_cell(c));
    auto coord = external2internal(c);
    return *_cells[coord.y][coord.x];
  }

protected: // fields
  std::vector<std::vector<std::unique_ptr<GridCell>>> _cells;
};

/* Unbounded implementation */

class UnboundedPlainGridMap : public PlainGridMap {
private: // fields
  static constexpr double Expansion_Rate = 1.2;
public: // methods
  UnboundedPlainGridMap(std::shared_ptr<GridCell> prototype,
                        const GridMapParams &params = MapValues::gmp)
    : PlainGridMap{prototype, params}
    , _origin{GridMap::origin()}, _unknown_cell{prototype->clone()} {}

  void update(const Coord &area_id,
              const AreaOccupancyObservation &aoo) override {
    ensure_inside(area_id);
    PlainGridMap::update(area_id, aoo);
  }

  void reset(const Coord &area_id, const GridCell &new_area) override {
    ensure_inside(area_id);
    PlainGridMap::reset(area_id, new_area);
  }

  const GridCell &operator[](const Coord& c) const override {
    if (!has_cell(c)) { return *_unknown_cell; }
    return PlainGridMap::operator[](c);
  }

  Coord origin() const override { return _origin; }

protected: // methods

  bool ensure_inside(const Coord &c) {
    if (has_cell(c)) return false;

    auto coord = external2internal(c);
    unsigned w = width(), h = height();
    unsigned prep_x = 0, app_x = 0, prep_y = 0, app_y = 0;
    std::tie(prep_x, app_x) = determine_cells_nm(0, coord.x, w);
    std::tie(prep_y, app_y) = determine_cells_nm(0, coord.y, h);

    unsigned new_w = prep_x + w + app_x, new_h = prep_y + h + app_y;
    #define UPDATE_DIM(dim, elem)                                    \
      if (dim < new_##dim && new_##dim < Expansion_Rate * dim) {     \
        double scale = prep_##elem / (new_##dim - dim);              \
        prep_##elem += (Expansion_Rate * dim - new_##dim) * scale;   \
        new_##dim = Expansion_Rate * dim;                            \
        app_##elem = new_##dim - (prep_##elem + dim);                \
      }

    UPDATE_DIM(w, x);
    UPDATE_DIM(h, y);
    #undef UPDATE_DIM

    // PERFORMANCE: _cells can be reused
    std::vector<std::vector<std::unique_ptr<GridCell>>> new_cells{new_h};
    for (size_t y = 0; y != new_h; ++y) {
      std::generate_n(std::back_inserter(new_cells[y]), new_w,
                      [this](){ return this->_unknown_cell->clone(); });
      if (y < prep_y || prep_y + h <= y) { continue; }

      std::move(_cells[y - prep_y].begin(), _cells[y - prep_y].end(),
                &new_cells[y][prep_x]);
    }

    std::swap(_cells, new_cells);
    set_height(new_h);
    set_width(new_w);
    _origin += Coord(prep_x, prep_y);

    assert(has_cell(c));
    return true;
  }

  std::tuple<unsigned, unsigned> determine_cells_nm(
    int min, int val, int max) const {
    assert(min <= max);
    unsigned prepend_nm = 0, append_nm = 0;
    if (val < min) {
      prepend_nm = min - val;
    } else if (max <= val) {
      append_nm = val - max + 1;
    }
    return std::make_tuple(prepend_nm, append_nm);
  }

private: // fields
  Coord _origin;
  std::shared_ptr<GridCell> _unknown_cell;
};

#endif
