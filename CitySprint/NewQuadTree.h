#ifndef NEW_QUAD_TREE_H
#define NEW_QUAD_TREE_H

#include <cmath>
#include <iostream>

// Used to hold details of a point
struct Point {
    int x;
    int y;
    Point(int _x, int _y) : x(_x), y(_y) {}
    Point() : x(0), y(0) {}
};

// The objects that we want stored in the quadtree
template <typename EntityType>
struct Node {
    Point pos;
    EntityType data;
    Node(Point _pos, EntityType _data) : pos(_pos), data(_data) {}
    Node() : data(EntityType()) {}
};

// The main quadtree class
template <typename EntityType>
class Quad {
    Point topLeft;
    Point botRight;
    Node<EntityType>* n;
    Quad* topLeftTree;
    Quad* topRightTree;
    Quad* botLeftTree;
    Quad* botRightTree;

public:
    Quad() : n(nullptr), topLeftTree(nullptr), topRightTree(nullptr), botLeftTree(nullptr), botRightTree(nullptr) {}
    Quad(Point topL, Point botR) : topLeft(topL), botRight(botR), n(nullptr), topLeftTree(nullptr), topRightTree(nullptr), botLeftTree(nullptr), botRightTree(nullptr) {}
    void insert(Node<EntityType>* node);
    Node<EntityType>* search(Point p);
    bool inBoundary(Point p);
};

#include "NewQuadTreeImplementation.h"

#endif
