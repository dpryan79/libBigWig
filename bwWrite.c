#include <limits.h>
#include <float.h>
#include <stdlib.h>
#include <string.h>
#include "bigWig.h"
//bwAddType1(bw, chrom, start, end, values, n)
//bwAddType2(bw, chrom, start, span, values, n)
//bwAddType3(bw, chrom, start, span, step, values, n)
//bwFinalize(bw)
//bwClose(bw)

//Create a chromList_t and attach it to a bigWigFile_t *. Returns NULL on error
//Note that chroms and lengths are duplicated, so you MUST free the input
chromList_t *bwCreateChromList(char **chroms, uint32_t *lengths, int64_t n) {
    int64_t i = 0;
    chromList_t *cl = calloc(1, sizeof(chromList_t));
    if(!cl) return NULL;

    cl->nKeys = n;
    cl->chrom = malloc(sizeof(char*)*n);
    cl->len = malloc(sizeof(uint32_t)*n);
    if(!cl->chrom) goto error;
    if(!cl->len) goto error;

    for(i=0; i<n; i++) {
        cl->len[i] = lengths[i];
        cl->chrom[i] = strdup(chroms[i]);
        if(!cl->chrom[i]) goto error;
    }

    return cl;

error:
    if(i) {
        int64_t j;
        for(j=0; j<i; j++) free(cl->chrom[j]);
    }
    if(cl) {
        if(cl->chrom) free(cl->chrom);
        if(cl->len) free(cl->len);
        free(cl);
    }
    return NULL;
}

//If maxZooms == 0, then 0 is used (i.e., there are no zoom levels). If maxZooms < 0 or > 65535 then it's autocomputed.
bigWigHdr_t *bwCreateHdr(int32_t maxZooms, int compress) {
    bigWigHdr_t *hdr = calloc(1, sizeof(bigWigHdr_t));
    if(!hdr) return NULL;

    hdr->version = 4;
    if(maxZooms < 0 || maxZooms > 65535) {
        hdr->nLevels = 10;
    } else {
        hdr->nLevels = 0;
    }

    hdr->bufSize = 0;
    if(compress) hdr->bufSize = 32768;
    hdr->minVal = DBL_MAX;
    hdr->maxVal = DBL_MIN;

    return hdr;
}

//return 0 on success
static int writeAtPos(void *ptr, size_t sz, size_t nmemb, size_t pos, FILE *fp) {
    size_t curpos = ftell(fp);
    if(fseek(fp, pos, SEEK_SET)) return 1;
    if(fwrite(ptr, sz, nmemb, fp) != nmemb) return 2;
    if(fseek(fp, curpos, SEEK_SET)) return 3;
    return 0;
}

//Are nblocks and nperblock correct?
//We lose keySize bytes on error
static int writeChromList(FILE *fp, chromList_t *cl) {
    uint16_t k;
    uint32_t j, magic = CIRTREE_MAGIC;
    uint32_t nperblock = (cl->nKeys>0xFFFF)?-1:cl->nKeys; //Items per leaf/non-leaf
    uint32_t nblocks = (cl->nKeys>>16)+1, keySize = 0, valSize = 8; //does the valSize even matter? I ignore it...
    uint64_t i, written = 0;
    uint8_t eight;
    char *chrom;
    size_t l;

    for(i=0; i<cl->nKeys; i++) {
        l = strlen(cl->chrom[i]);
        if(l>keySize) keySize = l;
    }
    l--; //We don't null terminate strings, because schiess mich tot
    chrom = calloc(keySize, sizeof(char));

    //Write the root node of a largely pointless tree
    if(fwrite(&magic, sizeof(uint32_t), 1, fp) != 1) return 1;
    if(fwrite(&nperblock, sizeof(uint32_t), 1, fp) != 1) return 2;
    if(fwrite(&keySize, sizeof(uint32_t), 1, fp) != 1) return 3;
    if(fwrite(&valSize, sizeof(uint32_t), 1, fp) != 1) return 4;
    if(fwrite(&(cl->nKeys), sizeof(uint64_t), 1, fp) != 1) return 5;

    //Padding?
    i=0;
    if(fwrite(&i, sizeof(uint64_t), 1, fp) != 1) return 6;

    //Do we need a non-leaf node?
    if(nblocks>1) {
        eight = 0;
        if(fwrite(&eight, sizeof(uint8_t), 1, fp) != 1) return 7;
        if(fwrite(&eight, sizeof(uint8_t), 1, fp) != 1) return 8; //padding
        j = 0;
        for(i=0; i<nperblock; i++) { //Why yes, this is pointless
            if(fwrite(chrom, keySize, 1, fp) != 1) return 9;
            if(fwrite(&j, sizeof(uint64_t), 1, fp) != 1) return 10;
        }
    }

    //Write the leaves
    for(i=0, j=0; i<nblocks; i++) {
        eight = 1;
        if(fwrite(&eight, sizeof(uint8_t), 1, fp) != 1) return 11;
        eight = 0;
        if(fwrite(&eight, sizeof(uint8_t), 1, fp) != 1) return 12;
        if(cl->nKeys - written < nperblock) nperblock = cl->nKeys - written;
        if(fwrite(&nperblock, sizeof(uint16_t), 1, fp) != 1) return 13;
        for(k=0; k<nperblock; k++) {
            if(j>=cl->nKeys) return 14;
            chrom = strncpy(chrom, cl->chrom[j], keySize);
            if(fwrite(chrom, keySize, 1, fp) != 1) return 15;
            if(fwrite(&j, sizeof(uint32_t), 1, fp) != 1) return 16;
            if(fwrite(&(cl->len[j++]), sizeof(uint32_t), 1, fp) != 1) return 17;
            written++;
        }
    }

    free(chrom);
    return 0;
}

