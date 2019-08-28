#include "bigWig.h"
#include <stdio.h>
#include <inttypes.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>

int almost_equal(float a, float b) {
	return fabs(a - b) < 1e-8;
}

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

    if(bwCreateHdr(ofp, 10)) goto error; //ten zoom levels
    ofp->cl = bwCreateChromList(ifp->cl->chrom, ifp->cl->len, ifp->cl->nKeys);
    if(!ofp->cl) goto error;

    if(bwWriteHdr(ofp)) goto error;
    bwClose(ifp);

    //Copy all of the intervals
    char *chroms[] = {"1", "1", "1", "1"};

    uint32_t starts[] = {100, 3000, 40004, 49001};
    uint32_t stops[] =  {101, 3044, 40068, 50000};
    float values[] = {10.1, 100.1, 1000.1, 5000.1};
    bwAddIntervals(ofp, chroms, starts, stops, values, 4);
    bwClose(ofp);


    ifp = bwOpen(argv[2], NULL, "r");
    if(!ifp) {
        fprintf(stderr, "An error occured while opening for reading %s\n", argv[2]);
        return 1;
    }

    float *output = malloc(sizeof(float)*50000);

    bwGetOverlappingValues(ifp, "1", 0, 50000, 0.0, output);
    int i;
    for(i=0; i < 100;i++){
        if(!almost_equal(output[i], 0.0)) { fprintf(stderr, "bad missing value at output[%d]: %.1f\n", i, output[i]); }
    }
    for(i=100; i < 101;i++){
        if(!almost_equal(output[i], 10.1)) { fprintf(stderr, "bad float value at output[%d]: %.1f\n", i, output[i]); }
    }
    for(i=3000; i < 3044;i++){
        if(!almost_equal(output[i], 100.1)) { fprintf(stderr, "bad float value at output[%d]: %.1f\n", i, output[i]); }
    }
    for(i=40004; i < 40068;i++){
        if(!almost_equal(output[i], 1000.1)) { fprintf(stderr, "bad float value at output[%d]: %.1f\n", i, output[i]); }
    }
    for(i=49001; i < 50000;i++){
        if(!almost_equal(output[i], 5000.1)) { fprintf(stderr, "bad float value at output[%d]: %.1f\n", i, output[i]); }
    }

    //fprintf(stderr, "output[3011] should be 100.1 %.1f\n", output[3011]);

    free(output);
    bwClose(ifp);
    bwCleanup();
    fprintf(stdout, "OK\n");

    return 0;

error:
    fprintf(stderr, "Got an error somewhere!\n");
    bwClose(ifp);
    bwClose(ofp);
    bwCleanup();
    return 1;
}
