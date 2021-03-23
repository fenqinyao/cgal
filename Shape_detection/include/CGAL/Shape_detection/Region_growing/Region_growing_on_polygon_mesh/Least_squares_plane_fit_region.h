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

#ifndef CGAL_SHAPE_DETECTION_REGION_GROWING_POLYGON_MESH_LEAST_SQUARES_PLANE_FIT_REGION_H
#define CGAL_SHAPE_DETECTION_REGION_GROWING_POLYGON_MESH_LEAST_SQUARES_PLANE_FIT_REGION_H

#include <CGAL/license/Shape_detection.h>

// STL includes.
#include <vector>

// Boost includes.
#include <boost/graph/properties.hpp>
#include <boost/graph/graph_traits.hpp>
#include <CGAL/boost/graph/named_params_helper.h>
#include <CGAL/boost/graph/Named_function_parameters.h>

// Face graph includes.
#include <CGAL/Iterator_range.h>
#include <CGAL/HalfedgeDS_vector.h>
#include <CGAL/boost/graph/iterator.h>
#include <CGAL/boost/graph/graph_traits_Surface_mesh.h>
#include <CGAL/boost/graph/graph_traits_Polyhedron_3.h>

// CGAL includes.
#include <CGAL/assertions.h>
#include <CGAL/number_utils.h>
#include <CGAL/Cartesian_converter.h>
#include <CGAL/Eigen_diagonalize_traits.h>
#include <CGAL/linear_least_squares_fitting_3.h>
#include <CGAL/Exact_predicates_inexact_constructions_kernel.h>

// Internal includes.
#include <CGAL/Shape_detection/Region_growing/internal/utils.h>

namespace CGAL {
namespace Shape_detection {
namespace Polygon_mesh {

  /*!
    \ingroup PkgShapeDetectionRGOnMesh

    \brief Region type based on the quality of the least squares plane
    fit applied to faces of a polygon mesh.

    This class fits a plane, using \ref PkgPrincipalComponentAnalysisDRef "PCA",
    to chunks of faces in a polygon mesh and controls the quality of this fit.
    If all quality conditions are satisfied, the chunk is accepted as a valid region,
    otherwise rejected.

    \tparam GeomTraits
    a model of `Kernel`

    \tparam PolygonMesh
    a model of `FaceListGraph`

    \tparam FaceRange
    a model of `ConstRange` whose iterator type is `RandomAccessIterator` and
    value type is the face type of a polygon mesh

    \tparam VertexToPointMap
    a model of `LValuePropertyMap` whose key type is the vertex type of a polygon mesh and
    value type is `Kernel::Point_3`

    \cgalModels `RegionType`
  */
  template<
  typename GeomTraits,
  typename PolygonMesh,
  typename FaceRange = typename PolygonMesh::Face_range,
  typename VertexToPointMap = typename boost::property_map<PolygonMesh, CGAL::vertex_point_t>::type>
  class Least_squares_plane_fit_region {

  public:
    /// \name Types
    /// @{

    /// \cond SKIP_IN_MANUAL
    using Traits = GeomTraits;
    using Face_graph = PolygonMesh;
    using Face_range = FaceRange;
    using Vertex_to_point_map = VertexToPointMap;
    /// \endcond

    /// Number type.
    typedef typename GeomTraits::FT FT;

    /// @}

  private:
    using Point_3 = typename Traits::Point_3;
    using Vector_3 = typename Traits::Vector_3;
    using Plane_3 = typename Traits::Plane_3;

    using ITraits = Exact_predicates_inexact_constructions_kernel;
    using IFT = typename ITraits::FT;
    using IPoint_3 = typename ITraits::Point_3;
    using IPlane_3 = typename ITraits::Plane_3;
    using IConverter = Cartesian_converter<Traits, ITraits>;

    using Squared_length_3 = typename Traits::Compute_squared_length_3;
    using Squared_distance_3 = typename Traits::Compute_squared_distance_3;
    using Scalar_product_3 = typename Traits::Compute_scalar_product_3;
    using Cross_product_3 = typename Traits::Construct_cross_product_vector_3;

    using Get_sqrt = internal::Get_sqrt<Traits>;
    using Sqrt = typename Get_sqrt::Sqrt;

