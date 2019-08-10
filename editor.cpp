
#include <string>
#include <cstdlib>
using namespace std;

#include <mat4x4.hpp>
#include <gtc/type_ptr.hpp>
#include <gtc/matrix_transform.hpp>

#include "log.h"
#include "graphics/platform_gl.h"
#include "graphics/gl_utils.h"
#include "graphics/glsl_prog.h"
#include "graphics/texture.h"
#include "graphics/gl_ext.h"
#include "renderer/font.h"
#include "renderer/Model.h"
#include "button.h"
#include "editor.h"
#include "engine.h"
#include "renderer/camera.h"
#include "game/IGame.h"
#include "resource/ResourceManager.h"

class EditorGame : public IGame
{
public:
	void Created();
	void Changed(int w, int h);
	void Draw();
	const char *GetGamedir(){
		return "editor";
	}
	void OnKey(int key, int scancode, int action, int mods);
	void OnTouch(float tx, float ty, int ta, int tf);
	void OnMouseMove(float x, float y);
	void OnScroll(float sx, float sy);
	
	void Update();
};

IGame *CreateGame(){
	return new EditorGame();
}

int scrWidth = 0;
int scrHeight = 0;
float aspect = 1;

ResourceManager resMan;

glslProg texProg;
glslProg lightingProg;
glslProg *curProg=0;
Texture testTex;
Texture whiteTex;
Texture normTex;
Font testFont;
Camera *curCamera=0;
glm::mat4 vpMtx(1);
glm::mat4 mdlMtx(1);
glm::mat4 mvpMtx(1);

//menu buttons
struct menuItem_t{
	const char *name;
	Editor*(*func)();
	Button b;
} menuList[]={
	{"Model viewer",&GetModelViewer},
	{"PCC viewer",&GetPCCViewer},
	{"Bink player",&GetBinkPlayer},
	{"Sound editor",&GetSoundEditor},
	{"Model editor",&GetModelEditor},
	{"Graph util",&GetGraphUtil}
};
int menuListNum = 6;
Button bQuit;

//string sLog;
Editor *editor=0;

double deltaTime=0;
double oldTime=0;

void InitButtons()
{
	float cy=0.2;
	for(int i=0;i<menuListNum;i++){
		menuList[i].b = Button(0.1,cy,0.5,0.08,menuList[i].name,true);
		cy += 0.1;
	}
	bQuit = Button(0.1,cy,0.5,0.08, "Quit",true);
}

void EditorGame::Created()
{
	Log("Init nenuzhno editor\n");

	GLExtensions::Init();

	texProg.CreateFromFile("generic", "col_tex");
	texProg.u_mvpMtx = texProg.GetUniformLoc("u_mvpMtx");
	texProg.u_color = texProg.GetUniformLoc("u_color");
	texProg.Use();
	glUniform4f(texProg.u_color,1,1,1,1);
	lightingProg.CreateFromFile("generic", "lighting");
	lightingProg.u_mvpMtx = lightingProg.GetUniformLoc("u_mvpMtx");
	lightingProg.u_modelMtx = lightingProg.GetUniformLoc("u_modelMtx");
	CheckGLError("Created shaders", __FILE__, __LINE__);
	
	GLubyte whiteData[] = {255};
	whiteTex.Create(1,1);
	whiteTex.Upload(0, GL_LUMINANCE, whiteData);
	CheckGLError("whiteTex.Upload", __FILE__, __LINE__);
	
	GLubyte normData[] = {127,127,255};
	normTex.Create(1,1);
	normTex.Upload(0, GL_RGB, normData);
	
	GLubyte testTexData[] =
	{
		31,31,31, 255,255,255, 31,31,31, 255,255,255,
		255,255,255, 31,31,31, 255,255,255, 31,31,31,
		31,31,31, 255,255,255, 31,31,31, 255,255,255,
		255,255,255, 31,31,31, 255,255,255, 31,31,31
	};
	testTex.Create(4,4);
	testTex.Upload(0, GL_RGB, testTexData);
	CheckGLError("testTex.Upload", __FILE__, __LINE__);

	resMan.Init();

	testFont = Font();
	//testFont.LoadBMFont("arial");
	//testFont.LoadBMFont("verdana");
	testFont.LoadBMFont("sansation", &resMan);

	CheckGLError("Created texture", __FILE__, __LINE__);
	
	glUseProgram(0);
	glBindTexture(GL_TEXTURE_2D, 0);
	glClearColor(0, 0, 0, 1.0f);
	//glClearColor(0.4f, 0.4f, 0.4f, 1.0f);
	CheckGLError("Created", __FILE__, __LINE__);
	
	InitButtons();
	
	oldTime = GetTime();
}

