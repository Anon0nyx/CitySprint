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
  std::cout << "Subdividing Nodes at (" << x << ", " << y << ") with size " << size << std::endl;
  int halfSize = size / 2;
  children[0] = std::make_unique<QuadTreeNode<EntityType>>(x, y, halfSize);
  children[1] = std::make_unique<QuadTreeNode<EntityType>>(x + halfSize, y, halfSize);
  children[2] = std::make_unique<QuadTreeNode<EntityType>>(x, y + halfSize, halfSize);
  children[3] = std::make_unique<QuadTreeNode<EntityType>>(x + halfSize, y + halfSize, halfSize);

  // Distribute existing entities to children
  for (auto& entity : entities) {
    if (entity != nullptr) {
      int midX = x + halfSize;
      int midY = y + halfSize;
      int index = (entity->midpoint[0] >= midX) + 2 * (entity->midpoint[1] >= midY);
      std::cout << "Moving entity with midpoint (" << entity->midpoint[0] << ", " << entity->midpoint[1] << ") to child " << index << std::endl;
      children[index]->insert(entity, 1, 0); // Insert into the appropriate child
    }
  }
  entities.clear();
}

template <typename EntityType>
void QuadTreeNode<EntityType>::insert(EntityType* entity, int depth, int maxDepth) {
  std::cout << "Inserting entity at depth " << depth << " with midpoint (" << entity->midpoint[0] << ", " << entity->midpoint[1] << ") into node at (" << x << ", " << y << ")" << std::endl;
  if (isLeaf()) {
    if (entities.size() < 4 || depth == maxDepth) {
      entities.push_back(entity);
    }
    else {
      subdivide();
      // Re-insert entities into the appropriate children
      for (auto& e : entities) {
        int midX = x + size / 2;
        int midY = y + size / 2;
        int index = (e->midpoint[0] >= midX) + 2 * (e->midpoint[1] >= midY);
        children[index]->insert(e, depth + 1, maxDepth);
      }
      entities.clear();
      // Insert the new entity into the appropriate child
      int midX = x + size / 2;
      int midY = y + size / 2;
      int index = (entity->midpoint[0] >= midX) + 2 * (entity->midpoint[1] >= midY);
      children[index]->insert(entity, depth + 1, maxDepth);
    }
  }
  else {
    int midX = x + size / 2;
    int midY = y + size / 2;
    int index = (entity->midpoint[0] >= midX) + 2 * (entity->midpoint[1] >= midY);
    children[index]->insert(entity, depth + 1, maxDepth);
  }
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
