#ifndef SLAM_CTOR_FEATURES_ANGLE_HISTOGRAM_H
#define SLAM_CTOR_FEATURES_ANGLE_HISTOGRAM_H

#include <vector>
#include <algorithm>

#include "../states/sensor_data.h"

class AngleHistogram {
private:
  using Storage = std::vector<unsigned>;
public:
  AngleHistogram(Storage::size_type resolution = 20)
    : _n{resolution}, _hist(_n, 0) /* NB: () - ctor, not {} - init list */
    , _ang_sum(_n, 0.0) {}

  auto reset(const LaserScan2D &scan) {
    std::fill(std::begin(_hist), std::end(_hist), 0);
    std::fill(std::begin(_ang_sum), std::end(_ang_sum), 0.0);
    _drift_dirs.resize(scan.points().size());
    using IndType = LaserScan2D::Points::size_type;
    for (IndType i = 1; i < scan.points().size(); ++i) {
      auto angle = undetectable_drift_direction(scan.points(), i);
      auto hist_i = hist_index(angle);
      _hist[hist_i]++;
      _ang_sum[hist_i] += angle;
      _drift_dirs[i] = angle;
    }
    return *this;
  }

  Storage::value_type operator[](std::size_t i) const { return _hist[i]; }

  Storage::value_type value(const LaserScan2D::Points &pts,
                            LaserScan2D::Points::size_type pt_i) const {
    if (pt_i == 0) {
      return pts.size(); // ~ignore the first point
    }
    return (*this)[hist_index(_drift_dirs[pt_i])];
  }

  Storage::size_type max_i() const {
    assert(0 < _hist.size());
    auto max_e = std::max_element(_hist.begin(), _hist.end());
    // assume random access iterator.
    return std::distance(_hist.begin(), max_e);
  }

  Storage::size_type min_i() const {
    assert(0 < _hist.size());
    auto max_e = std::min_element(_hist.begin(), _hist.end());
    // assume random access iterator.
    return std::distance(_hist.begin(), max_e);
  }

  auto least_freq_angle() const {
    auto i = min_i();
    return _ang_sum[i] / _hist[i];
  }

  double angle_step() const {
    return deg2rad(180) / _n;
  }

protected:
  std::size_t hist_index(double angle) const {
    assert(0 <= angle && angle < M_PI);
    auto hist_i = std::size_t(std::floor(angle / angle_step()));
    assert(hist_i < _hist.size());
    return hist_i;
  }

  double undetectable_drift_direction(const LaserScan2D::Points &pts,
                                      LaserScan2D::Points::size_type i) const {
    assert(0 < i && i < pts.size());
    auto &sp1 = pts[i-1], &sp2 = pts[i];
    double d_x = sp1.x() - sp2.x();
    double d_y = sp1.y() - sp2.y();
    double d_d = std::sqrt(d_x*d_x + d_y*d_y);

    return std::acos(d_x / d_d);
  }

private:
  Storage::size_type _n;
  Storage _hist;
  std::vector<double> _ang_sum, _drift_dirs;
};

#endif
