#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "io.h"
#include <inttypes.h>

int waitForReady(URL_t *URL, int recv, long timeout) {
    fd_set ifd, ofd, efd;
    struct timeval tv;
    curl_socket_t fd;
    long sockextr;
    CURLcode res;

    tv.tv_sec = timeout/1000;
    tv.tv_usec = (timeout % 1000) * 1000;

    FD_ZERO(&ifd);
    FD_ZERO(&ofd);
    FD_ZERO(&efd);

    res = curl_easy_getinfo(URL->x.curl, CURLINFO_LASTSOCKET, &sockextr);
    if(CURLE_OK != res) {
      fprintf(stderr, "[waitForReady] Getting CURLINFO_LASTSOCKET returned the following error: %s\n", curl_easy_strerror(res));
      return 1;
    }
    fd = sockextr;

    FD_SET(fd, &efd);
    if(recv) FD_SET(fd, &ifd);
    else FD_SET(fd, &ofd);

    return select(fd+1, &ifd, &ofd, &efd, &tv);
}

uint64_t parseHeader(URL_t *URL) {
    char *p = strstr(URL->memBuf, "Content-Length:");
    if(!p) return 0;
    return strtoull(p+strlen("Content-Length: "), NULL, 10);
}

uint64_t getContentLength(URL_t *URL) {
    double size;
    if(curl_easy_getinfo(URL->x.curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &size) != CURLE_OK) {
        return parseHeader(URL);
    }
    if(size== -1.0) return parseHeader(URL);
    return (uint64_t) size;
}

