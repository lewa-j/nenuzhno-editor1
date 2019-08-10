
#pragma once

#include "PCCTexture2D.h"
#include "PCCObject.h"

enum eBlendMode{
	eBlendOpaque,
	eBlendMasked,
	eBlendTranslucent,
	eBlendAdditive
};

class PCCMaterial: public PCCObject
{
public:
	PCCMaterial(int id){
		objId=id;
		expressionTextures=0;
		numExpressionTextures=0;
		diffuseTex=0;
		normalmapTex=0;
		blendMode = eBlendOpaque;
	}
	~PCCMaterial(){
		if(expressionTextures)
			delete[] expressionTextures;
	}
	
	int *expressionTextures;
	int numExpressionTextures;
	PCCTexture2D *diffuseTex;
	PCCTexture2D *normalmapTex;
	eBlendMode blendMode;
};

/*
MaterialInstanceConstant

VectorParameterValues ArrayProperty, size 548
ScalarParameterValues ArrayProperty, size 584
TextureParameterValues ArrayProperty, size 352
ParentLightingGuid StructProperty, size 16
PhysMaterial ObjectProperty
Parent ObjectProperty
bHasStaticPermutationResource BoolProperty
m_Guid StructProperty, size 16
LightingGuid StructProperty, size 16
*/
