
#pragma once

#include <vector>
#include <string>

#include "editor.h"
#include "button.h"
#include "pcc.h"
#include "PCCSystem.h"
#include "renderer/camera.h"

enum ePccViewerState
{
	ePccFiles,
	ePccFuncs,
	ePccLoading,
	ePccOpen,
	ePccPreviewTex,
	ePccPreviewStMesh,
	ePccPreviewSkMesh,
	ePccPreviewLevel,
	ePccTest,
	ePccAnim,
	ePccClosing
};
enum ePccDisplayMode
{
	eNames,
	eImports,
	eExports,
	eTree
};
enum ePccTexMode
{
	eRGBA,
	eRGB
};
//mesh
enum ePccMeshMode
{
	eMeshTexture,
	eMeshNormal,
	eMeshLight,
	eMeshNormalmap
};
#define eMeshNorm 1
#define eMeshTan 2
#define eMeshUV2 4
#define eMeshSkin 8
#define eMeshVBO 16

enum ePccFilterMode
{
	eAll,
	eMeshes,
	eTextures
};

struct treeNode_t
{
	std::string name;
	std::vector<treeNode_t*> nodes;
	int id;
	bool tag;
	bool active;

	treeNode_t()
	{
		id = 0;
		tag = false;
		active = false;
	}
};

template<typename T>
class EditorButtonT: public Button
{
public:
	EditorButtonT():Button(){pClass=0; func=0;}
	EditorButtonT(float nx, float ny, float nw, float nh, const char *t,T* c,void (T::*f)());

	T* pClass;
	void (T::*func)();

	virtual void Update();
};

class PCCViewer;

class EditorButton: public EditorButtonT<PCCViewer>{
public:
	EditorButton():EditorButtonT(){}
	EditorButton(float nx, float ny, float nw, float nh, const char *t,PCCViewer* v,void (PCCViewer::*f)()):EditorButtonT(nx,ny,nw,nh,t,v,f){}
};
/*
class EditorButton: public Button
{
public:
	EditorButton():Button(){pV=0; func=0;}
	EditorButton(float nx, float ny, float nw, float nh, const char *t,PCCViewer* v,void (PCCViewer::*f)());

	PCCViewer* pV;
	void (PCCViewer::*func)();

	virtual void Update();
};*/

class PCCViewList{
public:
	PCCViewList();
	void Init(PCCViewer *viewer, std::vector<Button*> &buttons);
	void Activate();
	void Scroll(float y);
	void Draw(float listY);

	EditorButton bOpen;
	//void Open();
	EditorButton bFunctions;
	//void Functions();

	Button *bList;
	float yo;
};

class PCCViewOpen{
public:
	PCCViewOpen();
	void Init(PCCViewer *viewer, std::vector<Button*> &buttons);
	void Activate();
	void Scroll(float y);
	void Draw(float aListY);
	void DrawLists();
	void DrawTree(treeNode_t *n, int d);

	void BuildTree(PCCFile *pPcc);
	bool CanPreview(treeNode_t *node);

	EditorButtonT<PCCViewOpen> bMode;
	void SwitchMode();
	ePccDisplayMode displayMode;
	EditorButtonT<PCCViewOpen> bFilter;
	void Filter();
	ePccFilterMode filterMode;
	EditorButton bView;
	//void View();

	PCCFile *pcc;
	treeNode_t *nodesList;
	treeNode_t *rootNode;
	int numNodes;
	treeNode_t *selectedNode;

	Button *bList;
	float yo;
	float cy;
	bool hit;
	float listY;
};

class PCCViewTexture{
public:
	PCCViewTexture();
	void Init(PCCViewer *pViewer, std::vector<Button*> &buttons);
	void Activate();
	void Scroll(float y);
	void Draw();

	void View(pccExport_t *pExp);
	void PreviewSwitch(bool next);
	void BuildPreviewList(pccExport_t *exp);
	bool SetPreviewTex(pccExport_t *exp);

	EditorButtonT<PCCViewTexture> bBack;
	void Back();
	EditorButtonT<PCCViewTexture> bNext;
	void Next();
	EditorButtonT<PCCViewTexture> bPrev;
	void Prev();
	//EditorButton bRenderMode;
	void RenderMode();
	//EditorButton bExport;
	void Export(treeNode_t *selectedNode);
	ePccTexMode texMode;
	PCCTexture2D *previewTex;
	std::vector<treeNode_t*> previewList;
	int previewListPos;

	float yo;

	Button *bRenderMode;
	PCCViewer *viewer;
};

