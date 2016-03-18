#include "bigWig.h"
#include <stdio.h>
#include <inttypes.h>
#include <stdlib.h>
#include <assert.h>

//This is an example call back function
CURLcode callBack(CURL *curl) {
    CURLcode rv;

    rv = curl_easy_setopt(curl, CURLOPT_USERNAME, "anonymous");
    if(rv != CURLE_OK) return rv;

    rv = curl_easy_setopt(curl, CURLOPT_PASSWORD, "libBigWig@github.com");
    return rv;
}

int main(int argc, char *argv[]) {
    bigWigFile_t *fp = NULL;
    int64_t i;

    if(argc != 2) {
        fprintf(stderr, "Usage: %s {file.bw|URL://path/file.bw}\n", argv[0]);
        return 1;
    }

    if(bwInit(1<<17) != 0) {
        fprintf(stderr, "Received an error in bwInit\n");
        return 1;
    }

    fp = bwOpen(argv[1], callBack, "r");
    if(!fp) {
        fprintf(stderr, "An error occured while opening %s\n", argv[1]);
        return 1;
    }

    //Return the number of chromosomes/contigs
    for(i=0; i<fp->cl->nKeys; i++) {
        printf("%s\t%"PRIu32"\n", fp->cl->chrom[i], fp->cl->len[i]);
    }

    bwClose(fp);
    bwCleanup();
    return 0;
}
