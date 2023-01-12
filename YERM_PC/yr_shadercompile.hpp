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

/* 알림
이 모듈은 셰이더 프로그램 제작 중에 빠른 실행 및 디버그를 할 수 있도록 하기 위한 것입니다.
SPIR-V로의 컴파일은 미리 할 수 있는 일이기 때문에 릴리즈 버전에서는 사용할 필요가 없습니다.
덧붙여 그 때문에 필요성이 낮아, 안드로이드(ARM64) 버전은 준비하지 않았습니다.
*/
#ifndef __YR_SHADER_COMPILE_HPP__
#define __YR_SHADER_COMPILE_HPP__

#include "../externals/boost/predef/compiler.h"
#include "../externals/boost/predef/platform.h"
#include "../externals/shaderc/shaderc.hpp"

#include "logger.hpp"

#include <cerrno>
#include <cstdint>
#include <cstdio>

namespace onart{
    /// @brief GLSL 코드를 SPIR-V로 컴파일합니다. 안드로이드에선 아무것도 하지 않습니다.
    /// @param fileName 파일 이름
    /// @param kind 셰이더 종류
    /// @param spvCode 결과를 받을 벡터. 원래 내용물은 덮어씌웁니다.
    inline void compile(const char* fileName, shaderc_shader_kind kind, std::vector<uint32_t>* spvCode);
    /// @brief GLSL 코드를 SPIR-V로 컴파일합니다. 안드로이드에선 아무것도 하지 않습니다.
    /// @param code 메모리 상 문자열의 변수
    /// @param size 크기
    /// @param kind 셰이더 종류
    /// @param spvCode 결과를 받을 벡터. 원래 내용물은 덮어씌웁니다.
    inline void compile(const char* code, size_t size, shaderc_shader_kind kind, std::vector<uint32_t>* spvCode);

    inline FILE* openFileR(const char* fileName) {
        FILE* fp = nullptr;
#if BOOST_COMP_MSVC
        fopen_s(&fp, fileName, "rb");
#else
        fp = fopen(fileName, "rb");
#endif
        return fp;
    }

    inline void readFile(FILE* fp, void* output, size_t size) {
#if BOOST_COMP_MSVC
        fread_s(output, size, 1, size, fp);
#else
        fread(output, 1, size, fp);
#endif
    }
    
#define compileVertexShader(fileName, spvCode) compile(fileName, shaderc_shader_kind::shaderc_vertex_shader, spvCode)
#define compileFragmentShader(fileName, spvCode) compile(fileName, shaderc_shader_kind::shaderc_fragment_shader, spvCode)
#define compileGeometryShader(fileName, spvCode) compile(fileName, shaderc_shader_kind::shaderc_geometry_shader, spvCode)
#define compileTessellationControlShader(fileName, spvCode) compile(fileName, shaderc_shader_kind::shaderc_tess_control_shader, spvCode)
#define compileTessellationEvaluationShader(fileName, spvCode) compile(fileName, shaderc_shader_kind::shaderc_tess_evaluation_shader, spvCode)

    inline void compile(const char* fileName, shaderc_shader_kind kind, std::vector<uint32_t>* spvCode){
#if !BOOST_PLAT_ANDROID        
        FILE* fp = openFileR(fileName);
        if(!fp){
            LOGWITH("Failed to open file");
            return;
        }
        
        fseek(fp, 0, SEEK_END);
        long sz = ftell(fp);
        fseek(fp, 0, SEEK_SET);        
        std::vector<char> content(sz);
        readFile(fp, content.data(), sz);
        compile(content.data(), sz, kind, spvCode);
        fclose(fp);
#endif
    }

    inline void compile(const char* code, size_t size, shaderc_shader_kind kind, std::vector<uint32_t>* spvCode) {
#if !BOOST_PLAT_ANDROID        
        shaderc::Compiler compiler;
        shaderc::CompileOptions opts;
        
        opts.SetOptimizationLevel(shaderc_optimization_level::shaderc_optimization_level_performance);
        shaderc::SpvCompilationResult result = compiler.CompileGlslToSpv(code, size, kind, "temp", opts);
        if(result.GetCompilationStatus() != shaderc_compilation_status::shaderc_compilation_status_success){
            LOGWITH(result.GetErrorMessage());
            return;
        }
        spvCode->clear();
        spvCode->reserve(result.cend() - result.cbegin());
        for(uint32_t c: result){ spvCode->push_back(c); }
    }
#endif
}

#endif