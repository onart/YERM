#ifndef __YR_SCENE_H__
#define __YR_SCENE_H__

#include "yr_math.hpp"
#include <vector>

namespace onart{

    struct alignas(16) a16 :public align16 {
        int x;
    };

    class Transform: public align16 {
    public:
        static Transform* create(Transform* parent = nullptr);
        ~Transform();
        void setParent(Transform* parent = nullptr);

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
        inline void setRotation(const Quaternion& r) { setLocalDirty(); localRotation = r; }
        inline void setRotation(float roll, float pitch, float yaw) { setLocalDirty(); localRotation = Quaternion::rotation(roll, pitch, yaw); }
        inline void setRotation(const vec3& axis, float angle) { setLocalDirty(); localRotation = Quaternion::rotation(axis, angle); }
        inline void addRotation(const Quaternion& r) { setLocalDirty(); localRotation = r * localRotation; }
        inline void addRotation(float roll, float pitch, float yaw) { setLocalDirty(); localRotation = Quaternion::rotation(roll, pitch, yaw) * localRotation; }
        inline void addRotation(const vec3& axis, float angle) { setLocalDirty(); localRotation = Quaternion::rotation(axis, angle) * localRotation; }
        
        void setGlobalPosition(const vec3& pos);
        void setGlobalRotation(const Quaternion& r);
        
    private:
        Transform() = default;
        void setLocalDirty();
        void setGlobalDirty();
        inline void updateLocalMatrix() { if (!dirty) return; localTransform = mat4::TRS(localPosition, localRotation, localScale); dirty = false; }
        void mat2prs();
        vec3 localPosition = vec3(0.0f);
        Quaternion localRotation = Quaternion();
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