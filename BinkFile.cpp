
//Minimalist bink video decoder
//Thanks to ffmpeg

#include "log.h"
#include "BinkFile.h"
#include <cstring>
#include "math.h"
#include <fstream>
using namespace std;

#include "BinkData.h"

BinkFile::BinkFile()
{
	in=0;
	frameOffsets=0;
	curFrame=0;
	codedBuffer=0;
	planesBuff=0;
	selectedTrack = 0;
}

BinkFile::~BinkFile()
{
	if(in)
		delete in;
	if(frameOffsets)
		delete[] frameOffsets;
	if(codedBuffer)
		delete[] codedBuffer;
	delete[] planesBuff;
	FreeBundles();
}

bool BinkFile::Open(const char *fileName)
{
	in = new ifstream(fileName, ios::binary);
	if(!in||!*in){
		Log("Error: can't open %s\n",fileName);
		return false;
	}
	Log("Loading: %s\n",fileName);
	
	in->read(ident,4);
	ident[4]=0;
	in->read((char*)&header,sizeof(binkHeader_t));
	Log("ident %s, length %d, frames %d, bigest frame size %d, size %dx%d, fps %d divider %d, flags %X, audio tracks count %d\n",
		ident,header.length,header.frames,header.maxFrameSize,header.width,header.height, header.fps,header.fpsDivider,header.flags,header.audioTrackNum);
	if(header.fpsDivider!=1){
		header.fps/=header.fpsDivider;
		Log("Divided fps %d\n",header.fps);
	}
	tracks.resize(header.audioTrackNum);
	if(header.audioTrackNum){
		uint32_t tbuff[header.audioTrackNum];
		//for
		//	4b max decoded size
		in->read((char*)tbuff,header.audioTrackNum*4);
		for(uint32_t i=0; i<header.audioTrackNum; i++){
			tracks[i].maxPacketSize = tbuff[i];
		}
		//for
		//	2b sample_rate
		//	2b flags
		struct{
			uint16_t rate;
			uint16_t flags;
		} tr[header.audioTrackNum]={0};
		in->read((char*)tr,header.audioTrackNum*4);
		for(uint32_t i=0; i<header.audioTrackNum; i++){
			tracks[i].rate = tr[i].rate;
			tracks[i].flags = tr[i].flags;
		}
		//for
		//	4b ids
		in->read((char*)tbuff,header.audioTrackNum*4);
		for(uint32_t i=0; i<header.audioTrackNum; i++){
			tracks[i].id = tbuff[i];
		}
		
		for(size_t i=0; i<tracks.size(); i++){
			if(i==selectedTrack)
				InitAudio(tracks[i]);
			Log("track %d: sr %d f %X ms %d id %d\n",i,tracks[i].rate, tracks[i].flags, tracks[i].maxPacketSize, tracks[i].id);
		}
		
		//int skipSize=12*header.audioTrackNum;
		//in->seekg(4+sizeof(binkHeader_t)+skipSize);
	}
	frameOffsets = new uint32_t[header.frames+1];
	in->read((char*)frameOffsets,(header.frames+1)*4);
	/*Log("frame offsets:\n");
	for(int i=0;i<header.frames+1;i++)
	{
		Log("%d: %d\n",i,frameOffsets[i]&~1);
		if(frameOffsets[i]&1)
			Log("keyframe\n");
	}*/
	in->seekg(4,ios::cur);
	//in.close();

	curFrame=0;
	codedBuffer = new char[header.maxFrameSize];
	int p0s=header.width*header.height;
	int p1s=(header.width>>1)*(header.height>>1);
	planesBuff = new uint8_t[p0s*2+p1s*4];
	planes[0] = planesBuff;
	planes[1] = planesBuff+p0s;
	planes[2] = planesBuff+p0s+p1s;
	planes[3] = planes[2]+p1s;
	planes[4] = planes[3]+p0s;
	planes[5] = planes[4]+p1s;
	
	//decode init
	static int16_t table[16 * 128][2];
	//if(!binkTrees[15].table)
	{
		for(int i=0;i<16;i++){
			const int maxbits = bink_tree_lens[i][15];
            binkTrees[i].table = table + i*128;
            binkTrees[i].table_allocated = 1 << maxbits;
            //binkTrees[i].Init(maxbits, 16, bink_tree_lens[i], 1, 1,bink_tree_bits[i], 1, 1, INIT_VLC_USE_NEW_STATIC | INIT_VLC_LE);
			init_vlc(&binkTrees[i], maxbits, 16,
                     bink_tree_lens[i], 1, 1,
                     bink_tree_bits[i], 1, 1, INIT_VLC_USE_NEW_STATIC | INIT_VLC_LE);
		}
	}
	
	InitBundles();
	
	return true;
}

int clamp(int v, int a, int b){
	return max(v,min(a,b));
}

void YuvToRgb(uint8_t *dst,int y,int cr,int cb){
	dst[0] = clamp(y + (1.370705 * (cr-128)),0,255);
    dst[1] = clamp(y - (0.698001 * (cr-128)) - (0.337633 * (cb-128)),0,255);
    dst[2] = clamp(y + (1.732446 * (cb-128)),0,255);
}

void YuvToRgb565(uint8_t *dst,int y,int cr,int cb){
	uint16_t temp = (clamp(y + (1.370705 * (cr-128)),0,255)<<8)&0b1111100000000000;
    temp |= (clamp(y - (0.698001 * (cr-128)) - (0.337633 * (cb-128)),0,255)<<3)&0b0000011111100000;
    temp |= (clamp(y + (1.732446 * (cb-128)),0,255)>>3);
	*(int16_t*)dst = temp;
}