class PCCViewModel{
public:
	PCCViewModel();
	~PCCViewModel();
	void Init(PCCViewer *pViewer, std::vector<Button*> &buttons);
	void Activate();
	void Scroll(float y);
	void Draw();
	void DrawAnim();

	void PreviewStaticMesh(PCCStaticMesh *pMesh,float r,int flags);
	void PreviewSkeletalMesh(SkeletalMesh *pMesh,float r,int flags);
	void DrawSkeletalMesh(SkeletalMesh *pMesh,int flags, glm::mat4 *skel=0);
	void DrawStaticMesh(PCCStaticMesh *pMesh,PCCMaterial** pMats=0,int flags=0);

	void View(pccExport_t *pExp, bool skeletal);
	void ViewAnim();
	void ReloadShaders();

	EditorButtonT<PCCViewModel> bBack;
	void Back();
	//EditorButton bRenderMode;
	void RenderMode();
	EditorButtonT<PCCViewModel> bSkel;
	void ShowSkel();
	bool showSkel;
	//EditorButton bExport;
	void Export(treeNode_t *selectedNode);

	PCCObject *previewMesh;
	ePccMeshMode meshMode;

	glslProg alphaTestProg;
	glslProg normalProg;
	glslProg normalmapProg;
	int u_viewPosNm;
	Camera meshCamera;
	glm::vec2 meshRot;

	glm::mat4 *testSkel;
	float animPos;
	int animMode;
	int numAnimMeshes;
	SkeletalMesh **animMeshes;

	bool skeletal;
	float yo;

	Button *bRenderMode;
	PCCViewer *viewer;
};

class PCCViewLevel{
public:
	PCCViewLevel();
	~PCCViewLevel();

	void Init(PCCViewer *pViewer, std::vector<Button*> &buttons);
	void Activate();
	void Resize(float aspect);
	void Draw();
	void Rotate(float x, float y);

	void ReloadShaders();

	bool ViewLevel(int idx);
	void SetupCamera(PCCLevel *lvl);
	void LoadLevel();
	void LoadLevelAsync();

	void DrawStaticMeshComponent(StaticMeshComponent *pSMC);
	void DrawLevel(PCCLevel *pLvl);

	EditorButtonT<PCCViewLevel> bBack;
	void Back();
	EditorButtonT<PCCViewLevel> bExport;
	void Export();

	glslProg lightmapProg;
	int u_lmScaleBias;
	int u_lmColScale;
	int u_lmColBias;
	glslProg lightmapDirProg;
	int u_lmdScaleBias;
	//int u_viewPos;
	glslProg lightmapVertProg;
	int a_lmDir;

	PCCLevel *previewLevel;
	Camera levelCamera;
	glm::vec3 lastRot;

	glm::mat4 levelMtx;
	float zfar;

	PCCLevel **testLevels;
	int numTestLevels;
	int texLod;


	PCCViewer *viewer;
};

class PCCViewer : public Editor
{
public:
	PCCViewer();
	~PCCViewer();
	
	void Resize(int w, int h);
	void Draw();
	void OnTouch(float x,float y,int a);
	void OnMouse(float mx, float my);
	void OnScroll(float sx, float sy);
	void OnKey(int k, int a);
private:
	PCCSystem pccSystem;
	std::vector<Button*> buttons;
	ePccViewerState state;
	void ChangeState(ePccViewerState s);

	//main
	EditorButton bClose;
	void Close();
	Button bList;
	float listY;
	PCCViewList pccList;
	void Open();
	void Functions();

	float loadingRot;
	//Funcs
	EditorButton bViewPCC;
	void ViewPCC();
	EditorButton bViewLevels;
	void ViewLevelsLoad();
	EditorButtonT<PCCViewModel> bViewAnim;
	//void ViewAnim();//in PCCViewModel

	//open
	PCCViewOpen pccOpen;
	void View();
	PCCFile *pcc;

	//preview
	PCCViewTexture pccViewTexture;
	PCCViewModel pccViewModel;
	PCCViewLevel pccViewLevel;

	EditorButton bRenderMode;
	void RenderMode();
	EditorButton bExport;
	void Export();

	Button bMove;
	glm::vec3 velocity;

	Button bScroll;
	float sty;
	float ly;
	float px;
	float py;

	void ReloadShaders();

	friend class PCCViewList;
	friend class PCCViewOpen;
	friend class PCCViewTexture;
	friend class PCCViewModel;
	friend class PCCViewLevel;
	friend void *TestLoadThread(void *arg);
	friend void *LoadLevelThread(void *arg);
};

