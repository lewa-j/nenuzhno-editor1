
//Minimalist bink video decoder
//Thanks to ffmpeg

#include "vlc.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#include <inttypes.h>
#include "log.h"

#define av_assert0(cond) ((void)0)
#define av_assert1(cond) ((void)0)
#define av_log(a,b,...) (LOG( __VA_ARGS__))
//#define ff_dlog(a,...) (LOG( __VA_ARGS__))
#define ff_dlog(...) ((void)0)
#define AVERROR(e) (-(e))
#define MKTAG(a,b,c,d) ((a) | ((b) << 8) | ((c) << 16) | ((unsigned)(d) << 24))
#define FFERRTAG(a, b, c, d) (-(int)MKTAG(a, b, c, d))
#define AVERROR_INVALIDDATA        FFERRTAG( 'I','N','D','A') ///< Invalid data found when processing input

#define FFSWAP(type,a,b) do{type SWAP_tmp= b; b= a; a= SWAP_tmp;}while(0)
#define FF_ARRAY_ELEMS(a) (sizeof(a) / sizeof((a)[0]))

#define FFMIN(a,b) ((a) > (b) ? (b) : (a))
#define FFMAX(a,b) ((a) > (b) ? (a) : (b))


#define AV_QSORT(p, num, type, cmp) do {\
    type *stack[64][2];\
    int sp= 1;\
    stack[0][0] = p;\
    stack[0][1] = (p)+(num)-1;\
    while(sp){\
        type *start= stack[--sp][0];\
        type *end  = stack[  sp][1];\
        while(start < end){\
            if(start < end-1) {\
                int checksort=0;\
                type *right = end-2;\
                type *left  = start+1;\
                type *mid = start + ((end-start)>>1);\
                if(cmp(start, end) > 0) {\
                    if(cmp(  end, mid) > 0) FFSWAP(type, *start, *mid);\
                    else                    FFSWAP(type, *start, *end);\
                }else{\
                    if(cmp(start, mid) > 0) FFSWAP(type, *start, *mid);\
                    else checksort= 1;\
                }\
                if(cmp(mid, end) > 0){ \
                    FFSWAP(type, *mid, *end);\
                    checksort=0;\
                }\
                if(start == end-2) break;\
                FFSWAP(type, end[-1], *mid);\
                while(left <= right){\
                    while(left<=right && cmp(left, end-1) < 0)\
                        left++;\
                    while(left<=right && cmp(right, end-1) > 0)\
                        right--;\
                    if(left <= right){\
                        FFSWAP(type, *left, *right);\
                        left++;\
                        right--;\
                    }\
                }\
                FFSWAP(type, end[-1], *left);\
                if(checksort && (mid == left-1 || mid == left)){\
                    mid= start;\
                    while(mid<end && cmp(mid, mid+1) <= 0)\
                        mid++;\
                    if(mid==end)\
                        break;\
                }\
                if(end-left < left-start){\
                    stack[sp  ][0]= start;\
                    stack[sp++][1]= right;\
                    start = left+1;\
                }else{\
                    stack[sp  ][0]= left+1;\
                    stack[sp++][1]= end;\
                    end = right;\
                }\
            }else{\
                if(cmp(start, end) > 0)\
                    FFSWAP(type, *start, *end);\
                break;\
            }\
        }\
    }\
} while (0)


#define GET_DATA(v, table, i, wrap, size)                   \
{                                                           \
    const uint8_t *ptr = (const uint8_t *)table + i * wrap; \
    switch(size) {                                          \
    case 1:                                                 \
        v = *(const uint8_t *)ptr;                          \
        break;                                              \
    case 2:                                                 \
        v = *(const uint16_t *)ptr;                         \
        break;                                              \
    case 4:                                                 \
        v = *(const uint32_t *)ptr;                         \
        break;                                              \
    default:                                                \
        av_assert1(0);                                      \
    }                                                       \
}

