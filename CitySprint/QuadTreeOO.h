#ifndef QUAD_TREE_H
#define QUAD_TREE_H

#include <vector>
#include <algorithm>
#include <memory>

template <typename EntityType>
class QuadTreeNode {
public:
  QuadTreeNode(int x, int y, int size);
  bool isLeaf() const;
  void insert(EntityType* entity, int depth, int maxDepth);
  void remove(EntityType* entity);
  void queryRange(int x, int y, int range, std::vector<EntityType*>& result) const;

private:
  void subdivide();
  bool intersects(int x, int y, int range) const;

  int x, y, size;
  std::vector<EntityType*> entities;
  std::unique_ptr<QuadTreeNode<EntityType>> children[4];
};

template <typename EntityType>
class QuadTree {
public:
  QuadTree(int width, int height, int maxDepth);
  void insertEntity(EntityType* entity);
  void removeEntity(EntityType* entity);
  std::vector<EntityType*> queryRange(int x, int y, int range) const;

private:
  std::unique_ptr<QuadTreeNode<EntityType>> root;
  int maxDepth;
};

#include "QuadTreeOOImplementation.h"

#endif // QUAD_TREE_H
