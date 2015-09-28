#include "bigWig.h"
#include <errno.h>
#include <stdlib.h>
#include <zlib.h>
#include <math.h>

//Returns -1 if there are no applicable levels, otherwise an integer indicating the most appropriate level.
static int32_t determineZoomLevel(bigWigFile_t *fp, int basesPerBin) {
    int32_t out = -1;
    uint32_t bestDiff = -1;
    uint16_t i;
    for(i=0; i<fp->hdr->nLevels; i++) {
        if(fp->hdr->zoomHdrs->level[i] <= basesPerBin) {
            if(fp->hdr->zoomHdrs->level[i] - basesPerBin < bestDiff) {
                bestDiff = fp->hdr->zoomHdrs->level[i] - basesPerBin;
                out = i;
            }
        }
    }
    return out;
}

/// @cond SKIP
struct val_t {
    uint32_t nBases;
    float min, max, sum, sumsq;
};
/// @endcond

//Returns NULL on error
static struct val_t *getVal(bigWigFile_t *fp, bwOverlapBlock_t *o, int i) {
    void *buf = NULL, *compBuf = NULL;
    uint32_t sz = fp->hdr->bufSize;
    int compressed = 0, rv;
    uint32_t *p;
    struct val_t *v = NULL;

    if(sz) {
        compressed = 1;
        buf = malloc(sz); 
    }
    sz = 0; //This is now the size of the compressed buffer

    if(bwSetPos(fp, o->offset[i])) goto error;

    v = malloc(sizeof(struct val_t));
    if(!v) goto error;

    if(sz < o->size[i]) compBuf = malloc(o->size[i]);
    if(!compBuf) goto error;

    if(bwRead(compBuf, o->size[i], 1, fp) != o->size[i]) goto error;
    if(compressed) {
        rv = uncompress(buf, (uLongf *) &fp->hdr->bufSize, compBuf, o->size[i]);
        if(rv != Z_OK) goto error;
    } else {
        buf = compBuf;
    }

    p = buf;
    v->nBases = p[3];
    v->min = ((float*) p)[4];
    v->max = ((float*) p)[5];
    v->sum = ((float*) p)[6];
    v->sumsq = ((float*) p)[7];

    free(buf);
    free(compBuf);
    return v;

error:
    if(buf) free(buf);
    if(compBuf) free(compBuf);
    if(v) free(v);
    return NULL;
}

//On error, errno is set to ENOMEM and NaN is returned (though NaN can be returned normally)
//Does UCSC compensate for partial block/range overlap?
static double blockMean(bigWigFile_t *fp, bwOverlapBlock_t *blocks) {
    uint32_t i;
    double output = 0.0, *means = NULL;
    uint32_t coverage = 0, *nBases = NULL;
    struct val_t *v = NULL;

    if(!blocks->n) return strtod("NaN", NULL);

    means = malloc(blocks->n*sizeof(double));
    nBases = calloc(blocks->n, sizeof(uint32_t));
    if((!means) || (!nBases)) goto error;

    //Iterate over the blocks
    for(i=0; i<blocks->n; i++) {
        v = getVal(fp, blocks, i);
        if(!v) goto error;
        means[i] = v->sum;
        nBases[i] = v->nBases;
        free(v);
    }

    for(i=0; i<blocks->n; i++) coverage += nBases[i];
    for(i=0; i<blocks->n; i++) output += means[i];

    free(means);
    free(nBases);
    return output/coverage;

error:
    if(nBases) free(nBases);
    if(means) free(means);
    if(v) free(v);
    errno = ENOMEM;
    return strtod("NaN", NULL);
}

static double intMean(bwOverlappingIntervals_t* ints, uint32_t start, uint32_t end) {
    double sum = 0.0;
    uint32_t nBases = 0, i, start_use, end_use;

    if(!ints->l) return strtod("NaN", NULL);

    for(i=0; i<ints->l; i++) {
        start_use = ints->start[i];
        end_use = ints->end[i];
        if(ints->start[i] < start) start_use = start;
        if(ints->end[i] > end) end_use = end;
        nBases += end_use-start_use;
        sum += (end_use-start_use)*ints->value[i];
    }

    return sum/nBases;
}

