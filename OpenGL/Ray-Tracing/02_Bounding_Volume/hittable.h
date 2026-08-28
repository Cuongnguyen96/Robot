#ifndef HITABLE_H
#define HITABLE_H

#include "ray.h"
#include "rtweekend.h"
#include "aabb.h"

class material; // Forwad declaration

// hit_record is a structure to bundle specific hit data.
class hit_record {
public:
    point3 p;    // The exact 3D point of intersection.
    vec3 normal; // The surface normal vector at that point.
    shared_ptr<material> mat; // The material
    double t;        // The ray parameter t at intersection (p = origin + t*direction).
    double u;
    double v;
    bool front_face; // Store fron face or back face

    void set_face_normal(const ray& r, const vec3& outward_normal) {
        // Set the hit record normal vector
        // Note: the parameter outward_normal is assumed to have unit length

        front_face = dot(r.direction(),  outward_normal) < 0;
        normal = front_face ? outward_normal : -outward_normal;
    }
};

// hittable is the abstract base class for any shape that interacts with a ray.
class hittable {
public:
    virtual ~hittable() = default;

    // Pure virtual hit function:
    // - r: The incoming ray.
    // - ray_tmin, ray_tmax: The valid interval for the hit's t value.
    // - rec: Reference to hit_record to populate with data if a hit occurs.
    // Returns true if hit, false if missed.
    virtual bool hit(const ray& r, interval ray_t, hit_record& rec) const = 0;

    virtual aabb bounding_box() const = 0;
};


#endif