void EditorGame::Changed(int w, int h)
{
	if(!w||!h)
		return;
	//LOG("Resize %dx%d\n",w,h);
	scrWidth = w;
	scrHeight = h;
	glViewport(0, 0, w, h);
	aspect = w/(float)h;
	InitButtons();
	if(editor){
		editor->Resize(w,h);
	}
}

void CloseEditor()
{
	if(editor){
		delete editor;
		editor = 0;
	}
}

void EditorGame::Update()
{
	for(int i=0;i<menuListNum;i++){
		if(menuList[i].b.pressed){
			menuList[i].b.pressed=0;
			editor = menuList[i].func();
			if(editor)
				editor->Resize(scrWidth,scrHeight);
		}
	}
	if(bQuit.pressed){
		bQuit.pressed=0;
		CloseEditor();
		exit(0);
	}
}

void SetColor(float r,float g,float b,float a)
{
	glUniform4f(curProg->u_color,r,g,b,a);
}

void DrawText(const char *t,float x,float y,float s)
{
	//TODO: optimize
	glEnable(GL_BLEND);
	glBlendFunc(1,1);
	testFont.Print(t,x*aspect,1-y,s);
	glDisable(GL_BLEND);
}

void DrawRect(float x, float y, float w, float h, bool transp, Texture* tex)
{
	if(transp){
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
		if(!tex)
			SetColor(0.4,0.4,0.4,0.5);
	}
	if(tex)
		tex->Bind();
	else
		whiteTex.Bind();
	float lx=x*aspect;
	float rx=(x+w)*aspect;
	float dy=1-(y+h);
	float verts[] = {
		lx,	1-y, 0,0,
		lx,	dy,	 0,1,
		rx,	dy,	 1,1,
		rx,	1-y, 1,0
	};
	
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 16, verts);
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 16, verts+2);
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
	if(transp){
		glDisable(GL_BLEND);
		SetColor(1,1,1,1);
	}
}

void DrawButton(Button *b)
{
	if(b->text){
		DrawRect(b->x,b->y,b->w,b->h);
		//testFont.Print(b->text,b->x*aspect+0.01,1-b->y-0.05,0.5);
		DrawText(b->text,b->x+0.01/aspect,b->y+0.05,0.5);
	}else
	{
		DrawRect(b->x,b->y,b->w,b->h);
	}
}

void DrawModel(Model *mdl)
{
	mdl->vbo.Bind();
/*	glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,mdl->vertexStride,0);
	glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,mdl->vertexStride,(void*)12);
	glVertexAttribPointer(2,2,GL_FLOAT,GL_FALSE,mdl->vertexStride,(void*)24);*/
	for(int i=0; i<mdl->vertexFormat.size; i++)
	{
		vertAttrib_t &va = mdl->vertexFormat[i];
		glVertexAttribPointer(va.id,va.size,va.type,va.norm,va.stride,(void*)va.offset);
	}
	mdl->vbo.Unbind();
	
	for(int i=0;i<mdl->submeshes.size;i++){
		if(mdl->indexCount){
			mdl->ibo.Bind();
			glDrawElements(GL_TRIANGLES,mdl->submeshes[i].count,mdl->indexType,(void *)(mdl->submeshes[i].offs*mdl->indexSize));
			mdl->ibo.Unbind();
		}else{
			glDrawArrays(GL_TRIANGLES, mdl->submeshes[i].offs, mdl->submeshes[i].count);
		}
	}
}

void UploadMtx(){
	glUniformMatrix4fv(curProg->u_mvpMtx,1,GL_FALSE,glm::value_ptr(mvpMtx));
	glUniformMatrix4fv(curProg->u_modelMtx,1,GL_FALSE,glm::value_ptr(mdlMtx));
}

void SetModelMtx(glm::mat4 m)
{
	mdlMtx = m;
	mvpMtx = vpMtx*mdlMtx;
	UploadMtx();
}

