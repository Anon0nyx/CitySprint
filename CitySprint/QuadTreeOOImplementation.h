#ifndef QUAD_TREE_IMPL_H
#define QUAD_TREE_IMPL_H

#include "QuadTreeOO.h"

template <typename EntityType>
QuadTreeNode<EntityType>::QuadTreeNode(int x, int y, int size) : x(x), y(y), size(size) {}

template <typename EntityType>
bool QuadTreeNode<EntityType>::isLeaf() const {
  return children[0] == nullptr;
}

template <typename EntityType>
void QuadTreeNode<EntityType>::insert(EntityType* entity, int depth, int maxDepth) {
  if (isLeaf()) {
    if (entities.size() < 4 || depth == maxDepth) {
      entities.push_back(entity);
    } else {
      subdivide();
      insert(entity, depth, maxDepth);
    }
  } else {
    int midX = x + size / 2;
    int midY = y + size / 2;
    int index = (entity->midpoint[0] > midX) + 2 * (entity->midpoint[1] > midY);
    children[index]->insert(entity, depth + 1, maxDepth);
  }
}

template <typename EntityType>
void QuadTreeNode<EntityType>::remove(EntityType* entity) {
  if (isLeaf()) {
    auto it = std::find(entities.begin(), entities.end(), entity);
    if (it != entities.end()) {
      entities.erase(it);
    }
  } else {
    int midX = x + size / 2;
    int midY = y + size / 2;
    int index = (entity->midpoint[0] > midX) + 2 * (entity->midpoint[1] > midY);
    children[index]->remove(entity);
  }
}

template <typename EntityType>
void QuadTreeNode<EntityType>::queryRange(int x, int y, int range, std::vector<EntityType*>& result) const {
  if (!intersects(x, y, range)) return;

  for (auto& entity : entities) {
    int dx = entity->midpoint[0] - x;
    int dy = entity->midpoint[1] - y;
    if (dx * dx + dy * dy <= range * range) {
      result.push_back(entity);
    }
  }

  if (!isLeaf()) {
    for (const auto& child : children) {
      child->queryRange(x, y, range, result);
    }
  }
}

template <typename EntityType>
void QuadTreeNode<EntityType>::subdivide() {
  std::cout << "Subdiving Nodes" << std::endl;
  int halfSize = size / 2;
  children[0] = std::make_unique<QuadTreeNode<EntityType>>(x, y, halfSize);
  children[1] = std::make_unique<QuadTreeNode<EntityType>>(x + halfSize, y, halfSize);
  children[2] = std::make_unique<QuadTreeNode<EntityType>>(x, y + halfSize, halfSize);
  children[3] = std::make_unique<QuadTreeNode<EntityType>>(x + halfSize, y + halfSize, halfSize);

  for (auto& entity : entities) {
    int midX = x + halfSize;
    int midY = y + halfSize;
    int index = (entity->midpoint[0] > midX) + 2 * (entity->midpoint[1] > midY);
    children[index]->entities.push_back(std::move(entity));
  }
  entities.clear();
}

template <typename EntityType>
bool QuadTreeNode<EntityType>::intersects(int x, int y, int range) const {
  return !(x + range < this->x || x - range > this->x + size || y + range < this->y || y - range > this->y + size);
}

template <typename EntityType>
QuadTree<EntityType>::QuadTree(int width, int height, int maxDepth) : maxDepth(maxDepth) {
  root = std::make_unique<QuadTreeNode<EntityType>>(0, 0, std::max(width, height));
}

template <typename EntityType>
void QuadTree<EntityType>::insertEntity(EntityType* entity) {
  root->insert(entity, 0, maxDepth);
}

template <typename EntityType>
void QuadTree<EntityType>::removeEntity(EntityType* entity) {
  root->remove(entity);
}

template <typename EntityType>
std::vector<EntityType*> QuadTree<EntityType>::queryRange(int x, int y, int range) const {
  std::vector<EntityType*> result;
  root->queryRange(x, y, range, result);
  return result;
}

#endif // QUAD_TREE_IMPL_H
