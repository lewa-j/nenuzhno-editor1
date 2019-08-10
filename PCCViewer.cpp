
#include <cmath>
#include <fstream>
#include <pthread.h>
#include <dirent.h>
using namespace std;

#include <gtc/matrix_transform.hpp>
#include <gtc/quaternion.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <gtx/matrix_interpolation.hpp>

#include "engine.h"
#include "PCCViewer.h"
#include "system/FileSystem.h"
#include "log.h"
#include "graphics/ArrayBuffer.h"

bool IsTexture(pccExport_t *pExp,PCCFile *pcc);
void *TestLoadThread(void *arg);
void GenerateRandomFrame(glm::mat4 *skel,SkeletalMesh *pMesh);
void InterpolateSkel(glm::mat4 *skel,float pos,SkeletalMesh *pMesh);
bool ExportDDS(const char *name, neArray<char> &data, int width, int height, int format);

Editor *GetPCCViewer(){return new PCCViewer();}

template<typename T>
EditorButtonT<T>::EditorButtonT(float nx, float ny, float nw, float nh, const char *t,T *c, void (T::*f)()):
			Button(nx,ny,nw,nh,t)
{
	pClass = c;
	func = f;
	/*if(adjust)
	{
		w/=aspect;
	}*/
	//LOG("Created EditorButton %f %f %f %f %s\n",x,y,w,h,t);
}

template<typename T>
void EditorButtonT<T>::Update()
{
	if(!active || !pClass || !func)
		return;
	if(pressed){
		pressed = false;
		(pClass->*func)();
	}
}

std::vector<string> pccFiles;
int numPccFiles=0;
int selectedPccFile=0;

pthread_mutex_t mutex;
pthread_mutex_t loadingMutex;
pthread_t loadingThread;

void LoadPCCList()
{
#if 0
	char path[256];
	g_fs.GetFilePath("pcc_list.txt",path);
	ifstream listFile(path);
	if(!listFile){
		Log("pcc_list.txt not found!\n");
		return;
	}

	listFile >> numPccFiles;
	pccFiles = new string[numPccFiles];
	for(int i=0;i<numPccFiles;i++){
		listFile >> pccFiles[i];
	}
	listFile.close();
#endif
	char path[1024]={0};
	g_fs.GetFilePath("../ME3", path, true);

	DIR *dir;
	struct dirent *entry;
	
	if(!(dir = opendir(path)))
		return;

	while((entry = readdir(dir))){
		if(strstr(entry->d_name,".pcc"))
			pccFiles.push_back(entry->d_name);
	}
	closedir( dir );
	
	numPccFiles = pccFiles.size();
}

PCCViewer::PCCViewer()
{
	pthread_mutex_init(&mutex, NULL);
	pthread_mutex_init(&loadingMutex, NULL);

	//main
	bClose = EditorButton(0.02,0.02,0.24,0.07, "Close",this,&PCCViewer::Close);
	buttons.push_back(&bClose);
	bList = Button(0.0,0.15,0.9,0.8);
	listY = 0;
	pccList.Init(this,buttons);
	//buttons.push_back(&bList);

	//funcs
	bViewPCC = EditorButton(0.1,0.2,0.6,0.07,"View PCC",this,&PCCViewer::ViewPCC);
	bViewLevels = EditorButton(0.1,0.3,0.6,0.07, "View levels",this,&PCCViewer::ViewLevelsLoad);
	//bViewAnim = EditorButton(0.1,0.4,0.6,0.07,"View anim",this,&PCCViewer::ViewAnim);
	bViewAnim = EditorButtonT<PCCViewModel>(0.1,0.4,0.6,0.07,"View anim",&pccViewModel,&PCCViewModel::ViewAnim);

	buttons.push_back(&bViewPCC);
	buttons.push_back(&bViewLevels);
	buttons.push_back(&bViewAnim);

	//loading
	loadingRot = 0;

	//open
	pccOpen.Init(this,buttons);

	//preview
	pccViewTexture.Init(this,buttons);
	pccViewModel.Init(this,buttons);
	pccViewLevel.Init(this,buttons);

	bRenderMode = EditorButton(0.1,0.84,0.4,0.07,"RGBA",this,&PCCViewer::RenderMode);
	bExport = EditorButton(0.52,0.84,0.38,0.07,"Export",this,&PCCViewer::Export);
	buttons.push_back(&bRenderMode);
	buttons.push_back(&bExport);

	//texLod = 0;
	bMove = Button(0.0,0.5,0.5,0.5);

	pcc = 0;

	bScroll = Button(0.8,0.1,0.2,0.9);
	sty = 0;
	ly = 0;
	
	//called in Resize()
	//ReloadShaders();

	for(uint32_t i=0; i<buttons.size();i++){
		buttons[i]->active = false;
	}

	ChangeState(ePccFiles);
	//ChangeState(ePccFuncs);

	LoadPCCList();
}

PCCViewer::~PCCViewer()
{
	pthread_mutex_destroy(&mutex);
	pthread_mutex_destroy(&loadingMutex);

	if(state==ePccLoading){
		pthread_join(loadingThread, NULL);
	}

	//in PCCSystem
	/*if(pcc)
	{
		delete pcc;
		pcc=0;
		delete[] nodesList;
	}*/

	//delete[] pccFiles;
	pccFiles.clear();
}

void PCCViewer::ChangeState(ePccViewerState s)
{
	state = s;

	for(uint32_t i=0; i<buttons.size(); i++){
		buttons[i]->active = false;
	}
	bList.active = false;

	bClose.active = true;
	switch(state){
	case ePccFiles:
		pccList.Activate();
		bList.active = true;
		break;
	case ePccOpen://from ePccFiles
		pccOpen.Activate();
		bList.active = true;
		break;
	case ePccFuncs:
		bViewPCC.active = true;
		bViewLevels.active = true;
		bViewAnim.active = true;
		//bList.active = true;
		break;
	case ePccPreviewTex:
		pccViewTexture.Activate();
		bExport.active = true;
		bRenderMode.active = true;
		break;
	case ePccPreviewStMesh:
		bExport.active = true;
	case ePccPreviewSkMesh:
		pccViewModel.Activate();
		bRenderMode.active = true;
		if(state!=ePccPreviewStMesh)
			pccViewModel.bSkel.active = true;
		break;
	case ePccPreviewLevel:
		pccViewLevel.Activate();
		break;
	case ePccAnim:
		bRenderMode.active = true;
		pccViewModel.bSkel.active = true;
		break;
	case ePccTest:
	case ePccLoading:
	case ePccClosing:
		//?
		break;
	}
}

void PCCViewer::Close()
{
	state = ePccClosing;
}

void PCCViewer::Open()
{
	if(pcc)
		EngineError("File allready openned\n");

	pcc = pccSystem.GetPCC(pccFiles[selectedPccFile].c_str());

	if(pcc){
		pccOpen.BuildTree(pcc);
		ChangeState(ePccOpen);

		if(pccFiles[selectedPccFile].find("BioA_Nor_")!=string::npos){
			pccSystem.GetPCC("BioP_Nor.pcc");
			pccSystem.GetPCC("SFXGame.pcc");
		}

		if(pccFiles[selectedPccFile].find("BioA_Gth002")!=string::npos){
			pccSystem.GetPCC("BioP_Gth002.pcc");
			//pccSystem.GetPCC("SFXGame.pcc");
		}
	}
	//yo=0;
	ly=0;
}

void PCCViewer::Functions(){
	ChangeState(ePccFuncs);
}

void PCCViewer::ViewPCC(){
	ChangeState(ePccFiles);
}

void PCCViewer::ViewLevelsLoad(){
	ChangeState(ePccLoading);
	pthread_create(&loadingThread, NULL, TestLoadThread, (void*)this);
}

void PCCViewer::View()
{
	if(!pccOpen.selectedNode||pccOpen.selectedNode==pccOpen.rootNode){
		return;
	}
	Log("View %s\n",pccOpen.selectedNode->name.c_str());

	pccExport_t *pExp = pcc->exports+(pccOpen.selectedNode->id-1);
	if(!pExp->class_){
		Log("PCCViewer::View: pExp->class_ == 0\n");
		return;
	}
	if(IsTexture(pExp, pcc)){
		pccViewTexture.View(pExp);
	}else{
		string &className=pcc->GetObjNameS(pExp->class_);
		if(className=="StaticMesh"||className=="SkeletalMesh"){
			pccViewModel.View(pExp,className=="SkeletalMesh");
		}else if(className=="Level"){
			//pccViewLevel.LoadLevelAsync();
			pccViewLevel.LoadLevel();
		}else if(className=="Material"||className=="MaterialInstanceConstant"){
			//just print
			pcc->GetMaterial(pExp,true);
		}else if(className=="AnimSet"){
			//just print
			pcc->GetAnimSet(pccOpen.selectedNode->id);
		}else if(className=="BioAnimSetData"){
			//just print
			pcc->GetBioAnimSetData(pccOpen.selectedNode->id);
		}else{
			Log("Unknown export class\n");
			pcc->PrintProperties(pExp);
		}
	}
}

