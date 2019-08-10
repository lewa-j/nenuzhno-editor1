
#include <stdlib.h>
#include <fstream>
#include <string>
using namespace std;
#include <gtc/matrix_transform.hpp>

#include "log.h"
#include "system/FileSystem.h"
#include "system/Config.h"
#include "ModelViewer.h"
#include "resource/nmf.h"

Editor *GetModelViewer(){return new ModelViewer();}

void ModelViewer::ReadConfig()
{
	ConfigFile config;
	config.Load("model_editor.txt");
	inName = config.values["in"];
	outName = config.values["out"];
	size = atof(config.values["size"].c_str());
	meshMat = config.values["meshMat"];

	if(size==0)
		size=1;
}

ModelViewer::ModelViewer()
{
	mesh = 0;
	mdl = 0;
	bClose = Button(0.02,0.02,0.5,0.07, "Close");
	bImportObj = Button(0.02,0.1,0.5,0.07, "Import obj");
	bImportMsh = Button(0.02,0.2,0.5,0.07, "Import msh");
	bExportMsh = Button(0.02,0.3,0.5,0.07, "Export msh");
	bExportNMF = Button(0.02,0.4,0.5,0.07, "Export NMF");
	bExportSMD = Button(0.02,0.5,0.5,0.07, "Export SMD");
	size = 1;
	ReadConfig();
	mtx = glm::mat4(1);
	//mtx = glm::translate(mtx,glm::vec3(0,0,-5));
	mtx = glm::scale(mtx,glm::vec3(size));
	bMove = Button(0.0,0.5,0.5,0.5);
	bScroll = Scroll(0.8,0.1,0.2,0.9);
	velocity = glm::vec3(0.0f);
	meshCamera.pos = glm::vec3(0,0,5);
	meshRot = glm::vec2(0);

	bScroll.pos=size;
}

ModelViewer::~ModelViewer()
{
	if(mesh){
		mesh->Free();
		delete mesh;
	}
	if(mdl){
		delete mdl;
	}
}

void ModelViewer::Resize(int w, int h)
{
	meshCamera.UpdateProj(90.0f, aspect, 0.01f, 50.0f);
}

void ModelViewer::Draw()
{
	if(bClose.pressed){
		bClose.pressed = 0;
		CloseEditor();
		return;
	}
	if(bImportObj.pressed){
		bImportObj.pressed = 0;
		if(mesh){
			mesh->Free();
			delete mesh;
		}
		mesh = LoadObjFile(inName.c_str(), false);
		velocity = glm::vec3(0.0f);
		meshCamera.pos=glm::vec3(0,0,5);
		meshRot=glm::vec2(0);
	}
	if(bImportMsh.pressed){
		bImportMsh.pressed = 0;
		if(mesh){
			mesh->Free();
			delete mesh;
		}
		mesh = LoadMeshFile(inName.c_str(), false);
		//Log("Loaded mesh %s: verts num %d, inds num %d\n",inName,mesh->numVerts,mesh->numInds);
		velocity = glm::vec3(0.0f);
		meshCamera.pos = glm::vec3(0,0,5);
		meshRot = glm::vec2(0);
	}
	if(bExportMsh.pressed){
		bExportMsh.pressed = 0;
		ExportMesh(outName.c_str(), mesh);
	}
	if(bExportNMF.pressed){
		bExportNMF.pressed = 0;
		mdl = MeshToModel(mesh, meshMat.c_str());
		ExportNMF(outName.c_str(), mdl);
	}
	if(bExportSMD.pressed){
		bExportSMD.pressed = 0;
		mdl = MeshToModel(mesh, meshMat.c_str());
		ExportSMD(outName.c_str(),(string("nenuzhno/")+outName).c_str(),1.0f/0.026f,mdl);
	}

	if(mesh)
	{
		meshCamera.UpdateView();
		SetCamera(&meshCamera);

		meshRot += glm::vec2(velocity.x,-velocity.z)*(float)deltaTime;
		size = bScroll.pos;
		
		mtx = glm::mat4(1.0);
		mtx = glm::scale(mtx,glm::vec3(size));
		mtx = glm::rotate(mtx,glm::radians(meshRot.y*90.0f),glm::vec3(1,0,0));
		mtx = glm::rotate(mtx,glm::radians(meshRot.x*90.0f),glm::vec3(0,1,0));
		//mtx = glm::scale(mtx,glm::vec3(glm::clamp(r+0.8f,0.1f,10.0f)));
		//mtx = glm::rotate(mtx,glm::radians(-90.0f),glm::vec3(1,0,0));
		//mtx = glm::translate(mtx,-pMesh->bounds.pos);
		
		SetModelMtx(mtx);
		SetLightingEnabled(true);
		whiteTex.Bind();
		glEnableVertexAttribArray(1);
		//glDisableVertexAttribArray(2);
		mesh->Bind();
		mesh->Draw();
		mesh->Unbind();
		glDisableVertexAttribArray(1);
		//glEnableVertexAttribArray(2);
		SetLightingEnabled(false);
		SetCamera(0);

		DrawText(inName.c_str(),0.02,0.95,0.3);
	}
	DrawButton(&bClose);
	DrawButton(&bImportObj);
	DrawButton(&bImportMsh);
	DrawButton(&bExportMsh);
	DrawButton(&bExportNMF);
	DrawButton(&bExportSMD);
}

static float starttx = 0;
static float startty = 0;
void ModelViewer::OnTouch(float x, float y, int a, int tf)
{
	float tx = x/scrWidth;
	float ty = y/scrHeight;
	if(a==0)
	{
		starttx = tx;
		startty = ty;
		
		bClose.Hit(tx,ty);
		bImportObj.Hit(tx,ty);
		bImportMsh.Hit(tx,ty);
		bExportMsh.Hit(tx,ty);
		bExportNMF.Hit(tx,ty);
		bExportSMD.Hit(tx,ty);
		bMove.Hit(tx,ty);
		bScroll.Hit(tx,ty,tf);
	}else if(a==1){
		velocity = glm::vec3(0);
		bMove.pressed = false;
		bScroll.Release(tf);
	}else if(a==2){
		if(bMove.pressed){
			velocity.x = -(starttx-tx);
			velocity.z = -(startty-ty);
		}
		bScroll.Move(tx,ty,tf);
	}
}

