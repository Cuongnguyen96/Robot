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

class translate : public hittable {
public:
    translate(shared_ptr<hittable> object, const vec3& offset) 
        : ptr(object), offset(offset) {
        bbox = object->bounding_box() + offset;
    }
    
    bool hit(const ray& r, interval ray_t, hit_record& rec) const override {
        // Move the ray backwards by the offset
        ray offset_r(r.origin() - offset, r.direction(), r.time());

        // Determine whether an intersection exists along the offset ray 
        if (!ptr->hit(offset_r, ray_t, rec)) {
            return false;
        }

        // Move the intersection point forward by the offset
        rec.p += offset;

        return true;
    }

    aabb bounding_box() const override { return bbox; }
private:
    shared_ptr<hittable> ptr;
    vec3 offset;
    aabb bbox;
};

class rotate_y : public hittable {
public:
    rotate_y(shared_ptr<hittable> object, double angle) 
        : ptr(object) {
            auto radians = degrees_to_radians(angle);
            sin_theta = std::sin(radians);
            cos_theta = std::cos(radians);
            bbox = object->bounding_box();

            point3 min( infinity,  infinity,  infinity);
            point3 max(-infinity, -infinity, -infinity);

            for (int i = 0; i < 2; i++) {
                for (int j = 0; j < 2; j++) {
                    for (int k = 0; k < 2; k++) {
                        auto x = i*bbox.x.max + (1-i)*bbox.x.min;
                        auto y = j*bbox.y.max + (1-j)*bbox.y.min;
                        auto z = k*bbox.z.max + (1-k)*bbox.z.min;

                        auto newx = cos_theta*x + sin_theta*z;
                        auto newz = -sin_theta*x + cos_theta*z;

                        vec3 tester(newx, y, newz);

                        for (int c = 0; c < 3; c++) {
                            min[c] = std::fmin(min[c], tester[c]);
                            max[c] = std::fmax(max[c], tester[c]);
                        }
                    }
                }
            }

            bbox = aabb(min, max);
    }

    bool hit(const ray& r, interval ray_t, hit_record& rec) const override {
        // Transform the ray from world space to the object space
        auto origin = point3(
            cos_theta*r.origin().x() - sin_theta*r.origin().z(),
            r.origin().y(),
            sin_theta*r.origin().x() + cos_theta*r.origin().z()
        );
        auto direction = vec3(
            cos_theta*r.direction().x() - sin_theta*r.direction().z(),
            r.direction().y(),
            sin_theta*r.direction().x() + cos_theta*r.direction().z()
        );
        ray rotated_r(origin, direction, r.time());

        // Determine whether an intersection exists along the rotated ray
        if (!ptr->hit(rotated_r, ray_t, rec)) {
            return false;
        }   

        // Transform the intersection point and normal back to world space
        rec.p = point3(
            cos_theta*rec.p.x() + sin_theta*rec.p.z(),
            rec.p.y(),
            -sin_theta*rec.p.x() + cos_theta*rec.p.z()
        );

        rec.normal = vec3(
            cos_theta*rec.normal.x() + sin_theta*rec.normal.z(),
            rec.normal.y(),
            -sin_theta*rec.normal.x() + cos_theta*rec.normal.z()
        );

        return true;       
    }

    aabb bounding_box() const override { return bbox;  }

private:
    shared_ptr<hittable> ptr;
    double sin_theta;
    double cos_theta;
    aabb bbox;
};

#endif