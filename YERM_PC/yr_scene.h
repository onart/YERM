#ifndef __YR_SCENE_H__
#define __YR_SCENE_H__

#include "yr_math.hpp"
#include <vector>

namespace onart{

    class Transform {
    public:
        static Transform* create(Transform* parent = nullptr);
        ~Transform();
        void setParent(Transform* parent = nullptr);

        void getGlobalTransform();
        inline const mat4& getLocalTransform() {
            updateLocalMatrix();
            return localTransform;
        }
        const mat4& getGlobalTransform();

        inline const vec3& getLocalPosition() const { return localPosition; }
        inline const vec3& getLocalScale() const { return localScale; }
        inline const Quaternion& getLocalRotation() const { return localRotation; }
        inline vec3 getGlobalPosition();
        inline Quaternion getGlobalRotation();

        inline void addPosition(const vec3& d) { setLocalDirty(); localPosition += d; }
        inline void setPosition(const vec3& d) { setLocalDirty(); localPosition = d; }
        inline void setPositionX(float x) { setLocalDirty(); localPosition.x = x; }
        inline void setPositionY(float y) { setLocalDirty(); localPosition.y = y; }
        inline void setPositionZ(float z) { setLocalDirty(); localPosition.z = z; }
        inline void setScaleX(float x) { setLocalDirty(); localScale.x = x; }
        inline void setScaleY(float y) { setLocalDirty(); localScale.y = y; }
        inline void setScaleZ(float z) { setLocalDirty(); localScale.z = z; }
        inline void setScale(const vec3& s) { setLocalDirty(); localScale = s; }
        inline void multiplyScale(const vec3& s) { setLocalDirty(); localScale *= s; }
        inline void setRotation(const Quaternion& r) { setLocalDirty(); localRoatation = r; }
        inline void setRotation(float roll, float pitch, float yaw) { setLocalDirty(); localRotation = Quaternion::euler(roll, pitch, yaw); }
        inline void setRotation(const vec3& axis, float angle) { setLocalDirty(); localRoatation = Quaternion::rotation(axis, angle); }
        inline void addRotation(const Quaternion& r) { setLocalDirty(); localRoatation = r * localRotation; }
        inline void addRotation(float roll, float pitch, float yaw) { setLocalDirty(); localRotation = Quaternion::euler(roll, pitch, yaw) * localRoatation; }
        inline void addRotation(const vec3& axis, float angle) { setLocalDirty(); localRoatation = Quaternion::rotation(axis, angle) * localRotation; }
        
        void setGlobalPosition(const vec3& pos);
        void setGlobalRotation(const Quaternion& r);
        
    private:
        Transform() = default;
        void setLocalDirty();
        void setGlobalDirty();
        inline void updateLocalMatrix() { if (!dirty) return; localTransform = mat4::TRS(localPosition, localRoatation, localScale); dirty = false; }
        vec3 localPosition = vec3(0.0f);
        Quaternion localRoatation = Quaternion();
        vec3 localScale = vec3(1.0f);
        mat4 localTransform;
        mat4 globalTransform;
        bool dirty = true;
        bool globalDirty = true;
        int bro = 0;
        std::vector<Transform*> children;
        Transform* parent = {};
    };
}

#endif