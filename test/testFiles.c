#include "bigWig.h"
#include <stdio.h>
#include <inttypes.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>
#include <time.h>

int almost_equal(float a, float b) {
	return fabs(a - b) < 1e-8;
}

int main(int argc, char *argv[]) {
    bigWigFile_t *afp = NULL;
    bigWigFile_t *bfp = NULL;
    if(argc != 4) {
	// see if the values in 2 bigwig files are identical
        fprintf(stderr, "Usage: %s a.bw b.bw chrom\n", argv[0]);
        return 1;
    }
    char *chrom = argv[3];

    afp = bwOpen(argv[1], NULL, "r");
    if(!afp) {
        fprintf(stderr, "An error occured while opening %s\n", argv[1]);
        return 1;
    }
    bfp = bwOpen(argv[2], NULL, "r");
    if(!bfp) {
        fprintf(stderr, "An error occured while opening %s\n", argv[2]);
        return 1;
    }

    uint32_t atid = bwGetTid(afp, chrom);
    if(atid == UINT32_MAX) {
	    fprintf(stderr, "chromosome: %s not found in %s\n", chrom, argv[1]);
	    return 1;
    }
    uint32_t btid = bwGetTid(bfp, chrom);
    if(btid == UINT32_MAX) {
	    fprintf(stderr, "chromosome: %s not found in %s\n", chrom, argv[2]);
	    return 1;
    }
    if (afp->cl->len[atid] != bfp->cl->len[btid]) {
	    fprintf(stderr, "differing lenghts for chromosome %s. %d vs %d\n",
		    chrom, afp->cl->len[atid], bfp->cl->len[btid]);
	    return 1;
    }
    uint32_t i;
    clock_t start = clock();
    float* avalues = malloc(sizeof(float) * afp->cl->len[atid]);
    for(i=0;i<afp->cl->len[atid];i++) { avalues[i] = 0; }
    clock_t end = clock();
    fprintf(stderr, "time to allocate: %.3f seconds\n", ((double) (end - start)) / CLOCKS_PER_SEC);


    float* bvalues = malloc(sizeof(float) * bfp->cl->len[btid]);
    for(i=0;i<bfp->cl->len[atid];i++) { bvalues[i] = 0; }
    int ret = 0;

     /*
     it = bwOverlappingIntervalsIterator(bfp, chrom, 0, bfp->cl->len[btid], 10000);
     while(it->data != NULL){
       for(i=0;i<it->intervals->l;i++){
	  for(p=it->intervals->start[i];p<it->intervals->end[i];p++){
		  bvalues[p] = it->intervals->value[i];
	  }
       }
       it = bwIteratorNext(it);
     }
     bwIteratorDestroy(it);
     */
    start = clock();
    bwGetOverlappingValues(bfp, chrom, 0, bfp->cl->len[btid], 0, bvalues);
    end = clock();
    fprintf(stderr, "get values in %.3f seconds\n", ((double) (end - start)) / CLOCKS_PER_SEC);

    start = clock();
    bwOverlapIterator_t *it = bwOverlappingIntervalsIterator(afp, chrom, 0, afp->cl->len[atid], 10000);

    uint32_t p;
    while(it->data != NULL){
      for(i=0;i<it->intervals->l;i++){
	  for(p=it->intervals->start[i];p<it->intervals->end[i];p++){
		  avalues[p] = it->intervals->value[i];
	  }
       }
       it = bwIteratorNext(it);
    }
    bwIteratorDestroy(it);
    end = clock();
    fprintf(stderr, "iterator in %.3f seconds\n", ((double) (end - start)) / CLOCKS_PER_SEC);


    for(i=0;i<afp->cl->len[atid];i++){
	    if(avalues[i] != bvalues[i]) {
		    fprintf(stderr, "different values at %d. %.2f vs %.2f\n", i, avalues[i], bvalues[i]);
		    ret = 1;
	    }
    }


    free(avalues);
    free(bvalues);

    //free(output);
    bwClose(afp);
    bwClose(bfp);
    bwCleanup();
    if(ret == 0) {
    fprintf(stdout, "OK\n");
    }
    return ret;
}