/*void SetPerspectiveMode(bool e)
{
	if(e){
		mvpMtx = glm::perspective(glm::radians(90.0f), aspect, 0.01f, 50.0f);
		glUniformMatrix4fv(curProg->u_mvpMtx,1,GL_FALSE,glm::value_ptr(mvpMtx));
		glEnable(GL_DEPTH_TEST);
		glEnable(GL_CULL_FACE);
	}else{
		mvpMtx = glm::translate(glm::mat4(1.0),glm::vec3(-1,-1,0));
		mvpMtx = glm::scale(mvpMtx,glm::vec3(2.0f/aspect,2.0f,2.0f));// *aspect
		glUniformMatrix4fv(curProg->u_mvpMtx,1,GL_FALSE,glm::value_ptr(mvpMtx));
		glDisable(GL_DEPTH_TEST);
		glDisable(GL_CULL_FACE);
	}
}*/
void SetCamera(Camera *cam)
{
	curCamera = cam;
	if(cam){
		vpMtx = cam->projMtx*cam->viewMtx;
		glEnable(GL_DEPTH_TEST);
		glEnable(GL_CULL_FACE);
	}else{
		vpMtx = glm::translate(glm::mat4(1.0),glm::vec3(-1,-1,0));
		vpMtx = glm::scale(vpMtx,glm::vec3(2.0f/aspect,2.0f,2.0f));//*aspect
		glDisable(GL_DEPTH_TEST);
		glDisable(GL_CULL_FACE);
	}
	mdlMtx = glm::mat4(1);//?
	mvpMtx = vpMtx;
	UploadMtx();
}

void SetLightingEnabled(bool val)
{
	if(val&&curProg!=&lightingProg){
		curProg=&lightingProg;
	}else if(!val&&curProg==&lightingProg){
		curProg=&texProg;
	}else{
		return;
	}
	curProg->Use();
	UploadMtx();
}

void SetProg(glslProg *p)
{
	if(p && curProg!=p){
		curProg = p;
	}else if(!p && curProg!=&texProg){
		curProg = &texProg;
	}else{
		return;
	}
	curProg->Use();
	UploadMtx();
}

void EditorGame::Draw()
{
	double startTime = GetTime();
	deltaTime = (startTime-oldTime);
	oldTime = startTime;
	
	if(!editor){
		Update();
	}

	glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
	CheckGLError("Clear", __FILE__, __LINE__);
	
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	curProg = &texProg;
	
	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(2);
	curProg->Use();
	SetColor(1,1,1,1);
	testTex.Bind();

	curCamera=0;
	vpMtx = glm::translate(glm::mat4(1.0),glm::vec3(-1,-1,0));
	vpMtx = glm::scale(vpMtx,glm::vec3(2.0f/aspect,2.0f,2.0f));//*aspect
	mdlMtx = glm::mat4(1);
	mvpMtx = vpMtx;
	UploadMtx();
	if(!editor){
		for(int i=0;i<menuListNum;i++){
			DrawButton(&menuList[i].b);
		}
		DrawButton(&bQuit);
	}else{
		editor->Draw();
	}
	//testFont.Print(sLog.c_str(),0,0.9,0.2);
	/*float verts[] = {
		-0.5,-0.5,0,1,
		-0.5,0,0,0,
		0,0,1,0,
		0,-0.5,1,1
	};
	
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 16, verts);
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 16, verts+2);
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
	*/
	glDisableVertexAttribArray(0);
	glDisableVertexAttribArray(2);
	
	CheckGLError("Draw", __FILE__, __LINE__);
}

void EditorGame::OnKey(int key, int scancode, int action, int mods)
{
	if(editor)
		editor->OnKey(key,action);
}

void EditorGame::OnTouch(float x, float y, int a, int tf)
{
#ifdef ANDROID
	y-=50;
#endif
	if(!editor){
		float tx = x/scrWidth;
		float ty = y/scrHeight;
		if(a==0){
			for(int i=0;i<menuListNum;i++){
				menuList[i].b.Hit(tx,ty);
			}
			bQuit.Hit(tx,ty);
		}
	}else{
		editor->OnTouch(x,y,a);
		editor->OnTouch(x,y,a,tf);
	}
}

void EditorGame::OnMouseMove(float x, float y)
{
	if(editor)
		editor->OnMouse(x,y);
}

void EditorGame::OnScroll(float sx, float sy)
{
	if(editor)
		editor->OnScroll(sx,sy);
}

Button::Button(float nx, float ny, float nw, float nh, bool adjust):
			x(nx),y(ny),w(nw),h(nh),text(0),active(true),pressed(false)
{
	if(adjust)
	{
		if(aspect>1)
			h*=aspect;
		else
			w/=aspect;
	}
	//LOG("Created button %f %f %f %f\n",x,y,w,h);
}

Button::Button(float nx, float ny, float nw, float nh, const char *t, bool adjust):
			x(nx),y(ny),w(nw),h(nh),text(t),active(true),pressed(false)
{
	if(adjust)
		w/=aspect;

	//LOG("Created text button %f %f %f %f %s\n",x,y,w,h,t);
}

void Button::Update(){}