void PCCViewer::RenderMode(){
	if(state==ePccPreviewTex){
		pccViewTexture.RenderMode();
	}else if(state==ePccPreviewStMesh||state==ePccPreviewSkMesh||state==ePccAnim){
		pccViewModel.RenderMode();
	}
}

void PCCViewer::Export(){
	if(state==ePccPreviewTex){
		pccViewTexture.Export(pccOpen.selectedNode);
	}else if(state==ePccPreviewStMesh){
		pccViewModel.Export(pccOpen.selectedNode);
	}
}

void PCCViewer::Resize(int w, int h)
{
	ReloadShaders();
	pccViewLevel.Resize(aspect);
}

void PCCViewer::ReloadShaders()
{
	glUseProgram(0);

	pccViewModel.ReloadShaders();
	pccViewLevel.ReloadShaders();
	
	glUseProgram(0);
}

void PCCViewer::Draw()
{
	pthread_mutex_lock(&mutex);

	for(uint32_t i=0; i<buttons.size();i++){
		buttons[i]->Update();
	}

	if(state == ePccClosing){
		glClearColor(0,0,0,1);
		pthread_mutex_unlock(&mutex);
		CloseEditor();
		return;
	}

	switch(state)
	{
	case ePccFiles:
		pccList.Draw(listY);
		break;
	case ePccFuncs:
		//only buttons
		break;
	case ePccLoading:
		{
		loadingRot += deltaTime*6.0f;
		glm::mat4 mtx = glm::mat4(1.0);
		mtx = glm::translate(mtx,glm::vec3(0.5f*aspect,0.5f,0));
		mtx = glm::rotate(mtx,loadingRot,glm::vec3(0,0,1));

		SetModelMtx(mtx);
		DrawRect(-0.1/aspect,0.9,0.2/aspect,0.2,false,&testTex);
		SetModelMtx(glm::mat4(1.0));

		DrawText("Loading...",0.4,0.8,0.5);
		break;
		}
	case ePccOpen:
		pccOpen.Draw(listY);
		break;
	case ePccPreviewTex:
		pccViewTexture.Draw();
		break;
	case ePccPreviewStMesh:
	case ePccPreviewSkMesh:
		pccViewModel.Draw();
		break;
	case ePccPreviewLevel:
	case ePccTest:
		pccViewLevel.Draw();
		break;
	case ePccAnim:
		pccViewModel.DrawAnim();
		break;
	case ePccClosing:
		break;
	}

	for(uint32_t i=0; i<buttons.size();i++){
		if(buttons[i]->active)
			DrawButton(buttons[i]);
	}
	pthread_mutex_unlock(&mutex);

	char tstr[256];
	snprintf(tstr,256,"delta: %f",deltaTime);
	DrawText(tstr,0.02,0.98,0.3);
}

float cameraSpeed = 50;
static float starttx = 0;
static float startty = 0;
void PCCViewer::OnTouch(float x, float y, int a)
{
	pthread_mutex_lock(&mutex);

	//LOG("PCCViewer::OnTouch %f %f %d\n", x, y, a);
	float tx = x/scrWidth;
	float ty = y/scrHeight;
	if(a==0){
		starttx = tx;
		startty = ty;
		sty=ty;
		px = tx;
		py = ty;

		for(uint32_t i=0; i<buttons.size();i++){
			if(buttons[i]->active)
				buttons[i]->Hit(tx,ty);
		}

		if(bList.active&&
			bList.Hit(tx,ty)){
			listY = ty;
		}
		//if(state==ePccPreviewLevel||state==ePccTest){
			bMove.Hit(tx,ty);
		//}
		bScroll.Hit(tx,ty);
	}else if(a==1){
		if(bScroll.pressed){
			ly+=(sty-ty);
			ly=max(0.0f,ly);
		}
		if(state==ePccPreviewLevel||state==ePccTest){
			if(!bMove.pressed){
				pccViewLevel.Rotate((py-ty)*cameraSpeed,-(px-tx)*cameraSpeed);
			}
		}
		velocity = glm::vec3(0);
		bMove.pressed = false;
		bScroll.pressed = false;
	}else if(a==2){
		if(bScroll.pressed){
			//float sy = ly+(sty-ty);
			float sy = py-ty;// TODO check
			if(state==ePccPreviewTex)
				pccViewTexture.Scroll(sy);
			else if(state==ePccPreviewStMesh||state==ePccPreviewSkMesh||state==ePccAnim)
				pccViewModel.Scroll(sy);
			else if(state==ePccFiles)
				pccList.Scroll(sy);
			else if(state==ePccOpen)
				pccOpen.Scroll(sy);
			//else
			//	yo = max(0.0f,sy);
		}

		if(bMove.pressed){
			velocity.x = -(starttx-tx);
			velocity.z = -(startty-ty);
		}
		if(state==ePccPreviewLevel||state==ePccTest){
			if(!bMove.pressed){
				//levelCamera.rot.x = lastRot.x+(startty-ty)*cameraSpeed;
				//levelCamera.rot.y = lastRot.y-(starttx-tx)*cameraSpeed;
				pccViewLevel.Rotate((py-ty)*cameraSpeed,-(px-tx)*cameraSpeed);
			}
		}
		px = tx;
		py = ty;
	}
	pthread_mutex_unlock(&mutex);
}

float oldx = 0;
float oldy = 0;
void PCCViewer::OnMouse(float mx, float my)
{
#if 0
	if(state==ePccPreviewLevel){
		float dx = mx-oldx;
		float dy = my-oldy;
		oldx = mx;
		oldy = my;
		levelCamera.rot.x += dy*0.3;
		levelCamera.rot.y -= dx*0.3;
	}
#endif
}

void PCCViewer::OnScroll(float sx, float sy)
{
	/*
	if(state==ePccPreviewTex||state==ePccPreviewStMesh||state==ePccPreviewSkMesh||state==ePccAnim)
		pyo = max(0.0f,pyo-sy*0.2f);
	else
		yo = max(0.0f,yo-sy*0.2f);
	*/
	if(state==ePccPreviewTex)
		pccViewTexture.Scroll(-sy*0.2f);
	else if(state==ePccPreviewStMesh||state==ePccPreviewSkMesh||state==ePccAnim)
		pccViewModel.Scroll(-sy*0.2f);
	else if(state==ePccFiles)
		pccList.Scroll(-sy*0.2f);
	else if(state==ePccOpen)
		pccOpen.Scroll(-sy*0.2f);
}

void PCCViewer::OnKey(int k, int a)
{
	//Log("PCCViewer::OnKey(k %d, a %d)\n",k,a);
//#define GLFW_KEY_B                  66
//AKEYCODE_B               = 30
	if(a==1&&k==66)
		ReloadShaders();
}

void *TestLoadThread(void *arg){
	PCCViewer *pv = (PCCViewer*)arg;

	char temp[256];
	g_fs.GetFilePath("test_levels.txt",temp);
	ifstream testFile(temp);
	if(!testFile){
		Log("test_levels.txt not found!\n");
		pthread_mutex_lock(&mutex);
		pv->ChangeState(ePccFuncs);
		pthread_mutex_unlock(&mutex);
		return 0;
	}

	glm::vec3 startPos(0.0);
	testFile >> startPos.x;
	testFile >> startPos.y;
	testFile >> startPos.z;
	testFile >> pv->pccViewLevel.texLod;
	testFile >> pv->pccViewLevel.numTestLevels;
	Log("Loading %d test levels\n",pv->pccViewLevel.numTestLevels);
	pv->pccViewLevel.testLevels = new PCCLevel*[pv->pccViewLevel.numTestLevels];
	int id = 0;
	int first=-1;
	SetLogMode(eLogToFile);
	for(int i=0;i<pv->pccViewLevel.numTestLevels;i++){
		testFile >> temp;
		testFile >> id;
		PCCFile *p = pv->pccSystem.GetPCC(temp);
		if(id){
			pv->pccViewLevel.testLevels[i] = p->GetLevel(id);
			if(first==-1)
				first = i;
		}
		else
			pv->pccViewLevel.testLevels[i] = 0;
	}
	SetLogMode(eLogBoth);
	testFile.close();
	if(first==-1)
		EngineError("TestLoadThread: first is -1");
	pv->pccViewLevel.SetupCamera(pv->pccViewLevel.testLevels[first]);
	pv->pccViewLevel.levelCamera.pos = startPos;

	pthread_mutex_lock(&mutex);
	pv->ChangeState(ePccTest);
	pthread_mutex_unlock(&mutex);

	return 0;
}