bool BinkFile::GetNextFrame(char *b, float scaling)
{
	//TestBitReader();
	if(curFrame>=header.frames)
		curFrame = 0;

	//bool isKeyFrame = frameOffsets[curFrame]&1;
	//if(isKeyFrame)
	//	Log("Frame %d is key\n",curFrame);

	int frameSize = (frameOffsets[curFrame+1] & ~1) - (frameOffsets[curFrame] & ~1);
	in->seekg(frameOffsets[curFrame] & ~1);
	in->read(codedBuffer, frameSize);

	//Log("frame size %d\n",frameSize);

	char *t = codedBuffer;
	for(uint32_t i=0;i<header.audioTrackNum;i++){
		uint32_t audioTrackLen = *(uint32_t*)t;
		if(i==selectedTrack && audioTrackLen)
			DecodeAudio((uint8_t*)t,tracks[i]);
		t+=4;
		//Log("audio track %d lenght: %d\n",i,audioTrackLen);
		t += audioTrackLen;
		frameSize -= 4+audioTrackLen;
	}

	uint32_t plane = *(uint32_t*)t;
	t+=4;
	//Log("plane %d\n",plane);
	br = BitReader((uint8_t*)t, frameSize<<3);
	//br.SkipBitsLong(32);//if ver >='i'
	
	DecodePlane(planes[0],planes[3],0,0);
	if((plane-4)!=br.GetBitsCount()/8)
	{
		Log("plane %d, readed %d\n",plane, br.GetBitsCount()/8);
		br.p=plane*8;
	}
	DecodePlane(planes[1],planes[4],1,1);
	DecodePlane(planes[2],planes[5],2,1);
	
	//TODO: convert yuv to rgb
	int w=header.width;
	int y,cr,cb;
#if 0
	for(int i=0;i<header.height;i++){
		for(int j=0;j<w;j++){
			y=planes[0][i*w+j];
			cr=planes[1][(i>>1)*(w>>1)+(j>>1)];
			cb=planes[2][(i>>1)*(w>>1)+(j>>1)];
			YuvToRgb((uint8_t*)b+(i*w+j)*3,y,cr,cb);
			/*b[(i*w+j)*3]=cb;
			b[(i*w+j)*3+1]=cb;
			b[(i*w+j)*3+2]=cb;*/
		}
	}
#endif
	int sh=header.height*scaling, sw=w*scaling;
	for(int i=0;i<sh;i++){
		for(int j=0;j<sw;j++){
			int usi=i/scaling, usj=j/scaling;
			y=planes[0][usi*w+usj];
			cr=planes[1][(usi>>1)*(w>>1)+(usj>>1)];
			cb=planes[2][(usi>>1)*(w>>1)+(usj>>1)];
#ifdef BINK_16BIT
			YuvToRgb565((uint8_t*)b+(i*sw+j)*2,y,cr,cb);
#else
			YuvToRgb((uint8_t*)b+(i*sw+j)*3,y,cr,cb);
#endif
			/*b[(i*w+j)*3]=cb;
			b[(i*w+j)*3+1]=cb;
			b[(i*w+j)*3+2]=cb;*/
		}
	}
	
	//swap planes
	{
		uint8_t *temp;
		temp=planes[0];planes[0]=planes[3];planes[3]=temp;
		temp=planes[1];planes[1]=planes[4];planes[4]=temp;
		temp=planes[2];planes[2]=planes[5];planes[5]=temp;
	}
	
	curFrame++;
	return true;
}

bool BinkFile::SkipFrames(int idx)
{
	if(idx>=header.frames){
		curFrame=0;
		return 0;
	}
	//for(int i=curFrame;i<=idx;i++)
	for(int i=idx;i>curFrame;i--)
	{
		if(frameOffsets[i]&(1<<31)){
			curFrame=i;
			return 1;
		}
	}
	curFrame=idx;
	return 1;
}

#ifndef zero_extend
static inline const unsigned zero_extend(unsigned val, unsigned bits)
{
    return (val << ((8 * sizeof(int)) - bits)) >> ((8 * sizeof(int)) - bits);
}
#endif
#ifdef ANDROID
const uint8_t ff_log2_tab[256]={
        0,0,1,1,2,2,2,2,3,3,3,3,3,3,3,3,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
        5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,
        6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
        6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
        7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
        7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
        7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
        7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7
};
static inline const int log2(unsigned int v)
{
    int n = 0;
    if (v & 0xffff0000) {
        v >>= 16;
        n += 16;
    }
    if (v & 0xff00) {
        v >>= 8;
        n += 8;
    }
    n += ff_log2_tab[v];

    return n;
}
#endif

#define AV_RL32(x)                                \
    (((uint32_t)((const uint8_t*)(x))[3] << 24) |    \
               (((const uint8_t*)(x))[2] << 16) |    \
               (((const uint8_t*)(x))[1] <<  8) |    \
                ((const uint8_t*)(x))[0])