//Does UCSC compensate for partial block/range overlap?
static double blockDev(bigWigFile_t *fp, bwOverlapBlock_t *blocks) {
    uint32_t i;
    double *means = NULL, *ssqs = NULL;
    double mean = 0.0, ssq = 0.0;
    uint32_t coverage = 0, *nBases = NULL;
    struct val_t *v = NULL;

    if(!blocks->n) return strtod("NaN", NULL);

    means = malloc(blocks->n*sizeof(double));
    ssqs = malloc(blocks->n*sizeof(double));
    nBases = calloc(blocks->n, sizeof(uint32_t));
    if((!means) || (!nBases)) goto error;

    //Iterate over the blocks
    for(i=0; i<blocks->n; i++) {
        v = getVal(fp, blocks, i);
        if(!v) goto error;
        means[i] = v->sum;
        ssqs[i] = v->sumsq;
        nBases[i] = v->nBases;
        free(v);
    }

    for(i=0; i<blocks->n; i++) coverage += nBases[i];
    for(i=0; i<blocks->n; i++) mean += means[i];
    for(i=0; i<blocks->n; i++) ssq += ssqs[i];

    free(means);
    free(ssqs);
    free(nBases);
    if(coverage<2) return strtod("NaN", NULL);
    return sqrt((ssq-pow(mean,2.0)/coverage)/(coverage-1)); //The precision of this isn't great, but there's no other method...

error:
    if(nBases) free(nBases);
    if(means) free(means);
    if(ssqs) free(ssqs);
    if(v) free(v);
    errno = ENOMEM;
    return strtod("NaN", NULL);
}

//This uses compensated summation to account for finite precision math
static double intDev(bwOverlappingIntervals_t* ints, uint32_t start, uint32_t end) {
    double sum = 0.0, sum2 = 0.0, mean;
    uint32_t nBases = 0, i, start_use, end_use;

    if(!ints->l) return strtod("NaN", NULL);
    mean = intMean(ints, start, end);

    for(i=0; i<ints->l; i++) {
        start_use = ints->start[i];
        end_use = ints->end[i];
        if(ints->start[i] < start) start_use = start;
        if(ints->end[i] > end) end_use = end;
        nBases += end_use-start_use;
        sum += (end_use-start_use)*(ints->value[i]-mean);
        sum2 += (end_use-start_use)*pow(ints->value[i]-mean,2.0);
    }

    return sqrt((sum2-pow(sum,2.0)/nBases)/(nBases-1));
}

static double blockMax(bigWigFile_t *fp, bwOverlapBlock_t *blocks) {
    uint32_t i;
    double o;
    struct val_t *v = NULL;

    if(!blocks->n) return strtod("NaN", NULL);

    //Get the first block
    v = getVal(fp, blocks, 0);
    if(!v) goto error;
    o = v->max;
    free(v);
    //Iterate over the remaining blocks
    for(i=1; i<blocks->n; i++) {
        v = getVal(fp, blocks, i);
        if(!v) goto error;
        if(v->max > o) o=v->max;
        free(v);
    }

    return o;

error:
    if(v) free(v);
    errno = ENOMEM;
    return strtod("NaN", NULL);
}

static double intMax(bwOverlappingIntervals_t* ints) {
    uint32_t i;
    double o;

    if(ints->l < 1) return strtod("NaN", NULL);

    o = ints->value[0];
    for(i=1; i<ints->l; i++) {
        if(ints->value[i] > o) o = ints->value[i];
    }

    return o;
}

static double blockMin(bigWigFile_t *fp, bwOverlapBlock_t *blocks) {
    uint32_t i;
    double o;
    struct val_t *v = NULL;

    if(!blocks->n) return strtod("NaN", NULL);

    //Get the first block
    v = getVal(fp, blocks, 0);
    if(!v) goto error;
    o = v->min;
    free(v);
    //Iterate over the remaining blocks
    for(i=1; i<blocks->n; i++) {
        v = getVal(fp, blocks, i);
        if(!v) goto error;
        if(v->max < o) o=v->min;
        free(v);
    }

    return o;

error:
    if(v) free(v);
    errno = ENOMEM;
    return strtod("NaN", NULL);
}

static double intMin(bwOverlappingIntervals_t* ints) {
    uint32_t i;
    double o;

    if(ints->l < 1) return strtod("NaN", NULL);

    o = ints->value[0];
    for(i=1; i<ints->l; i++) {
        if(ints->value[i] < o) o = ints->value[i];
    }

    return o;
}

