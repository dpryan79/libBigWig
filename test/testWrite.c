#include "bigWig.h"
#include <stdio.h>
#include <inttypes.h>
#include <stdlib.h>
#include <assert.h>

int main(int argc, char *argv[]) {
    bigWigFile_t *ifp = NULL;
    bigWigFile_t *ofp = NULL;
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

    ofp->hdr = bwCreateHdr(0, 1); //No zoom levels but compressed
    if(!ofp->hdr) goto error;
    ofp->cl = bwCreateChromList(ifp->cl->chrom, ifp->cl->len, ifp->cl->nKeys);
    if(!ofp->cl) goto error;

    if(bwWriteHdr(ofp)) goto error;

    bwClose(ifp);
    bwClose(ofp);
    bwCleanup();

    return 0;

error:
    bwClose(ifp);
    bwClose(ofp);
    bwCleanup();
    return 1;
}
