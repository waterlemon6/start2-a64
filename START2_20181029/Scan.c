#include <stdio.h>
#include <sys/time.h>
#include <unistd.h>
#include "Scan.h"

extern int Y_RTable[256], Y_GTable[256], Y_BTable[256];
extern int Cr_RTable[256], Cr_GTable[256], Cr_BTable[256];
extern int Cb_RTable[256], Cb_GTable[256], Cb_BTable[256];
extern unsigned char Y_YTable[256];

struct timeval tv_scan1, tv_scan2;

void ScanTimeSet(void)
{
    struct timeval tv;
    gettimeofday(&tv_scan1, NULL);
    usleep(10);
    gettimeofday(&tv, NULL);
    if(timercmp(&tv, &tv_scan1, <))
        tv_scan1 = tv;
    tv_scan2 = tv_scan1;
    PR("Scan sec:%lu, usec:%lu\n", tv_scan1.tv_sec, tv_scan1.tv_usec);
}

long ScanLinesGet(int dpi, int depth)
{
    __time_t scanTime;
    struct timeval tv;
    gettimeofday(&tv, NULL);
    while((tv.tv_sec - tv_scan2.tv_sec > 2) || (tv_scan2.tv_sec - tv.tv_sec > 2)) {
        gettimeofday(&tv, NULL);
        PR("Error in getting time.\n");
        usleep(10);
    }
    tv_scan2 = tv;
    scanTime = (tv_scan2.tv_sec - tv_scan1.tv_sec) * 1000000 + tv_scan2.tv_usec - tv_scan1.tv_usec;

    switch (dpi) {
        case 200:
            if (depth == 3)
                return ((scanTime / SCAN_TIME_PER_LINE_200DPI) / depth);
            else
                return (scanTime / SCAN_TIME_PER_LINE_200DPI_ONE_CHANNEL);
        case 300:
            return ((scanTime / SCAN_TIME_PER_LINE_300DPI) / depth);
        case 600:
            return ((scanTime / SCAN_TIME_PER_LINE_600DPI) / depth);
        default:
            return 0;
    }
}

void ScanTimeShow(void)
{
    __time_t scanTime = (tv_scan2.tv_sec - tv_scan1.tv_sec) * 1000000 + tv_scan2.tv_usec - tv_scan1.tv_usec;;
    PR("scan sec:%lu, usec:%lu, scan time:%lu\n", tv_scan2.tv_sec, tv_scan2.tv_usec, scanTime);
}

int CompressProcessPrepare(unsigned char page, ConfigMessageTypeDef *ConfigMessage, CompressProcessTypeDef *CompressProcess, int videoPortOffset)
{
    unsigned short offset = 0;

    CompressProcess->imageAttr.color = ConfigMessage->color;
    switch (CompressProcess->imageAttr.color) {
        case 'C':
            CompressProcess->imageAttr.depth = 3;
            break;
        case 'G':
        case 'I':
            CompressProcess->imageAttr.depth = 1;
            break;
        default:
            return 0;
    }

    CompressProcess->imageAttr.dpi = ConfigMessage->dpi;
    CompressProcess->frame = 1;
    CompressProcess->rowsPerLine = 3;
    switch (CompressProcess->imageAttr.dpi) {
        case 200:
            offset = CIS_WIDTH_200DPI;
            if(CompressProcess->imageAttr.depth == 3) {
                CompressProcess->frame = 1;
                CompressProcess->rowsPerLine = 3;
            }
            else  {
                CompressProcess->frame = 1;
                CompressProcess->rowsPerLine = 1;
            }
            break;
        case 300:
            offset = CIS_WIDTH_300DPI;
            if(CompressProcess->imageAttr.depth == 3) {
                CompressProcess->frame = 2;
                CompressProcess->rowsPerLine = 3;
            }
            else  {
                CompressProcess->frame = 1;
                CompressProcess->rowsPerLine = 1;
            }
            break;
        case 600:
            offset = CIS_WIDTH_600DPI;
            if(CompressProcess->imageAttr.depth == 3) {
                CompressProcess->frame = 6;
                CompressProcess->rowsPerLine = 6;
            }
            else  {
                CompressProcess->frame = 2;
                CompressProcess->rowsPerLine = 2;
            }
            break;
        default:
            return 0;
    }

    switch (page) {
        case PAGE_TOP:
            CompressProcess->imageAttr.leftEdge = ConfigMessage->topLeftEdge + offset + videoPortOffset;
            CompressProcess->imageAttr.rightEdge = ConfigMessage->topRightEdge + offset + videoPortOffset;
            CompressProcess->imageAttr.topEdge = ConfigMessage->topUpEdge;
            CompressProcess->imageAttr.bottomEdge = ConfigMessage->topUpEdge + ConfigMessage->topHeight;
            break;
        case PAGE_BOTTOM:
            CompressProcess->imageAttr.leftEdge = ConfigMessage->bottomLeftEdge + videoPortOffset;
            CompressProcess->imageAttr.rightEdge = ConfigMessage->bottomRightEdge + videoPortOffset;
            CompressProcess->imageAttr.topEdge = ConfigMessage->bottomUpEdge;
            CompressProcess->imageAttr.bottomEdge = ConfigMessage->bottomUpEdge + ConfigMessage->bottomHeight;
            break;
        default:
            return 0;
    }

    CompressProcess->imageAttr.width = CompressProcess->imageAttr.rightEdge - CompressProcess->imageAttr.leftEdge;
    CompressProcess->imageAttr.height = CompressProcess->imageAttr.bottomEdge - CompressProcess->imageAttr.topEdge;
    CompressProcess->maxScanLines = ConfigMessage->maxLines;
    return 1;
}