BitReader::BitReader(uint8_t *nb, int n){
	b=nb;
	s=n;
	p=0;
}
int BitReader::GetBit(){
	uint8_t res = (b[p>>3]>>(p&7))&1;
	p++;
	return res;
}
int BitReader::GetBits(int n)
{
	register int tmp;
   	unsigned int re_cache = AV_RL32(b+(p>>3))>>(p&7);
	tmp = zero_extend(re_cache, n);
	p += n;
	return tmp;
}
float BitReader::GetFloat()
{
	int power = GetBits(5);
    float f = ldexpf(GetBits(23), power - 23);
    if(GetBit())
        f = -f;
    return f;
}
uint32_t BitReader::GetBitsCount(){
	return p;
}
void BitReader::SkipBitsLong(int n){
	p += n;
}
void BitReader::Align32()
{
    int n = (-GetBitsCount()) & 31;
    if (n) SkipBitsLong(n);
}
int BitReader::GetVlc(int16_t (*table)[2], int bits, int maxDepth){
	int code;
	unsigned int re_cache = AV_RL32(b+(p>>3))>>(p&7);
	
	//do {                                                        
        int n;
		//int nb_bits;                                         
        unsigned int index;                                     
                                                                
        index = zero_extend(re_cache, bits);
	    //LOG("GelVlc: bits %d, index %d\n",bits,index);
        code  = table[index][0];                                
        n     = table[index][1];
        //LOG("GelVlc: code %d, n %d\n",code,n);
        /*if (maxDepth > 1 && n < 0) {
            p += bits;                     
            re_cache = AV_RL32(b+(p>>3))>>(p&7);                             
                                                                
            nb_bits = -n;                                       
                                                                
            index = zero_extend(re_cache, nb_bits) + code;       
            code  = table[index][0];                            
            n     = table[index][1];                            
            if (maxDepth > 2 && n < 0) {                       
                p += nb_bits;              
                re_cache = AV_RL32(b+(p>>3))>>(p&7);                         
                                                                
                nb_bits = -n;                                   
                                                                
                index = zero_extend(re_cache, nb_bits) + code;   
                code  = table[index][0];                        
                n     = table[index][1];                        
            }                                                   
        }*/
		re_cache >>= n;
		p += n;
    //} while (0);
	
	return code;
}

void TestBitReader()
{
	int a=77;//01001101
	BitReader br((uint8_t*)&a,8);
	LOG("%d is:\n",a);
	//LOG("%d%d%d%d%d%d%d%d\n",br.GetBit(),br.GetBit(),br.GetBit(),br.GetBit(),br.GetBit(),br.GetBit(),br.GetBit(),br.GetBit());
	LOG("%d %d %d\n",br.GetBits(3),br.GetBits(2),br.GetBits(3));
}

const uint16_t ff_wma_critical_freqs[25] = {
      100,   200,  300,  400,  510,  630,   770,   920,
     1080,  1270, 1480, 1720, 2000, 2320,  2700,  3150,
     3700,  4400, 5300, 6400, 7700, 9500, 12000, 15500,
    24500,
};

static const uint8_t rle_length_tab[16] = {
    2, 3, 4, 5, 6, 8, 9, 10, 11, 12, 13, 14, 15, 16, 32, 64
};

static float quantTable[96];

void BinkFile::InitAudio(BinkAudioTrack &t)
{
	t.channels = t.flags & BINK_AUD_STEREO ? 2:1;
	
	int frameLenBits;
	if (t.rate < 22050){
        frameLenBits = 9;
    }else if (t.rate < 44100){
        frameLenBits = 10;
    }else{
        frameLenBits = 11;
    }
	t.frameLen = 1 << frameLenBits;
	//dct
	t.root = t.frameLen / (sqrt(t.frameLen) * 32768.0);
	
	int rateHalf = (t.rate+1) / 2;
	for(t.bandsCount=1; t.bandsCount<25; t.bandsCount++)
	{
		if(rateHalf <= ff_wma_critical_freqs[t.bandsCount-1])
			break;
	}
	
	for(int i = 0; i < 96; i++){
		quantTable[i] = expf(i * 0.15289164787221953823f) * t.root;
    }
	
	Log("Bink InitAudio: frameLen %d, root %f, bandsCount %d\n",t.frameLen,t.root,t.bandsCount);
}

int BinkFile::DecodeAudio(uint8_t *buff, BinkAudioTrack &t)
{
	bool useDct = t.flags & BINK_AUD_USEDCT;
	
	uint32_t len = *(uint32_t*)buff;
	buff += 4;
	uint32_t decLen = *(uint32_t*)buff;
	//Log("Audio decode. Frame %d. len %d, decoded len %d\n",curFrame,len,decLen);
	BitReader abr = BitReader(buff, len<<3);
	
	abr.SkipBitsLong(32);
	
	if(useDct)
		abr.SkipBitsLong(2);
	
	float quant[25];
	
	for(int ch=0; ch<t.channels; ch++)
	{
		float coeffs[2];
		//dct
		coeffs[0] = abr.GetFloat()*t.root;
		coeffs[1] = abr.GetFloat()*t.root;
		
		for(int j=0;j<t.bandsCount;j++)
		{
			int v = abr.GetBits(8);
			quant[j] = quantTable[min(v,95)];
		}
		
		int i=2;
		while(i < t.frameLen)
		{
			int v = abr.GetBit();
			int j;
			if(v){
				v = abr.GetBits(4);
				j = i+rle_length_tab[v]*8;
			}else{
				j = i+8;
			}
			
			j = min(j, t.frameLen);
			
			int width = abr.GetBits(4);
			if(width==0)
			{
				i = j;
				
			}else{
				while(i < j)
				{
					
					int coeff = abr.GetBits(width);
					if(coeff){
						int v = abr.GetBit();
						if(v){
						}else{
							
						}
					}else{
						
					}
					i++;
				}
			}
		}
	}
	
	abr.Align32();
	if(abr.p>abr.s)
		Log("Invalid audio block at frame %d, %d/%d bits. coded %d decoded %d\n",curFrame,abr.p,abr.s,len,decLen);
	
	return 0;
}
void BinkFile::InitBundles()
{
	int bw=(header.width+7)>>3;
	int bh=(header.height+7)>>3;
	int blocks = bw*bh;
	LOG("blocks: %d\n",blocks);
	
	for(int i=0;i<BINK_NB_SRC;i++){
		bundles[i].data=new uint8_t[blocks*64];
	}
}

