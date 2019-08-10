
#include "engine.h"
#include "LevelRenderer.h"
#include "PCCViewer.h"

LevelRenderer::LevelRenderer(){
	
}

void LevelRenderer::SetLevel(PCCLevel *lvl){
	level = lvl;
}

void LevelRenderer::Draw(Camera *cam){
	
}

using namespace std;
#include "log.h"

extern glm::mat4 levelMtx;
extern float zfar;

int lastLmType=0;

void PCCViewLevel::DrawStaticMeshComponent(StaticMeshComponent *pSMC){
	if(!pSMC->pMesh->LODs[0].numIndices)
		return;

	int flags=0;
	if(lastLmType!=pSMC->lightmapType){
		if(pSMC->lightmapType==1){
			SetProg(&lightmapVertProg);
		}else if(pSMC->lightmapType==2){
			SetProg(&lightmapDirProg);
			//glEnableVertexAttribArray(3);
		}else if(pSMC->lightmapType==4){
			SetProg(&lightmapProg);
		}else{
			SetProg(0);
			//glDisableVertexAttribArray(3);
		}
	}
	lastLmType = pSMC->lightmapType;

	if(pSMC->lightmapType==1){
		flags=eMeshNorm;
		//glEnableVertexAttribArray(1);//norm
		glEnableVertexAttribArray(3);//uv2
		glEnableVertexAttribArray(a_lmDir);
		glVertexAttribPointer(3, 4, GL_UNSIGNED_BYTE, GL_TRUE, 8, pSMC->lm1DVerts);//uv2
		glVertexAttribPointer(a_lmDir, 4, GL_UNSIGNED_BYTE, GL_TRUE, 8, pSMC->lm1DVerts+4);//lmDir
	}else if(pSMC->lightmapType==2){
		flags=eMeshUV2;//Norm|Tan//7
		glUniform4fv(u_lmdScaleBias,1,(float*)&pSMC->lmScaleAndBias);
		glActiveTexture(GL_TEXTURE2);
		if(pSMC->lmColTex->GetGLTexture(&whiteTex,texLod))
			pSMC->lmColTex->GetGLTexture()->Bind();
		glActiveTexture(GL_TEXTURE3);
		if(pSMC->lmDirTex->GetGLTexture(&whiteTex,texLod))
			pSMC->lmDirTex->GetGLTexture()->Bind();
		glActiveTexture(GL_TEXTURE0);
		//glEnableVertexAttribArray(1);//norm
		glEnableVertexAttribArray(3);//uv2
		//glEnableVertexAttribArray(4);//tan
	}else if(pSMC->lightmapType==3){

	}else if(pSMC->lightmapType==4){
		flags=eMeshUV2;
		glUniform4fv(u_lmScaleBias,1,(float*)&pSMC->lmScaleAndBias);
		glUniform4fv(u_lmColScale,1,(float*)&pSMC->lmColScale);
		glUniform4fv(u_lmColBias,1,(float*)&pSMC->lmColBias);
		glActiveTexture(GL_TEXTURE2);
		if(pSMC->lmColTex->GetGLTexture(&whiteTex,texLod))
			pSMC->lmColTex->GetGLTexture()->Bind();
		glActiveTexture(GL_TEXTURE0);
		glEnableVertexAttribArray(3);//uv2
	}
	
	flags |= eMeshVBO;

	viewer->pccViewModel.DrawStaticMesh(pSMC->pMesh,pSMC->pMaterials,flags);

	if(pSMC->lightmapType==1||pSMC->lightmapType==2||pSMC->lightmapType==3||pSMC->lightmapType==4){
		//glDisableVertexAttribArray(1);//norm
		glDisableVertexAttribArray(3);//uv2
		//glDisableVertexAttribArray(4);//tan
		glDisableVertexAttribArray(a_lmDir);
	}
}

void PCCViewLevel::SetupCamera(PCCLevel *lvl)
{
	if(!lvl)
		EngineError("SetupCamera without Level");
	glm::vec3 bbsize = lvl->bbmax-lvl->bbmin;
	levelCamera.pos = lvl->bbmin+bbsize*0.5f;
	levelCamera.pos.y = lvl->bbmax.y;//+bbsize.x*0.4f;

	levelCamera.pos = glm::mat3(levelMtx)*levelCamera.pos;
	//levelCamera.rot.x = 90.0f;
	levelCamera.rot=glm::vec3(0,180.0f,0);
	lastRot=levelCamera.rot;
	levelCamera.UpdateView();
	levelCamera.UpdateProj(90,aspect,0.1f,zfar);

	Log("SetupCamera: pos (%f,%f,%f)\n",levelCamera.pos.x,levelCamera.pos.y,levelCamera.pos.z);
}

