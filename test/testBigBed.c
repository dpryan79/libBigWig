#include "bigWig.h"
#include <stdio.h>
#include <inttypes.h>
#include <stdlib.h>
#include <assert.h>

void bwPrintHdr(bigWigFile_t *bb) {
    uint64_t i;
    int64_t i64;
    printf("Version:    %"PRIu16"\n", bb->hdr->version);
    printf("Levels:     %"PRIu16"\n", bb->hdr->nLevels);
    printf("ctOffset:   0x%"PRIx64"\n", bb->hdr->ctOffset);
    printf("dataOffset: 0x%"PRIx64"\n", bb->hdr->dataOffset);
    printf("indexOffset:        0x%"PRIx64"\n", bb->hdr->indexOffset);
    printf("fieldCount: %"PRIu32"\n", bb->hdr->fieldCount);
    printf("definedFieldCount: %"PRIu32"\n", bb->hdr->definedFieldCount);
    printf("sqlOffset:  0x%"PRIx64"\n", bb->hdr->sqlOffset);
    printf("summaryOffset:      0x%"PRIx64"\n", bb->hdr->summaryOffset);
    printf("bufSize:    %"PRIu32"\n", bb->hdr->bufSize);

    if(bb->hdr->nLevels) {
        printf("	i	level	data	index\n");
    }
    for(i=0; i<bb->hdr->nLevels; i++) {
        printf("\t%"PRIu64"\t%"PRIu32"\t%"PRIx64"\t%"PRIx64"\n", i, bb->hdr->zoomHdrs->level[i], bb->hdr->zoomHdrs->dataOffset[i], bb->hdr->zoomHdrs->indexOffset[i]);
    }

    printf("nBasesCovered:      %"PRIu64"\n", bb->hdr->nBasesCovered);
    printf("minVal:     %f\n", bb->hdr->minVal);
    printf("maxVal:     %f\n", bb->hdr->maxVal);
    printf("sumData:    %f\n", bb->hdr->sumData);
    printf("sumSquared: %f\n", bb->hdr->sumSquared);

    //Chromosome idx/name/length
    if(bb->cl) {
        printf("Chromosome List\n");
        printf("  idx\tChrom\tLength (bases)\n");
        for(i64=0; i64<bb->cl->nKeys; i64++) {
            printf("  %"PRIu64"\t%s\t%"PRIu32"\n", i64, bb->cl->chrom[i64], bb->cl->len[i64]);
        }
    }
}

void bwPrintIndexNode(bwRTreeNode_t *node, int level) {
    uint16_t i;
    if(!node) return;
    for(i=0; i<node->nChildren; i++) {
        if(node->isLeaf) {
            printf("  %i\t%"PRIu32"\t%"PRIu32"\t%"PRIu32"\t%"PRIu32"\t0x%"PRIx64"\t%"PRIu64"\n", level,\
                node->chrIdxStart[i], \
                node->baseStart[i], \
                node->chrIdxEnd[i], \
                node->baseEnd[i], \
                node->dataOffset[i], \
                node->x.size[i]);
        } else {
            printf("  %i\t%"PRIu32"\t%"PRIu32"\t%"PRIu32"\t%"PRIu32"\t0x%"PRIx64"\tNA\n", level,\
                node->chrIdxStart[i], \
                node->baseStart[i], \
                node->chrIdxEnd[i], \
                node->baseEnd[i], \
                node->dataOffset[i]);
            bwPrintIndexNode(node->x.child[i], level+1);
        }
    }
}

void bwPrintIndexTree(bigWigFile_t *fp) {
    printf("\nIndex tree:\n");
    printf("nItems:\t%"PRIu64"\n", fp->idx->nItems);
    printf("chrIdxStart:\t%"PRIu32"\n", fp->idx->chrIdxStart);
    printf("baseStart:\t%"PRIu32"\n", fp->idx->baseStart);
    printf("chrIdxEnd:\t%"PRIu32"\n", fp->idx->chrIdxEnd);
    printf("baseEnd:\t%"PRIu32"\n", fp->idx->baseEnd);
    printf("idxSize:\t%"PRIu64"\n", fp->idx->idxSize);
    printf("  level\tchrIdxStart\tbaseStart\tchrIdxEnd\tbaseEnd\tchild\tsize\n");
    bwPrintIndexNode(fp->idx->root, 0);
}

int main(int argc, char *argv[]) {
    bigWigFile_t *fp = NULL;
    bbOverlappingEntries_t *o;
    uint32_t i;
    char *sql;
    if(argc != 2) {
        fprintf(stderr, "Usage: %s {file.bb|URL://path/file.bb}\n", argv[0]);
        return 1;
    }

    if(bwInit(1<<17) != 0) {
        fprintf(stderr, "Received an error in bwInit\n");
        return 1;
    }

    assert(bwIsBigWig(argv[1], NULL) == 0);
    assert(bbIsBigBed(argv[1], NULL) == 1);

    fp = bbOpen(argv[1], NULL);
    if(!fp) {
        fprintf(stderr, "An error occured while opening %s\n", argv[1]);
        return 1;
    }

    bwPrintHdr(fp);
    bwPrintIndexTree(fp);

    sql = bbGetSQL(fp);
    if(sql) {
        printf("SQL is: %s\n", sql);
        free(sql);
    }

    //Presumably this is the sort of interface that's needed...
    o = bbGetOverlappingEntries(fp, "chr1", 4450000, 4500000, 1);
    printf("%"PRIu32" entries overlap\n", o->l);
    for(i=0; i<o->l; i++) {
        printf("%"PRIu32"-%"PRIu32"\t %s\n", o->start[i], o->end[i], o->str[i]);
    }
    if(o) bbDestroyOverlappingEntries(o);

    //Ensure that we can fetch entries with no strings
    o = bbGetOverlappingEntries(fp, "chr1", 4450000, 4500000, 0);
    printf("%"PRIu32" entries overlap\n", o->l);
    for(i=0; i<o->l; i++) {
        printf("%"PRIu32"-%"PRIu32"\n", o->start[i], o->end[i]);
    }
    if(o) bbDestroyOverlappingEntries(o);

    bwClose(fp);
    bwCleanup();
    return 0;
}
