#ifndef CCollisionOBBBoxH
#define CCollisionOBBBoxH

#include "chai3d.h"
#include <algorithm>
#include <cmath>

namespace chai3d {

class cCollisionOBBBox {
public:
    cVector3d m_center;
    cVector3d m_extent; // e0, e1, e2
    cVector3d u[3];    // 3 local orthonormal axes

    cCollisionOBBBox() {
        m_center.zero();
        m_extent.zero();
        u[0].set(1, 0, 0);
        u[1].set(0, 1, 0);
        u[2].set(0, 0, 1);
    }

    void padBox(double a_epsilon = 1e-5) {
        m_extent.add(cVector3d(a_epsilon, a_epsilon, a_epsilon));
    }

    bool intersectSegment(const cVector3d& a_segmentPointA, 
                          const cVector3d& a_segmentPointB, 
                          double& a_tMin, 
                          double& a_tMax) const 
    {
        cVector3d p = a_segmentPointA - m_center;
        cVector3d d = a_segmentPointB - a_segmentPointA;

        double tmin = 0.0;
        double tmax = 1.0;

        for (int i = 0; i < 3; i++) {
            double direction_proj = cDot(d, u[i]);
            double origin_proj = cDot(p, u[i]);

            if (std::abs(direction_proj) < 1e-8) {
                if (std::abs(origin_proj) > m_extent(i)) return false;
            } else {
                double ood = 1.0 / direction_proj;
                double t1 = (-m_extent(i) - origin_proj) * ood;
                double t2 = ( m_extent(i) - origin_proj) * ood;

                if (t1 > t2) std::swap(t1, t2);
                tmin = std::max(tmin, t1);
                tmax = std::min(tmax, t2);

                if (tmin > tmax) return false;
            }
        }
        a_tMin = tmin;
        a_tMax = tmax;
        return true;
    }
};

} // namespace chai3d

#endif
