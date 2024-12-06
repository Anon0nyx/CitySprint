#ifndef QUAD_TREE_IMPL_H
#define QUAD_TREE_IMPL_H

#include "QuadTree.h"

template <typename EntityType>
QuadTree<EntityType> createQuadTree(int width, int height, int maxDepth) {
  QuadTree<EntityType> quadTree;
  quadTree.maxDepth = maxDepth;
  quadTree.root = std::make_unique<QuadTreeNode<EntityType>>(0, 0, std::max(width, height));
  return quadTree;
}

template <typename EntityType>
void insertEntity(QuadTree<EntityType>& quadTree, EntityType* entity) {
  insert(quadTree.root.get(), entity, 0, quadTree.maxDepth);
}

template <typename EntityType>
void removeEntity(QuadTree<EntityType>& quadTree, EntityType* entity) {
  remove(quadTree.root.get(), entity);
}

template <typename EntityType>
std::vector<EntityType*> queryRange(const QuadTree<EntityType>& quadTree, int x, int y, int range) {
  std::vector<EntityType*> result;
  queryRange(quadTree.root.get(), x, y, range, result);
  return result;
}

template <typename EntityType>
void insert(QuadTreeNode<EntityType>* node, EntityType* entity, int depth, int maxDepth) {
  if (node->isLeaf()) {
    if (node->entities.size() < 4 || depth == maxDepth) {
      node->entities.push_back(entity);
    }
    else {
      subdivide(node);
      insert(node, entity, depth, maxDepth);
    }
  }
  else {
    int midX = node->x + node->size / 2;
    int midY = node->y + node->size / 2;
    int index = (entity->midpoint[0] > midX) + 2 * (entity->midpoint[1] > midY);
    insert(node->children[index].get(), entity, depth + 1, maxDepth);
  }
}

template <typename EntityType>
void remove(QuadTreeNode<EntityType>* node, EntityType* entity) {
  if (node->isLeaf()) {
    auto it = std::find(node->entities.begin(), node->entities.end(), entity);
    if (it != node->entities.end()) {
      node->entities.erase(it);
    }
  }
  else {
    int midX = node->x + node->size / 2;
    int midY = node->y + node->size / 2;
    int index = (entity->midpoint[0] > midX) + 2 * (entity->midpoint[1] > midY);
    remove(node->children[index].get(), entity);
  }
}

template <typename EntityType>
void queryRange(QuadTreeNode<EntityType>* node, int x, int y, int range, std::vector<EntityType*>& result) {
  if (!intersects(x, y, range, node)) return;

  for (auto& entity : node->entities) {
    int dx = entity->midpoint[0] - x;
    int dy = entity->midpoint[1] - y;
    if (dx * dx + dy * dy <= range * range) {
      result.push_back(entity);
    }
  }

  if (!node->isLeaf()) {
    for (auto& child : node->children) {
      queryRange(child.get(), x, y, range, result);
    }
  }
}

template <typename EntityType>
void subdivide(QuadTreeNode<EntityType>* node) {
  int halfSize = node->size / 2;
  node->children[0] = std::make_unique<QuadTreeNode<EntityType>>(node->x, node->y, halfSize);
  node->children[1] = std::make_unique<QuadTreeNode<EntityType>>(node->x + halfSize, node->y, halfSize);
  node->children[2] = std::make_unique<QuadTreeNode<EntityType>>(node->x, node->y + halfSize, halfSize);
  node->children[3] = std::make_unique<QuadTreeNode<EntityType>>(node->x + halfSize, node->y + halfSize, halfSize);

  for (auto& entity : node->entities) {
    int midX = node->x + halfSize;
    int midY = node->y + halfSize;
    int index = (entity->midpoint[0] > midX) + 2 * (entity->midpoint[1] > midY);
    node->children[index]->entities.push_back(std::move(entity));
  }
  node->entities.clear();
}

template <typename EntityType>
inline bool intersects(int x, int y, int range, QuadTreeNode<EntityType>* node) {
  return !(x + range < node->x || x - range > node->x + node->size || y + range < node->y || y - range > node->y + node->size);
}

#endif // QUAD_TREE_IMPL_H