void PCCViewLevel::DrawLevel(PCCLevel *pLvl)
{
	if(!pLvl){
		Log("DrawLevel: pLvl is NULL");
		return;
	}

	//glCullFace(GL_FRONT);
	glDisable(GL_CULL_FACE);

	GLfloat vertices[] =
	{
		 0.5f, -0.5f,  0.5f,
		 0.5f, -0.5f, -0.5f,
		-0.5f, -0.5f, -0.5f,
		-0.5f, -0.5f,  0.5f,
		 0.5f, 0.5f,  0.5f,
		 0.5f, 0.5f, -0.5f,
		-0.5f, 0.5f, -0.5f,
		-0.5f, 0.5f,  0.5f
	};
	int inds[] =
	{
		0,1, 1,2, 2,3, 3,0,
		4,5, 5,6, 6,7, 7,4,
		0,4, 1,5, 2,6, 3,7
	};

	whiteTex.Bind();

	glm::vec3 bbsize = pLvl->bbmax-pLvl->bbmin;
	glm::mat4 mtx = glm::mat4(1.0);
	mtx = glm::translate(mtx,pLvl->bbmin+bbsize*0.5f);
	mtx = glm::scale(mtx,bbsize);
	SetModelMtx(levelMtx*mtx);

	glDisableVertexAttribArray(2);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 12, vertices);
	glDrawElements(GL_LINES, 24, GL_UNSIGNED_INT, inds);
	glEnableVertexAttribArray(2);

	lastLmType=0;

	for(int i=0;i<pLvl->objectsCount;i++){
		if(pLvl->objects[i]){
			if(pLvl->objects[i]->type==eStaticMeshActor){
				StaticMeshActor *sma = (StaticMeshActor*)pLvl->objects[i];
				mtx = sma->modelMtx;
				if(sma->pStatMeshComp&&sma->pStatMeshComp->pMesh){
					glm::vec3 bp=sma->pStatMeshComp->pMesh->bounding.pos;
					glm::vec3 bhs=sma->pStatMeshComp->pMesh->bounding.size;
					BoundingBox bbox(bp-bhs,bp+bhs);
					if(levelCamera.frustum.Contains(bbox,levelMtx*mtx)){
						SetModelMtx(levelMtx*mtx);
						//DrawStaticMesh(sma->pStatMeshComp->pMesh);
						DrawStaticMeshComponent(sma->pStatMeshComp);
					}
				}else{
					whiteTex.Bind();
					mtx = glm::scale(mtx,glm::vec3(100.0f));
					SetModelMtx(levelMtx*mtx);
					glDisableVertexAttribArray(2);
					glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 12, vertices);
					glDrawElements(GL_LINES, 24, GL_UNSIGNED_INT, inds);
					glEnableVertexAttribArray(2);
				}
			}else if(pLvl->objects[i]->type==eStaticMeshCollectionActor){
				StaticMeshCollectionActor *smca = (StaticMeshCollectionActor*)pLvl->objects[i];
				for(int j=0;j<smca->objectsCount;j++){
					mtx = smca->matrices[j];
					if(smca->pMeshes[j]&&smca->pMeshes[j]->pMesh){
						glm::vec3 bp=smca->pMeshes[j]->pMesh->bounding.pos;
						glm::vec3 bhs=smca->pMeshes[j]->pMesh->bounding.size;
						BoundingBox bbox(bp-bhs,bp+bhs);
						if(levelCamera.frustum.Contains(bbox,levelMtx*mtx)){
							SetModelMtx(levelMtx*mtx);
							//DrawStaticMesh(smca->pMeshes[j]->pMesh);
							DrawStaticMeshComponent(smca->pMeshes[j]);
						}
					}else{
						whiteTex.Bind();
						mtx = glm::scale(mtx,glm::vec3(100.0f));
						SetModelMtx(levelMtx*mtx);
						glDisableVertexAttribArray(2);
						glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 12, vertices);
						glDrawElements(GL_LINES, 24, GL_UNSIGNED_INT, inds);
						glEnableVertexAttribArray(2);
					}
				}
			}else{
				//other object
			}
		}
	}

	SetProg(0);

	//glCullFace(GL_BACK);

}
