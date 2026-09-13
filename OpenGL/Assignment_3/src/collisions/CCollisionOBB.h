#ifndef CCollisionOBBH
#define CCollisionOBBH

#include "chai3d.h"
#include "collisions/CGenericCollision.h"
#include "collisions/CCollisionOBBBox.h"
#include <vector>

namespace chai3d {

class cCollisionOBBNode;
class cCollisionOBBLeaf;
class cCollisionOBBInternal;

class cCollisionOBB : public cGenericCollision
{
public:
    cCollisionOBB();
    virtual ~cCollisionOBB();

    void initialize(cMesh* a_mesh, const double a_boundaryRadius = 0.0);
    void initialize(cTriangleArrayPtr a_triangles, const double a_boundaryRadius = 0.0);
    void initialize(cTriangleArrayPtr a_triangles, cVertexArrayPtr a_vertices, const double a_boundaryRadius = 0.0);
    void initialize(cTriangleArrayPtr a_triangles, cVertexArrayPtr a_vertices, const std::vector<unsigned int>& a_indices, const double a_boundaryRadius = 0.0);

    virtual bool computeCollision(cGenericObject* a_object,
                                  cVector3d& a_segmentPointA,
                                  cVector3d& a_segmentPointB,
                                  cCollisionRecorder& a_recorder,
                                  cCollisionSettings& a_settings) override;

    virtual void render(cRenderOptions& a_options) override;

    cCollisionOBBInternal* m_internalNodes;
    cCollisionOBBLeaf* m_leaves;
    cCollisionOBBNode* m_root;
    cTriangleArrayPtr m_triangles;
    cVertexArrayPtr m_vertices;
    bool m_useNeighbors;

protected:
    cCollisionOBBNode* buildTree(int& a_nextInternalNode, 
                                 int& a_nextLeafNode, 
                                 std::vector<unsigned int>& a_nodeIndices);
};

class cCollisionOBBNode
{
public:
    cCollisionOBBNode() : m_nodeType(-1) {}
    virtual ~cCollisionOBBNode() {}

    cCollisionOBBBox m_bbox;
    int m_nodeType;

    virtual bool computeCollision(cGenericObject* a_object,
                                  cVector3d& a_segmentPointA,
                                  cVector3d& a_segmentPointB,
                                  cCollisionRecorder& a_recorder,
                                  cCollisionSettings& a_settings) = 0;

    virtual void fitBox(cTriangleArrayPtr a_triangles, cVertexArrayPtr a_vertices, const std::vector<unsigned int>& a_indices) = 0;
    virtual void render(int a_depth) = 0;
};

class cCollisionOBBLeaf : public cCollisionOBBNode
{
public:
    cCollisionOBBLeaf();
    virtual ~cCollisionOBBLeaf() {}

    cTriangleArrayPtr m_triangles;
    cVertexArrayPtr m_vertices;
    unsigned int m_triangleIndex;

    virtual bool computeCollision(cGenericObject* a_object,
                                  cVector3d& a_segmentPointA,
                                  cVector3d& a_segmentPointB,
                                  cCollisionRecorder& a_recorder,
                                  cCollisionSettings& a_settings) override;

    virtual void fitBox(cTriangleArrayPtr a_triangles, cVertexArrayPtr a_vertices, const std::vector<unsigned int>& a_indices) override;
    virtual void render(int a_depth) override;
};

class cCollisionOBBInternal : public cCollisionOBBNode
{
public:
    cCollisionOBBInternal();
    virtual ~cCollisionOBBInternal() {}

    cCollisionOBBNode* m_leftSubTree;
    cCollisionOBBNode* m_rightSubTree;

    virtual bool computeCollision(cGenericObject* a_object,
                                  cVector3d& a_segmentPointA,
                                  cVector3d& a_segmentPointB,
                                  cCollisionRecorder& a_recorder,
                                  cCollisionSettings& a_settings) override;

    virtual void fitBox(cTriangleArrayPtr a_triangles, cVertexArrayPtr a_vertices, const std::vector<unsigned int>& a_indices) override;
    virtual void render(int a_depth) override;
};

} // namespace chai3d

#endif