  public:
    /// \name Initialization
    /// @{

    /*!
      \brief initializes all internal data structures.

      \param pmesh
      an instance of `PolygonMesh` that represents a polygon mesh

      \param distance_threshold
      the maximum distance from the furthest vertex of a face to a plane. %Default is 1.

      \param angle_threshold
      the maximum accepted angle in degrees between the normal of a face and
      the normal of a plane. %Default is 25 degrees.

      \param min_region_size
      the minimum number of faces a region must have. %Default is 1.

      \param vertex_to_point_map
      an instance of `VertexToPointMap` that maps a polygon mesh
      vertex to `Kernel::Point_3`

      \param traits
      an instance of `GeomTraits`

      \pre `faces(pmesh).size() > 0`
      \pre `distance_threshold >= 0`
      \pre `angle_threshold >= 0 && angle_threshold <= 90`
      \pre `min_region_size > 0`
    */
    template<typename NamedParameters>
    Least_squares_plane_fit_region(
      const PolygonMesh& pmesh,
      const NamedParameters& np,
      const VertexToPointMap vertex_to_point_map = VertexToPointMap(),
      const GeomTraits traits = GeomTraits()) :
    m_face_graph(pmesh),
    m_face_range(faces(m_face_graph)),
    m_vertex_to_point_map(vertex_to_point_map),
    m_squared_length_3(traits.compute_squared_length_3_object()),
    m_squared_distance_3(traits.compute_squared_distance_3_object()),
    m_scalar_product_3(traits.compute_scalar_product_3_object()),
    m_cross_product_3(traits.construct_cross_product_vector_3_object()),
    m_sqrt(Get_sqrt::sqrt_object(traits)),
    m_iconverter() {

      CGAL_precondition(m_face_range.size() > 0);
      m_distance_threshold = parameters::choose_parameter(
        parameters::get_parameter(np, internal_np::distance_threshold), FT(1));
      CGAL_precondition(m_distance_threshold >= FT(0));

      const FT angle_deg_threshold = parameters::choose_parameter(
        parameters::get_parameter(np, internal_np::angle_deg_threshold), FT(25));
      CGAL_precondition(angle_deg_threshold >= FT(0) && angle_deg_threshold <= FT(90));

      m_min_region_size = parameters::choose_parameter(
        parameters::get_parameter(np, internal_np::min_region_size), 1);
      CGAL_precondition(m_min_region_size > 0);

      const FT cos_value_threshold = static_cast<FT>(std::cos(CGAL::to_double(
        (angle_deg_threshold * static_cast<FT>(CGAL_PI)) / FT(180))));
      m_cos_value_threshold = parameters::choose_parameter(
        parameters::get_parameter(np, internal_np::cos_value_threshold), cos_value_threshold);
      CGAL_precondition(m_cos_value_threshold >= FT(0) && m_cos_value_threshold <= FT(1));

      m_sort_regions = parameters::choose_parameter(
        parameters::get_parameter(np, internal_np::sort_regions), false);
    }

    /// @}

    /// \name Access
    /// @{

    /*!
      \brief implements `RegionType::is_part_of_region()`.

      This function controls if a face with the index `query_index` is within
      the `distance_threshold` from the corresponding plane and if the angle
      between its normal and the plane's normal is within the `angle_threshold`.
      If both conditions are satisfied, it returns `true`, otherwise `false`.

      \param query_index
      index of the query face

      The first and third parameters are not used in this implementation.

      \return Boolean `true` or `false`

      \pre `query_index >= 0 && query_index < faces(pmesh).size()`
    */
    bool is_part_of_region(
      const std::size_t,
      const std::size_t query_index,
      const std::vector<std::size_t>&) const {

      CGAL_precondition(query_index < m_face_range.size());
      const auto face = *(m_face_range.begin() + query_index);

      const FT squared_distance_to_fitted_plane = get_max_squared_distance(face);
      if (squared_distance_to_fitted_plane < FT(0)) return false;
      const FT squared_distance_threshold =
        m_distance_threshold * m_distance_threshold;

      Vector_3 face_normal;
      get_face_normal(face, face_normal);
      // The sign of this scalar product is important, as it indicates
      // into which side of the plane the face's normal points.
      const FT cos_value = m_scalar_product_3(face_normal, m_normal_of_best_fit);
      const FT squared_cos_value = cos_value * cos_value;

      FT squared_cos_value_threshold =
        m_cos_value_threshold * m_cos_value_threshold;
      squared_cos_value_threshold *= m_squared_length_3(face_normal);
      squared_cos_value_threshold *= m_squared_length_3(m_normal_of_best_fit);

      return (
        ( squared_distance_to_fitted_plane <= squared_distance_threshold ) &&
        ( squared_cos_value >= squared_cos_value_threshold ));
    }