//Fill the buffer, note that URL may be left in an unusable state on error!
//To do, what happens if a we don't get a 200/206 response?
//What happens with ftp?
CURLcode urlFetchData(URL_t *URL) {
    CURLcode rv = CURLE_AGAIN;
    size_t len, size;
    char *p;
    int i = 0;

    if(URL->filePos != -1) URL->filePos += URL->bufLen;
    else URL->filePos = 0;

    //Get the first chunk of possibly many
    while(rv == CURLE_AGAIN) {
        rv = curl_easy_recv(URL->x.curl, URL->memBuf+URL->bufLen, URL->bufSize, &len);
        if(rv == CURLE_OK) {
            if(URL->bufLen == 0) {
                URL->bufPos = 0;
                size = getContentLength(URL);
                p = strstr((char*) URL->memBuf, "\r\n\r\n")+4;
                //Will "p" always be legit? What about FTP?
                if(p) {
                    len -= (((void*)p)-URL->memBuf);
                    URL->memBuf = memmove(URL->memBuf, p, len);
                }
            }
            URL->bufLen += len;
            if((URL->bufLen < URL->bufSize) && (URL->bufPos+URL->bufLen-1 < size)) {
                 rv = CURLE_AGAIN;
                 continue;
            }
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
        if(!URL->bufLen) {
            if(!waitForReady(URL, 1, 600)) {
                fprintf(stderr, "[url_fread] (A) waitForReady returned 1!\n");
                return 0;
            }
            rv = urlFetchData(URL);
            if(rv != CURLE_OK) {
                fprintf(stderr, "[url_fread] urlFetchData returned %s\n", curl_easy_strerror(rv));
                return 0;
            }  
        } else if(URL->bufLen < URL->bufPos + remaining) { //Copy the remaining buffer and reload the buffer as needed
            p = memmove(p, URL->memBuf+URL->bufPos, URL->bufLen - URL->bufPos);
            if(!p) return 0;
            p += URL->bufLen - URL->bufPos;
            remaining -= URL->bufLen - URL->bufPos;
            //Load more into the buffer, since we're likely to need the next segment
            if(!waitForReady(URL, 1, 600)) {
                fprintf(stderr, "[url_fread] (B) waitForReady returned 1!\n");
                return 0;
            }
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

//This doesn't currently work with FTP files!
char *getRequest(char *fname, uint64_t start, uint64_t len, enum bigWigFile_type_enum type) {
    char *p1, *p2, *request;
    switch(type) {
    case BWG_HTTP :
        p1 = fname + 7;
        break;
    case BWG_HTTPS :
        p1 = fname + 8;
        break;
    default :
        fprintf(stderr, "[getRequest] ftp isn't yet supported\n");
        return NULL;
    }
    p2 = strchr(p1, '/');
    if(!p2) return NULL;

    *(p2++) = '\0';

    request = malloc(strlen(p1) + strlen(p2) + 33 + 1000);
    if(!request) return NULL;

    sprintf(request, "GET /%s HTTP/1.0\r\nHost: %s\r\nRange: bytes=%"PRIu64"-%"PRIu64"\r\n\r\n", p2, p1, start, start+len-1);
    *(--p2) = '/'; //Don't forget to replace the "/"!

    return request;
}

//Seek to an arbitrary location, returning a CURLcode
//Note that a local file returns CURLE_OK on success or CURLE_FAILED_INIT on any error;
CURLcode urlSeek(URL_t *URL, size_t pos) {
    char range[1024], *req = NULL;
    CURLcode rv;
    size_t l;

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
            //Maybe this works for FTP?
            sprintf(range,"%"PRIu64"-%"PRIu64, pos, pos+URL->bufSize-1);
            rv = curl_easy_setopt(URL->x.curl, CURLOPT_RANGE, range);
            if(rv != CURLE_OK) {
                fprintf(stderr, "[urlSeek] Couldn't set the range (%s)\n", range);
                return rv;
            }
            rv = curl_easy_perform(URL->x.curl);
            if(rv != CURLE_OK) {
                fprintf(stderr, "[urlSeek] curl_easy_perform received an error!\n");
                return rv;
            }
            if(!waitForReady(URL, 0, 600)) { //1 minute timeout, this should be a global
                return CURLE_FAILED_INIT; //Arbitrary
            }
            req = getRequest(URL->fname, pos, URL->bufSize, URL->type);
            if(!req) {
                fprintf(stderr, "[urlSeek] Somehow, we couldn't construct the request?!?\n");
                return CURLE_FAILED_INIT;
            }
            rv = curl_easy_send(URL->x.curl, req, strlen(req), &l);
            free(req);
            return rv;
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
    char *url = NULL, *req = NULL, *p;
    size_t l;
    CURLcode code;
    char range[1024];

    URL->fname = fname;

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
        //Construct the URL/file name needed for the request
        url = strdup(fname);
        if(!url) {
            fprintf(stderr, "[urlOpen] Not enough free memory to make the url!\n");
            return NULL;
        }
        p = strchr(url+8,'/');
        if(!p) {
            //There's no "/" in the file name, this can't be correct
            fprintf(stderr, "[urlOpen] The URL must contain a file name!\n");
            return NULL;
        }
        *p = '\0';
        req = getRequest(URL->fname, 0, GLOBAL_DEFAULTBUFFERSIZE, URL->type);
        if(!req) {
            free(url);
            fprintf(stderr, "[urlOpen] Couldn't construct the URL!\n");
        }
        //Remote file, set up the memory buffer and get CURL ready
        URL->memBuf = malloc(GLOBAL_DEFAULTBUFFERSIZE);
        if(!(URL->memBuf)) {
            free(URL);
            fprintf(stderr, "[urlOpen] Couldn't allocate enough space for the file buffer!\n");
            return NULL;
        }
        URL->bufSize = GLOBAL_DEFAULTBUFFERSIZE;
        URL->x.curl = curl_easy_init();
        if(!(URL->x.curl)) {
            fprintf(stderr, "[urlOpen] curl_easy_init() failed!\n");
            goto error;
        }
        //Follow redirects
        if(curl_easy_setopt(URL->x.curl, CURLOPT_FOLLOWLOCATION, 1L) != CURLE_OK) {
            fprintf(stderr, "[urlOpen] Failed instructing curl to follow redirects!\n");
            goto error;
        }
        //Set the URL
        if(curl_easy_setopt(URL->x.curl, CURLOPT_URL, url) != CURLE_OK) {
            fprintf(stderr, "[urlOpen] Couldn't set CURLOPT_URL!\n");
            goto error;
        }
        //We don't want to automatically load the whole file...
        if(curl_easy_setopt(URL->x.curl, CURLOPT_CONNECT_ONLY, 1L) != CURLE_OK) {
            fprintf(stderr, "[urlOpen] Couldn't set CURLOPT_CONNECT_ONLY!\n");
            goto error;
        }
        //Set the range, which doesn't do anything for HTTP
        sprintf(range, "0-%"PRIu64, URL->bufSize-1);
        if(curl_easy_setopt(URL->x.curl, CURLOPT_RANGE, range) != CURLE_OK) {
            fprintf(stderr, "[urlOpen] Couldn't set CURLOPT_RANGE (%s)!\n", range);
            goto error;
        }
        if(curl_easy_perform(URL->x.curl) != CURLE_OK) {
            fprintf(stderr, "[urlOpen] curl_easy_perform received an error!\n");
            goto error;
        }
        //Wait for the socket to become active
        if(!waitForReady(URL, 0, 600)) { //1 minute timeout, this should be a global
            goto error;
        }
        code = curl_easy_send(URL->x.curl, req, strlen(req), &l);
        if(code != CURLE_OK) {
            fprintf(stderr, "[urlOpen] The initial curl_easy_send had an error: %s\n", curl_easy_strerror(code));
            goto error;
        }
    }
    if(url) free(url);
    if(req) free(req);
    return URL;

error:
    if(url) free(url);
    if(req) free(req);
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