void BinkFile::FreeBundles()
{
	for(int i=0;i<BINK_NB_SRC;i++){
		delete[] bundles[i].data;
	}
}

void BinkFile::InitLengths(int width, int bw)
{
	//width align 8
	
	bundles[BINK_SRC_BLOCK_TYPES].len = log2((width >> 3) + 511) + 1;
    bundles[BINK_SRC_SUB_BLOCK_TYPES].len = log2((width >> 4) + 511) + 1;
    bundles[BINK_SRC_COLORS].len = log2(bw*64 + 511) + 1;
	
    bundles[BINK_SRC_INTRA_DC].len =
    bundles[BINK_SRC_INTER_DC].len =
    bundles[BINK_SRC_X_OFF].len =
    bundles[BINK_SRC_Y_OFF].len = log2((width >> 3) + 511) + 1;
	
    bundles[BINK_SRC_PATTERN].len = log2((bw << 3) + 511) + 1;
    bundles[BINK_SRC_RUN].len = log2(bw*48 + 511) + 1;
}

void FillPixels(uint8_t *dst,int stride,uint8_t val, int size=8){
	for(int i=0;i<size;i++){
		for(int j=0;j<size;j++){
			dst[i*stride+j]=val;
		}
	}
}

void CopyPixels(uint8_t *dst,int stride,uint8_t *src, int stride2){
	for(int i=0;i<8;i++){
		for(int j=0;j<8;j++){
			dst[i*stride+j]=src[i*stride2+j];
		}
	}
}

void CopyPixelsScaled(uint8_t *dst,int stride,uint8_t *src){
	for(int i=0;i<8;i++){
		for(int j=0;j<8;j++){
			dst[i*2*stride+j*2]=src[i*8+j];
			dst[i*2*stride+j*2+1]=src[i*8+j];
			dst[(i*2+1)*stride+j*2]=src[i*8+j];
			dst[(i*2+1)*stride+j*2+1]=src[i*8+j];
		}
	}
}

void AddPixels8(uint8_t *dst, int stride, int16_t *src)
{
	for(int i=0;i<8;i++){
		for(int j=0;j<8;j++){
			dst[i*stride+j] += src[i*8+j];
		}
	}
}