//returns 0 on success
//Still need to fill in indexOffset, 
int bwWriteHdr(bigWigFile_t *bw) {
    uint32_t magic = BIGWIG_MAGIC;
    uint16_t two = 4;
    FILE *fp;
    void *p = calloc(58, sizeof(uint8_t)); //58 bytes of nothing
    if(!bw->isWrite) return 1;

    //The header itself, largely just reserving space...
    fp = bw->URL->x.fp;
    if(!fp) return 2;
    if(fseek(fp, 0, SEEK_SET)) return 3;
    if(fwrite(&magic, sizeof(uint32_t), 1, fp) != 1) return 4;
    if(fwrite(&two, sizeof(uint16_t), 1, fp) != 1) return 5;
    if(fwrite(p, sizeof(uint8_t), 58, fp) != 58) return 6;

    //Write the chromosome list as a stupid freaking tree (because let's TREE ALL THE THINGS!!!)
    bw->hdr->ctOffset = ftell(fp);
    if(writeChromList(fp, bw->cl)) return 7;
    if(writeAtPos(&(bw->hdr->ctOffset), sizeof(uint64_t), 1, 0x8, fp)) return 8;

    //Empty zoom headers
    if(bw->hdr->nLevels) {
        for(two=0; two<bw->hdr->nLevels; two++) {
            if(fwrite(p, sizeof(uint8_t), 12, fp) != 12) return 9;
        }
    }

    //Update summaryOffset and write an empty summary block
    bw->hdr->summaryOffset = ftell(fp);
    if(fwrite(p, sizeof(uint8_t), 20, fp) != 20) return 10;
    if(writeAtPos(&(bw->hdr->summaryOffset), sizeof(uint64_t), 1, 0x2c, fp)) return 11;

    //Update the dataOffset
    bw->hdr->dataOffset = ftell(fp);
    if(writeAtPos(&bw->hdr->dataOffset, sizeof(uint64_t), 1, 0x10, fp)) return 12;

    //Save space for the number of blocks
    if(fwrite(p, sizeof(uint8_t), 8, fp) != 8) return 13;

    free(p);
    return 0;
}

/*
opening a bigWigFile_t should create a buffer for storing data entries

data block:
uint64_t number of blocks (2 / 110851)
some blocks

data block index header
uint32_t magic
uint32_t blockSize (256 in the example) maximum number of children
uint64_t number of blocks (2 / 110851)
uint32_t startTid
uint32_t startPos
uint32_t endTid
uint32_t endPos
uint64_t index size? (0x1E7 / 0x1AF0401F) index address?
uint32_t itemsPerBlock (1 / 1)
uint32_t padding

data block index node non-leaf
uint8_t isLeaf
uint8_t padding
uint16_t nChildren (2, 256)
uint32_t startTid
uint32_t startPos
uint32_t endTid
uint32_t endPos
uint64_t dataOffset (0x1AF05853, 0x1AF07057)

data block index node leaf
uint8_t isLeaf
uint8_t padding
uint16_t nChildren (2)
uint32_t startTid
uint32_t startPos
uint32_t endTid
uint32_t endPos
uint64_t dataOffset (0x198, 0x1CF)
uint64_t dataSize (55, 24)
*/
