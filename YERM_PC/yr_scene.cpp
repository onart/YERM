#include "yr_scene.h"

namespace onart {

    void Transform::setLocalDirty() {
        dirty = true;
        setGlobalDirty();
    }

    void Transform::setGlobalDirty() {
        if (!globalDirty) {
            globalDirty = true;
            for (Transform* chlid : children) {
                child->setGlobalDirty();
            }
        }
    }

    Transform* Transform::create(Transform* parent) {
        Transform* newTr = new Transform;
        if (!parent) { return newTr; }
        newTr->parent = parent;
        newTr->bro = parent->children.size();
        parent->children.push_back(newTr);
    }

    Transform::~Transform() {
        if (parent) {
            parent->children.back()->bro = bro;
            parent->children[bro] = parent->children.back();
            parent->children.pop_back();
        }
        for (Transform* child : children) {
            child->parent = nullptr;
            delete child;
        }
    }

    const mat4& Transform::getGlobalTransform() {
        if (!parent) return getLocalTransform();
        if (globalDirty) {
            globalTransform = parent->getGlobalTransform() * getLocalTransform();
            globalDirty = false;
        }
        return globalTransform;
    }

    void Transform::setGlobalPosition(const vec3& pos) {
        if (!parent) return setPosition(pos);
        setGlobalDirty();
        getGlobalTransform();
        globalTransform._14 = pos.x;
        globalTransform._24 = pos.y;
        globalTransform._34 = pos.z;
        mat4 upper = parent->getGlobalTransform().affineInverse();
        localTransform = upper * globalTransform;
        // FACT: We only need to update localPosition
        // proof:
        // G' = P * L'
        // P^-1 * G' = L'
        // ( RS1 T_P ) ( RS2 T_G ) = ( RS1RS2 RS1*T_G + T_P )
        // ( 0   1  )  ( 0   1  )    (  0          1     )
        // changing T_G to T_G' only changes upper right side
        localPosition = localTransform.col(3);
    }

    void Transform::setGlobalRotation(const Quaternion& r) {
        if (!parent) return setRotation(r);
        setRotation(parent->getGlobalRotation().inverse() * r);
    }

    vec3 Transform::getGlobalPosition() {
        getGlobalTransform();
        return globalTransform.col(3);
    }

    Quaternion Transform::getGlobalRotation() {
        if (!parent) return localRoatation;
        return parent->getGlobalRotation() * localRoatation;
    }
}