int BinkFile::DecodePlane(uint8_t *b, uint8_t *p,int idx,int isChroma)
{
	int i,j;
	int v, col[2];
	uint8_t *dst=b;
	uint8_t *prev=p;
	const uint8_t *scan;
	//int16_t block[64];
	int32_t dctblock[64];
	int coordmap[64];
	
	uint8_t tempblock[64];
	int xoff, yoff;
	
	int bw = isChroma ? (header.width+15)>>4 : (header.width+7)>>3;
	int bh = isChroma ? (header.height+15)>>4 : (header.height+7)>>3;
	int width = header.width>>isChroma;
	int stride = width;
	
	InitLengths(max(width,8),bw);
	for(i=0;i<BINK_NB_SRC;i++){
		ReadBundle(i);
	}
	
	for (i = 0; i < 64; i++)
        coordmap[i] = (i & 7) + (i >> 3) * stride;
	
	for(int by=0;by<bh;by++)
	{
		ReadBlockTypes(bundles+BINK_SRC_BLOCK_TYPES);
		ReadBlockTypes(bundles+BINK_SRC_SUB_BLOCK_TYPES);
		ReadColors(bundles+BINK_SRC_COLORS);
		ReadPatterns(bundles+BINK_SRC_PATTERN);
		ReadMotionValues(bundles+BINK_SRC_X_OFF);
		ReadMotionValues(bundles+BINK_SRC_Y_OFF);
		ReadDcs(bundles+BINK_SRC_INTRA_DC,0);
		ReadDcs(bundles+BINK_SRC_INTER_DC,1);
		ReadRuns(bundles+BINK_SRC_RUN);
		
		if(by==bh)
			break;
		dst = b+by*8*stride;
		prev = p+by*8*stride;
		
		for(int bx=0; bx<bw; bx++, dst+=8, prev+=8){
			int blk = GetValue(BINK_SRC_BLOCK_TYPES);
			if(blk!=0&& blk!=1&& blk!=2&& blk!=3&& blk!=5&& blk!=6&& blk!=7&& blk!=8&& blk!=9){
				Log("blk %dx%d: %d\n",bx,by,blk);
			}
			if ((by & 1) && blk == SCALED_BLOCK) {
                bx++;
                dst  += 8;
                prev += 8;
                continue;
            }
			
			switch(blk){
			case SKIP_BLOCK: //0
				CopyPixels(dst,stride,prev,stride);
				break;
			case SCALED_BLOCK: //1
				blk=GetValue(BINK_SRC_SUB_BLOCK_TYPES);
				switch(blk){
				case RUN_BLOCK: //3
					scan = bink_patterns[br.GetBits(4)];
					i = 0;
					do{
						int run = GetValue(BINK_SRC_RUN)+1;
						i+=run;
						if (i > 64) {
							Log("Run went out of bounds\n");
							return -1;
						}
						if(br.GetBit()){
							v=GetValue(BINK_SRC_COLORS);
							for (j = 0; j < run; j++){
								tempblock[*scan++] = v;
							}
						}else{
							for (j = 0; j < run; j++){
								v = GetValue(BINK_SRC_COLORS);
								tempblock[*scan++] = v;
							}
						}
					}while (i < 63);
					if (i == 63){
						v = GetValue(BINK_SRC_COLORS);
						tempblock[*scan++] = v;
					}
					break;
				case INTRA_BLOCK: //5
					memset(dctblock,0,sizeof(int32_t)*64);
					dctblock[0] = GetValue(BINK_SRC_INTRA_DC);
					ReadDctCoeffs(dctblock,bink_scan, bink_intra_quant, -1);
					IdctPut(tempblock,8,dctblock);
					break;
				case FILL_BLOCK: //6
					v = GetValue(BINK_SRC_COLORS);
					FillPixels(dst,stride,v,16);
					break;
				case PATTERN_BLOCK: //8
               		for (i = 0; i < 2; i++)
                    	col[i] = GetValue(BINK_SRC_COLORS);
                	for (i = 0; i < 8; i+=1) {
                	    v = GetValue(BINK_SRC_PATTERN);
                	    for (j = 0; j < 8; j+=1, v >>= 1){
                    	    tempblock[i*8 + j] = col[v & 1];
						}
                	}
                	break;
				case RAW_BLOCK: //9
					for (i = 0; i < 8; i++){
						memcpy(tempblock + i*8, bundles[BINK_SRC_COLORS].curPtr + i*8, 8);
					}
					bundles[BINK_SRC_COLORS].curPtr += 64;
                	break;
				default:
					Log("sub blk %dx%d: %d\n",bx,by,blk);
					FillPixels(tempblock,stride,0xFF,8);
				}
				if(blk!=FILL_BLOCK)
					CopyPixelsScaled(dst,stride,tempblock);
				
				bx++;
				dst+=8;
				prev+=8;
				break;
			case MOTION_BLOCK: //2
				xoff = GetValue(BINK_SRC_X_OFF);
				yoff = GetValue(BINK_SRC_Y_OFF);
				CopyPixels(dst,stride,prev+yoff*stride+xoff,stride);
				break;
			case RUN_BLOCK: //3
				scan = bink_patterns[br.GetBits(4)];
				i=0;
				do{
					int run = GetValue(BINK_SRC_RUN)+1;
					i+=run;
					if (i > 64) {
                        Log("Run went out of bounds\n");
                        return -1;
                    }
					if(br.GetBit()){
						v=GetValue(BINK_SRC_COLORS);
						for (j = 0; j < run; j++){
                            dst[coordmap[*scan++]] = v;
						}
					}else{
                        for (j = 0; j < run; j++){
							v = GetValue(BINK_SRC_COLORS);
                            dst[coordmap[*scan++]] = v;
						}
                    }
				}while (i < 63);
                if (i == 63){
					v = GetValue(BINK_SRC_COLORS);
                    dst[coordmap[*scan++]] = v;
				}
				break;
			/*case RESIDUE_BLOCK: //4
				//Log("RESIDUE_BLOCK\n");
				xoff = GetValue(BINK_SRC_X_OFF);
				yoff = GetValue(BINK_SRC_Y_OFF);
				CopyPixels(dst,stride,prev+yoff*stride+xoff,stride);
				//TODO
				
				AddPixels8(dst, stride, block);
				break;*/
			case INTRA_BLOCK: //5
				memset(dctblock,0,sizeof(int32_t)*64);
				dctblock[0] = GetValue(BINK_SRC_INTRA_DC);
				ReadDctCoeffs(dctblock,bink_scan, bink_intra_quant, -1);
				IdctPut(dst,stride,dctblock);
				//FillPixels(dst,stride,0x808000,true);
				break;
			case FILL_BLOCK: //6
				v = GetValue(BINK_SRC_COLORS);
				FillPixels(dst,stride,v);
				break;
			case INTER_BLOCK: //7
				xoff = GetValue(BINK_SRC_X_OFF);
				yoff = GetValue(BINK_SRC_Y_OFF);
				CopyPixels(dst,stride,prev+yoff*stride+xoff,stride);
				
				memset(dctblock,0,sizeof(int32_t)*64);
				dctblock[0] = GetValue(BINK_SRC_INTER_DC);
				ReadDctCoeffs(dctblock,bink_scan, bink_inter_quant, -1);
				IdstAdd(dst,stride,dctblock);
				break;
			case PATTERN_BLOCK: //8
                for (i = 0; i < 2; i++)
                    col[i] = GetValue(BINK_SRC_COLORS);
                for (i = 0; i < 8; i++) {
                    v = GetValue(BINK_SRC_PATTERN);
                    for (j = 0; j < 8; j++, v >>= 1){
                        dst[i*stride + j] = col[v & 1];
					}
                }
                break;
			case RAW_BLOCK: //9
                for (i = 0; i < 8; i++){
                    memcpy(dst + i*stride, bundles[BINK_SRC_COLORS].curPtr + i*8, 8);
				}
                bundles[BINK_SRC_COLORS].curPtr += 64;
                break;
			default:
				if(blk>9){
					LOG("Block %dx%d type out of bounds: %d\n",bx,by,blk);
					return -1;
				}
				FillPixels(dst,stride,0xFF);
			}
		}
	}
	if(br.GetBitsCount() & 0x1F) //next plane data starts at 32-bit boundary
		br.SkipBitsLong(32 - (br.GetBitsCount() & 0x1F));
	
	return 0;
}

