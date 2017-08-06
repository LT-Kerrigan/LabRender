//
//  Mesh.cpp
//  LabApp
//
//  Created by Nick Porcino on 2013 12/29.
//  Copyright (c) 2013 Nick Porcino. All rights reserved.
//

#if defined(_WINDOWS) && !defined(_CRT_SECURE_NO_WARNINGS)
# define _CRT_SECURE_NO_WARNINGS
#endif

#include "LabRender/Model.h"

#include "LabRender/gl4.h"
#include "LabRender/FrameBuffer.h"
#include "LabRender/Material.h"
#include "LabRender/MathTypes.h"
#include "LabRender/ShaderBuilder.h"
#include "LabRender/Utils.h"
#include "LabRender/Vertex.h"

#include <iostream>
#include <sstream>
#include <map>
#include <vector>

#include <LabText/TextScanner.hpp>

using std::shared_ptr;
using std::unique_ptr;
using std::vector;
using std::string;

namespace lab {

    std::shared_ptr<Shader> ModelPart::makeShader(FrameBuffer& fbo, ModelPart& mesh,
                                                  ModelPart::ShaderType shaderType,
                                                  char const*const vshSrc, char const*const fshSrc) 
	{

        checkError(ErrorPolicy::onErrorThrow, TestConditions::exhaustive, "ModelPart::makeShader begin");
        VAO* const vao = mesh.verts();
        vao->uploadVerts();
        vao->bindVAO();

        // attributes

        bool hasPositionsAttr = vao->hasAttribute("a_position");

        if (!hasPositionsAttr)
            return std::shared_ptr<Shader>();

        bool hasNormalsAttr =       vao->hasAttribute("a_normal");
        bool hasTextureCoordsAttr = vao->hasAttribute("a_uv");
        bool hasVertexColorAttr =   vao->hasAttribute("a_color");
        bool hasTextureCubeAttr =   vao->hasAttribute("a_uvw");
        // todo - tangent basis for normal mapping

        shared_ptr<Material> material = mesh.material;

        bool hasTexture;
        if (!!material)
            hasTexture = !!material->propertyInlet(ShaderMaterial::baseColorName());
        else
            hasTexture = false;

        bool deferred = fbo.drawBuffers.size() > 0;

        // variant known, create variant identifier

        string variantName;
        if (deferred)                            variantName += "D";
        if (hasTexture)                          variantName += "t";
        if (shaderType == ShaderType::skyShader) variantName += "S";

        variantName += "/";
        if (hasPositionsAttr)     variantName += "P";
        if (hasNormalsAttr)       variantName += "N";
        if (hasTextureCoordsAttr) variantName += "T";
        if (hasVertexColorAttr)   variantName += "C";
        if (hasTextureCubeAttr)   variantName += "3";

        string shaderName;

        switch (shaderType) 
		{
            case ModelPart::ShaderType::skyShader:    shaderName = "sky/"; break;
            case ModelPart::ShaderType::customShader: shaderName = "custom/"; break;
            default:
            case ModelPart::ShaderType::meshShader:   shaderName = "mesh/"; break;
        }
        shaderName += variantName;

        // hash in unique source to shader name if source was provided

        if (vshSrc) {
            uint64_t hash = TextScanner::Hash(vshSrc, strlen(vshSrc));
            std::stringstream ss;
            ss << "/v" << hash;
            shaderName += ss.str();
        }
        if (fshSrc) {
            uint64_t hash = TextScanner::Hash(fshSrc, strlen(fshSrc));
            std::stringstream ss;
            ss << "/f" << hash;
            shaderName += ss.str();
        }

        // create the shader if it doesn't already exist

        ShaderBuilder sb;
        if (sb.cache()->hasShader(shaderName))
            return sb.cache()->shader(shaderName);

        Semantic varyings[] {
            { SemanticType::vec4_st, "v_pos", AutomaticUniform::none, 0 },
            { SemanticType::vec3_st, "v_normal", AutomaticUniform::none, 1 },
            { SemanticType::vec2_st, "v_uv", AutomaticUniform::none, 2 },
            { SemanticType::vec4_st, "v_color", AutomaticUniform::none, 3 }
        };

        if (hasTextureCubeAttr) {
            varyings[2].type = SemanticType::vec3_st;
            varyings[2].name = "v_uvw";
        }

        Semantic uniforms[] {
            { SemanticType::mat4_st,      "u_model", AutomaticUniform::none, 0 },
            { SemanticType::mat4_st,      "u_view", AutomaticUniform::none, 1 },
            { SemanticType::mat4_st,      "u_modelView", AutomaticUniform::none, 2 },
            { SemanticType::mat4_st,      "u_modelViewProj", AutomaticUniform::none, 3 },
            { SemanticType::vec4_st,      "u_viewRect", AutomaticUniform::none, 4 },
            { SemanticType::mat4_st,      "u_viewProj", AutomaticUniform::none, 5 },
            { SemanticType::sampler2D_st, "u_texture", AutomaticUniform::none, 6 },
            { SemanticType::float_st,     "u_offset", AutomaticUniform::none, 7 },
            { SemanticType::mat4_st,      "u_jacobian", AutomaticUniform::none, 8 }
        };

        // substitute the cube sampler if necessary. It's okay for the sky shader
        // but pretty sketchy otherwise.
        if (hasTextureCubeAttr || shaderType == ShaderType::skyShader) 
            uniforms[6].type = SemanticType::samplerCube_st;

        sb.setGbuffer(fbo);
        sb.setAttributes(mesh);
        sb.setVaryings(varyings, hasVertexColorAttr? 4 : 3);
        sb.setUniforms(uniforms, sizeof(uniforms)/sizeof(Semantic));
        std::shared_ptr<Shader> shader = std::make_shared<Shader>();

        // vertex shader
        string vsh;
        if (vshSrc)
            vsh.assign(vshSrc);
        else {
            vsh = "void main() {\n" glsl(
                                         vec4 pos = vec4(a_position, 1.0);
                                         vec4 n = u_jacobian * vec4(a_normal, 1.0);
                                         vec4 newPos = u_modelViewProj * pos;
                                         gl_Position = newPos;
                                         vert.v_pos = newPos;
                                         vert.v_normal = n.xyz;
                                         );

            if (shaderType == ShaderType::skyShader) vsh += "\n vert.v_uvw = normalize(a_position.xyz); \n";
            else if (hasTextureCubeAttr)             vsh += "\n vert.v_uvw = a_uvw; \n";
            else if (hasTextureCoordsAttr)           vsh += "\n vert.v_uv = a_uv; \n";

            if (hasVertexColorAttr)                  vsh += "\n vert.v_color = a_color; \n";
            vsh += "}\n";
        }

        // fragment shader
        string fsh;
        if (fshSrc)
            fsh.assign(fshSrc);
        else {
            fsh = "void main() { \n";
            if (deferred) {
                fsh += glsl( o_normalTexture = vec4(vert.v_normal, 1.0);
                             o_positionTexture = vert.v_pos; );
                if (hasTexture && hasVertexColorAttr)
                    fsh += glsl( o_diffuseTexture = texture(u_texture, vert.v_uv) * vert.v_color; );
                else if (hasTextureCubeAttr)
                    fsh += glsl( o_diffuseTexture = texture(u_texture, vert.v_uvw).bgra; );
                else if (hasTexture)
                    fsh += glsl( o_diffuseTexture = texture(u_texture, vert.v_uv).bgra; );
                else if (hasVertexColorAttr)
                    fsh += glsl( o_diffuseTexture = vert.v_color; );
                else
                    fsh += glsl( o_diffuseTexture = vec4(1.0,1.0,1.0,1.0); );
            }
            else {
                // forward shaded
                if (shaderType == ShaderType::skyShader)
                    fsh += glsl(float ndotl = 1.0;);        // emissive
                else
                    fsh += glsl( float ndotl = clamp(dot(normalize(vec3(0.0,-1.0,1.0)), vert.v_normal), 0.0, 1.0); );
                if (hasTexture && hasVertexColorAttr)
                    fsh += glsl( o_diffuseTexture = texture(u_texture, vert.v_uv) * vert.v_color * ndotl; );
                else if (hasTextureCubeAttr)
                    fsh += glsl( o_diffuseTexture = texture(u_texture, vert.v_uvw).bgra * ndotl; );
                else if (hasTexture)
                    fsh += glsl( o_diffuseTexture = texture(u_texture, vert.v_uv) * ndotl; );
                else if (hasVertexColorAttr)
                    fsh += glsl( o_diffuseTexture = vert.v_color * ndotl; );
                else
                    fsh += glsl( o_diffuseTexture = vec4(1.0,1.0,1.0,1.0) * ndotl; );
            }
            fsh += "}\n";
        }

        shader = sb.makeShader(shaderName, vsh.c_str(), fsh.c_str(), * mesh.verts());
        sb.cache()->add(shaderName, shader);

        vao->unbindVAO();

        return shader;
    }



