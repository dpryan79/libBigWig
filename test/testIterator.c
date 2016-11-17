#include "bigWig.h"
#include <stdio.h>
#include <inttypes.h>
#include <stdlib.h>
#include <assert.h>

int main(int argc, char *argv[]) {
    bigWigFile_t *fp = NULL;
    uint32_t i, chunk = 0, tid, blocksPerIteration;
    char *sql, *chrom;
    bwOverlapIterator_t *iter;
    if(argc != 4) {
        fprintf(stderr, "Usage: %s {file.bb|URL://path/file.bb} chromosome blocksPerIteration\n", argv[0]);
        return 1;
    }
    chrom = argv[2];
    blocksPerIteration = strtoul(argv[3], NULL, 10);

    if(bwInit(1<<17) != 0) {
        fprintf(stderr, "Received an error in bwInit\n");
        return 1;
    }

    if(bwIsBigWig(argv[1], NULL)) {
        fp = bwOpen(argv[1], NULL, "r");
    } else if(bbIsBigBed(argv[1], NULL)) {
        fp = bbOpen(argv[1], NULL);
    }

    if(!fp) {
        fprintf(stderr, "An error occured while opening %s\n", argv[1]);
        return 1;
    }

    sql = bbGetSQL(fp);
    if(sql) {
        printf("SQL is: %s\n", sql);
        free(sql);
    }

    //So we can get the bounds
    tid = bwGetTid(fp, chrom);

    if(fp->type == 0) {
        iter = bwOverlappingIntervalsIterator(fp, chrom, 0, fp->cl->len[tid], blocksPerIteration);
    } else {
        iter = bbOverlappingEntriesIterator(fp, chrom, 0, fp->cl->len[tid], 1, blocksPerIteration);
    }
    while(iter->data) {
        if(fp->type == 0) {
            for(i=0; i<iter->intervals->l; i++) {
                printf("chunk %"PRIu32" entry %"PRIu32" %s:%"PRIu32"-%"PRIu32" %f\n", chunk, i, chrom, iter->intervals->start[i], iter->intervals->end[i], iter->intervals->value[i]);
            }
        } else {
            for(i=0; i<iter->entries->l; i++) {
                printf("chunk %"PRIu32" entry %"PRIu32" %s:%"PRIu32"-%"PRIu32" %s\n", chunk, i, chrom, iter->entries->start[i], iter->entries->end[i], iter->entries->str[i]);
            }
        }
        chunk++;
        iter = bwIteratorNext(iter);
    }
    bwIteratorDestroy(iter);

    bwClose(fp);
    bwCleanup();
    return 0;
}