void BinkFile::ReadBundle(int idx)
{
	if(idx==BINK_SRC_COLORS){
		for(int i=0;i<16;i++){
			ReadTree(&colHigh[i]);
		}
		colLastVal=0;
	}
	if(idx!=BINK_SRC_INTRA_DC && idx!=BINK_SRC_INTER_DC)
		ReadTree(&bundles[idx].tree);
	
	bundles[idx].curDec = bundles[idx].curPtr = bundles[idx].data;
}

void BinkFile::ReadTree(binkTree *tree)
{
	tree->vlcNum = br.GetBits(4);
	//LOG("tree vlc num %d\n",tree->vlcNum);
	if(!tree->vlcNum){
		for(int i=0;i<16;i++)
			tree->syms[i]=i;
		return;
	}
	uint8_t tmp[16]={0};
	uint8_t tmp2[16];
	uint8_t *in=tmp, *out=tmp2;
	int len=0;
	if(br.GetBit()){
		len = br.GetBits(3);
		//LOG("len3: %d\n",len);
		for(int i=0;i<=len;i++){
			tree->syms[i]=br.GetBits(4);
			//LOG("sym %d = %d\n",i,tree->syms[i]);
			tmp[tree->syms[i]]=1;
		}
		for(int i=0;i<16&&len<15;i++){
			if(!tmp[i])
				tree->syms[++len] = i;
		}
	}else{
		len = br.GetBits(2);
		//LOG("len2: %d\n",len);
		for(int i=0;i<16;i++)
			tmp[i]=i;
		for(int i=0;i<=len;i++){
			int size = 1<<i;
			for(int t=0;t<16;t+=size<<1){
				Merge(out+t,in+t,size);
			}
			uint8_t* _t=in; in=out; out=_t;
		}
		memcpy(tree->syms,in,16);
	}
}

void BinkFile::Merge(uint8_t *dst, uint8_t *src, int size)
{
	uint8_t *src2 = src + size;
    int size2 = size;

    do {
        if (!br.GetBit()) {
            *dst++ = *src++;
            size--;
        } else {
            *dst++ = *src2++;
            size2--;
        }
    } while (size && size2);

    while (size--)
        *dst++ = *src++;
    while (size2--)
        *dst++ = *src2++;
}

int BinkFile::GetHuff(binkTree *t)
{
	return t->syms[br.GetVlc(binkTrees[t->vlcNum].table, binkTrees[t->vlcNum].bits, 1)];
}

int BinkFile::GetValue(int idx)
{
	if(idx<BINK_SRC_X_OFF || idx==BINK_SRC_RUN)
		return *bundles[idx].curPtr++;
	if(idx==BINK_SRC_X_OFF || idx==BINK_SRC_Y_OFF)
		return (int8_t)*bundles[idx].curPtr++;
	int ret = *(int16_t*)bundles[idx].curPtr;
	bundles[idx].curPtr += 2;
	return ret;
}

int BinkFile::ReadDctCoeffs(int32_t *block, const uint8_t *scan,
                           const int32_t quantMatrices[16][64], int q)
{
	int coef_list[128];
    int mode_list[128];
	int listStart=64, listEnd=64, listPos;
	int t;
	int coefCount = 0;
    int coefIdx[64];
    int quantIdx;
    const int32_t *quant;
	
	coef_list[listEnd] = 4;  mode_list[listEnd++] = 0;
    coef_list[listEnd] = 24; mode_list[listEnd++] = 0;
    coef_list[listEnd] = 44; mode_list[listEnd++] = 0;
    coef_list[listEnd] = 1;  mode_list[listEnd++] = 3;
    coef_list[listEnd] = 2;  mode_list[listEnd++] = 3;
    coef_list[listEnd] = 3;  mode_list[listEnd++] = 3;
	
	for(int bits = br.GetBits(4)-1; bits>=0; bits--){
		listPos = listStart;
		while(listPos<listEnd){
			if(!(mode_list[listPos] | coef_list[listPos]) || !br.GetBit()){
				listPos++;
				continue;
			}
			int ccoef = coef_list[listPos];
			int mode = mode_list[listPos];
			switch(mode){
			case 0:
				coef_list[listPos]=ccoef+4;
				mode_list[listPos]=1;
			case 2:
				if(mode==2){
					coef_list[listPos]=0;
					mode_list[listPos++]=0;
				}
				for(int i=0;i<4;i++,ccoef++){
					if(br.GetBit()){
						coef_list[--listStart]=ccoef;
						mode_list[listStart]=3;
					}else{
						if(!bits){
							t = 1-(br.GetBit()<<1);
						}else{
							t = br.GetBits(bits) | 1<<bits;
							int sign = -br.GetBit();
							t = (t^sign)-sign;
						}
						block[scan[ccoef]] = t;
						coefIdx[coefCount++] = ccoef;
					}
				}
				break;
			case 1:
				mode_list[listPos]=2;
				for(int i=0;i<3;i++){
					ccoef+=4;
					coef_list[listEnd]=ccoef;
					mode_list[listEnd++]=2;
				}
				break;
			case 3:
				if(!bits){
					t = 1-(br.GetBit()<<1);
				}else{
					t = br.GetBits(bits) | 1<<bits;
					int sign = -br.GetBit();
					t = (t^sign)-sign;
				}
				block[scan[ccoef]]=t;
				coefIdx[coefCount++] = ccoef;
				coef_list[listPos]=0;
				mode_list[listPos++]=0;
				break;
			}
		}
	}
	
	if(q==-1){
		quantIdx = br.GetBits(4);
	}else{
		quantIdx=q;
		if(quantIdx>15){
            LOG("quantIndex %d out of range\n", quantIdx);
			return -1;
		}
	}
	
	quant = quantMatrices[quantIdx];
	
	block[0]=(block[0]*quant[0])>>11;
	for(int i=0;i<coefCount;i++){
		int idx = coefIdx[i];
		block[scan[idx]] = (block[scan[idx]]*quant[idx])>>11;
	}
	return 0;
}