void *LoadLevelThread(void *arg){
	PCCViewer *pv = (PCCViewer*)arg;
	bool ret = pv->pccViewLevel.ViewLevel(pv->pccOpen.selectedNode->id);

	pthread_mutex_lock(&mutex);
	if(ret){
		pv->ChangeState(ePccPreviewLevel);
	}else{
		Log("Load level error\n");
		pv->ChangeState(ePccOpen);
	}
	pthread_mutex_unlock(&mutex);

	return 0;
}

PCCViewList::PCCViewList(){}

void PCCViewList::Init(PCCViewer *viewer, std::vector<Button*> &buttons)
{
	bOpen = EditorButton(0.28,0.02,0.4,0.07, "Open PCC",viewer,&PCCViewer::Open);
	bFunctions = EditorButton(0.7,0.02,0.28,0.07, "Functions",viewer,&PCCViewer::Functions);
	buttons.push_back(&bOpen);
	buttons.push_back(&bFunctions);

	yo = 0;
	bList = &viewer->bList;
}

void PCCViewList::Activate()
{
	bOpen.active = true;
	bFunctions.active = true;
}

void PCCViewList::Scroll(float y)
{
	yo = max(0.0f,yo+y);
}

void PCCViewList::Draw(float listY)
{
	//DrawButton(&bList);

	float cy = bList->y;
	for(int i=0; i<numPccFiles; i++)
	{
		float y = cy-yo;
		if(y<bList->y){
			cy+=0.04;
			continue;
		}
		if(selectedPccFile==i){
			DrawRect(0.02,y,0.8,0.04);
		}
		DrawText(pccFiles[i].c_str(),0.02,y+0.03,0.3);
		if(bList->pressed){
			if(listY>=y&&listY<y+0.04){
				selectedPccFile = i;
			}
		}
		cy+=0.04;
		if(y>1.0)
			break;
	}
	bList->pressed = false;
}

PCCViewOpen::PCCViewOpen(){}

void PCCViewOpen::Init(PCCViewer *viewer, std::vector<Button *> &buttons)
{
	bMode = EditorButtonT<PCCViewOpen>(0.28,0.02,0.34,0.07, "Tree",this,&PCCViewOpen::SwitchMode);
	displayMode = eTree;
	bFilter = EditorButtonT<PCCViewOpen>(0.64,0.1,0.34,0.07,"Filter:",this,&PCCViewOpen::Filter);
	filterMode = eAll;
	bView = EditorButton(0.64,0.02,0.34,0.07,"View",viewer,&PCCViewer::View);
	buttons.push_back(&bMode);
	buttons.push_back(&bFilter);
	buttons.push_back(&bView);

	selectedNode = 0;
	nodesList = 0;
	rootNode = 0;
	numNodes = 0;

	yo = 0;
	bList = &viewer->bList;
}

void PCCViewOpen::Activate()
{
	bMode.active = true;
	bFilter.active = (displayMode==eExports);
	bView.text = "View";
	bView.active = CanPreview(selectedNode);
}

void PCCViewOpen::Scroll(float y)
{
	yo = max(0.0f,yo+y);
}

void PCCViewOpen::Draw(float aListY)
{
	listY = aListY;
	DrawButton(bList);
	glEnable(GL_SCISSOR_TEST);
	glScissor(bList->x*scrWidth,(1.0f-(bList->y+bList->h))*scrHeight,bList->w*scrWidth,bList->h*scrHeight);
	DrawLists();
	glDisable(GL_SCISSOR_TEST);
}

void PCCViewOpen::DrawLists()
{
	hit = false;
	switch(displayMode)
	{
		case eNames:
			for(int i=0; i<pcc->header.nameCount; i++)
			{
				float y=bList->y+i*0.04-yo+0.03;
				if(y<bList->y)
					continue;
				char tstr[256];
				snprintf(tstr,256,"%d: %s",i, pcc->nameTable[i].c_str());
				DrawText(tstr,0.02,y,0.3);
				if(y>1.0)
					break;
			}
			break;
		case eImports:
			for(int i=0; i<pcc->header.importCount; i++)
			{
				float y = bList->y+i*0.04-yo+0.03;
				if(y<bList->y)
					continue;
				char tstr[256];
				snprintf(tstr,256,"%d: %s.%s (%s) link %d\n",i, pcc->GetName(pcc->imports[i].packageName),
					 pcc->GetName(pcc->imports[i].importName),pcc->GetName(pcc->imports[i].className),pcc->imports[i].link);
				DrawText(tstr,0.02,y,0.3);
				if(y>1.0)
					break;
			}
			break;
		case eExports:
			cy = bList->y-yo;
			for(int i=0; i<pcc->header.exportCount; i++)
			{
				float y = cy+0.03;
				pccExport_t *pExp = pcc->exports+i;
				if(filterMode){
					if(filterMode==eTextures){
						if(!IsTexture(pExp,pcc))
							continue;
						//PCCTexture2D *pTex=pcc->GetTexture(pExp);
						//if(!pTex||(pTex->imgInfo.storageType!=pccSto&&pTex->imgInfo.storageType!=arcCpr))
						//	continue;
					}else if(filterMode==eMeshes){
						if(!pExp->class_||
							(pcc->GetObjNameS(pExp->class_)!="StaticMesh"&&
							pcc->GetObjNameS(pExp->class_)!="SkeletalMesh"))
							continue;
					}
				}
				if(y<bList->y){
					cy+=0.04;
					continue;//above the screen
				}
				char tstr[256];
				if(IsTexture(pExp,pcc)){
					PCCTexture2D *pTex = pcc->GetTexture(pExp);
					snprintf(tstr,256,"%d: %s(%d), class %s (%dx%d, stor %d), link %d, size %d, offset %d\n",
						i, pcc->GetName(pExp->objectName),pExp->objectNameNumber,pcc->GetObjName(pExp->class_), pTex->width,pTex->height,pTex->imgInfos[pTex->firstLod].storageType, pExp->link, pExp->dataSize,pExp->dataOffset);

				}else{
					snprintf(tstr,256,"%d: %s(%d), class %s, link %d, size %d, offset %d\n",
						i, pcc->GetName(pExp->objectName),pExp->objectNameNumber,pcc->GetObjName(pExp->class_),pExp->link, pExp->dataSize,pExp->dataOffset);
				}
				if(bList->pressed){
					if(listY>=y&&listY<y+0.04){
						selectedNode = nodesList+i+1;
						hit = true;
					}
				}
				if(selectedNode==nodesList+i+1){
					DrawRect(0.02,y-0.03,0.8,0.04);
				}
				DrawText(tstr,0.02,y,0.3);
				cy+=0.04;
				if(y>1.0)
					break;
			}
			break;
		case eTree:
			cy=bList->y;
			DrawTree(rootNode,0);
			/*float cy=0.35;
			float cx=0;
			treeNode_t *pNode=rootNode;
			stack<treeNode_t*>nodesStack;
			nodesStack.push(rootNode);

			for(int i=0;i<pNode->nodes.size();i++)
			{
				nodesStack.push(pNode);
				pNode = pNode->nodes[i];
				DrawText(pNode->name.c_str(),cx+0.02,cy,0.3);
				if(pNode->nodes.size())
				{
					DrawText("+",cx,cy,0.3);
					cy+=0.04;
					cx+=0.02;
					for(int j=0;j<pNode->nodes.size();j++)
					{
						DrawText(pNode->nodes[j]->name.c_str(),cx+0.02,cy,0.3);
					}
					cx-=0.03;
				}
				cy+=0.04;
				pNode=nodesStack.top();
				nodesStack.pop();
			}*/
			break;
	}
	if(bList->pressed)
	{
		bList->pressed = false;
		if(selectedNode&&hit){
			if(!selectedNode->nodes.empty()){
				selectedNode->active = !selectedNode->active;
			}
			bView.active = CanPreview(selectedNode);
		}
	}
}

void PCCViewOpen::DrawTree(treeNode_t *n, int d)
{
	float y = cy-yo;
	float to = 0.03f;
	if(y>1.0)
		return;
	if(y+to>=bList->y){
		if(bList->pressed){
			if(listY>=y&&listY<y+0.04){
				selectedNode = n;
				hit = true;
			}
		}
		if(selectedNode==n){
			DrawRect((d*0.03+0.02)/aspect,y,0.8,0.04);
		}
		DrawText(n->name.c_str(),(d*0.03+0.02)/aspect,y+to,0.3);
		if(!n->nodes.empty()){
			if(n->active){
				DrawText("-",d*0.03/aspect,y+to,0.3);
			}else{
				DrawText("+",d*0.03/aspect,y+to,0.3);
			}
		}
	}
	cy+=0.04;
	if(n->active){
		for(uint32_t i=0;i<n->nodes.size();i++){
			DrawTree(n->nodes[i],d+1);
		}
	}
}