    void ModelPart::draw(FrameBuffer& fbo, Renderer::RenderLock& rl) {
        if (_verts && !_shader) {
            string vsh;
            string fsh;

            if (!!material) {
                shared_ptr<InOut> vsIO = material->propertyInlet(ShaderMaterial::vertexShaderFileName());
                shared_ptr<InOut> fsIO = material->propertyInlet(ShaderMaterial::fragmentShaderFileName());

                if (!!vsIO && !!fsIO) {
                    string vs = vsIO->value<string>();
                    string fs = fsIO->value<string>();
                    //const OutletData<string>* vs = vsIO->out<string>();
                    //const OutletData<string>* fs = fsIO->out<string>();
                    //std::string foo = vs->value();
                    //const char* n = vs->value().c_str();
                    //FILE* f = fopen(n, "rb");
                    FILE* f = fopen(vs.c_str(), "rb");
                    if (f) {
                        fseek(f, 0, SEEK_END);
                        size_t l = ftell(f);
                        fseek(f, 0, SEEK_SET);
                        uint8_t* data = new uint8_t[l];
                        fread(data, 1, l, f);
                        fclose(f);
                        vsh.assign((char*)data);
                        delete[] data;
                    }
                    //n = fs->value().c_str();
                    //f = fopen(n, "rb");
                    f = fopen(fs.c_str(), "rb");
                    if (f) {
                        fseek(f, 0, SEEK_END);
                        size_t l = ftell(f);
                        fseek(f, 0, SEEK_SET);
                        uint8_t* data = new uint8_t[l];
                        fread(data, 1, l, f);
                        fclose(f);
                        fsh.assign((char*)data);
                        delete[] data;
                    }
                }
            }

            if (!vsh.length() || !fsh.length()) {
                // if a shader has not been externally supplied, assume a default mesh shader
                _shader = makeShader(fbo, *this, _shaderType, 0, 0);
            }
            else {
                _shader = makeShader(fbo, *this, _shaderType, vsh.c_str(), fsh.c_str());
            }
        }
        if (_verts && _shader) {
            _shader->bind(rl);
            if (_shaderType == ShaderType::skyShader) {
                lab::m44f invMv = rl.context.viewMatrices.mv;
                // remove translation
                invMv.columns[3].x = 0;
                invMv.columns[3].y = 0;
                invMv.columns[3].z = 0;
                _shader->uniform("u_modelView", invMv);
                lab::m44f mvproj = matrix_multiply(rl.context.viewMatrices.projection, invMv);
                _shader->uniform("u_modelViewProj", mvproj);
            }
            else {
                _shader->uniform("u_view", rl.context.viewMatrices.view);
                _shader->uniform("u_modelView", rl.context.viewMatrices.mv);
                _shader->uniform("u_modelViewProj", rl.context.viewMatrices.mvp);
            }

            lab::m44f jacobian = rl.context.viewMatrices.model;
            jacobian.columns[3].x = 0;
            jacobian.columns[3].y = 0;
            jacobian.columns[3].z = 0;
            jacobian = matrix_transpose(matrix_invert(jacobian));
            _shader->uniform("u_jacobian", jacobian);

            bool depthWriteSet = true;
            bool depthRangeSet = false;
            bool depthFuncSet = false;
            if (!!material) {
                shared_ptr<InOut> baseColorInOut = material->propertyInlet(ShaderMaterial::baseColorName());
                if (!!baseColorInOut) {
                    shared_ptr<Texture> texture = baseColorInOut->value<shared_ptr<Texture>>();
                    int unit = rl.context.activeTextureUnit;
                    texture->bind(unit);
                    _shader->uniformInt("u_texture", unit);
                    rl.context.activeTextureUnit++;
                }
                shared_ptr<InOut> dwInOut = material->propertyInlet(ShaderMaterial::depthWriteName());
                if (!!dwInOut) {
                    depthWriteSet = dwInOut->value<float>() > 0;
                    glDepthMask(depthWriteSet? GL_TRUE : GL_FALSE);
                }
                shared_ptr<InOut> drIO = material->propertyInlet(ShaderMaterial::depthRangeName());
                if (!!drIO) {
                    glm::vec2 drange = drIO->value<glm::vec2>();
                    glDepthRange(drange.x, drange.y);
                    depthRangeSet = true;
                }
                shared_ptr<InOut> dfIO = material->propertyInlet(ShaderMaterial::depthFuncName());
                if (!!dfIO) {
                    depthFuncSet = true;
                    int dfunc = GL_LESS;
                    string df = dfIO->value<string>();
                    if      (df == "less")     dfunc = GL_LESS;
                    else if (df == "lequal")   dfunc = GL_LEQUAL;
                    else if (df == "never")    dfunc = GL_NEVER;
                    else if (df == "equal")    dfunc = GL_EQUAL;
                    else if (df == "greater")  dfunc = GL_GREATER;
                    else if (df == "notequal") dfunc = GL_NOTEQUAL;
                    else if (df == "gequal")   dfunc = GL_GEQUAL;
                    else if (df == "always")   dfunc = GL_ALWAYS;
                    glDepthFunc(dfunc);
                }
            }
            glDisable(GL_CULL_FACE);
            
            // Draw the model
            //
            _verts->draw();
            
            if (!depthWriteSet) {
                glDepthMask(GL_TRUE);
            }
            if (!depthRangeSet) {
                glDepthRange(0, 1);
            }
            if (!depthFuncSet) {
                glDepthFunc(GL_LESS);
            }
            _shader->unbind();
        }
    }

    void ModelPart::setVAO(std::unique_ptr<VAO> vao, Bounds localBounds) {
        _verts = std::move(vao);
        _localBounds = localBounds;
    }


    void Model::update(double time) {
        for (auto p : _parts)
            p->update(time);
    }

    void Model::draw() {
        for (auto p : _parts)
            p->draw();
    }

    void Model::draw(FrameBuffer& fbo, Renderer::RenderLock & rl) {
        for (auto p : _parts)
            p->draw(fbo, rl);
    }

    Bounds Model::localBounds() const {
        Bounds bounds;
        bounds.first = {FLT_MAX, FLT_MAX, FLT_MAX};
        bounds.second = {-FLT_MAX, -FLT_MAX, -FLT_MAX};
        for (auto p : _parts)
            bounds = extendBounds(bounds, p->localBounds());
        return bounds;
    }

}