//Does UCSC compensate for only partial block/interval overlap?
static double blockCoverage(bigWigFile_t *fp, bwOverlapBlock_t *blocks) {
    uint32_t i;
    double o = 0.0;
    struct val_t *v = NULL;

    if(!blocks->n) return strtod("NaN", NULL);

    //Iterate over the blocks
    for(i=0; i<blocks->n; i++) {
        v = getVal(fp, blocks, i);
        if(!v) goto error;
        o += v->nBases;
        free(v);
    }

    return o;

error:
    if(v) free(v);
    errno = ENOMEM;
    return strtod("NaN", NULL);
}

static double intCoverage(bwOverlappingIntervals_t* ints, uint32_t start, uint32_t end) {
    uint32_t i, start_use, end_use;
    double o = 0.0;

    if(!ints->l) return strtod("NaN", NULL);

    for(i=0; i<ints->l; i++) {
        start_use = ints->start[i];
        end_use = ints->end[i];
        if(start_use < start) start_use = start;
        if(end_use > end) end_use = end;
        o += end_use - start_use;
    }

    return o/(end-start);
}

//Returns NULL on error, otherwise a double* that needs to be free()d
double *bwStatsFromZoom(bigWigFile_t *fp, int32_t level, uint32_t tid, uint32_t start, uint32_t end, uint32_t nBins, enum bwStatsType type) {
    bwOverlapBlock_t *blocks = NULL;
    double *output = NULL;
    uint32_t span, pos, i;

    if(!fp->hdr->zoomHdrs->idx[level]) {
        fp->hdr->zoomHdrs->idx[level] = bwReadIndex(fp, fp->hdr->zoomHdrs->indexOffset[level]);
        if(!fp->hdr->zoomHdrs->idx[level]) return NULL;
    }

    output = malloc(sizeof(double)*nBins);
    if(!output) return NULL;

    for(i=0, pos=start; i<nBins; i++) {
        if(i==nBins-1) span = end-pos;
        blocks = walkRTreeNodes(fp, fp->hdr->zoomHdrs->idx[i]->root, tid, pos, pos+span);
        if(!blocks) goto error;

        //Decompress if needed
 
        switch(type) {
        case 0:
            //mean
            output[i] = blockMean(fp, blocks);
            break;
        case 1:
            //stdev
            output[i] = blockDev(fp, blocks);
            break;
        case 2:
            //max
            output[i] = blockMax(fp, blocks);
            break;
        case 3:
            //min
            output[i] = blockMin(fp, blocks);
            break;
        case 4:
            //cov
            output[i] = blockCoverage(fp, blocks)/span;
            break;
        default:
            goto error;
            break;
        }
        if(errno) goto error;

        destroyBWOverlapBlock(blocks);
    }

    return output;

error:
    if(blocks) destroyBWOverlapBlock(blocks);
    if(output) free(output);
    return NULL;
}

double *bwStatsFromFull(bigWigFile_t *fp, char *chrom, uint32_t start, uint32_t end, uint32_t nBins, enum bwStatsType type) {
    bwOverlappingIntervals_t *ints = NULL;
    double *output = malloc(sizeof(double)*nBins);
    uint32_t i, pos, span = floor(((double)(end-start))/nBins), end2;
    if(span < 1) return NULL;
    if(!output) return NULL;

    for(i=0; i<nBins; i++) {
        pos = start+i*span;
        end2 = pos+span;
        if(end2 > end || i == nBins-1) end2=end;
        ints = bwGetOverlappingIntervals(fp, chrom, pos, end2);

        if(!ints) {
            output[i] = strtod("NaN", NULL);
            continue;
        }

        switch(type) {
        case 0:
            output[i] = intMean(ints, pos, end2);
            break;
        case 1:
            output[i] = intDev(ints, pos, end2);
            break;
        case 2:
            output[i] = intMax(ints);
            break;
        case 3:
            output[i] = intMin(ints);
            break;
        case 4:
            output[i] = intCoverage(ints, pos, end2);
            break;
        }
        bwDestroyOverlappingIntervals(ints);
    }

    return output;
}

//Returns a list of floats of length nBins that must be free()d
//On error, NULL is returned
double *bwStats(bigWigFile_t *fp, char *chrom, uint32_t start, uint32_t end, uint32_t nBins, enum bwStatsType type) {
    int32_t level = determineZoomLevel(fp, lrint(ceil((double)(end-start)/nBins)));
    uint32_t tid = bwGetTid(fp, chrom);
    if(tid == -1) return NULL;

    if(level == -1) return bwStatsFromFull(fp, chrom, start, end, nBins, type);
    return bwStatsFromZoom(fp, level, tid, start, end, nBins, type);
}