void PCCViewOpen::SwitchMode()
{
	displayMode = (ePccDisplayMode)((displayMode+1)%4);
	bFilter.active = false;
	//bView.active = false;
	switch(displayMode)
	{
		case eNames:
			bMode.text="Names";
			break;
		case eImports:
			bMode.text="Imports";
			break;
		case eExports:
			bMode.text="Exports";
			bFilter.active = true;
			break;
		case eTree:
			bMode.text="Tree";
			break;
	}
	yo=0;// individual offsets?
	//ly=0;
}

void PCCViewOpen::Filter()
{
	filterMode = (ePccFilterMode)((filterMode+1)%3);
	switch(filterMode)
	{
		case eAll:
			bFilter.text="Filter:";
			break;
		case eMeshes:
			bFilter.text="Filter: Mesh";
			break;
		case eTextures:
			bFilter.text="Filter: Texture";
			break;
	}
}

void PCCViewOpen::BuildTree(PCCFile *pPcc)
{
	pcc = pPcc;

	numNodes = pcc->header.exportCount+1;
	nodesList = new treeNode_t[numNodes];
	rootNode = nodesList;
	rootNode->name = "root";
	rootNode->tag = true;
	rootNode->active = true;
	for(int i=0; i<pcc->header.exportCount; i++)
	{
		pccExport_t *pExp = pcc->exports+i;
		treeNode_t *pNode = nodesList+i+1;
		pNode->id = i+1;
		char tstr[256];
		snprintf(tstr,256,"%d. %s(%d) (%s)", i,pcc->GetName(pExp->objectName), pExp->objectNameNumber, pcc->GetObjName(pExp->class_));
		pNode->name = tstr;
	}
	int curIndex;
	for(int i=1; i<=pcc->header.exportCount; i++)
	{
		treeNode_t *pNode = nodesList+i;
		curIndex=i;
		while(!pNode->tag)
		{
			pNode->tag = true;
			pccExport_t *pExp = pcc->exports+curIndex-1;
			curIndex = pExp->link;
			int link = curIndex;
			//if(link==0)
			//	pNode->active = true;
			if(link>=0)
			{
				nodesList[link].nodes.push_back(pNode);
				pNode = nodesList+link;
			}
			else
			{
				Log("Warn: Export %d link: %d\n", curIndex, link);
			}
		}
	}
}

bool PCCViewOpen::CanPreview(treeNode_t *node)
{
	if(!node||node==rootNode){
		return false;
	}

	pccExport_t *pExp = pcc->exports+(node->id-1);
	if(!pExp->class_)
		return false;
	if(IsTexture(pExp,pcc)){
		PCCTexture2D *pTex=pcc->GetTexture(pExp,true);
		if(pTex&&(pTex->imgInfos[pTex->firstLod].storageType==pccSto||pTex->imgInfos[pTex->firstLod].storageType==arcCpr))
			return true;
	}

	string &className = pcc->GetObjNameS(pExp->class_);
	if(className=="StaticMesh"||className=="SkeletalMesh"){
		return true;
	}
	if(className=="Material"||className=="MaterialInstanceConstant"){
		return true;
	}
	if(className=="Level"){
		return true;
	}
	if(className=="StaticMeshActor"||className=="StaticMeshComponent")
		return true;
	if(className=="AnimSet"||className=="AnimSequence"||className=="BioAnimSetData")
		return true;

	//TODO: other types
	return false;
}

bool IsTexture(pccExport_t *pExp,PCCFile *pcc){
	if(!pExp->class_)
		return false;
	string *cl = pcc->GetObjNameSP(pExp->class_);
	return cl&&(*cl=="Texture2D"||*cl=="LightMapTexture2D"||*cl=="TextureFlipBook"||*cl=="ShadowMapTexture2D");
}

PCCViewTexture::PCCViewTexture(){}

void PCCViewTexture::Init(PCCViewer *pViewer, std::vector<Button *> &buttons)
{
	viewer = pViewer;

	bNext = EditorButtonT<PCCViewTexture>(0.91,0.84,0.07,0.07,">",this,&PCCViewTexture::Next);
	bPrev = EditorButtonT<PCCViewTexture>(0.02,0.84,0.07,0.07,"<",this,&PCCViewTexture::Prev);
	bBack = EditorButtonT<PCCViewTexture>(0.64,0.02,0.34,0.07,"Back",this,&PCCViewTexture::Back);
	buttons.push_back(&bNext);
	buttons.push_back(&bPrev);
	buttons.push_back(&bBack);

	texMode = eRGBA;
	previewTex = 0;
	previewList.clear();
	previewListPos = 0;
	yo = 0;

	bRenderMode = &viewer->bRenderMode;
}

void PCCViewTexture::Activate()
{
	bNext.active = true;
	bPrev.active = true;
	bBack.active = true;
}

void PCCViewTexture::Scroll(float y)
{
	yo = max(0.0f,yo+y);
}

void PCCViewTexture::Draw()
{
	float h=0.9*aspect/((float)previewTex->width/previewTex->height);
	if(texMode==eRGBA)
		DrawRect(0.02,0.1-yo,0.96,h);
	DrawRect(0.02,0.1-yo,0.96,h,texMode==eRGBA,previewTex->GetGLTexture());
	DrawText(viewer->pcc->GetObjName(previewTex->objId),0.02,0.95,0.3);
}

void PCCViewTexture::View(pccExport_t *pExp)
{
	if(SetPreviewTex(pExp)){
		//LOG("Loaded succesfully\n");
		viewer->ChangeState(ePccPreviewTex);
		texMode = (ePccTexMode)(texMode-1);
		RenderMode();
		BuildPreviewList(pExp);
	}else{
		Log("Error\n");
	}
}

void PCCViewTexture::PreviewSwitch(bool next)
{
	if(previewList.size()<2)
		return;
	if(next)
		previewListPos=(previewListPos+1)%previewList.size();
	else
	{
		previewListPos-=1;
		if(previewListPos<0)
			previewListPos+=previewList.size();
	}
	Log("new preview list pos %d\n",previewListPos);
	viewer->pccOpen.selectedNode = previewList[previewListPos];
	if(previewTex){
		//delete previewTex;
		previewTex = 0;
	}
	SetPreviewTex(viewer->pcc->exports+(viewer->pccOpen.selectedNode->id-1));
}

void PCCViewTexture::Next()
{
	PreviewSwitch(true);
}

void PCCViewTexture::Prev()
{
	PreviewSwitch(false);
}

void PCCViewTexture::Back()
{
	if(previewTex){
		previewTex = 0;
		viewer->ChangeState(ePccOpen);
		//ly = 0;
		glClearColor(0,0,0,1);
	}
}

void PCCViewTexture::RenderMode()
{
	texMode=(ePccTexMode)((texMode+1)%2);
	switch(texMode){
	case eRGB:
		bRenderMode->text="RGB";
		break;
	case eRGBA:
		bRenderMode->text="RGBA";
		break;
	}
}

void PCCViewTexture::Export(treeNode_t *selectedNode)
{
	neArray<char> data;
	int lod=0;
	int format;
	previewTex->GetData(lod,data,format);
	int w = previewTex->imgInfos[lod].w;
	int h = previewTex->imgInfos[lod].h;
	Log("Export tex: %s %dx%d\n",/*selectedNode->name.c_str()*/previewTex->pcc->GetObjName(selectedNode->id),w,h);
	ExportDDS(previewTex->pcc->GetObjName(selectedNode->id),data,w,h,format);
}

bool PCCViewTexture::SetPreviewTex(pccExport_t *exp)
{
	yo=0;
	//ly=0;
	if(previewTex)
	{
		Log("Error: previewTex!=0\n");
		return false;
	}
	previewTex = viewer->pcc->GetTexture(exp,true);
	if(!previewTex)
		return false;
	Texture *t = previewTex->GetGLTexture();
	if(!t)
		Log("Can't GetGLTexture\n");
	//else
	//	Log("SetPreviewTex t=%p\n",t);
	return (t!=0);
}

