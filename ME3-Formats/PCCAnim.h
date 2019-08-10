
#pragma once

#include "PCCObject.h"

class BioAnimSetData: public PCCObject
{
public:
	BioAnimSetData();

	const char **trackBoneNames;
	int numTrackBoneNames;
	bool bAnimRotationOnly;
};

class AnimSet: public PCCObject
{
public:
	AnimSet();
	
	int numSeqs;
	BioAnimSetData *animSetData;
};