static const uint8_t bink_rlelens[4] = { 4, 8, 12, 32 };
int BinkFile::ReadBlockTypes(binkBundle *b)
{
	int t=0, v=0, last=0;
	
	if (!b->curDec || (b->curDec > b->curPtr))
        return 0;
    t = br.GetBits(b->len);
    if (!t) {
        b->curDec = NULL;
        return 0;
    }
	//LOG("ReadBlockTypes: len=%d, t=%d\n",b->len,t);
	uint8_t *decEnd = b->curDec+t;
	//TODO: check end
	
	if(br.GetBit()){
		v=br.GetBits(4);
		//LOG("bit 1, v=%d\n",v);
		memset(b->curDec,v,t);
		b->curDec+=t;
	}else{
		while(b->curDec<decEnd){
			v = GetHuff(&b->tree);
			//LOG("bit 0, v=%d\n",v);
			if(v<12){
				last=v;
				*b->curDec++ = v;
			}else{
				int run = bink_rlelens[v-12];
				//TODO: check end
				memset(b->curDec,last,run);
				b->curDec+=run;
			}
		}
	}
	return 0;
}

int BinkFile::ReadColors(binkBundle *b)
{
	int t=0, v=0;
	
	if (!b->curDec || (b->curDec > b->curPtr))
        return 0;
    t = br.GetBits(b->len);
    if (!t) {
        b->curDec = NULL;
        return 0;
    }
	//LOG("ReadColors: len=%d, t=%d\n",b->len,t);
	uint8_t *decEnd = b->curDec+t;
	//TODO: check end
	
	if(br.GetBit()){
		colLastVal = GetHuff(colHigh+colLastVal);
		v = GetHuff(&b->tree);
		v = (colLastVal<<4) | v;
		//LOG("bit 1, v=%d\n",v);
		memset(b->curDec,v,t);
		b->curDec+=t;
	}else{
		while(b->curDec<decEnd){
			colLastVal = GetHuff(colHigh+colLastVal);
			v = GetHuff(&b->tree);
			v = (colLastVal<<4) | v;
			*b->curDec++ = v;
		}
	}
	return 0;
}

int BinkFile::ReadPatterns(binkBundle *b)
{
	int t=0, v=0;
	
	if (!b->curDec || (b->curDec > b->curPtr))
        return 0;
    t = br.GetBits(b->len);
    if (!t) {
        b->curDec = NULL;
        return 0;
    }
	//LOG("ReadPatterns: len=%d, t=%d\n",b->len,t);
	uint8_t *decEnd = b->curDec+t;
	//TODO: check end
	
	while(b->curDec<decEnd){
		v = GetHuff(&b->tree);
		v |= (GetHuff(&b->tree)<<4);
		*b->curDec++ = v;
	}

	return 0;
}

int BinkFile::ReadMotionValues(binkBundle *b)
{
	int t=0, v=0, sign=0;
	
	if (!b->curDec || (b->curDec > b->curPtr))
        return 0;
    t = br.GetBits(b->len);
    if (!t) {
        b->curDec = NULL;
        return 0;
    }
	//LOG("ReadMotionValues: len=%d, t=%d\n",b->len,t);
	uint8_t *decEnd = b->curDec+t;
	//TODO: check end
	
	if(br.GetBit()){
		v=br.GetBits(4);
		//LOG("bit 1, v=%d\n",v);
		if(v){
			sign = -br.GetBit();
			v = (v^sign)-sign;
		}
		memset(b->curDec,v,t);
		b->curDec+=t;
	}else{
		while(b->curDec<decEnd){
			v = GetHuff(&b->tree);
			if(v){
				sign = -br.GetBit();
				v = (v^sign)-sign;
			}
			*b->curDec++ = v;
		}
	}
	return 0;
}