void PCCViewTexture::BuildPreviewList(pccExport_t *exp)
{
	vector<treeNode_t*> *nodes = &viewer->pccOpen.nodesList[exp->link].nodes;
	previewList.clear();
	previewListPos = 0;
	for(uint32_t i=0; i<nodes->size(); i++)
	{
		treeNode_t *pNode = (*nodes)[i];
		if(pNode==viewer->pccOpen.selectedNode)
			previewListPos = previewList.size();

		pccExport_t *pExp = viewer->pcc->exports+(pNode->id-1);
		if(IsTexture(pExp,viewer->pcc)){
			PCCTexture2D *pTex = viewer->pcc->GetTexture(pExp);
			if(pTex&&(pTex->imgInfos[pTex->firstLod].storageType==pccSto||pTex->imgInfos[pTex->firstLod].storageType==arcCpr))
				previewList.push_back(pNode);
		}
	}
/*
	LOG("preview pos %d, list (%d):\n",previewListPos,previewList.size());
	for(int i=0;i<previewList.size();i++)
	{
		LOG("%s",previewList[i]->name.c_str());
	}
*/
}

PCCViewModel::PCCViewModel()
{}

PCCViewModel::~PCCViewModel()
{
	if(testSkel)
		delete[] testSkel;
	if(animMeshes)
		delete[] animMeshes;
}

void PCCViewModel::Init(PCCViewer *pViewer, std::vector<Button *> &buttons)
{
	viewer = pViewer;

	bSkel = EditorButtonT<PCCViewModel>(0.52,0.84,0.4,0.07,"Show skeleton",this,&PCCViewModel::ShowSkel);
	bBack = EditorButtonT<PCCViewModel>(0.64,0.02,0.34,0.07,"Back",this,&PCCViewModel::Back);
	buttons.push_back(&bSkel);
	buttons.push_back(&bBack);

	showSkel = false;
	meshMode = eMeshTexture;
	previewMesh = 0;

	testSkel=0;
	animMode=0;
	animMeshes=0;
	numAnimMeshes=0;
	yo = 0;

	bRenderMode = &viewer->bRenderMode;
}

void PCCViewModel::Activate()
{
	bBack.active = true;
}

void PCCViewModel::Scroll(float y)
{
	yo = max(0.0f,yo+y);
}

void PCCViewModel::Draw()
{
	//float speed = 2.0f;
	//meshCamera.pos += glm::mat3(glm::inverse(levelCamera.viewMtx))*velocity*speed*(float)deltaTime;
	meshRot += glm::vec2(viewer->velocity.x,-viewer->velocity.z)*(float)deltaTime;

	int flags=0;
	if(meshMode==eMeshNormal){
		SetProg(&normalProg);
		flags|=eMeshNorm;
	}else if(meshMode==eMeshLight){
		SetLightingEnabled(true);
		flags|=eMeshNorm;
	}else if(meshMode==eMeshNormalmap){
		SetProg(&normalmapProg);
		glUniform3fv(u_viewPosNm,1,&meshCamera.pos.x);
		flags|=eMeshNorm|eMeshTan;
	}
	if(skeletal)
		PreviewSkeletalMesh((SkeletalMesh*)previewMesh,yo,flags);
	else
		PreviewStaticMesh((PCCStaticMesh*)previewMesh,yo,flags);

	SetProg(0);
	DrawText(viewer->pcc->GetObjName(previewMesh->objId),0.02,0.95,0.3);
}

void PCCViewModel::DrawAnim()
{
	SkeletalMesh *pMesh = animMeshes[0];
	for(int i=1;!pMesh&&i<numAnimMeshes;i++){
		pMesh=animMeshes[i];
	}
	if(!pMesh)
		return;
	if(animMode==1){
		animPos += deltaTime*4.0f;
		if(animPos>1.0f){
			animPos = 0.0f;
			GenerateRandomFrame(testSkel,pMesh);
		}
		InterpolateSkel(testSkel,animPos,pMesh);
	}
	meshRot += glm::vec2(viewer->velocity.x,-viewer->velocity.z)*(float)deltaTime;

	meshCamera.UpdateView();
	meshCamera.UpdateProj(75.0f, aspect, 0.01f, 50.0f);
	SetCamera(&meshCamera);

	glm::mat4 mtx = glm::mat4(1.0);
	mtx = glm::scale(mtx,glm::vec3(2.0/pMesh->bounds.radius));
	mtx = glm::rotate(mtx,glm::radians(meshRot.y*90.0f),glm::vec3(1,0,0));
	mtx = glm::rotate(mtx,glm::radians(meshRot.x*90.0f+90.0f),glm::vec3(0,1,0));
	mtx = glm::scale(mtx,glm::vec3(glm::clamp(yo+0.1f,0.1f,10.0f)));
	mtx = glm::scale(mtx,glm::vec3(-1,1,1));
	mtx = glm::rotate(mtx,glm::radians(-90.0f),glm::vec3(1,0,0));
	mtx = glm::translate(mtx,-pMesh->bounds.pos);
	SetModelMtx(mtx);

	for(int i=0;i<numAnimMeshes;i++)
	{
		if(!animMeshes[i])
			continue;
		int flags=0;
		if(meshMode==eMeshNormal){
			SetProg(&normalProg);
			flags|=eMeshNorm;
		}else if(meshMode==eMeshLight){
			SetLightingEnabled(true);
			flags|=eMeshNorm;
		}else if(meshMode==eMeshNormalmap){
			SetProg(&normalmapProg);
			glUniform3fv(u_viewPosNm,1,&meshCamera.pos.x);
			flags|=eMeshNorm|eMeshTan;
		}
		glm::mat4 *sk=0;
		if(animMode==1&&i==0){
			sk = testSkel+pMesh->numBones*2;
			flags |= eMeshSkin;
		}
		DrawSkeletalMesh(animMeshes[i],flags,sk);
		flags &= ~eMeshSkin;
	}

	SetCamera(0);
	SetProg(0);
	//DrawText(selectedNode->name.c_str(),0.02,0.95,0.3);
}

void PCCViewModel::PreviewStaticMesh(PCCStaticMesh *pMesh,float r,int flags)
{
	//meshCamera.pos = glm::vec3(0,0.5,3);//*pMesh->bounding.radius;
	//meshCamera.rot = glm::vec3(10,0,0);
	meshCamera.UpdateView();
	meshCamera.UpdateProj(75.0f, aspect, 0.01f, 50.0f);
	SetCamera(&meshCamera);

	glm::mat4 mtx = glm::mat4(1.0);
	mtx = glm::scale(mtx,glm::vec3(2.0/pMesh->bounding.radius));
	//mtx = glm::rotate(mtx,glm::radians(r*90.0f),glm::vec3(0,1,0));
	mtx = glm::rotate(mtx,glm::radians(meshRot.y*90.0f),glm::vec3(1,0,0));
	mtx = glm::rotate(mtx,glm::radians(meshRot.x*90.0f),glm::vec3(0,1,0));
	mtx = glm::scale(mtx,glm::vec3(glm::clamp(r+0.8f,0.1f,10.0f)));
	mtx = glm::scale(mtx,glm::vec3(-1,1,1));
	mtx = glm::rotate(mtx,glm::radians(-90.0f),glm::vec3(1,0,0));
	mtx = glm::translate(mtx,-pMesh->bounding.pos);
	SetModelMtx(mtx);

	if(flags&eMeshNorm)
		glEnableVertexAttribArray(1);
	if(flags&eMeshTan)
		glEnableVertexAttribArray(4);

	DrawStaticMesh(pMesh,0,flags);

	glDisableVertexAttribArray(1);
	glDisableVertexAttribArray(4);

	SetCamera(0);
}

#include <gtc/random.hpp>
void GenerateRandomFrame(glm::mat4 *skel,SkeletalMesh *pMesh)
{
	int nb=pMesh->numBones;
	memcpy(skel,skel+nb,nb*64);
	int i=0;
	if(nb>8)
		i=8;
	for(;i<nb;i++)
	{
		skel[i+nb] = glm::mat4(1.0f);
		skel[i+nb] = glm::translate(skel[i+nb],glm::linearRand(glm::vec3(-0.5f,-1.0f,-1.0f),glm::vec3(0.5f,1.0f,1.0f)));
		if(i){
			skel[i+nb] = skel[pMesh->skeleton[i].parent+nb]*skel[i+nb];
		}
	}
}

void InterpolateSkel(glm::mat4 *skel,float pos,SkeletalMesh *pMesh)
{
	int nb = pMesh->numBones;
	for(int i=0;i<nb;i++)
	{
		skel[nb*2+i] = glm::interpolate(skel[i], skel[nb+i], pos);
	}
}