    /*!
      \brief implements `RegionType::is_valid_region()`.

      This function controls if the `region` contains at least `min_region_size` faces.

      \param region
      indices of faces included in the region

      \return Boolean `true` or `false`
    */
    inline bool is_valid_region(const std::vector<std::size_t>& region) const {
      return ( region.size() >= m_min_region_size );
    }

    /*!
      \brief implements `RegionType::update()`.

      This function fits the least squares plane to all vertices of the faces
      from the `region`.

      \param region
      indices of faces included in the region

      \pre `region.size() > 0`
    */
    void update(const std::vector<std::size_t>& region) {

      CGAL_precondition(region.size() > 0);
      if (region.size() == 1) { // create new reference plane and normal
        const std::size_t face_index = region[0];
        CGAL_precondition(face_index < m_face_range.size());

        // The best fit plane will be a plane through this face centroid with
        // its normal being the face's normal.
        Point_3 face_centroid;
        const auto face = *(m_face_range.begin() + face_index);
        get_face_centroid(face, face_centroid);
        get_face_normal(face, m_normal_of_best_fit);
        m_plane_of_best_fit = Plane_3(face_centroid, m_normal_of_best_fit);

      } else { // update reference plane and normal

        std::vector<IPoint_3> points;
        points.reserve(region.size());
        for (const std::size_t face_index : region) {
          CGAL_precondition(face_index < m_face_range.size());
          const auto face = *(m_face_range.begin() + face_index);

          const auto hedge = halfedge(face, m_face_graph);
          const auto vertices = vertices_around_face(hedge, m_face_graph);
          CGAL_postcondition(vertices.size() > 0);

          for (const auto vertex : vertices) {
            const Point_3& point = get(m_vertex_to_point_map, vertex);
            points.push_back(m_iconverter(point));
          }
        }
        CGAL_postcondition(points.size() >= region.size());

        IPlane_3 fitted_plane;
        IPoint_3 fitted_centroid;

        // The best fit plane will be a plane fitted to all vertices of all
        // region faces with its normal being perpendicular to the plane.
        // Given that the points, and no normals, are used in estimating
        // the plane, the estimated normal will point into an arbitray
        // one of the two possible directions.
        // We flip it into the correct direction (the one that the majority
        // of faces agree with) below.
        // This fix is proposed by nh2:
        // https://github.com/CGAL/cgal/pull/4563
        CGAL::linear_least_squares_fitting_3(
          points.begin(), points.end(),
          fitted_plane, fitted_centroid,
          CGAL::Dimension_tag<0>(), ITraits(),
          CGAL::Eigen_diagonalize_traits<IFT, 3>());

        const Plane_3 unoriented_plane_of_best_fit = Plane_3(
          static_cast<FT>(fitted_plane.a()),
          static_cast<FT>(fitted_plane.b()),
          static_cast<FT>(fitted_plane.c()),
          static_cast<FT>(fitted_plane.d()));
        const Vector_3 unoriented_plane_normal =
          unoriented_plane_of_best_fit.orthogonal_vector();

        // Compute actual direction of plane's normal sign
        // based on faces, which belong to that region.
        // Approach:
        // Each face gets one vote to keep or flip the current plane normal.
        Vector_3 face_normal;
        long votes_to_keep_normal = 0;
        for (const std::size_t face_index : region) {
          const auto face = *(m_face_range.begin() + face_index);
          get_face_normal(face, face_normal);
          const bool agrees =
            m_scalar_product_3(face_normal, unoriented_plane_normal) > FT(0);
          votes_to_keep_normal += (agrees ? 1 : -1);
        }
        const bool flip_normal = (votes_to_keep_normal < 0);

        m_plane_of_best_fit = flip_normal
          ? unoriented_plane_of_best_fit.opposite()
          : unoriented_plane_of_best_fit;
        m_normal_of_best_fit = flip_normal
          ? (-1 * unoriented_plane_normal)
          : unoriented_plane_normal;
      }
    }

