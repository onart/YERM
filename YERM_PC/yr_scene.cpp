#include "yr_scene.h"

namespace onart {

    void Transform::setLocalDirty() {
        dirty = true;
        setGlobalDirty();
    }

    void Transform::setGlobalDirty() {
        if (!globalDirty) {
            globalDirty = true;
            for (Transform* child : children) {
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
        return newTr;
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
        return globalTransform.col(3).xyz();
    }

    Quaternion Transform::getGlobalRotation() {
        if (!parent) return localRotation;
        return parent->getGlobalRotation() * localRotation;
    }

    void Transform::setParent(Transform* p) {
        if (p == this) p = nullptr;
        else if (parent == p) return;

        setGlobalDirty();
        getGlobalTransform();
        parent->children.back()->bro = bro;
        parent->children[bro] = parent->children.back();
        parent->children.pop_back();

        parent = p;
        if (parent) { 
            bro = parent->children.size();
            parent->children.push_back(this);
            const mat4& parentTransform = parent->getGlobalTransform();
            localTransform = parentTransform.affineInverse() * globalTransform;
        }
        else { 
            bro = 0;
            localTransform = globalTransform;
        }

        mat2prs();
    }

    void Transform::mat2prs() {
        localPosition = localTransform.col(3);
        localScale = localTransform.row(0);
        vec3 rsr2(localTransform.row(1).xyz());
        vec3 rsr3(localTransform.row(2).xyz());
        localScale *= localScale;	rsr2 *= rsr2;	rsr3 *= rsr3;
        localScale += rsr2;	localScale += rsr3;
        sqrt4(localScale.entry);
        mat4 rot(localTransform * mat4::scale(vec3(1) / localScale));
        float tr = rot.trace() - rot[15];		// = 2cosx + 1
        float cos2 = (tr + 1) * 0.25f;			// = (1 + cosx) / 2 = cos^2(x/2)
        float sin2 = 1 - cos2;
        float c = (tr - 1) * 0.5f;	// = cosx
        c = c > 1 ? 1 : c;
        float s = sqrtf(1 - c * c);
        if (s <= FLT_EPSILON) {
            memset(&localRotation, 0, sizeof(localRotation));
            localRotation.c1 = 1;
        }
        else {
            localRotation.ci = (rot.at(2, 1) - rot.at(1, 2));	// 2ci * sinx
            localRotation.cj = (rot.at(0, 2) - rot.at(2, 0));	// 2cj * sinx
            localRotation.ck = (rot.at(1, 0) - rot.at(0, 1));	// 2ck * sinx
            localRotation *= 0.5f / s * sqrtf(sin2);
            localRotation.c1 = sqrtf(cos2);
        }
    }
}