void PCCViewModel::PreviewSkeletalMesh(SkeletalMesh *pMesh,float r,int flags)
{
	//meshCamera.pos = glm::vec3(0,0.4,2);//*pMesh->bounds.radius;
	//meshCamera.rot = glm::vec3(10,0,0);

	meshCamera.UpdateView();
	meshCamera.UpdateProj(75.0f, aspect, 0.01f, 50.0f);
	SetCamera(&meshCamera);

	glm::mat4 mtx = glm::mat4(1.0);
	mtx = glm::scale(mtx,glm::vec3(2.0/pMesh->bounds.radius));
	//mtx = glm::rotate(mtx,glm::radians(r*90.0f+90.0f),glm::vec3(0,1,0));
	mtx = glm::rotate(mtx,glm::radians(meshRot.y*90.0f),glm::vec3(1,0,0));
	mtx = glm::rotate(mtx,glm::radians(meshRot.x*90.0f+90.0f),glm::vec3(0,1,0));
	mtx = glm::scale(mtx,glm::vec3(glm::clamp(r+0.8f,0.1f,10.0f)));
	mtx = glm::scale(mtx,glm::vec3(-1,1,1));
	mtx = glm::rotate(mtx,glm::radians(-90.0f),glm::vec3(1,0,0));
	mtx = glm::translate(mtx,-pMesh->bounds.pos);
	SetModelMtx(mtx);

	DrawSkeletalMesh(pMesh,flags,0);

#if 0
	//display normals
	glm::vec3 *normalVerts = new glm::vec3[lod->numVertsArr*2];
	memset(normalVerts,0,lod->numVertsArr*24);
	for(int i=0;i<lod->numVertsArr;i++){
		//int8_t *vb = (int8_t*)lod->verts+(i*lod->vertSize);
		uint8_t *vb = (uint8_t*)lod->verts+(i*lod->vertSize);
		normalVerts[i*2]=*(glm::vec3*)(vb+16);
		//-z,-x,-y
		uint32_t pn = *(uint32_t*)(vb+4);
		//pn=pn^0x80808080;
		//glm::vec3 norm(-(vb[4]/127.5f),-(vb[5]/127.5f),-(vb[6]/127.5f));
		glm::vec3 norm((int8_t)((pn)&0xFF)/127.5f,(int8_t)((pn>>8)&0xFF)/127.5f,(int8_t)((pn>>16)&0xFF)/127.5f);
		normalVerts[i*2+1]=normalVerts[i*2]+norm*pMesh->bounds.radius*0.02f;
	}
	whiteTex.Bind();
	glDisableVertexAttribArray(2);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 12, normalVerts);
	glDrawArrays(GL_LINES,0,lod->numVertsArr*2);
	glEnableVertexAttribArray(2);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 12, 0);
	delete[] normalVerts;
#endif

	SetCamera(0);
}

void PCCViewModel::DrawStaticMesh(PCCStaticMesh *pMesh,PCCMaterial** pMats, int flags)
{
	StaticMeshLod_t *lod=pMesh->LODs;
	char *posOffs=(char*)lod->verts;
	char *uvOffs=(char*)lod->UVs;
	char *idxOffs=(char*)lod->indices;
	if(flags&eMeshVBO){
		if(!lod->vbo)
			pMesh->InitBuffers();
		posOffs=0;
		uvOffs=(char*)lod->vboUVOffs;
		lod->vbo->Bind();
		idxOffs=0;
		lod->ibo->Bind();
	}
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 12, posOffs);//pos
	if(flags&eMeshNorm)
		glVertexAttribPointer(1, 4, GL_BYTE, GL_TRUE, lod->UVStrip, uvOffs+4);//norm
	glVertexAttribPointer(2, 2, GL_HALF_FLOAT, GL_FALSE, lod->UVStrip, uvOffs+8);//uv
	if(flags&eMeshUV2)
		glVertexAttribPointer(3, 2, GL_HALF_FLOAT, GL_FALSE, lod->UVStrip, uvOffs+12);//uv2
	if(flags&eMeshTan)
		glVertexAttribPointer(4, 3, GL_BYTE, GL_TRUE, lod->UVStrip, uvOffs);//tan

	if(flags&eMeshVBO){
		lod->vbo->Unbind();
	}
	if(!pMats)
		pMats = pMesh->materials;
	for(int i=0;i<lod->sectionCount;i++){
		if(pMats[i]){
			if(pMats[i]->diffuseTex&&
				pMats[i]->diffuseTex->GetGLTexture(&testTex,viewer->pccViewLevel.texLod)){
				pMats[i]->diffuseTex->GetGLTexture()->Bind();
			}else{
				//whiteTex.Bind();
				testTex.Bind();
			}
/*
			if(pMats[i]->blendMode==eBlendMasked&&!(flags&4))
				SetProg(&alphaTestProg);
			else if(pMats[i]->blendMode==eBlendAdditive){
				glEnable(GL_BLEND);
				glBlendFunc(1,1);
			}else if(pMats[i]->blendMode==eBlendTranslucent){
				glEnable(GL_BLEND);
				glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
			}
*/
			if(flags&eMeshTan){
				glActiveTexture(GL_TEXTURE1);
				if(pMats[i]->normalmapTex&&
					pMats[i]->normalmapTex->GetGLTexture(&normTex,viewer->pccViewLevel.texLod)){
					pMats[i]->normalmapTex->GetGLTexture()->Bind();
				}else{
					normTex.Bind();
				}
				glActiveTexture(GL_TEXTURE0);
			}
		}

		if(lod->numIndices>0){
			glDrawElements(GL_TRIANGLES,lod->sections[i].numFaces*3,GL_UNSIGNED_SHORT,idxOffs+lod->sections[i].firstIdx*2);
		}
/*
		if(pMats[i]){
			if(pMats[i]->blendMode==eBlendMasked&&!(flags&4)){
				SetProg(0);
			}else if(pMats[i]->blendMode==eBlendAdditive||pMats[i]->blendMode==eBlendTranslucent){
				glDisable(GL_BLEND);
			}
		}
*/
	}
	if(flags&eMeshVBO){
		lod->ibo->Unbind();
	}
}

void PCCViewModel::DrawSkeletalMesh(SkeletalMesh *pMesh,int flags, glm::mat4 *skel){

	SkelLod_t *lod=pMesh->LODs;

	//skin
	glm::vec3 *skinVerts=0;
	if((flags&eMeshSkin)&&skel){
		skinVerts = new glm::vec3[lod->numVerts];
		SMLchunk_t *ch = lod->chunks;
		//glm::mat4 *curSkel = pMesh->skelMats;
		glm::mat4 *curSkel = skel;
		for(int c=0;c<lod->chunksCount;c++,ch++){
			for(int i=ch->firstVtx;i<ch->firstVtx+ch->rigidVertsNum+ch->softVertsNum;i++){
				glm::vec4 orig = glm::vec4(*(glm::vec3*)(lod->verts+lod->vertSize*i+16),1.0f);
				glm::vec3 outv = glm::vec3(orig);
				int b = ((int8_t*)lod->verts)[lod->vertSize*i+8];
				float w = ((uint8_t*)lod->verts)[lod->vertSize*i+12]/255.0f;
				b=ch->bones[b];
				if(b>-1){
					outv = glm::vec3(curSkel[b] * orig * w);
					//orig = orig*curSkel[b];
					//orig = orig-curSkel[b][3];
					for(int j=1;j<4;j++){
						int b = ((int8_t*)lod->verts)[lod->vertSize*i+8+j];
						float w = ((uint8_t*)lod->verts)[lod->vertSize*i+12+j]/255.0f;
						if(b<0||w==0.0f)
							break;
						outv += glm::vec3(curSkel[b] * orig * w);
					}
				}
				skinVerts[i] = outv;
			}
		}
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 12, skinVerts);
	}
	else//skin
	{
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, lod->vertSize, lod->verts+16);
	}

	if(flags&eMeshNorm){
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 4, GL_BYTE, GL_TRUE, lod->vertSize, lod->verts+4);
	}
	glVertexAttribPointer(2, 2, GL_HALF_FLOAT, GL_FALSE, lod->vertSize, lod->verts+28);
	if(flags&eMeshTan){
		glEnableVertexAttribArray(4);
		glVertexAttribPointer(4, 3, GL_BYTE, GL_TRUE, lod->vertSize, lod->verts);
	}

	for(int i=0;i<lod->sectionsCount;i++){
		SMLsection_t *sec=lod->sections+i;
		if(pMesh->pMaterials[sec->matIdx]&&
				pMesh->pMaterials[sec->matIdx]->diffuseTex){
			Texture *t = pMesh->pMaterials[sec->matIdx]->diffuseTex->GetGLTexture(&testTex);
			if(t)
				t->Bind();
		}else{
			testTex.Bind();
		}
		if(flags&eMeshTan){
			glActiveTexture(GL_TEXTURE1);
			if(pMesh->pMaterials[sec->matIdx]&&
				pMesh->pMaterials[sec->matIdx]->normalmapTex&&
				pMesh->pMaterials[sec->matIdx]->normalmapTex->GetGLTexture(&normTex)){
				Texture *t = pMesh->pMaterials[sec->matIdx]->normalmapTex->GetGLTexture(&normTex);
				if(t)
					t->Bind();
			}else{
				normTex.Bind();
			}
			glActiveTexture(GL_TEXTURE0);
		}
		glDrawElements(GL_TRIANGLES,sec->numTris*3,GL_UNSIGNED_SHORT,lod->inds+sec->firstIdx*2);
	}

	if(flags&eMeshNorm)
		glDisableVertexAttribArray(1);
	if(flags&eMeshTan)
		glDisableVertexAttribArray(4);

	//skeleton
	if(showSkel){
		glm::vec3 *skelVerts=new glm::vec3[pMesh->numBones*2];

		for(int i=0;i<pMesh->numBones;i++){
			Bone &b = pMesh->skeleton[i];
			skelVerts[i*2] = glm::vec3(0);
			//skelVerts[i*2+1] = glm::vec3(pMesh->skelMats[i][3]);
			if(skel){
				skelVerts[i*2+1] = glm::vec3(skel[i]*pMesh->skelMats[i]*glm::vec4(0,0,0,1));
			}else{
				skelVerts[i*2+1] = glm::vec3(pMesh->skelMats[i]*glm::vec4(0,0,0,1));
			}
			//skelVerts[i*2+1] = glm::vec3(glm::vec4(0,0,0,1)*pMesh->skelMats[i]);
			if(i){
				skelVerts[i*2] = skelVerts[(b.parent)*2+1];
			}
		}
		SetProg(0);
		whiteTex.Bind();
		glDisable(GL_DEPTH_TEST);
		glDisableVertexAttribArray(2);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 12, skelVerts);
		glDrawArrays(GL_LINES,0,pMesh->numBones*2);
		glEnableVertexAttribArray(2);
		glEnable(GL_DEPTH_TEST);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 12, 0);
		delete[] skelVerts;
	}
	if(skinVerts)
		delete[] skinVerts;
}