int BinkFile::ReadDcs(binkBundle *b, int hasSign)
{
	int len, len2, v, v2, sign, bsize;
	int16_t *dst = (int16_t*)b->curDec;
	
	if (!b->curDec || (b->curDec > b->curPtr))
        return 0;
    len = br.GetBits(b->len);
    if (!len) {
        b->curDec = NULL;
        return 0;
    }
	//LOG("ReadDcs: b.len=%d, len=%d\n",b->len,len);
	//uint8_t *decEnd = b->curDec+len;
	
	//TODO: check end
	
	v = br.GetBits(11-hasSign);
	if(v&&hasSign){
		sign = -br.GetBit();
		v = (v^sign)-sign;
	}
	//LOG("v=%d\n",v);
	*dst++ = v;
	len--;
	for(int i=0;i<len;i+=8){
		len2 = min(len-i,8);
		bsize=br.GetBits(4);
		//LOG("len2=%d, bsize=%d\n",len2,bsize);
		if(bsize){
			for(int j=0;j<len2;j++){
				v2=br.GetBits(bsize);
				if(v2){
					sign = -br.GetBit();
					v2 = (v2^sign)-sign;
				}
				v+=v2;
				*dst++ = v;
				if (v < -32768 || v > 32767) {
                    LOG("Error: DC value went out of bounds: %d\n", v);
                    return -1;//AVERROR_INVALIDDATA;
                }
			}
		}else{
			for(int j=0;j<len2;j++){
				*dst++ = v;
			}
		}
	}
	
	b->curDec = (uint8_t*)dst;
	
	return 0;
}

int BinkFile::ReadRuns(binkBundle *b)
{
	int t=0, v=0;
	
	if (!b->curDec || (b->curDec > b->curPtr))
        return 0;
    t = br.GetBits(b->len);
    if (!t) {
        b->curDec = NULL;
        return 0;
    }
	//LOG("ReadRuns: len=%d, t=%d\n",b->len,t);
	uint8_t *decEnd = b->curDec+t;
	//TODO: check end
	
	if(br.GetBit()){
		v=br.GetBits(4);
		//LOG("bit 1, v=%d\n",v);
		memset(b->curDec,v,t);
		b->curDec+=t;
	}else{
		while(b->curDec<decEnd){
			*b->curDec++ = GetHuff(&b->tree);
		}
	}
	return 0;
}


#define A1  2896 /* (1/sqrt(2))<<12 */
#define A2  2217
#define A3  3784
#define A4 -5352

#define IDCT_TRANSFORM(dest,s0,s1,s2,s3,s4,s5,s6,s7,d0,d1,d2,d3,d4,d5,d6,d7,munge,src) {\
    const int a0 = (src)[s0] + (src)[s4]; \
    const int a1 = (src)[s0] - (src)[s4]; \
    const int a2 = (src)[s2] + (src)[s6]; \
    const int a3 = (A1*((src)[s2] - (src)[s6])) >> 11; \
    const int a4 = (src)[s5] + (src)[s3]; \
    const int a5 = (src)[s5] - (src)[s3]; \
    const int a6 = (src)[s1] + (src)[s7]; \
    const int a7 = (src)[s1] - (src)[s7]; \
    const int b0 = a4 + a6; \
    const int b1 = (A3*(a5 + a7)) >> 11; \
    const int b2 = ((A4*a5) >> 11) - b0 + b1; \
    const int b3 = (A1*(a6 - a4) >> 11) - b2; \
    const int b4 = ((A2*a7) >> 11) + b3 - b1; \
    (dest)[d0] = munge(a0+a2   +b0); \
    (dest)[d1] = munge(a1+a3-a2+b2); \
    (dest)[d2] = munge(a1-a3+a2+b3); \
    (dest)[d3] = munge(a0-a2   -b4); \
    (dest)[d4] = munge(a0-a2   +b4); \
    (dest)[d5] = munge(a1-a3+a2-b3); \
    (dest)[d6] = munge(a1+a3-a2-b2); \
    (dest)[d7] = munge(a0+a2   -b0); \
}

#define MUNGE_NONE(x) (x)
#define IDCT_COL(dest,src) IDCT_TRANSFORM(dest,0,8,16,24,32,40,48,56,0,8,16,24,32,40,48,56,MUNGE_NONE,src)

#define MUNGE_ROW(x) (((x) + 0x7F)>>8)
#define IDCT_ROW(dest,src) IDCT_TRANSFORM(dest,0,1,2,3,4,5,6,7,0,1,2,3,4,5,6,7,MUNGE_ROW,src)

static inline void IdctCol(int *dest, const int32_t *src)
{
    if ((src[8]|src[16]|src[24]|src[32]|src[40]|src[48]|src[56])==0) {
        dest[0]  =
        dest[8]  =
        dest[16] =
        dest[24] =
        dest[32] =
        dest[40] =
        dest[48] =
        dest[56] = src[0];
    } else {
        IDCT_COL(dest, src);
    }
}

void BinkFile::IdctPut(uint8_t *dst, int stride, int32_t *block){
	int i;
    int temp[64];
    for (i = 0; i < 8; i++)
        IdctCol(temp+i, block+i);
    for (i = 0; i < 8; i++) {
        IDCT_ROW( (&dst[i*stride]), (&temp[8*i]) );
    }
}

void BinkFile::IdstAdd(uint8_t *dst, int stride, int32_t *block)
{
	int i;
    int temp[64];

    for (i = 0; i < 8; i++)
        IdctCol(temp+i, block+i);
    for (i = 0; i < 8; i++) {
        IDCT_ROW( (&block[i*8]), (&temp[8*i]) );
    }

    for (i = 0; i < 8; i++, dst+=stride, block +=8) {
		for(int j=0; j<8; j++)
			dst[j] += block[j];
    }
}



