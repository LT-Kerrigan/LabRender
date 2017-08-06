//
//  ShaderBuilder.cpp
//  LabApp
//
//  Created by Nick Porcino on 2014 01/28.
//  Copyright (c) 2014 Nick Porcino. All rights reserved.
//

#include "LabRender/ShaderBuilder.h"
#include "LabRender/Model.h"
#include "LabRender/FrameBuffer.h"

#include <set>
#include <map>
#include <string>
#include <sstream>
#include "LabRender/gl4.h"
#include "LabRender/Utils.h"

namespace lab {
    
    using namespace std;

    ShaderBuilder::Cache* _cache() 
	{
        static std::once_flag once;
        static ShaderBuilder::Cache* _shaderCache = 0;
        std::call_once(once, []() { _shaderCache = new ShaderBuilder::Cache(); });
        return _shaderCache;
    }
    
    class ShaderBuilder::Cache::Detail 
	{
    public:
        Detail() { }
        std::map<std::string, std::shared_ptr<Shader>> shaders;
    };
    
    ShaderBuilder::Cache::Cache() : _detail(new Detail()) { }
    ShaderBuilder::Cache::~Cache() { delete _detail; }
    
    bool ShaderBuilder::Cache::hasShader(const std::string& shader) const {
        Cache *cache = _cache();
        return cache->_detail->shaders.find(shader) != _detail->shaders.end();
    }
    
    void ShaderBuilder::Cache::add(const std::string& name, std::shared_ptr<Shader> shader) {
        Cache *cache = _cache();
        cache->_detail->shaders[name] = shader;
    }
    
    std::shared_ptr<Shader> ShaderBuilder::Cache::shader(const std::string& name) const {
        if (!hasShader(name))
            return std::shared_ptr<Shader>();
        
        Cache *cache = _cache();
        return cache->_detail->shaders.find(name)->second;
    }
    
    ShaderBuilder::Cache* ShaderBuilder::cache() {
        return _cache();
    }
    
    const char* preamble() {
        return "\
#version 410\n\
#extension GL_ARB_explicit_attrib_location: enable\n\
#extension GL_ARB_separate_shader_objects: enable\n\
#define texture2D texture\n";
    }
    
    
    std::string generateFragment() {
        std::stringstream s;
        s << preamble();
        return s.str();
    }
    
    
    ShaderBuilder::~ShaderBuilder() {
        clear();
    }
    
    void ShaderBuilder::clear() {
        for (auto i : uniforms) delete i;
        for (auto i : attributes) delete i;
        for (auto i : varyings) delete i;
        for (auto i : outputs) delete i;
        uniforms.clear();
        attributes.clear();
        varyings.clear();
        outputs.clear();
    }
    
    void ShaderBuilder::setGbuffer(const FrameBuffer& fbo) {
        // if no draw buffers specified on the fbo, then the fbo is representing the default gl draw buffer
        if (fbo.drawBuffers.size() == 0)
            outputs.insert(new Semantic(SemanticType::vec4_st, "o_color", 0));
        else
            for (int i = 0; i < fbo.drawBuffers.size(); ++i)
                outputs.insert(new Semantic(fbo.samplerType[i], fbo.drawBufferNames[i].c_str(), fbo.drawBuffers[i] - GL_COLOR_ATTACHMENT0));
    }
    
    void ShaderBuilder::setAttributes(const ModelPart& mesh) {
        VAO* vao = mesh.verts();
        vao->uploadVerts();
        for (int i = 0; i < vao->attributes.size(); ++i) {
            attributes.insert(new Semantic(vao->attributes[i]));
        }
    }
    
    void ShaderBuilder::setUniforms(const ShaderSpec& spec) {
        std::set<Semantic*> u = Semantic::makeSemantics(spec.uniforms);
        for (auto i : u) {
            uniforms.insert(i);
        }
    }
    
    void ShaderBuilder::setVaryings(const ShaderSpec& spec) {
        std::set<Semantic*> u = Semantic::makeSemantics(spec.varyings);
        for (auto i : u) {
            varyings.insert(i);
        }
    }
    
    void ShaderBuilder::setUniforms(Semantic const*const semantics, int count) {
        for (int i = 0; i < count; ++i)
            uniforms.insert(new Semantic(semantics[i]));
    }
    
    void ShaderBuilder::setVaryings(Semantic const*const semantics, int count) {
        for (int i = 0; i < count; ++i)
            varyings.insert(new Semantic(semantics[i]));
    }

    
    std::string ShaderBuilder::generateVertexShader(const char* body) {
        std::stringstream s;
        s << preamble();
        
        for (auto a : attributes) {
            s << a->attributeString() << std::endl;
        }
        for (auto u : uniforms) {
            s << u->uniformString() << std::endl;
        }
        if (varyings.size() > 0) {
            s << "out Vert {\n";
            for (auto v : varyings) {
                s << "   " << semanticTypeToString(v->type) << " " << v->name << ";" << std::endl;
            }
            s << "} vert;\n";
        }
        s << body << std::endl;
        
        return s.str();
    }
    std::string ShaderBuilder::generateFragmentShader(const char* body) {
        
        std::stringstream s;
        s << preamble();
        
        for (auto o : outputs) {
            s << o->outputString() << std::endl;
        }
        for (auto u : uniforms) {
            s << u->uniformString() << std::endl;
        }
        if (varyings.size() > 0) {
            s << std::endl << "in Vert {" << std::endl;
            for (auto v : varyings) {
                s << "   " << semanticTypeToString(v->type) << " " << v->name << ";" << std::endl;
            }
            s << "} vert;" << std::endl;
        }
        s << body << std::endl;
        
        return s.str();
    }
    
    std::shared_ptr<Shader> ShaderBuilder::makeShader(const ShaderSpec & spec, const VAO & vao, bool printShader) {
        string vrtx = loadFile(spec.vertexShaderPath.c_str());
        string fgmt = loadFile(spec.fragmentShaderPath.c_str());
        string fgmt_postAmble = loadFile(spec.fragmentShaderPostamblePath.c_str(), false);
        fgmt += fgmt_postAmble;
        
        auto shader = makeShader(spec.name, vrtx.c_str(), fgmt.c_str(), vao, printShader);

        for (auto u : spec.uniforms)
            if (u.automatic != AutomaticUniform::none)
                shader->automatics.push_back(u);

        return shader;
    }

    
    std::shared_ptr<Shader> ShaderBuilder::makeShader(const std::string & name,
                                                      const char* vtxCode, const char* fgmtCode,
                                                      const VAO & vao,
                                                      bool printShader) {
        std::string vtx = generateVertexShader(vtxCode);
        std::string fgm = generateFragmentShader(fgmtCode);

        if (printShader) {
            printf("Vertex Shader\n__________________________\n%s\n\n\n\n", vtx.c_str());
            printf("\nFragment Shader\n__________________________\n%s\n\n\n\n", fgm.c_str());
        }
        
        vao.bindVAO();
        std::shared_ptr<Shader>shader = std::make_shared<Shader>();
        shader->shader(name, Shader::ProgramType::Vertex, false, vtx.c_str()).
                shader(name, Shader::ProgramType::Fragment, false, fgm.c_str()).link();
        vao.unbindVAO();

        return shader;
    }
    
    
}