void CompressProcessArrangeOneLine(const unsigned char *src, unsigned char *dst, ImageAttributionsTypeDef *attr)
{
    unsigned char R, G, B;
    int srcCount = 0, dstCount = 0;

    switch (attr->dpi) {
        case 600:
            if (attr->depth == 3) {
                for (srcCount = attr->leftEdge; srcCount < attr->rightEdge; srcCount++) {
                    B = src[srcCount];
                    R = src[srcCount + VIDEO_PORT_WIDTH * 2];
                    G = src[srcCount + VIDEO_PORT_WIDTH * 4];

                    dst[dstCount] = (unsigned char)((Y_RTable[R] + Y_GTable[G] + Y_BTable[B]) >> 16);
                    dstCount++;
                    dst[dstCount] = (unsigned char)((Cb_RTable[R] + Cb_GTable[G] + Cb_BTable[B]) >> 16);
                    dstCount++;
                    dst[dstCount] = (unsigned char)((Cr_RTable[R] + Cr_GTable[G] + Cr_BTable[B]) >> 16);
                    dstCount++;
                }
            }
            else if (attr->depth == 1) {
                for (srcCount = attr->leftEdge; srcCount < attr->rightEdge; srcCount++) {
                    dst[dstCount] = Y_YTable[src[srcCount]];
                    dstCount++;
                }
            }
            break;
        case 200:
        case 300:
            if (attr->depth == 3) {
                for (srcCount = attr->leftEdge; srcCount < attr->rightEdge; srcCount++) {
                    B = src[srcCount];
                    R = src[srcCount + VIDEO_PORT_WIDTH];
                    G = src[srcCount + VIDEO_PORT_WIDTH * 2];

                    dst[dstCount] = (unsigned char)((Y_RTable[R] + Y_GTable[G] + Y_BTable[B]) >> 16);
                    dstCount++;
                    dst[dstCount] = (unsigned char)((Cb_RTable[R] + Cb_GTable[G] + Cb_BTable[B]) >> 16);
                    dstCount++;
                    dst[dstCount] = (unsigned char)((Cr_RTable[R] + Cr_GTable[G] + Cr_BTable[B]) >> 16);
                    dstCount++;
                }
            }
            else if (attr->depth == 1) {
                for (srcCount = attr->leftEdge; srcCount < attr->rightEdge; srcCount++) {
                    dst[dstCount] = Y_YTable[src[srcCount]];
                    dstCount++;
                }
            }
            break;
        default:
            break;
    }
}
