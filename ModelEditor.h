
#pragma once

#include "editor.h"
#include "button.h"
#include "renderer/mesh.h"
#include "renderer/Model.h"

class DynamicModel;

class ModelEditor: public Editor
{
public:
	ModelEditor();
	~ModelEditor();
	void Resize(int w, int h);
	void Draw();
	void OnTouch(float x,float y,int a);
private:
	void ReadConfig();
	Button bClose;
	Button bNew;
	Button bExportNMF;
	std::string outName;
	float size;
	DynamicModel *mdl;
	std::string meshMat;
	glm::mat4 mtx;
	Camera meshCamera;
	Button bMove;
	glm::vec3 velocity;
	glm::vec2 meshRot;
};