static int alloc_table(VLC *vlc, int size, int use_static)
{
    int index = vlc->table_size;

    vlc->table_size += size;
    /*if (vlc->table_size > vlc->table_allocated) {
        if (use_static)
            abort(); // cannot do anything, init_vlc() is used with too little memory
        vlc->table_allocated += (1 << vlc->bits);
        vlc->table = av_realloc_f(vlc->table, vlc->table_allocated, sizeof(VLC_TYPE) * 2);
        if (!vlc->table) {
            vlc->table_allocated = 0;
            vlc->table_size = 0;
            return AVERROR(ENOMEM);
        }
        memset(vlc->table + vlc->table_allocated - (1 << vlc->bits), 0, sizeof(VLC_TYPE) * 2 << vlc->bits);
    }*/
    return index;
}

static int compare_vlcspec(const VLCcode *a, const VLCcode *b)
{
    const VLCcode *sa = a, *sb = b;
    return (sa->code >> 1) - (sb->code >> 1);
}

static int build_table(VLC *vlc, uint32_t table_nb_bits, uint32_t nb_codes,
                       VLCcode *codes, int flags)
{
	uint32_t table_size, table_index, index, code_prefix, symbol, subtable_bits;
	uint32_t i, j, k, n, nb, inc;
    uint32_t code;
    volatile VLC_TYPE (* volatile table)[2]; // the double volatile is needed to prevent an internal compiler error in gcc 4.2

    table_size = 1 << table_nb_bits;
    if (table_nb_bits > 30)
       return -1;
    table_index = alloc_table(vlc, table_size, flags & INIT_VLC_USE_NEW_STATIC);
    ff_dlog(NULL, "new table index=%d size=%d\n", table_index, table_size);
    if (table_index < 0)
        return table_index;
    table = (volatile VLC_TYPE (*)[2])&vlc->table[table_index];

    /* first pass: map codes and compute auxiliary table sizes */
    for (i = 0; i < nb_codes; i++) {
        n      = codes[i].bits;
        code   = codes[i].code;
        symbol = codes[i].symbol;
        //ff_dlog(NULL, "i=%d n=%d code=0x%"PRIx32"\n", i, n, code);
        if (n <= table_nb_bits) {
            /* no need to add another table */
            j = code >> (32 - table_nb_bits);
            nb = 1 << (table_nb_bits - n);
            inc = 1;
            if (flags & INIT_VLC_LE) {
                j = bitswap_32(code);
                inc = 1 << n;
            }
            for (k = 0; k < nb; k++) {
				uint32_t bits = table[j][1];
                ff_dlog(NULL, "%4x: code=%d n=%d\n", j, i, n);
                if (bits != 0 && bits != n) {
                    av_log(NULL, AV_LOG_ERROR, "incorrect codes\n");
                    return AVERROR_INVALIDDATA;
                }
                table[j][1] = n; //bits
                table[j][0] = symbol;
                j += inc;
            }
        } else {
            /* fill auxiliary table recursively */
            n -= table_nb_bits;
            code_prefix = code >> (32 - table_nb_bits);
            subtable_bits = n;
            codes[i].bits = n;
            codes[i].code = code << table_nb_bits;
            for (k = i+1; k < nb_codes; k++) {
                n = codes[k].bits - table_nb_bits;
                if (n <= 0)
                    break;
                code = codes[k].code;
                if (code >> (32 - table_nb_bits) != code_prefix)
                    break;
                codes[k].bits = n;
                codes[k].code = code << table_nb_bits;
                subtable_bits = FFMAX(subtable_bits, n);
            }
            subtable_bits = FFMIN(subtable_bits, table_nb_bits);
            j = (flags & INIT_VLC_LE) ? bitswap_32(code_prefix) >> (32 - table_nb_bits) : code_prefix;
            table[j][1] = -subtable_bits;
            ff_dlog(NULL, "%4x: n=%d (subtable)\n",
                    j, codes[i].bits + table_nb_bits);
            index = build_table(vlc, subtable_bits, k-i, codes+i, flags);
            if (index < 0)
                return index;
            /* note: realloc has been done, so reload tables */
            table = (volatile VLC_TYPE (*)[2])&vlc->table[table_index];
            table[j][0] = index; //code
            i = k-1;
        }
    }

    for (i = 0; i < table_size; i++) {
        if (table[i][1] == 0) //bits
            table[i][0] = -1; //codes
    }

    return table_index;
}

