// Copyright (c) 2018 INRIA Sophia-Antipolis (France).
// All rights reserved.
//
// This file is part of CGAL (www.cgal.org).
//
// $URL$
// $Id$
// SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-Commercial
//
//
// Author(s)     : Florent Lafarge, Simon Giraudot, Thien Hoang, Dmitry Anisimov
//

#ifndef CGAL_SHAPE_DETECTION_REGION_GROWING_INTERNAL_UTILS_H
#define CGAL_SHAPE_DETECTION_REGION_GROWING_INTERNAL_UTILS_H

#include <CGAL/license/Shape_detection.h>

// STL includes.
#include <vector>
#include <algorithm>

// Boost headers.
#include <boost/mpl/has_xxx.hpp>

// CGAL includes.
#include <CGAL/assertions.h>
#include <CGAL/number_utils.h>
#include <CGAL/Cartesian_converter.h>
#include <CGAL/Eigen_diagonalize_traits.h>
#include <CGAL/linear_least_squares_fitting_2.h>
#include <CGAL/linear_least_squares_fitting_3.h>
#include <CGAL/Exact_predicates_inexact_constructions_kernel.h>

namespace CGAL {
namespace Shape_detection {
namespace internal {

  template<typename GeomTraits>
  class Default_sqrt {

  private:
    using Traits = GeomTraits;
    using FT = typename Traits::FT;

  public:
    const FT operator()(const FT value) const {
      CGAL_precondition(value >= FT(0));
      return static_cast<FT>(CGAL::sqrt(CGAL::to_double(value)));
    }
  };

  BOOST_MPL_HAS_XXX_TRAIT_NAMED_DEF(Has_nested_type_Sqrt, Sqrt, false)

  // Case: do_not_use_default = false.
  template<typename GeomTraits,
  bool do_not_use_default = Has_nested_type_Sqrt<GeomTraits>::value>
  class Get_sqrt {

  public:
    using Traits = GeomTraits;
    using Sqrt = Default_sqrt<Traits>;

    static Sqrt sqrt_object(const Traits& ) {
      return Sqrt();
    }
  };

  // Case: do_not_use_default = true.
  template<typename GeomTraits>
  class Get_sqrt<GeomTraits, true> {

  public:
    using Traits = GeomTraits;
    using Sqrt = typename Traits::Sqrt;

    static Sqrt sqrt_object(const Traits& traits) {
      return traits.sqrt_object();
    }
  };

  template<typename FT>
  struct Compare_scores {

    const std::vector<FT>& m_scores;
    Compare_scores(const std::vector<FT>& scores) :
    m_scores(scores)
    { }

    bool operator()(const std::size_t i, const std::size_t j) const {
      CGAL_precondition(i < m_scores.size());
      CGAL_precondition(j < m_scores.size());
      return m_scores[i] > m_scores[j];
    }
  };

  template<
  typename InputRange,
  typename PointMap,
  typename Line_2>
  const typename Kernel_traits<Line_2>::Kernel::FT
  create_line_from_points_2(
    const InputRange& input_range, const PointMap point_map,
    const std::vector<std::size_t>& region, Line_2& line) {

    using Traits = typename Kernel_traits<Line_2>::Kernel;
    using FT = typename Traits::FT;

    using ITraits = CGAL::Exact_predicates_inexact_constructions_kernel;
    using IConverter = Cartesian_converter<Traits, ITraits>;

    using IFT = typename ITraits::FT;
    using IPoint_2 = typename ITraits::Point_2;
    using ILine_2 = typename ITraits::Line_2;

    std::vector<IPoint_2> points;
    CGAL_precondition(region.size() > 0);
    points.reserve(region.size());
    const IConverter iconverter = IConverter();

    for (const std::size_t point_index : region) {
      CGAL_precondition(point_index < input_range.size());
      const auto& key = *(input_range.begin() + point_index);
      const auto& point = get(point_map, key);
      points.push_back(iconverter(point));
    }
    CGAL_postcondition(points.size() == region.size());

    ILine_2 fitted_line;
    IPoint_2 fitted_centroid;
    const IFT score = CGAL::linear_least_squares_fitting_2(
      points.begin(), points.end(),
      fitted_line, fitted_centroid,
      CGAL::Dimension_tag<0>(), ITraits(),
      CGAL::Eigen_diagonalize_traits<IFT, 2>());

    line = Line_2(
      static_cast<FT>(fitted_line.a()),
      static_cast<FT>(fitted_line.b()),
      static_cast<FT>(fitted_line.c()));
    return static_cast<FT>(score);
  }

  template<
  typename InputRange,
  typename PointMap,
  typename Plane_3>
  const typename Kernel_traits<Plane_3>::Kernel::FT
  create_plane_from_points(
    const InputRange& input_range, const PointMap point_map,
    const std::vector<std::size_t>& region, Plane_3& plane) {

    using Traits = typename Kernel_traits<Plane_3>::Kernel;
    using FT = typename Traits::FT;

    using ITraits = CGAL::Exact_predicates_inexact_constructions_kernel;
    using IConverter = Cartesian_converter<Traits, ITraits>;

    using IFT = typename ITraits::FT;
    using IPoint_3 = typename ITraits::Point_3;
    using IPlane_3 = typename ITraits::Plane_3;

    std::vector<IPoint_3> points;
    CGAL_precondition(region.size() > 0);
    points.reserve(region.size());
    const IConverter iconverter = IConverter();

    for (const std::size_t point_index : region) {
      CGAL_precondition(point_index < input_range.size());
      const auto& key = *(input_range.begin() + point_index);
      const auto& point = get(point_map, key);
      points.push_back(iconverter(point));
    }
    CGAL_postcondition(points.size() == region.size());

    IPlane_3 fitted_plane;
    IPoint_3 fitted_centroid;
    const IFT score = CGAL::linear_least_squares_fitting_3(
      points.begin(), points.end(),
      fitted_plane, fitted_centroid,
      CGAL::Dimension_tag<0>(), ITraits(),
      CGAL::Eigen_diagonalize_traits<IFT, 3>());

    plane = Plane_3(
      static_cast<FT>(fitted_plane.a()),
      static_cast<FT>(fitted_plane.b()),
      static_cast<FT>(fitted_plane.c()),
      static_cast<FT>(fitted_plane.d()));
    return static_cast<FT>(score);
  }

  template<
  typename InputRange,
  typename PointMap,
  typename Plane_3>
  void create_planes_from_points(
    const InputRange& input_range, const PointMap point_map,
    std::vector< std::vector<std::size_t> >& regions,
    std::vector<Plane_3>& planes) {

    planes.clear();
    planes.reserve(regions.size());

    Plane_3 plane;
    for (const auto& region : regions) {
      create_plane_from_points(input_range, point_map, region, plane);
      planes.push_back(plane);
    }
    CGAL_postcondition(planes.size() == regions.size());
  }

} // namespace internal
} // namespace Shape_detection
} // namespace CGAL

#endif // CGAL_SHAPE_DETECTION_REGION_GROWING_INTERNAL_UTILS_H