void PCCViewModel::View(pccExport_t *pExp, bool aSkeletal)
{
	skeletal = aSkeletal;
	if(!skeletal){
		previewMesh = viewer->pcc->GetStaticMesh(pExp);
		((PCCStaticMesh*)previewMesh)->LoadMaterials(viewer->pcc);
		viewer->ChangeState(ePccPreviewStMesh);
	}else{
		previewMesh = viewer->pcc->GetSkeletalMesh(pExp);
		((SkeletalMesh*)previewMesh)->LoadMaterials(viewer->pcc);
		if(showSkel){
			((SkeletalMesh*)previewMesh)->SetupSkeleton();
		}
		viewer->ChangeState(ePccPreviewSkMesh);
	}
	meshMode=(ePccMeshMode)(meshMode-1);
	RenderMode();
	yo=0;
	//ly=0;
	meshCamera.pos = glm::vec3(0,0.3,3);//st
	//meshCamera.pos = glm::vec3(0,0.2,2);
	meshCamera.rot = glm::vec3(10,0,0);
	meshRot = glm::vec2(0.0);
	glClearColor(0.6f,0.6f,0.6f,1);
}

void PCCViewModel::ViewAnim()
{
	char temp[256];
	g_fs.GetFilePath("pcc_anim.txt",temp);
	ifstream animFile(temp);
	if(!animFile){
		Log("pcc_anim.txt not found!\n");
		return;
	}

	if(testSkel){
		delete[] testSkel;
		testSkel=0;
	}
	animFile >> animMode;

	animFile >> numAnimMeshes;
	animMeshes = new SkeletalMesh*[numAnimMeshes];
	int id=0;
	for(int i=0;i<numAnimMeshes;i++){
		animFile >> temp;
		animFile >> id;
		PCCFile *p = viewer->pccSystem.GetPCC(temp);
		if(p&&id){
			animMeshes[i] = p->GetSkeletalMesh(p->exports+id);
			if(animMeshes[i]){
				animMeshes[i]->LoadMaterials(p);
				animMeshes[i]->SetupSkeleton();
				if(animMode==1&&i==0){
					if(!testSkel){
						int nb=animMeshes[i]->numBones;
						testSkel = new glm::mat4[nb*3];
						for(int j=nb; j<nb*2; j++){
							testSkel[j]=glm::mat4(1.0f);
						}
						GenerateRandomFrame(testSkel,animMeshes[i]);
					}
				}
			}
		}else{
			animMeshes[i]=0;
		}
	}
	animPos=0;
	animFile.close();
	viewer->ChangeState(ePccAnim);

	meshMode=(ePccMeshMode)(meshMode-1);
	RenderMode();
	yo=0.8f;
	//ly=0;
	meshCamera.pos = glm::vec3(0,0.3,3);
	meshCamera.rot = glm::vec3(10,0,0);
	meshRot = glm::vec2(0.0);
	glClearColor(0.6f,0.6f,0.6f,1);
}

void PCCViewModel::ReloadShaders()
{
	normalProg.CreateFromFile("generic", "normal");
	normalProg.u_mvpMtx = normalProg.GetUniformLoc("u_mvpMtx");
	normalProg.u_modelMtx = normalProg.GetUniformLoc("u_modelMtx");

	normalmapProg.CreateFromFile("normalmap", "normalmap");
	normalmapProg.u_mvpMtx = normalmapProg.GetUniformLoc("u_mvpMtx");
	normalmapProg.u_modelMtx = normalmapProg.GetUniformLoc("u_modelMtx");
	u_viewPosNm = normalmapProg.GetUniformLoc("u_viewPos");
	int u_normalmap = normalmapProg.GetUniformLoc("u_normalmap");
	normalmapProg.Use();
	glUniform1i(u_normalmap,1);

	alphaTestProg.CreateFromFile("generic","alpha_test");
	alphaTestProg.u_mvpMtx = alphaTestProg.GetUniformLoc("u_mvpMtx");
	//alphaTestProg.u_modelMtx = alphaTestProg.GetUniformLoc("u_modelMtx");
}

void PCCViewModel::Back()
{
	if(previewMesh){
		previewMesh = 0;
		if(testSkel){
			delete[] testSkel;
			testSkel=0;
		}
		viewer->ChangeState(ePccOpen);
		//ly=0;
		glClearColor(0,0,0,1);
	}
}

void PCCViewModel::RenderMode()
{
	meshMode=(ePccMeshMode)((meshMode+1)%4);
	switch(meshMode){
	case eMeshTexture:
		bRenderMode->text="Texture";
		break;
	case eMeshNormal:
		bRenderMode->text="Normal";
		break;
	case eMeshLight:
		bRenderMode->text="Shaded";
		break;
	case eMeshNormalmap:
		bRenderMode->text="Normalmap";
		break;
	}
}

void PCCViewModel::ShowSkel(){
	showSkel=!showSkel;
	if(previewMesh)
		((SkeletalMesh*)previewMesh)->SetupSkeleton();
}

void PCCViewModel::Export(treeNode_t *selectedNode)
{
	Log("Export mesh: %s\n",viewer->pcc->GetObjName(selectedNode->id));
	Model *mdl = ((PCCStaticMesh*)previewMesh)->ToModel(viewer->pcc);
	ExportSMD(viewer->pcc->GetObjName(selectedNode->id),"me3/",1,mdl);
}

PCCViewLevel::PCCViewLevel(){}

PCCViewLevel::~PCCViewLevel()
{
	if(previewLevel)
		delete previewLevel;
	if(testLevels){
		for(int i=0;i<numTestLevels;i++){
			delete testLevels[i];
		}
		delete[] testLevels;
	}
}

void PCCViewLevel::Init(PCCViewer *pViewer, std::vector<Button *> &buttons)
{
	viewer = pViewer;
	bBack = EditorButtonT<PCCViewLevel>(0.64,0.02,0.34,0.07,"Back",this,&PCCViewLevel::Back);
	bExport = EditorButtonT<PCCViewLevel>(0.52,0.84,0.38,0.07,"Export",this,&PCCViewLevel::Export);
	buttons.push_back(&bBack);
	buttons.push_back(&bExport);

	previewLevel = 0;

	testLevels = 0;
	numTestLevels = 0;

	zfar = 23000;
	levelMtx = glm::scale( glm::rotate( glm::mat4(1),glm::radians(90.0f), glm::vec3(-1.0f, 0, 0)),glm::vec3(-0.01f,0.01f,0.01f));
}

void PCCViewLevel::Activate()
{
	bBack.active = true;
	bExport.active = true;
}

void PCCViewLevel::Resize(float aspect)
{
	levelCamera.UpdateProj(90,aspect,0.1f,zfar);
}

