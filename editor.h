
#pragma once

#include <mat4x4.hpp>

#include "graphics/platform_gl.h"
#include "graphics/texture.h"
#include "renderer/camera.h"
#include "graphics/glsl_prog.h"
#include "button.h"

class Model;

extern int scrWidth;
extern int scrHeight;
extern float aspect;
extern Texture whiteTex;
extern Texture normTex;
extern Texture testTex;
extern double deltaTime;

void CloseEditor();
void DrawRect(float x, float y, float w, float h, bool transp=true, Texture *tex=0);
void DrawButton(Button *b);
void DrawText(const char *t,float x,float y,float s);
void DrawModel(Model *mdl);
void SetModelMtx(glm::mat4 m);
//void SetPerspectiveMode(bool e=true);
void SetCamera(Camera *cam);
void SetProg(glslProg *p);
void SetLightingEnabled(bool val);
void SetColor(float r,float g,float b,float a);

class Editor
{
public:
	//Editor(){}
	virtual ~Editor(){}
	virtual void Resize(int w,int h)=0;
	virtual void Draw()=0;
	virtual void OnTouch(float x,float y,int a){}
	virtual void OnTouch(float x,float y,int a, int tf){}
	virtual void OnMouse(float ma, float my){}
	virtual void OnScroll(float sx,float sy){}
	virtual void OnKey(int k,int a){}
private:

};

Editor *GetSoundEditor();
Editor *GetModelViewer();
Editor *GetModelEditor();
Editor *GetPCCViewer();
Editor *GetBinkPlayer();
Editor *GetGraphUtil();

//ModelUtils.cpp
class Model;
class Mesh;
Model *MeshToModel(Mesh *msh, const char* mat);
void ExportMesh(const char *name, Mesh *mesh);
void ExportNMF(const char *name, Model *mdl);
bool ExportSMD(const char *name,const char *mdlPath,float scale,Model *model);