int ff_init_vlc_sparse(VLC *vlc_arg, int nb_bits, int nb_codes,
                       const void *bits, int bits_wrap, int bits_size,
                       const void *codes, int codes_wrap, int codes_size,
                       const void *symbols, int symbols_wrap, int symbols_size,
                       int flags)
{
    VLCcode *buf;
	int i, j, ret;
    VLCcode localbuf[1500]; // the maximum currently needed is 1296 by rv34
    VLC localvlc, *vlc;

    vlc = vlc_arg;
    vlc->bits = nb_bits;
    if (flags & INIT_VLC_USE_NEW_STATIC) {
        av_assert0(nb_codes + 1 <= FF_ARRAY_ELEMS(localbuf));
        buf = localbuf;
        localvlc = *vlc_arg;
        vlc = &localvlc;
        vlc->table_size = 0;
    } else {/*
        vlc->table           = NULL;
        vlc->table_allocated = 0;
        vlc->table_size      = 0;

        buf = av_malloc_array((nb_codes + 1), sizeof(VLCcode));
        if (!buf)
            return AVERROR(ENOMEM);*/
    }


    av_assert0(symbols_size <= 2 || !symbols);
    j = 0;
#define COPY(condition)\
    for (i = 0; i < nb_codes; i++) {                                        \
        GET_DATA(buf[j].bits, bits, i, bits_wrap, bits_size);               \
        if (!(condition))                                                   \
            continue;                                                       \
        if (buf[j].bits > 3*nb_bits || buf[j].bits>32) {                    \
            av_log(NULL, AV_LOG_ERROR, "Too long VLC (%d) in init_vlc\n", buf[j].bits);\
            /*if (!(flags & INIT_VLC_USE_NEW_STATIC))*/                         \
            /*    av_free(buf);*/                                               \
            return -1;                                                      \
        }                                                                   \
        GET_DATA(buf[j].code, codes, i, codes_wrap, codes_size);            \
        if (buf[j].code >= (1LL<<buf[j].bits)) {                            \
             /*av_log(NULL, AV_LOG_ERROR, "Invalid code %"PRIx32" for %d in "*/  \
                   /*"init_vlc\n", buf[j].code, i);*/                           \
            /*if (!(flags & INIT_VLC_USE_NEW_STATIC))*/                         \
            /*    av_free(buf);*/                                               \
            return -1;                                                      \
        }                                                                   \
        if (flags & INIT_VLC_LE)                                            \
            buf[j].code = bitswap_32(buf[j].code);                          \
        else                                                                \
            buf[j].code <<= 32 - buf[j].bits;                               \
        if (symbols)                                                        \
            GET_DATA(buf[j].symbol, symbols, i, symbols_wrap, symbols_size) \
        else                                                                \
            buf[j].symbol = i;                                              \
        j++;                                                                \
    }
    COPY(buf[j].bits > nb_bits);
    // qsort is the slowest part of init_vlc, and could probably be improved or avoided
    AV_QSORT(buf, j, struct VLCcode, compare_vlcspec);
    COPY(buf[j].bits && buf[j].bits <= nb_bits);
    nb_codes = j;

    ret = build_table(vlc, nb_bits, nb_codes, buf, flags);

    if (flags & INIT_VLC_USE_NEW_STATIC) {
        if(vlc->table_size != vlc->table_allocated)
            av_log(NULL, AV_LOG_ERROR, "needed %d had %d\n", vlc->table_size, vlc->table_allocated);

        av_assert0(ret >= 0);
		if(ret < 0)
			Log("ff_init_vlc_sparse: ret < 0\n");
        *vlc_arg = *vlc;
    } else {
        /*av_free(buf);
        if (ret < 0) {
            av_freep(&vlc->table);
            return ret;
        }*/
    }
	return 0;
}

