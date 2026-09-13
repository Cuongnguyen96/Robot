#include "chai3d.h"
#include "collisions/CCollisionOBB.h"
#include <GL/gl.h>
#include <algorithm>
#include <cmath>

namespace chai3d {

// =============================================================================
// solveJacobi
// =============================================================================
// Computes all eigenvalues and eigenvectors of a real 3x3 symmetric matrix 'a'
// using the Jacobi Diagonalization Method (Givens rotations).
//
// Parameters:
//   a[3][3] : Input 3x3 symmetric covariance matrix (destroyed during computation).
//   d[3]    : Output array containing the 3 computed eigenvalues.
//   v[3][3] : Output 3x3 matrix whose COLUMNS store the 3 corresponding eigenvectors.
//
// Returns:
//   true if the matrix successfully diagonalized within 50 iterations; false otherwise.
// =============================================================================
static bool solveJacobi(double a[3][3], double d[3], double v[3][3])
{
    // -------------------------------------------------------------------------
    // Step 1: Initialize eigenvector matrix 'v' to Identity Matrix (I)
    //         and initial eigenvalues 'd' to the diagonal elements of 'a'.
    // -------------------------------------------------------------------------
    for (int i = 0; i < 3; i++)
    {
        for (int j = 0; j < 3; j++)
        {
            v[i][j] = (i == j) ? 1.0 : 0.0;
        }
        d[i] = a[i][i];
    }

    // Maximum number of Jacobi sweeps allowed (typically converges in 5-10 sweeps)
    const int max_iterations = 50;

    for (int iter = 0; iter < max_iterations; iter++)
    {
        // ---------------------------------------------------------------------
        // Step 2: Calculate 'sm', the sum of absolute values of upper off-diagonal
        //         elements sm = |a[0][1]| + |a[0][2]| + |a[1][2]|.
        // ---------------------------------------------------------------------
        double sm = 0.0;
        for (int i = 0; i < 2; i++)
        {
            for (int j = i + 1; j < 3; j++)
            {
                sm += std::abs(a[i][j]);
            }
        }

        // Convergence Check: If the sum of off-diagonal elements is effectively 0,
        // the matrix 'a' is fully diagonalized. Return success.
        if (sm < 1e-15)
        {
            return true;
        }

        // Threshold Jacobi strategy: For the first 3 iterations, skip rotating
        // off-diagonal elements smaller than 'thresh' to accelerate overall convergence.
        double thresh = (iter < 3) ? (0.2 * sm / 9.0) : 0.0;

        // ---------------------------------------------------------------------
        // Step 3: Sweep through all 3 upper off-diagonal elements (p, q)
        //         (0,1), (0,2), and (1,2).
        // ---------------------------------------------------------------------
        for (int p = 0; p < 2; p++)
        {
            for (int q = p + 1; q < 3; q++)
            {
                double g = 100.0 * std::abs(a[p][q]);

                // Underflow / Precision Protection: After iteration 3, if |a[p][q]| is
                // negligible compared to the magnitudes of diagonal elements d[p] and d[q],
                // zero out a[p][q] directly to prevent unnecessary rotation computations.
                if (iter > 3 && (std::abs(d[p]) + g == std::abs(d[p]))
                             && (std::abs(d[q]) + g == std::abs(d[q])))
                {
                    a[p][q] = 0.0;
                }
                else if (std::abs(a[p][q]) > thresh)
                {
                    // ---------------------------------------------------------
                    // Step 4: Calculate Givens Rotation parameters (c = cos theta, s = sin theta)
                    // ---------------------------------------------------------
                    double h = d[q] - d[p]; // Difference between diagonal elements
                    double t;

                    // If |a[p][q]| is extremely small relative to |h|, use linear approximation
                    if (std::abs(h) + g == std::abs(h))
                    {
                        t = (a[p][q]) / h;
                    }
                    else
                    {
                        // Calculate cot(2*theta) = (d_q - d_p) / (2 * a_pq)
                        double theta = 0.5 * h / (a[p][q]);

                        // Compute t = tan(theta) using numerically stable quadratic formula:
                        // t = sgn(theta) / (|theta| + sqrt(1 + theta^2)), ensuring |t| <= 1 (|theta| <= pi/4)
                        t = 1.0 / (std::abs(theta) + std::sqrt(1.0 + theta * theta));
                        if (theta < 0.0) t = -t;
                    }

                    // Compute c = cos(theta) and s = sin(theta)
                    double c = 1.0 / std::sqrt(1.0 + t * t);
                    double s = t * c;

                    // Tau = tan(theta/2) = s / (1 + c), used for stable Givens updates:
                    // x' = x - s*(y + x*tau),  y' = y + s*(x - y*tau)
                    double tau = s / (1.0 + c);

                    h = t * a[p][q];

                    // Update eigenvalues d[p] and d[q]
                    d[p] -= h;
                    d[q] += h;
                    a[p][q] = 0.0; // The off-diagonal element a[p][q] is eliminated (driven to zero)

                    // ---------------------------------------------------------
                    // Step 5: Update off-diagonal elements in row/col p and q.
                    //         Memory stores upper-triangle only (i < j).
                    // ---------------------------------------------------------

                    // Range 1: j < p (Elements in rows 0..p-1 at columns p and q)
                    for (int j = 0; j <= p - 1; j++)
                    {
                        g = a[j][p]; h = a[j][q];
                        a[j][p] = g - s * (h + g * tau);
                        a[j][q] = h + s * (g - h * tau);
                    }

                    // Range 2: p < j < q (Elements in row p at col j, and row j at col q)
                    for (int j = p + 1; j <= q - 1; j++)
                    {
                        g = a[p][j]; h = a[j][q];
                        a[p][j] = g - s * (h + g * tau);
                        a[j][q] = h + s * (g - h * tau);
                    }

                    // Range 3: j > q (Elements in row p at col j, and row q at col j)
                    for (int j = q + 1; j < 3; j++)
                    {
                        g = a[p][j]; h = a[q][j];
                        a[p][j] = g - s * (h + g * tau);
                        a[q][j] = h + s * (g - h * tau);
                    }

                    // ---------------------------------------------------------
                    // Step 6: Accumulate Givens rotation into eigenvector matrix 'v'
                    //         V_new = V * R_pq(theta)
                    // ---------------------------------------------------------
                    for (int j = 0; j < 3; j++)
                    {
                        g = v[j][p]; h = v[j][q];
                        v[j][p] = g - s * (h + g * tau);
                        v[j][q] = h + s * (g - h * tau);
                    }
                }
            }
        }
    }

    // Failed to converge within 50 iterations
    return false;
}

cCollisionOBB::cCollisionOBB()
{
    m_internalNodes = nullptr;
    m_leaves = nullptr;
    m_root = nullptr;
    m_triangles = nullptr;
    m_vertices = nullptr;
    m_useNeighbors = false;
}

cCollisionOBB::~cCollisionOBB()
{
    if (m_internalNodes != nullptr) delete[] m_internalNodes;
    if (m_leaves != nullptr) delete[] m_leaves;
}

void cCollisionOBB::initialize(cMesh* a_mesh, const double a_boundaryRadius)
{
    if (a_mesh == nullptr) return;
    initialize(a_mesh->m_triangles, a_mesh->m_vertices, a_boundaryRadius);
}

void cCollisionOBB::initialize(cTriangleArrayPtr a_triangles, const double a_boundaryRadius)
{
    if (!a_triangles) return;
    initialize(a_triangles, a_triangles->m_vertices, a_boundaryRadius);
}

void cCollisionOBB::initialize(cTriangleArrayPtr a_triangles, cVertexArrayPtr a_vertices, const double a_boundaryRadius)
{
    if (!a_triangles || !a_vertices) return;
    int numTriangles = a_triangles->getNumElements();
    if (numTriangles == 0) return;

    std::vector<unsigned int> indices(numTriangles);
    for (int i = 0; i < numTriangles; i++) {
        indices[i] = i;
    }
    initialize(a_triangles, a_vertices, indices, a_boundaryRadius);
}

void cCollisionOBB::initialize(cTriangleArrayPtr a_triangles, cVertexArrayPtr a_vertices, const std::vector<unsigned int>& a_indices, const double a_boundaryRadius)
{
    m_triangles = a_triangles;
    m_vertices = a_vertices;
    if (!m_triangles || !m_vertices) return;

    int numTriangles = (int)a_indices.size();
    if (numTriangles == 0) return;

    if (m_internalNodes != nullptr) delete[] m_internalNodes;
    if (m_leaves != nullptr) delete[] m_leaves;

    m_internalNodes = new cCollisionOBBInternal[numTriangles];
    m_leaves = new cCollisionOBBLeaf[numTriangles];

    int nextInternalNode = 0;
    int nextLeafNode = 0;

    std::vector<unsigned int> workingIndices = a_indices;
    m_root = buildTree(nextInternalNode, nextLeafNode, workingIndices);
}

// =============================================================================
// buildTree
// =============================================================================
// Recursively constructs a balanced top-down Oriented Bounding Box (OBB) tree.
//
// Parameters:
//   a_nextInternalNode : Reference to index in pre-allocated m_internalNodes array.
//   a_nextLeafNode     : Reference to index in pre-allocated m_leaves array.
//   a_nodeIndices      : Vector of triangle indices belonging to the current node.
//
// Returns:
//   Pointer to the constructed cCollisionOBBNode (either internal or leaf node).
// =============================================================================
cCollisionOBBNode* cCollisionOBB::buildTree(int& a_nextInternalNode, 
                                            int& a_nextLeafNode, 
                                            std::vector<unsigned int>& a_nodeIndices)
{
    int numTriangles = (int)a_nodeIndices.size();
    if (numTriangles == 0) return nullptr;

    // -------------------------------------------------------------------------
    // Base Case: Single triangle remaining -> Create a Leaf Node
    // -------------------------------------------------------------------------
    if (numTriangles == 1)
    {
        cCollisionOBBLeaf* leaf = &m_leaves[a_nextLeafNode++];
        leaf->m_triangles = m_triangles;
        leaf->m_vertices = m_vertices;
        leaf->m_triangleIndex = a_nodeIndices[0];
        leaf->fitBox(m_triangles, m_vertices, a_nodeIndices);
        return leaf;
    }

    // -------------------------------------------------------------------------
    // Recursive Case: Multiple triangles -> Create an Internal Node
    // -------------------------------------------------------------------------
    cCollisionOBBInternal* internalNode = &m_internalNodes[a_nextInternalNode++];
    internalNode->fitBox(m_triangles, m_vertices, a_nodeIndices);

    // -------------------------------------------------------------------------
    // Step 1: Identify the longest axis of the fitted OBB extent
    // -------------------------------------------------------------------------
    int longestAxis = 0;
    if (internalNode->m_bbox.m_extent(1) > internalNode->m_bbox.m_extent(0))
    {
        longestAxis = 1;
    }
    if (internalNode->m_bbox.m_extent(2) > internalNode->m_bbox.m_extent(longestAxis))
    {
        longestAxis = 2;
    }

    // Get the splitting axis direction vector and center plane coordinate
    cVector3d splitAxis = internalNode->m_bbox.u[longestAxis];
    double splitCenter = cDot(internalNode->m_bbox.m_center, splitAxis);

    // -------------------------------------------------------------------------
    // Step 2: Partition triangles into Left and Right sets based on centroid projection
    // -------------------------------------------------------------------------
    std::vector<unsigned int> leftIndices;
    std::vector<unsigned int> rightIndices;

    for (size_t i = 0; i < a_nodeIndices.size(); i++)
    {
        unsigned int idx = a_nodeIndices[i];
        unsigned int v0 = m_triangles->getVertexIndex0(idx);
        unsigned int v1 = m_triangles->getVertexIndex1(idx);
        unsigned int v2 = m_triangles->getVertexIndex2(idx);

        cVector3d p0 = m_vertices->getLocalPos(v0);
        cVector3d p1 = m_vertices->getLocalPos(v1);
        cVector3d p2 = m_vertices->getLocalPos(v2);

        // Compute triangle centroid in 3D local coordinates
        cVector3d centroid = (p0 + p1 + p2) / 3.0;

        // Project centroid onto split axis and compare against split center
        if (cDot(centroid, splitAxis) <= splitCenter)
        {
            leftIndices.push_back(idx);
        }
        else
        {
            rightIndices.push_back(idx);
        }
    }

    // -------------------------------------------------------------------------
    // Step 3: Degenerate case fallback -> 50/50 median split if one side is empty
    // -------------------------------------------------------------------------
    if (leftIndices.empty() || rightIndices.empty())
    {
        leftIndices.clear();
        rightIndices.clear();
        size_t half = numTriangles / 2;
        for (size_t i = 0; i < half; i++)
        {
            leftIndices.push_back(a_nodeIndices[i]);
        }
        for (size_t i = half; i < (size_t)numTriangles; i++)
        {
            rightIndices.push_back(a_nodeIndices[i]);
        }
    }

    // -------------------------------------------------------------------------
    // Step 4: Recursively construct left and right subtrees
    // -------------------------------------------------------------------------
    internalNode->m_leftSubTree = buildTree(a_nextInternalNode, a_nextLeafNode, leftIndices);
    internalNode->m_rightSubTree = buildTree(a_nextInternalNode, a_nextLeafNode, rightIndices);

    return internalNode;
}

bool cCollisionOBB::computeCollision(cGenericObject* a_object,
                                     cVector3d& a_segmentPointA,
                                     cVector3d& a_segmentPointB,
                                     cCollisionRecorder& a_recorder,
                                     cCollisionSettings& a_settings)
{
    if (m_root == nullptr) return false;

    double tMin, tMax;
    if (!m_root->m_bbox.intersectSegment(a_segmentPointA, a_segmentPointB, tMin, tMax))
    {
        return false;
    }

    return m_root->computeCollision(a_object, a_segmentPointA, a_segmentPointB, a_recorder, a_settings);
}

void cCollisionOBB::render(cRenderOptions& a_options)
{
    if (m_root != nullptr)
    {
        m_root->render(0);
    }
}

// Leaf Node Methods
cCollisionOBBLeaf::cCollisionOBBLeaf()
{
    m_nodeType = 1;
    m_triangles = nullptr;
    m_vertices = nullptr;
    m_triangleIndex = 0;
}

// =============================================================================
// cCollisionOBBLeaf::fitBox
// =============================================================================
// Computes a tight-fitting Oriented Bounding Box (OBB) around a single 3D triangle.
//
// Parameters:
//   a_triangles : Pointer to array of triangles in the mesh.
//   a_vertices  : Pointer to array of vertex coordinates.
//   a_indices   : Vector containing the single triangle index for this leaf.
// =============================================================================
void cCollisionOBBLeaf::fitBox(cTriangleArrayPtr a_triangles, cVertexArrayPtr a_vertices, const std::vector<unsigned int>& a_indices)
{
    if (!a_triangles || !a_vertices || a_indices.empty()) return;

    // -------------------------------------------------------------------------
    // Step 1: Extract 3D vertices p, q, r for the single triangle
    // -------------------------------------------------------------------------
    unsigned int idx = a_indices[0];
    m_triangleIndex = idx;

    unsigned int v0 = a_triangles->getVertexIndex0(idx);
    unsigned int v1 = a_triangles->getVertexIndex1(idx);
    unsigned int v2 = a_triangles->getVertexIndex2(idx);

    cVector3d p = a_vertices->getLocalPos(v0);
    cVector3d q = a_vertices->getLocalPos(v1);
    cVector3d r = a_vertices->getLocalPos(v2);

    // -------------------------------------------------------------------------
    // Step 2: Construct Orthonormal Local Coordinate Axes (u0, u1, u2)
    // -------------------------------------------------------------------------
    // Axis u0: First edge vector (p -> q) normalized
    cVector3d e0 = q - p;
    e0.normalize();

    cVector3d e1 = r - p;
    e1.normalize();

    // Axis u2: Triangle face normal vector perpendicular to the triangle plane
    cVector3d normal = cCross(e0, e1);
    normal.normalize();

    // Axis u1: Perpendicular to e0 within the triangle plane
    e1 = cCross(normal, e0);
    e1.normalize();

    m_bbox.u[0] = e0;
    m_bbox.u[1] = e1;
    m_bbox.u[2] = normal;

    // -------------------------------------------------------------------------
    // Step 3: Project vertices onto local axes to compute extent and center
    // -------------------------------------------------------------------------
    double mins[3], maxs[3];
    cVector3d pts[3] = {p, q, r};
    for (int i = 0; i < 3; i++)
    {
        mins[i] = 1e10;
        maxs[i] = -1e10;
        for (int k = 0; k < 3; k++)
        {
            double proj = cDot(pts[k], m_bbox.u[i]);
            mins[i] = std::min(mins[i], proj);
            maxs[i] = std::max(maxs[i], proj);
        }
        m_bbox.m_extent(i) = (maxs[i] - mins[i]) * 0.5;
    }

    // Compute center position in 3D space
    m_bbox.m_center = m_bbox.u[0] * (mins[0] + m_bbox.m_extent(0)) +
                      m_bbox.u[1] * (mins[1] + m_bbox.m_extent(1)) +
                      m_bbox.u[2] * (mins[2] + m_bbox.m_extent(2));

    // -------------------------------------------------------------------------
    // Step 4: Expand thin planar extent along face normal to prevent zero-thickness
    // -------------------------------------------------------------------------
    m_bbox.padBox(1e-5);
}

bool cCollisionOBBLeaf::computeCollision(cGenericObject* a_object,
                                         cVector3d& a_segmentPointA,
                                         cVector3d& a_segmentPointB,
                                         cCollisionRecorder& a_recorder,
                                         cCollisionSettings& a_settings)
{
    if (!m_triangles) return false;
    return m_triangles->computeCollision(m_triangleIndex, a_object, a_segmentPointA, a_segmentPointB, a_recorder, a_settings);
}

// =============================================================================
// cCollisionOBBLeaf::render
// =============================================================================
// Renders the OpenGL debug wireframe visualization of a leaf node OBB box.
// Leaf nodes are drawn in bright green (0.0, 1.0, 0.0) with a line width of 1.0.
//
// Parameters:
//   a_depth : Current recursion depth in the OBB tree traversal.
// =============================================================================
void cCollisionOBBLeaf::render(int a_depth)
{
    // Step 1: Push OpenGL matrix state and translate to leaf OBB center
    glPushMatrix();
    glTranslated(m_bbox.m_center.x(), m_bbox.m_center.y(), m_bbox.m_center.z());

    // Step 2: Construct 4x4 rotation matrix from local orientation axes u0, u1, u2
    double mat[16] = {
        m_bbox.u[0].x(), m_bbox.u[0].y(), m_bbox.u[0].z(), 0,
        m_bbox.u[1].x(), m_bbox.u[1].y(), m_bbox.u[1].z(), 0,
        m_bbox.u[2].x(), m_bbox.u[2].y(), m_bbox.u[2].z(), 0,
        0,               0,               0,               1
    };
    glMultMatrixd(mat);

    // Step 3: Set wireframe color (Bright Green) and line width
    glColor3f(0.0f, 1.0f, 0.0f);
    glLineWidth(1.0f);

    // Step 4: Render bottom face loop (-extent.z)
    glBegin(GL_LINE_LOOP);
    glVertex3d(-m_bbox.m_extent(0), -m_bbox.m_extent(1), -m_bbox.m_extent(2));
    glVertex3d( m_bbox.m_extent(0), -m_bbox.m_extent(1), -m_bbox.m_extent(2));
    glVertex3d( m_bbox.m_extent(0),  m_bbox.m_extent(1), -m_bbox.m_extent(2));
    glVertex3d(-m_bbox.m_extent(0),  m_bbox.m_extent(1), -m_bbox.m_extent(2));
    glEnd();

    // Step 5: Render top face loop (+extent.z)
    glBegin(GL_LINE_LOOP);
    glVertex3d(-m_bbox.m_extent(0), -m_bbox.m_extent(1),  m_bbox.m_extent(2));
    glVertex3d( m_bbox.m_extent(0), -m_bbox.m_extent(1),  m_bbox.m_extent(2));
    glVertex3d( m_bbox.m_extent(0),  m_bbox.m_extent(1),  m_bbox.m_extent(2));
    glVertex3d(-m_bbox.m_extent(0),  m_bbox.m_extent(1),  m_bbox.m_extent(2));
    glEnd();

    // Step 6: Render 4 vertical connecting edges
    glBegin(GL_LINES);
    glVertex3d(-m_bbox.m_extent(0), -m_bbox.m_extent(1), -m_bbox.m_extent(2));
    glVertex3d(-m_bbox.m_extent(0), -m_bbox.m_extent(1),  m_bbox.m_extent(2));

    glVertex3d( m_bbox.m_extent(0), -m_bbox.m_extent(1), -m_bbox.m_extent(2));
    glVertex3d( m_bbox.m_extent(0), -m_bbox.m_extent(1),  m_bbox.m_extent(2));

    glVertex3d( m_bbox.m_extent(0),  m_bbox.m_extent(1), -m_bbox.m_extent(2));
    glVertex3d( m_bbox.m_extent(0),  m_bbox.m_extent(1),  m_bbox.m_extent(2));

    glVertex3d(-m_bbox.m_extent(0),  m_bbox.m_extent(1), -m_bbox.m_extent(2));
    glVertex3d(-m_bbox.m_extent(0),  m_bbox.m_extent(1),  m_bbox.m_extent(2));
    glEnd();

    // Step 7: Restore previous OpenGL matrix state
    glPopMatrix();
}

// Internal Node Methods
cCollisionOBBInternal::cCollisionOBBInternal()
{
    m_nodeType = 0;
    m_leftSubTree = nullptr;
    m_rightSubTree = nullptr;
}

// =============================================================================
// fitBox (cCollisionOBBInternal)
// =============================================================================
// Computes an optimal tight-fitting Oriented Bounding Box (OBB) for a group
// of multiple 3D triangles using Gottschalk '96 Continuous Surface Covariance
// Matrix (PCA) and Jacobi Eigensolver.
//
// Parameters:
//   a_triangles : Pointer to the array of mesh triangles.
//   a_vertices  : Pointer to the array of mesh vertices.
//   a_indices   : Vector of triangle indices belonging to this internal node.
// =============================================================================
void cCollisionOBBInternal::fitBox(cTriangleArrayPtr a_triangles, cVertexArrayPtr a_vertices, const std::vector<unsigned int>& a_indices)
{
    if (!a_triangles || !a_vertices || a_indices.empty()) return;

    // -------------------------------------------------------------------------
    // Step 1: Compute Total Surface Area and Global Surface Centroid (m_bar)
    // -------------------------------------------------------------------------
    double area_sum = 0.0;
    cVector3d centroid_hull(0, 0, 0);

    for (size_t i = 0; i < a_indices.size(); i++)
    {
        unsigned int idx = a_indices[i];
        unsigned int v0 = a_triangles->getVertexIndex0(idx);
        unsigned int v1 = a_triangles->getVertexIndex1(idx);
        unsigned int v2 = a_triangles->getVertexIndex2(idx);

        cVector3d p = a_vertices->getLocalPos(v0);
        cVector3d q = a_vertices->getLocalPos(v1);
        cVector3d r = a_vertices->getLocalPos(v2);

        // Compute triangle area a_k = 0.5 * ||(q - p) x (r - p)||
        cVector3d e0 = q - p;
        cVector3d e1 = r - p;
        double a_k = 0.5 * cCross(e0, e1).length();

        area_sum += a_k;
        // Compute triangle centroid m_k = (p + q + r) / 3.0
        cVector3d m_k = (p + q + r) / 3.0;
        centroid_hull += a_k * m_k;
    }

    if (area_sum > 0.0)
    {
        centroid_hull /= area_sum; // Normalize to get global surface centroid c
    }

    // -------------------------------------------------------------------------
    // Step 2: Accumulate Gottschalk '96 Continuous Surface Covariance Matrix C
    // -------------------------------------------------------------------------
    double C_mat[3][3] = { {0,0,0}, {0,0,0}, {0,0,0} };

    for (size_t i = 0; i < a_indices.size(); i++)
    {
        unsigned int idx = a_indices[i];
        unsigned int v0 = a_triangles->getVertexIndex0(idx);
        unsigned int v1 = a_triangles->getVertexIndex1(idx);
        unsigned int v2 = a_triangles->getVertexIndex2(idx);

        cVector3d p = a_vertices->getLocalPos(v0);
        cVector3d q = a_vertices->getLocalPos(v1);
        cVector3d r = a_vertices->getLocalPos(v2);

        cVector3d e0 = q - p;
        cVector3d e1 = r - p;
        double a_k = 0.5 * cCross(e0, e1).length();

        cVector3d m_k = (p + q + r) / 3.0;

        // Formula: C_ij_raw = sum( (a_k / 12) * [9*m_i*m_j + p_i*p_j + q_i*q_j + r_i*r_j] )
        for (int r_idx = 0; r_idx < 3; r_idx++)
        {
            for (int c_idx = 0; c_idx < 3; c_idx++)
            {
                double val = (9.0 * m_k(r_idx) * m_k(c_idx) + p(r_idx)*p(c_idx) + q(r_idx)*q(c_idx) + r(r_idx)*r(c_idx)) * (a_k / 12.0);
                C_mat[r_idx][c_idx] += val;
            }
        }
    }

    // Final normalization: C_ij = (C_ij_raw / area_sum) - c_i * c_j
    for (int r_idx = 0; r_idx < 3; r_idx++)
    {
        for (int c_idx = 0; c_idx < 3; c_idx++)
        {
            C_mat[r_idx][c_idx] = (area_sum > 0.0) ? (C_mat[r_idx][c_idx] / area_sum) : 0.0;
            C_mat[r_idx][c_idx] -= centroid_hull(r_idx) * centroid_hull(c_idx);
        }
    }

    // -------------------------------------------------------------------------
    // Step 3: Solve Jacobi Eigensolver to extract Eigenvectors as Local Axes
    // -------------------------------------------------------------------------
    double eigenvalues[3];
    double eigenvectors[3][3];
    if (solveJacobi(C_mat, eigenvalues, eigenvectors))
    {
        // Columns of eigenvector matrix store principal axes u0, u1, u2
        m_bbox.u[0].set(eigenvectors[0][0], eigenvectors[1][0], eigenvectors[2][0]);
        m_bbox.u[1].set(eigenvectors[0][1], eigenvectors[1][1], eigenvectors[2][1]);
        m_bbox.u[2].set(eigenvectors[0][2], eigenvectors[1][2], eigenvectors[2][2]);

        m_bbox.u[0].normalize();
        m_bbox.u[1].normalize();
        // Ensure right-handed orthonormal basis: u2 = u0 x u1
        m_bbox.u[2] = cCross(m_bbox.u[0], m_bbox.u[1]);
        m_bbox.u[2].normalize();
    }
    else
    {
        // Fallback to world aligned axes if Jacobi fails
        m_bbox.u[0].set(1.0, 0.0, 0.0);
        m_bbox.u[1].set(0.0, 1.0, 0.0);
        m_bbox.u[2].set(0.0, 0.0, 1.0);
    }

    // -------------------------------------------------------------------------
    // Step 4: Project all triangle vertices onto axes u0, u1, u2 for Extents & Center
    // -------------------------------------------------------------------------
    double mins[3] = {1e10, 1e10, 1e10};
    double maxs[3] = {-1e10, -1e10, -1e10};

    for (size_t i = 0; i < a_indices.size(); i++)
    {
        unsigned int idx = a_indices[i];
        unsigned int v0 = a_triangles->getVertexIndex0(idx);
        unsigned int v1 = a_triangles->getVertexIndex1(idx);
        unsigned int v2 = a_triangles->getVertexIndex2(idx);

        cVector3d pts[3] = {
            a_vertices->getLocalPos(v0),
            a_vertices->getLocalPos(v1),
            a_vertices->getLocalPos(v2)
        };

        for (int k = 0; k < 3; k++)
        {
            for (int axis = 0; axis < 3; axis++)
            {
                double proj = cDot(pts[k], m_bbox.u[axis]);
                mins[axis] = std::min(mins[axis], proj);
                maxs[axis] = std::max(maxs[axis], proj);
            }
        }
    }

    // Extent is half of the bounding range along each local axis
    for (int axis = 0; axis < 3; axis++)
    {
        m_bbox.m_extent(axis) = (maxs[axis] - mins[axis]) * 0.5;
    }

    // OBB center position in local coordinates
    m_bbox.m_center = m_bbox.u[0] * (mins[0] + m_bbox.m_extent(0)) +
                      m_bbox.u[1] * (mins[1] + m_bbox.m_extent(1)) +
                      m_bbox.u[2] * (mins[2] + m_bbox.m_extent(2));

    // -------------------------------------------------------------------------
    // Step 5: Add safety padding to prevent zero-thickness degenerate volumes
    // -------------------------------------------------------------------------
    m_bbox.padBox(1e-5);
}

bool cCollisionOBBInternal::computeCollision(cGenericObject* a_object,
                                             cVector3d& a_segmentPointA,
                                             cVector3d& a_segmentPointB,
                                             cCollisionRecorder& a_recorder,
                                             cCollisionSettings& a_settings)
{
    bool hitLeft = false;
    double tMinL, tMaxL;
    if (m_leftSubTree != nullptr && m_leftSubTree->m_bbox.intersectSegment(a_segmentPointA, a_segmentPointB, tMinL, tMaxL))
    {
        hitLeft = m_leftSubTree->computeCollision(a_object, a_segmentPointA, a_segmentPointB, a_recorder, a_settings);
    }

    bool hitRight = false;
    double tMinR, tMaxR;
    if (m_rightSubTree != nullptr && m_rightSubTree->m_bbox.intersectSegment(a_segmentPointA, a_segmentPointB, tMinR, tMaxR))
    {
        hitRight = m_rightSubTree->computeCollision(a_object, a_segmentPointA, a_segmentPointB, a_recorder, a_settings);
    }

    return (hitLeft || hitRight);
}

// =============================================================================
// cCollisionOBBInternal::render
// =============================================================================
// Recursively renders the OpenGL debug wireframe visualization of internal OBB nodes.
// Color coding indicates tree depth level:
//   - Depth 0 (Root Node) : Red (1.0, 0.0, 0.0)
//   - Depth 1             : Orange (1.0, 0.5, 0.0)
//   - Depth 2+            : Yellow (1.0, 1.0, 0.0)
//
// Parameters:
//   a_depth : Current recursion depth level in the OBB tree.
// =============================================================================
void cCollisionOBBInternal::render(int a_depth)
{
    // Limit wireframe rendering to top 4 levels (depth < 4) to prevent visual clutter
    if (a_depth < 4)
    {
        // Step 1: Save OpenGL state and translate matrix to node OBB center
        glPushMatrix();
        glTranslated(m_bbox.m_center.x(), m_bbox.m_center.y(), m_bbox.m_center.z());

        // Step 2: Build 4x4 orientation matrix from local axes u0, u1, u2
        double mat[16] = {
            m_bbox.u[0].x(), m_bbox.u[0].y(), m_bbox.u[0].z(), 0,
            m_bbox.u[1].x(), m_bbox.u[1].y(), m_bbox.u[1].z(), 0,
            m_bbox.u[2].x(), m_bbox.u[2].y(), m_bbox.u[2].z(), 0,
            0,               0,               0,               1
        };
        glMultMatrixd(mat);

        // Step 3: Color-code node level (Red for Root, Orange for Lv1, Yellow for Lv2+)
        if (a_depth == 0) glColor3f(1.0f, 0.0f, 0.0f);
        else if (a_depth == 1) glColor3f(1.0f, 0.5f, 0.0f);
        else glColor3f(1.0f, 1.0f, 0.0f);

        // Adjust line width based on tree depth
        glLineWidth(1.5f - a_depth * 0.25f);

        // Step 4: Render bottom face loop (-extent.z)
        glBegin(GL_LINE_LOOP);
        glVertex3d(-m_bbox.m_extent(0), -m_bbox.m_extent(1), -m_bbox.m_extent(2));
        glVertex3d( m_bbox.m_extent(0), -m_bbox.m_extent(1), -m_bbox.m_extent(2));
        glVertex3d( m_bbox.m_extent(0),  m_bbox.m_extent(1), -m_bbox.m_extent(2));
        glVertex3d(-m_bbox.m_extent(0),  m_bbox.m_extent(1), -m_bbox.m_extent(2));
        glEnd();

        // Step 5: Render top face loop (+extent.z)
        glBegin(GL_LINE_LOOP);
        glVertex3d(-m_bbox.m_extent(0), -m_bbox.m_extent(1),  m_bbox.m_extent(2));
        glVertex3d( m_bbox.m_extent(0), -m_bbox.m_extent(1),  m_bbox.m_extent(2));
        glVertex3d( m_bbox.m_extent(0),  m_bbox.m_extent(1),  m_bbox.m_extent(2));
        glVertex3d(-m_bbox.m_extent(0),  m_bbox.m_extent(1),  m_bbox.m_extent(2));
        glEnd();

        // Step 6: Render 4 vertical connecting edges
        glBegin(GL_LINES);
        glVertex3d(-m_bbox.m_extent(0), -m_bbox.m_extent(1), -m_bbox.m_extent(2));
        glVertex3d(-m_bbox.m_extent(0), -m_bbox.m_extent(1),  m_bbox.m_extent(2));

        glVertex3d( m_bbox.m_extent(0), -m_bbox.m_extent(1), -m_bbox.m_extent(2));
        glVertex3d( m_bbox.m_extent(0), -m_bbox.m_extent(1),  m_bbox.m_extent(2));

        glVertex3d( m_bbox.m_extent(0),  m_bbox.m_extent(1), -m_bbox.m_extent(2));
        glVertex3d( m_bbox.m_extent(0),  m_bbox.m_extent(1),  m_bbox.m_extent(2));

        glVertex3d(-m_bbox.m_extent(0),  m_bbox.m_extent(1), -m_bbox.m_extent(2));
        glVertex3d(-m_bbox.m_extent(0),  m_bbox.m_extent(1),  m_bbox.m_extent(2));
        glEnd();

        glPopMatrix();
    }

    // Step 7: Recursively traverse and render left and right subtrees
    if (m_leftSubTree != nullptr) m_leftSubTree->render(a_depth + 1);
    if (m_rightSubTree != nullptr) m_rightSubTree->render(a_depth + 1);
}

} // namespace chai3d
