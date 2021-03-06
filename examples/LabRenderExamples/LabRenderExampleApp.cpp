//
//  main.cpp
//  LabRenderExamples
//
//  Created by Nick Porcino on 5/10/15.
//  Copyright (c) 2015 Planet IX. All rights reserved.
//

#include "LabRenderDemoApp.h"
#include "extras/modelLoader.h"

#include <LabRender/Camera.h>
#include <LabRender/PassRenderer.h>
#include <LabRender/UtilityModel.h>
#include <LabRender/Utils.h>

#include <LabCmd/FFI.h>

using namespace std;
using lab::v2i;
using lab::v2f;
using lab::v3f;
using lab::v4f;

class PingCommand : public lab::Command
{
public:
    PingCommand()
    {
        run =
            [](const lab::Command & cmd, const string & path, const std::vector<Argument> &, lab::Ack & ack) {
                ack.payload.push_back(Argument("pong"));
                cout << "Sending pong" << endl;
        };
    }
    virtual ~PingCommand() {}

    virtual std::string name() const override { return "ping"; }
};

class LoadMeshCommand : public lab::Command
{
public:
    LoadMeshCommand(lab::Renderer * renderer, lab::DrawList * drawlist)
    : _renderer(renderer), _drawlist(drawlist)
    {
        run =
            [this](const lab::Command & cmd, const string & path, const std::vector<Argument> & args, lab::Ack & ack) {
                string filepath = args[0].stringArg;
                _renderer->enqueCommand([this, filepath]() {
                    shared_ptr<lab::ModelBase> model = lab::loadMesh(filepath);
                    if (model) {
                        _drawlist->deferredMeshes.clear();
                        _drawlist->deferredMeshes.emplace_back(model);
                    }
                });
            };

        parameterSpecification.push_back(make_pair<string, SemanticType>("path", SemanticType::string_st));
    }

    virtual ~LoadMeshCommand() {}
    virtual std::string name() const override { return "loadMesh"; }

    lab::Renderer * _renderer;
    lab::DrawList * _drawlist;
};




class LabRenderExampleApp : public lab::GLFWAppBase {
public:
    shared_ptr<lab::PassRenderer> dr;
    lab::DrawList drawList;
    lab::Camera camera;
	lab::CameraRig cameraRig;

    lab::OSCServer oscServer;
    lab::WebSocketsServer wsServer;

    v2f initialMousePosition;
    v2f previousMousePosition;

    LabRenderExampleApp()
    : oscServer("labrender")
    , wsServer("labrender")
    , previousMousePosition(V2F(0,0))
    {
		const char * env = getenv("ASSET_ROOT");
		if (env)
			lab::addPathVariable("$(ASSET_ROOT)", env);
		else
			lab::addPathVariable("$(ASSET_ROOT)", ASSET_ROOT);

		//std::string path = "$(ASSET_ROOT)/pipelines/deferred.json";
		//std::string path = "$(ASSET_ROOT)/pipelines/shadertoy.json";
		std::string path = "$(ASSET_ROOT)/pipelines/deferred_fxaa.json";
		std::cout << "Loading pipeline configuration " << path << std::endl;
        dr = make_shared<lab::PassRenderer>();
        dr->configure(path.c_str());
    }

    void createScene()
	{
        std::vector<std::shared_ptr<lab::ModelBase>>& meshes = drawList.deferredMeshes;
        //shared_ptr<lab::ModelBase> model = lab::Model::loadMesh("$(ASSET_ROOT)/models/starfire.25.obj");
        shared_ptr<lab::ModelBase> model = lab::loadMesh("$(ASSET_ROOT)/models/ShaderBall/shaderBallNoCrease/shaderBall.obj");
        meshes.push_back(model);

		shared_ptr<lab::UtilityModel> cube = make_shared<lab::UtilityModel>();
		cube->createCylinder(0.5f, 0.5f, 2.f, 16, 3, false);
		meshes.push_back(cube);

        static float foo = 0.f;
        camera.position = {foo, 0, -1000};
        lab::Bounds bounds = model->localBounds();
        bounds = model->transform.transformBounds(bounds);
        camera.frame(bounds);

        shared_ptr<lab::Command> command = make_shared<PingCommand>();
        oscServer.registerCommand(command);
        command = make_shared<LoadMeshCommand>(dr.get(), &drawList);
        oscServer.registerCommand(command);

        const int PORT_NUM = 9109;
        oscServer.start(PORT_NUM);

        wsServer.start(PORT_NUM + 1);
    }

    void render()
	{
        lab::checkError(lab::ErrorPolicy::onErrorThrow,
                              lab::TestConditions::exhaustive, "main loop start");

        v2i fbSize = frameBufferDimensions();

        drawList.jacobian = camera.mount.jacobian();
        drawList.view = camera.mount.viewTransform();
        drawList.proj = camera.optics.perspective(float(fbSize.x) / float(fbSize.y));

        lab::PassRenderer::RenderLock rl(dr, renderTime(), mousePosition());
		v2i fbOffset = V2I(0, 0);
        renderStart(rl, renderTime(), fbOffset, fbSize);

        dr->render(rl, fbSize, drawList);

        renderEnd(rl);

        lab::checkError(lab::ErrorPolicy::onErrorThrow,
                              lab::TestConditions::exhaustive, "main loop end");
    }

    virtual void keyPress(int key) override {
        switch (key) {
            case GLFW_KEY_C: cameraRig.set_mode(lab::CameraRig::Mode::Crane); break;
            case GLFW_KEY_D: cameraRig.set_mode(lab::CameraRig::Mode::Dolly); break;
            case GLFW_KEY_T:
            case GLFW_KEY_O: cameraRig.set_mode(lab::CameraRig::Mode::TurnTableOrbit); break;
        }
    }

    virtual void mouseDown(v2f windowSize, v2f pos) override {
        previousMousePosition = pos;
        initialMousePosition = pos;
    }

    virtual void mouseDrag(v2f windowSize, v2f pos) override {
        v2f delta = pos - previousMousePosition;
        previousMousePosition = pos;

        float distanceX = 20.f * delta.x / windowSize.x;
        float distanceY = -20.f * delta.y / windowSize.y;

		//printf("%f %f\n", pos.x, pos.y);

        cameraRig.interact(&camera, V2F(distanceX, distanceY));
    }

    virtual void mouseUp(v2f windowSize, v2f pos) override {
        if (vector_length(previousMousePosition - initialMousePosition) < 2) {
            // test for clicked object
            //
        }
    }
    virtual void mouseMove(v2f windowSize, v2f pos) override {
    }
};


int main(void)
{
    shared_ptr<LabRenderExampleApp> appPtr = make_shared<LabRenderExampleApp>();

	lab::checkError(lab::ErrorPolicy::onErrorThrow,
		lab::TestConditions::exhaustive, "main loop start");

	LabRenderExampleApp *app = appPtr.get();

    app->createScene();

    while (!app->isFinished())
    {
        app->render();
        app->frameEnd();
    }

    exit(EXIT_SUCCESS);
}
