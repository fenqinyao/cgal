#define CGAL_TRACE_STREAM std::cerr

#include <CGAL/Octree.h>
#include <CGAL/Octree/IO.h>
#include <CGAL/Octree/Tree_walker_criterion.h>

#include <CGAL/Simple_cartesian.h>
#include <CGAL/Point_set_3.h>

typedef CGAL::Simple_cartesian<double> Kernel;
typedef Kernel::Point_3 Point;
typedef CGAL::Point_set_3<Point> Point_set;
typedef CGAL::Octree::Octree
        <Point_set, typename Point_set::Point_map>
        Octree;

int test_preorder_1_node() {

  // Define the dataset
  Point_set points;
  points.insert({-1, -1, -1});
  auto point_map = points.point_map();

  // Create the octree
  Octree octree(points, point_map);
  octree.refine(10, 1);

  // Create the range
  auto tree_walker = CGAL::Octree::Tree_walker::Preorder();
  auto first = tree_walker.first(&octree.root());
  auto nodes = octree.nodes(first, tree_walker);

  // Check each item in the range
  auto iter = nodes.begin();
  assert(*iter == octree.root());

  return 0;
}

int test_preorder_9_nodes() {

  // Define the dataset
  Point_set points;
  points.insert({-1, -1, -1});
  points.insert({1, -1, -1});
  auto point_map = points.point_map();

  // Create the octree
  Octree octree(points, point_map);
  octree.refine(10, 1);

  // Create the range
  auto tree_walker = CGAL::Octree::Tree_walker::Preorder();
  auto first = tree_walker.first(&octree.root());
  auto nodes = octree.nodes(first, tree_walker);


  // Check each item in the range
  auto iter = nodes.begin();
  assert(*iter == octree.root());
  for (int i = 0; i < 8; ++i) {
    iter++;
    assert(*iter == octree.root()[i]);
  }

  return 0;
}

int test_preorder_25_nodes() {

  // Define the dataset
  Point_set points;
  points.insert({1, 1, 1});
  points.insert({1, 1, 2});
  points.insert({1, 1, 3});
  points.insert({1, 1, 4});
  auto point_map = points.point_map();

  // Create the octree
  Octree octree(points, point_map);
  octree.refine(10, 1);
  std::cout << octree;

  // Create the range
  auto tree_walker = CGAL::Octree::Tree_walker::Preorder();
  auto first = tree_walker.first(&octree.root());
  auto nodes = octree.nodes(first, tree_walker);

  // Check each item in the range
  auto iter = nodes.begin();
  assert(*iter == octree.root());
  iter++;
  assert(*iter == octree.root()[0]);
  iter++;
  assert(*iter == octree.root()[1]);
  iter++;
  assert(*iter == octree.root()[2]);
  iter++;
  assert(*iter == octree.root()[3]);
  for (int i = 0; i < 8; ++i) {
    iter++;
    assert(*iter == octree.root()[3][i]);
  }
  iter++;
  assert(*iter == octree.root()[4]);
  iter++;
  assert(*iter == octree.root()[5]);
  iter++;
  assert(*iter == octree.root()[6]);
  iter++;
  assert(*iter == octree.root()[7]);
  for (int i = 0; i < 8; ++i) {
    iter++;
    assert(*iter == octree.root()[7][i]);
  }

  return 0;
}

int main(void) {

  test_preorder_1_node();
  test_preorder_9_nodes();
  test_preorder_25_nodes();
//  test_preorder_print();
//  test_postorder_print();
//  test_leaves_print();

  return 0;
}
