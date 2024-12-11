#ifndef NEW_QUAD_TREE_IMPL_H
#define NEW_QUAD_TREE_IMPL_H

#include <cmath>
#include <iostream>
#include "NewQuadTree.h"

// Insert a node into the quadtree
template <typename EntityType>
void Quad<EntityType>::insert(Node<EntityType>* node) {
    if (node == nullptr)
        return;

    if (!inBoundary(node->pos))
        return;

    if (abs(topLeft.x - botRight.x) <= 1 && abs(topLeft.y - botRight.y) <= 1) {
        if (n == nullptr)
            n = node;
        return;
    }

    if ((topLeft.x + botRight.x) / 2 >= node->pos.x) {
        if ((topLeft.y + botRight.y) / 2 >= node->pos.y) {
            if (topLeftTree == nullptr) {
                topLeftTree = new Quad(Point(topLeft.x, topLeft.y), Point((topLeft.x + botRight.x) / 2, (topLeft.y + botRight.y) / 2));
            }
            topLeftTree->insert(node);
        } else {
            if (botLeftTree == nullptr) {
                botLeftTree = new Quad(Point(topLeft.x, (topLeft.y + botRight.y) / 2), Point((topLeft.x + botRight.x) / 2, botRight.y));
            }
            botLeftTree->insert(node);
        }
    } else {
        if ((topLeft.y + botRight.y) / 2 >= node->pos.y) {
            if (topRightTree == nullptr) {
                topRightTree = new Quad(Point((topLeft.x + botRight.x) / 2, topLeft.y), Point(botRight.x, (topLeft.y + botRight.y) / 2));
            }
            topRightTree->insert(node);
        } else {
            if (botRightTree == nullptr) {
                botRightTree = new Quad(Point((topLeft.x + botRight.x) / 2, (topLeft.y + botRight.y) / 2), Point(botRight.x, botRight.y));
            }
            botRightTree->insert(node);
        }
    }
}

// Find a node in a quadtree
template <typename EntityType>
Node<EntityType>* Quad<EntityType>::search(Point p) {
    if (!inBoundary(p))
        return nullptr;

    if (n != nullptr)
        return n;

    if ((topLeft.x + botRight.x) / 2 >= p.x) {
        if ((topLeft.y + botRight.y) / 2 >= p.y) {
            if (topLeftTree == nullptr)
                return nullptr;
            return topLeftTree->search(p);
        } else {
            if (botLeftTree == nullptr)
                return nullptr;
            return botLeftTree->search(p);
        }
    } else {
        if ((topLeft.y + botRight.y) / 2 >= p.y) {
            if (topRightTree == nullptr)
                return nullptr;
            return topRightTree->search(p);
        } else {
            if (botRightTree == nullptr)
                return nullptr;
            return botRightTree->search(p);
        }
    }
}

// Check if current quadtree contains the point
template <typename EntityType>
bool Quad<EntityType>::inBoundary(Point p) {
    return (p.x >= topLeft.x && p.x <= botRight.x && p.y >= topLeft.y && p.y <= botRight.y);
}

#endif // NEW_QUAD_TREE_IMPL_H