void PCCViewLevel::Draw()
{
	float speed=40;
	glm::vec3 moveVec = viewer->velocity;
	if(glm::length(moveVec)>0.5f){
		moveVec*=pow(glm::length(moveVec)*2.0f,3.0f);
	}
	levelCamera.pos += glm::mat3(glm::inverse(levelCamera.viewMtx))*moveVec*speed*(float)deltaTime;
	levelCamera.UpdateView();
	levelCamera.UpdateFrustum();
	SetCamera(&levelCamera);
	/*SetProg(&lightmapProg);
	glUniform3fv(u_viewPos,1,&levelCamera.pos.x);
	SetProg(0);*/

	if(viewer->state == ePccPreviewLevel)
		DrawLevel(previewLevel);
	else{
		for(int i=0;i<numTestLevels;i++){
			if(!testLevels[i])
				continue;
			DrawLevel(testLevels[i]);
		}
	}

	SetCamera(0);

	char tstr[256];
	snprintf(tstr,256,"camera pos: (%.2f,%.2f,%.2f) vel len %f",levelCamera.pos.x,levelCamera.pos.y,levelCamera.pos.z,glm::length(viewer->velocity));
	DrawText(tstr,0.02,0.92,0.3);
}

void PCCViewLevel::Rotate(float x, float y)
{
	levelCamera.rot.x+=x;
	levelCamera.rot.y+=y;
}

void PCCViewLevel::ReloadShaders()
{
	lightmapProg.CreateFromFile("lightmap","lightmap");
	lightmapProg.u_mvpMtx = lightmapProg.GetUniformLoc("u_mvpMtx");
	//lightmapProg.u_modelMtx = lightmapProg.GetUniformLoc("u_modelMtx");
	u_lmScaleBias = lightmapProg.GetUniformLoc("u_lmScaleBias");
	u_lmColScale = lightmapProg.GetUniformLoc("u_lmColScale");
	u_lmColBias = lightmapProg.GetUniformLoc("u_lmColBias");
	//u_viewPos = lightmapProg.GetUniformLoc("u_viewPos");
	int u_lightmap = lightmapProg.GetUniformLoc("u_lightmap");
	lightmapProg.Use();
	glUniform4f(u_lmColScale,1,1,1,1);
	glUniform1i(u_lightmap,2);

	lightmapDirProg.CreateFromFile("lightmap","lightmap_dir");
	lightmapDirProg.u_mvpMtx = lightmapDirProg.GetUniformLoc("u_mvpMtx");
	u_lmdScaleBias = lightmapDirProg.GetUniformLoc("u_lmScaleBias");
	u_lightmap = lightmapDirProg.GetUniformLoc("u_lightmap");
	int u_lightmapDir = lightmapDirProg.GetUniformLoc("u_lightmapDir");
	lightmapDirProg.Use();
	glUniform1i(u_lightmap,2);
	glUniform1i(u_lightmapDir,3);

	lightmapVertProg.CreateFromFile("lightmap_vert","lightmap_vert");
	lightmapVertProg.u_mvpMtx = lightmapVertProg.GetUniformLoc("u_mvpMtx");
	//lightmapVertProg.u_modelMtx = lightmapVertProg.GetUniformLoc("u_modelMtx");
	a_lmDir = lightmapVertProg.GetAttribLoc("a_lmDir");
}

void PCCViewLevel::Back()
{
	if(previewLevel){
		delete previewLevel;
		previewLevel = 0;
	}
	viewer->ChangeState(ePccOpen);
	glClearColor(0,0,0,1);
}

void PCCViewLevel::Export()
{
	Log("Export level %s: num objs %d\n",previewLevel->pcc->pccName.c_str(),previewLevel->objectsCount);

	char path[1024];
	string folder = string("export/maps/");
	string fileName = previewLevel->pcc->pccName;
	fileName = fileName.substr(fileName.find("/"));
	fileName = fileName.substr(0,fileName.find("."));
	g_fs.GetFilePath((folder+fileName+".vmf").c_str(),path,true);
	ofstream vmfFile(path);
	if(!vmfFile){
		Log( "Can't open output file %s\n", path);
		return;
	}

	vmfFile<<
	"versioninfo\n"
	"{\n"
	"	\"formatversion\" \"100\"\n"
	"	\"prefab\" \"1\"\n"
	"}\n";
	vmfFile
	<<"visgroups\n"
	<<"{\n"
	<<"}\n";
	vmfFile
	<<"viewsettings\n"
	<<"{\n"
	<<"}\n";
	vmfFile<<
	"world\n"
	"{\n"
	"	\"id\" \"1\"\n"
	"	\"classname\" \"worldspawn\"\n";
/*
	"	solid\n"
	"	{\n"
	"	}\n"
	"	group\n"
	"	{\n"
	"	}\n"
*/
	vmfFile<<
	"}\n";

/*	entity
	{
		"id" "24"
		"classname" "light"
		"_light" "255 255 255 200"
		"_lightHDR" "-1 -1 -1 1"
		"_lightscaleHDR" "1"
		"_quadratic_attn" "1"
		"origin" "256 33 240"
		editor
		{
			"color" "220 30 220"
			"visgroupshown" "1"
			"visgroupautoshown" "1"
			"logicalpos" "[0 1500]"
		}
	}*/
/*	entity
	{
		"id" "39"
		"classname" "prop_static"
		"angles" "0 0 0"
		"fademindist" "-1"
		"fadescale" "1"
		"model" "models/props_canal/bridge_pillar02.mdl"
		"skin" "0"
		"solid" "6"
		"origin" "416 160 33"
		editor
		{
			"color" "255 255 0"
			"visgroupshown" "1"
			"visgroupautoshown" "1"
			"logicalpos" "[0 3000]"
		}
	}*/
	for(int i=0; i<previewLevel->objectsCount;i++){
		if(!previewLevel->objects[i])
			continue;
		Log("Object %d: objId %d, name %s, type %s\n",i,previewLevel->objects[i]->objId,previewLevel->pcc->GetObjName(previewLevel->objects[i]->objId),previewLevel->pcc->GetClassName(previewLevel->objects[i]->objId));
		if(previewLevel->objects[i]->type==eStaticMeshCollectionActor){
			StaticMeshCollectionActor *smca = (StaticMeshCollectionActor *)previewLevel->objects[i];
			Log("SMCA: obj count %d\n",smca->objectsCount);
			for(int j=0; j<smca->objectsCount; j++){
				if(smca->pMeshes[j]->pMesh){
					const char *modelName = previewLevel->pcc->GetObjName(smca->pMeshes[j]->pMesh->objId);
					Log("mesh %d: %d %s %s\n",j,smca->objectsIdxs[j],previewLevel->pcc->GetObjName(smca->objectsIdxs[j]),modelName);

					glm::mat4 mtx = smca->matrices[j];
					glm::vec3 pos = glm::vec3(mtx[3]);
					glm::vec3 rot = glm::eulerAngles(glm::quat(glm::mat3(mtx)))*(180.0f/glm::pi<float>());
					rot.z -= 90;

					vmfFile<<
					"entity\n"
					"{\n";
					if(smca->pMeshes[j]->scale!=glm::vec3(1.0f)){
						vmfFile<<
						"	\"classname\" \"prop_dynamic\"\n"
						"	\"modelscale\" \""<<smca->pMeshes[j]->scale.x<<"\"\n";
					}else{
						vmfFile<<
						"	\"classname\" \"prop_static\"\n";
					}
					vmfFile<<
					//"	\"angles\" \""<<rot.x<<" "<<rot.y<<" "<<rot.z<<"\"\n"
					"	\"angles\" \""<<rot.y<<" "<<rot.z<<" "<<rot.x<<"\"\n"<<
					"	\"model\" \"models/me3/"<<modelName<<".mdl\"\n"
					"	\"origin\" \""<<pos.x<<" "<<pos.y<<" "<<pos.z<<"\"\n"
					"}\n";
				}
			}
		}
	}
	vmfFile.close();
}

void PCCViewLevel::LoadLevelAsync(){
	viewer->ChangeState(ePccLoading);
	pthread_create(&loadingThread, NULL, LoadLevelThread, (void*)viewer);
}

void PCCViewLevel::LoadLevel(){
	viewer->ChangeState(ePccLoading);
	bool ret = ViewLevel(viewer->pccOpen.selectedNode->id);

	if(ret){
		viewer->ChangeState(ePccPreviewLevel);
	}else{
		Log("Load level error\n");
		viewer->ChangeState(ePccOpen);
	}
}


bool PCCViewLevel::ViewLevel(int idx)
{
	SetLogMode(eLogToFile);
	previewLevel = viewer->pcc->GetLevel(viewer->pccOpen.selectedNode->id);
	SetLogMode(eLogBoth);
	if(!previewLevel)
		return false;

	SetupCamera(previewLevel);

	if(pccFiles[selectedPccFile].find("BioA_ProEar_")!=string::npos){
		levelCamera.pos = glm::mat3(levelMtx)*glm::vec3(-63000,30000,15000);
	}
	levelCamera.UpdateView();

	return true;
}
