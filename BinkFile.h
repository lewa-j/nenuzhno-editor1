
//Minimalist bink video decoder
//Thanks to ffmpeg

#pragma once

#include <istream>
#include "stdint.h"
#include "vlc.h"

#define BINK_16BIT 1

struct binkHeader_t
{
	int length;
	int frames;
	int maxFrameSize;
	int frames2;//?
	int width;
	int height;
	int fps;
	int fpsDivider;
	int flags;
	int audioTrackNum;
};

enum binkSources {
    BINK_SRC_BLOCK_TYPES = 0, ///< 8x8 block types
    BINK_SRC_SUB_BLOCK_TYPES, ///< 16x16 block types (a subset of 8x8 block types)
    BINK_SRC_COLORS,          ///< pixel values used for different block types
    BINK_SRC_PATTERN,         ///< 8-bit values for 2-colour pattern fill
    BINK_SRC_X_OFF,           ///< X components of motion value
    BINK_SRC_Y_OFF,           ///< Y components of motion value
    BINK_SRC_INTRA_DC,        ///< DC values for intrablocks with DCT
    BINK_SRC_INTER_DC,        ///< DC values for interblocks with DCT
    BINK_SRC_RUN,             ///< run lengths for special fill block

    BINK_NB_SRC
};

enum BlockTypes {
    SKIP_BLOCK = 0, ///< skipped block
    SCALED_BLOCK,   ///< block has size 16x16
    MOTION_BLOCK,   ///< block is copied from previous frame with some offset
    RUN_BLOCK,      ///< block is composed from runs of colours with custom scan order
    RESIDUE_BLOCK,  ///< motion block with some difference added
    INTRA_BLOCK,    ///< intra DCT block
    FILL_BLOCK,     ///< block is filled with single colour
    INTER_BLOCK,    ///< motion block with DCT applied to the difference
    PATTERN_BLOCK,  ///< block is filled with two colours following custom pattern
    RAW_BLOCK,      ///< uncoded 8x8 block
};

struct binkTree
{
	int vlcNum;
	uint8_t syms[16];
};

struct binkBundle
{
	int len;
	binkTree tree;
	uint8_t *data;
	uint8_t *curDec;
	uint8_t *curPtr;
};

class BitReader
{
public:
	BitReader(){}
	BitReader(uint8_t *nb, int n);
	int GetBit();
	int GetBits(int n);
	uint32_t GetBitsCount();
	void SkipBitsLong(int n);
	int GetVlc(int16_t (*table)[2], int bits, int maxDepth);
	
	uint8_t *b;
	int s;
	int p;
};

class BinkFile
{
public:
	BinkFile();
	~BinkFile();
	
	bool Open(const char *fileName);
	bool GetNextFrame(char *b, float scaling=1.0f);
	bool SkipFrames(int i);
	
	std::istream *in;
	char ident[5];
	binkHeader_t header;
	uint32_t *frameOffsets;
	int curFrame;
	char *codedBuffer;
	uint8_t *planesBuff;
	uint8_t *planes[6];
	BitReader br;
	binkBundle bundles[BINK_NB_SRC];
	binkTree colHigh[16];
	int colLastVal;
	VLC binkTrees[16];
	
	void InitBundles();
	void FreeBundles();
	void InitLengths(int width, int bw);
	int DecodePlane(uint8_t *b,uint8_t *p,int idx,int isChroma);
	void ReadBundle(int idx);
	void ReadTree(binkTree *tree);
	void Merge(uint8_t *dst,uint8_t *src, int size);
	
	int GetHuff(binkTree *t);
	int GetValue(int idx);
	
	int ReadBlockTypes(binkBundle *bundle);
	int ReadColors(binkBundle *bundle);
	int ReadPatterns(binkBundle *bundle);
	int ReadMotionValues(binkBundle *bundle);
	int ReadDcs(binkBundle *bundle,int hasSign);
	int ReadRuns(binkBundle *bundle);
	
	int ReadDctCoeffs(int32_t *block, const uint8_t *scan,const int32_t quant_matrices[16][64], int q);
	void IdctPut(uint8_t *dst, int stride, int32_t *block);
	void IdstAdd(uint8_t *dst, int stride, int32_t *block);
};

