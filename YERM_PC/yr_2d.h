// Copyright 2022 onart@github. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#ifndef __YR_2D_H__
#define __YR_2D_H__

#include "yr_graphics.h"

namespace onart{
    class Transform;


    using _2dvertex_t = YRGraphics::Vertex<float[2], float[2]>;
    YRGraphics::pPipeline get2DDefaultPipeline();
    YRGraphics::pPipeline get2DInstancedPipeline();
    YRGraphics::pMesh get2DDefaultQuad();

    class Sprite{
        public:
            void draw(const class Transform&);
        private:
            Sprite();
            struct VisualElement* elem;
    };

    void addSprite(class Scene& scene);
}

#endif