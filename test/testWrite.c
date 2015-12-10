#include "bigWig.h"
#include <stdio.h>
#include <inttypes.h>
#include <stdlib.h>
#include <assert.h>

int main(int argc, char *argv[]) {
    bigWigFile_t *ifp = NULL;
    bigWigFile_t *ofp = NULL;
    uint32_t tid, i;
    char **chroms;
    bwOverlappingIntervals_t *o;
    if(argc != 3) {
        fprintf(stderr, "Usage: %s {inputfile.bw|URL://path/inputfile.bw} outputfile.bw\n", argv[0]);
        return 1;
    }

    if(bwInit(1<<17) != 0) {
        fprintf(stderr, "Received an error in bwInit\n");
        return 1;
    }

    ifp = bwOpen(argv[1], NULL, "r");
    if(!ifp) {
        fprintf(stderr, "An error occured while opening %s\n", argv[1]);
        return 1;
    }

    ofp = bwOpen(argv[2], NULL, "w");
    if(!ofp) {
        bwClose(ifp);
        fprintf(stderr, "An error occured while opening %s\n", argv[2]);
        return 1;
    }

    if(bwCreateHdr(ofp, 10)) goto error; //ten zoom levels
    ofp->cl = bwCreateChromList(ifp->cl->chrom, ifp->cl->len, ifp->cl->nKeys);
    if(!ofp->cl) goto error;

    if(bwWriteHdr(ofp)) goto error;

    //Copy all of the intervals
    for(tid = 0; tid < ofp->cl->nKeys; tid++) {
        o = bwGetOverlappingIntervals(ifp, ofp->cl->chrom[tid], 0, ofp->cl->len[tid]);
        if(!o) goto error;
        if(o->l) {
            chroms = malloc(o->l * sizeof(char*));
            if(!chroms) goto error;
            for(i=0; i<o->l; i++) chroms[i] = ofp->cl->chrom[tid];
            bwAddIntervals(ofp, chroms, o->start, o->end, o->value, o->l);
            free(chroms);
        }
        bwDestroyOverlappingIntervals(o);
    }

    bwClose(ifp);
    bwClose(ofp);
    bwCleanup();

    return 0;

error:
    fprintf(stderr, "Got an error somewhere!\n");
    bwClose(ifp);
    bwClose(ofp);
    bwCleanup();
    return 1;
}