    /// @}

  private:
    const Face_graph& m_face_graph;
    const Face_range m_face_range;

    FT m_distance_threshold;
    FT m_cos_value_threshold;
    std::size_t m_min_region_size;
    bool m_sort_regions;

    const Vertex_to_point_map m_vertex_to_point_map;

    const Squared_length_3 m_squared_length_3;
    const Squared_distance_3 m_squared_distance_3;
    const Scalar_product_3 m_scalar_product_3;
    const Cross_product_3 m_cross_product_3;
    const Sqrt m_sqrt;

    const IConverter m_iconverter;

    Plane_3 m_plane_of_best_fit;
    Vector_3 m_normal_of_best_fit;

    // Compute centroid of the face.
    template<typename Face>
    void get_face_centroid(const Face& face, Point_3& face_centroid) const {

      const auto hedge = halfedge(face, m_face_graph);
      const auto vertices = vertices_around_face(hedge, m_face_graph);
      CGAL_precondition(vertices.size() > 0);

      FT sum = FT(0), x = FT(0), y = FT(0), z = FT(0);
      for (const auto vertex : vertices) {
        const Point_3& point = get(m_vertex_to_point_map, vertex);
        x += point.x();
        y += point.y();
        z += point.z();
        sum += FT(1);
      }
      CGAL_precondition(sum > FT(0));
      x /= sum;
      y /= sum;
      z /= sum;
      face_centroid = Point_3(x, y, z);
    }

    // Compute normal of the face.
    template<typename Face>
    void get_face_normal(const Face& face, Vector_3& face_normal) const {

      const auto hedge = halfedge(face, m_face_graph);
      const auto vertices = vertices_around_face(hedge, m_face_graph);
      CGAL_precondition(vertices.size() >= 3);

      auto vertex = vertices.begin();
      const Point_3& point1 = get(m_vertex_to_point_map, *vertex); ++vertex;
      const Point_3& point2 = get(m_vertex_to_point_map, *vertex); ++vertex;
      const Point_3& point3 = get(m_vertex_to_point_map, *vertex);

      const Vector_3 u = point2 - point1;
      const Vector_3 v = point3 - point1;
      face_normal = m_cross_product_3(u, v);
      CGAL_postcondition(face_normal != Vector_3());
    }

    // The maximum squared distance from the vertices of the face
    // to the best fit plane.
    template<typename Face>
    const FT get_max_squared_distance(const Face& face) const {

      FT max_squared_distance = -FT(1);
      const FT a = CGAL::abs(m_plane_of_best_fit.a());
      const FT b = CGAL::abs(m_plane_of_best_fit.b());
      const FT c = CGAL::abs(m_plane_of_best_fit.c());
      const FT d = CGAL::abs(m_plane_of_best_fit.d());
      if (a == FT(0) && b == FT(0) && c == FT(0) && d == FT(0))
        return max_squared_distance;

      const auto hedge = halfedge(face, m_face_graph);
      const auto vertices = vertices_around_face(hedge, m_face_graph);
      CGAL_precondition(vertices.size() > 0);

      for (const auto vertex : vertices) {
        const Point_3& point = get(m_vertex_to_point_map, vertex);
        const FT squared_distance = m_squared_distance_3(point, m_plane_of_best_fit);
        max_squared_distance = (CGAL::max)(squared_distance, max_squared_distance);
      }
      CGAL_postcondition(max_squared_distance >= FT(0));
      return max_squared_distance;
    }
  };

} // namespace Polygon_mesh
} // namespace Shape_detection
} // namespace CGAL

#endif // CGAL_SHAPE_DETECTION_REGION_GROWING_POLYGON_MESH_LEAST_SQUARES_PLANE_FIT_REGION_H
