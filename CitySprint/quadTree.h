#ifndef QUAD_TREE_H
#define QUAD_TREE_H

#include <vector>
#include <algorithm>
#include <memory>

template <typename EntityType>
struct QuadTreeNode {
  int x, y, size;
  std::vector<EntityType*> entities;
  std::unique_ptr<QuadTreeNode<EntityType>> children[4];

  QuadTreeNode(int x, int y, int size) : x(x), y(y), size(size) {}
  bool isLeaf() const {
    return children[0] == nullptr;
  }
};

template <typename EntityType>
struct QuadTree {
  std::unique_ptr<QuadTreeNode<EntityType>> root;
  int maxDepth;
};

template <typename EntityType>
QuadTree<EntityType> createQuadTree(int width, int height, int maxDepth);

template <typename EntityType>
void insertEntity(QuadTree<EntityType>& quadTree, EntityType* entity);

template <typename EntityType>
void removeEntity(QuadTree<EntityType>& quadTree, EntityType* entity);

template <typename EntityType>
std::vector<EntityType*> queryRange(const QuadTree<EntityType>& quadTree, int x, int y, int range);

#include "quadTreeImplementation.h"

#endif // QUAD_TREE_H
