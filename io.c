#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "io.h"
#include <inttypes.h>

//Fill the buffer, note that URL may be left in an unusable state on error!
//To do: the sleep time and number of cycles should be user modifiable.
CURLcode urlFetchData(URL_t *URL) {
    CURLcode rv = CURLE_AGAIN;
    int i = 0;

    if(URL->filePos != -1) URL->filePos += URL->bufLen;
    else URL->filePos = 0;

    while(rv == CURLE_AGAIN) {
        rv = curl_easy_recv(URL->x.curl, URL->memBuf, URL->bufSize, &(URL->bufLen));
        if(rv == CURLE_OK) {
            URL->bufPos = 0;
            return CURLE_OK;
        } else if(rv == CURLE_AGAIN) {
            if(i>=GLOBAL_DEFAULTNFETCHITERATIONS) return rv;
            i++;
            sleep(GLOBAL_DEFAULTNSECONDS);
            continue;
        }
        return rv;
    }
    return rv;
}

//Read data into a buffer, ideally from a buffer already in memory
size_t url_fread(void *obuf, size_t obufSize, URL_t *URL) {
    size_t remaining = obufSize;
    void *p = obuf;
    CURLcode rv;
    while(remaining) {
        if(URL->bufLen >= URL->bufPos + remaining) { //Copy the remaining buffer and reload the buffer as needed
            p = memmove(p, URL->memBuf+URL->bufPos, URL->bufLen - URL->bufPos);
            if(!p) return 0;
            p += URL->bufLen - URL->bufPos;
            remaining -= URL->bufLen - URL->bufPos;
            //Load more into the buffer, since we're likely to need the next segment
            rv = urlFetchData(URL);
            if(rv != CURLE_OK) {
                fprintf(stderr, "[url_fread] urlFetchData returned %s\n", curl_easy_strerror(rv));
                return 0;
            }
        } else {
            p = memmove(p, URL->memBuf+URL->bufPos, remaining);
            if(!p) return 0;
            URL->bufPos += remaining;
            remaining = 0;
        }
    }
    return obufSize;
}

//Returns the number of bytes requested or a smaller number on error
//Note that in the case of remote files, the actual amount read may be less than the return value!
size_t urlRead(URL_t *URL, void *buf, size_t bufSize) {
    if(URL->type==0) {
        return fread(buf, bufSize, 1, URL->x.fp);
    } else {
        return url_fread(buf, bufSize, URL);
    }
}

//Seek to an arbitrary location, returning a CURLcode
//Note that a local file returns CURLE_OK on success or CURLE_FAILED_INIT on any error;
CURLcode urlSeek(URL_t *URL, size_t pos) {
    if(URL->type == BWG_FILE) {
        if(fseek(URL->x.fp, pos, SEEK_SET) == 0) {
            return CURLE_OK;
        } else {
            return CURLE_FAILED_INIT; //This is arbitrary
        }
    } else {
        //If the location is covered by the buffer then don't seek!
        if(pos < URL->filePos || pos >= URL->filePos+URL->bufSize) {
            URL->filePos = pos;
            URL->bufLen = 0; //Otherwise, filePos will get incremented on the next read!
            return curl_easy_setopt(URL->x.curl, CURLOPT_RESUME_FROM_LARGE, pos);
        } else {
            URL->bufPos = pos-URL->filePos;
            return CURLE_OK;
        }
    }
}

URL_t *urlOpen(char *fname) {
    URL_t *URL = calloc(1, sizeof(URL_t));
    if(!URL) return NULL;
    URL->filePos = -1;

    //Set the protocol
    if(strncmp(fname, "http://", 7) == 0) URL->type = BWG_HTTP;
    else if(strncmp(fname, "https://", 8) == 0) URL->type = BWG_HTTPS;
    else if(strncmp(fname, "ftp://", 6) == 0) URL->type = BWG_FTP;
    else URL->type = BWG_FILE;

    //local file?
    if(URL->type == BWG_FILE) {
        URL->x.fp = fopen(fname, "rb");
        if(!(URL->x.fp)) {
            free(URL);
            fprintf(stderr, "[urlOpen] Couldn't open %s\n", fname);
            return NULL;
        }
    } else {
        //Remote file, set up the memory buffer and get CURL ready
        URL->memBuf = malloc(GLOBAL_DEFAULTBUFFERSIZE);
        if(!(URL->memBuf)) {
            free(URL);
            fprintf(stderr, "[urlOpen] Couldn't allocate enough space for the file buffer!\n");
            return NULL;
        }
        URL->x.curl = curl_easy_init();
        if(!(URL->x.curl)) {
            fprintf(stderr, "[urlOpen] curl_easy_init() failed!\n");
            goto error;
        }
        //Set the URL
        if(curl_easy_setopt(URL->x.curl, CURLOPT_URL, fname) != CURLE_OK) {
            fprintf(stderr, "[urlOpen] Couldn't set CURLOPT_URL!\n");
            goto error;
        }
        //We don't want to automatically load the whole file...
        if(curl_easy_setopt(URL->x.curl, CURLOPT_CONNECT_ONLY, 1L) != CURLE_OK) {
            fprintf(stderr, "[urlOpen] Couldn't set CURLOPT_CONNECT_ONLY!\n");
            goto error;
        }
        if(curl_easy_perform(URL->x.curl) != CURLE_OK) {
            fprintf(stderr, "[urlOpen] curl_easy_perform received an error!\n");
            goto error;
        }
    }
    return URL;

error:
    free(URL->memBuf);
    curl_easy_cleanup(URL->x.curl);
    free(URL);
    return NULL;
}

//Performs the necessary free() operations and handles cleaning up curl
void urlClose(URL_t *URL) {
    if(URL->type == BWG_FILE) {
        fclose(URL->x.fp);
    } else {
        free(URL->memBuf);
        curl_easy_cleanup(URL->x.curl);
    }
    free(URL);
}
