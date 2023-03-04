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
#include "yr_model.h"
#define TINYGLTF_IMPLEMENTATION
#include "../externals/single_header/tiny_gltf.h"
#include "../externals/boost/predef/platform.h"

#if BOOST_PLAT_ANDROID
#include <game-activity/native_app_glue/android_native_app_glue.h>
#endif

namespace onart{

    std::map<int32_t, std::shared_ptr<Model>> Model::models;

    void Model::init(void* v){
#if BOOST_PLAT_ANDROID
        //if(v) tinygltf::asset_manager = reinterpret_cast<android_app*>(v)->activity->assetManager;
#endif
    }

    pModel Model::load(const char* fileName, int32_t name, uint64_t flags) {
        tinygltf::TinyGLTF loader;
        tinygltf::Model model;
        std::string err;
        std::string warn;
        bool res = loader.LoadASCIIFromFile(&model,&err,&warn,fileName);
        if(!warn.empty()){
            LOGWITH(warn);
        }
        if(!res){
            LOGWITH(err);
            return {};
        }
        for(tinygltf::Image& img: model.images){
            
        }
        for(tinygltf::Material& material: model.materials){

        }
        for(tinygltf::Node& node: model.nodes){

        }
        for(tinygltf::Mesh& mesh: model.meshes){

        }
        return {};
    }
}