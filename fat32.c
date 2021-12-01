#define _FILE_OFFSET_BITS 64

#include "fat32.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

unsigned char *buffer;

fat32Head* createHead(int fd) {
    buffer = malloc(BUFFER_SIZE);
    fat32Head* h = (fat32Head*)(malloc(sizeof(fat32Head)));
    fat32BS *bs = (fat32BS*)(malloc(sizeof(fat32BS)));
    
    // Read first 512 bytes -> boot sector
    int bs_count = read(fd, buffer, sizeof(fat32BS));
    if (!bs_count) {
        printf("Error (%d) - Boot Sector \n", bs_count);
        return NULL;
    }
    memcpy(bs, buffer, sizeof(fat32BS));
    h->bs = bs;

    return h;
}

int checkIfFAT32(fat32Head* h) {
    int TotalDataSec = h->bs->BPB_TotSec32 - (h->bs->BPB_RsvdSecCnt + (h->bs->BPB_NumFATs*h->bs->BPB_FATSz32)+0);
    return TotalDataSec/h->bs->BPB_SecPerClus; // CountofClusters
} 

/* Given a file descriptor and a FAT32 header, load up FSInfo Secctor 
    aka. Sector 1 in Reserved Area */
void loadFSI(int fd, fat32Head* h) {
    FSI *fsi = (FSI*)(malloc(sizeof(FSI)));
    /* Skipping 512 Byte (Sector 0 aka BPB) */
    lseek(fd, h->bs->BPB_FSInfo*h->bs->BPB_BytesPerSec, SEEK_SET);
    read(fd, buffer, h->bs->BPB_BytesPerSec*1);
    memcpy(fsi, buffer, sizeof(FSI));
    h->fsi = fsi;
}

void loadRootDir(int fd, fat32Head* h, int FirstDataSector) {
    // First, skip reserved area + 2*FAT in bytes
    lseek(fd, FirstDataSector*h->bs->BPB_BytesPerSec, SEEK_SET);
    fat32Dir *dir = (fat32Dir*)(malloc(sizeof(fat32Dir)));
    read(fd, buffer, sizeof(fat32Dir));
    memcpy(dir, buffer,  sizeof(fat32Dir));
    h->dir = dir;
}

/* return the sector # of cluster N */
int findFirstDataSectorOfClusterN(fat32Head* h, int N, int FirstDataSector) {
    //printf("%d", FirstDataSector + (N-2)*h->bs->BPB_SecPerClus);
    return FirstDataSector + (N-2)*h->bs->BPB_SecPerClus;
}

/*  A helper function that given a valid cluster number, 
    where in the FAT32(s) is the entry for that cluster number? */
uint32_t getFATEntryForClusterN(int fd, int N, fat32Head* h) {
    int FATPosition = h->bs->BPB_RsvdSecCnt;
    int FATOffset = N*4;
    uint32_t *FAT = malloc(4);
    lseek(fd, FATPosition*h->bs->BPB_BytesPerSec+FATOffset, SEEK_SET);
    read(fd, FAT, 4);
    //int ThisFATSecNum = h->bs->BPB_RsvdSecCnt + (FATOffset/h->bs->BPB_BytesPerSec);
    // TODO
    return FAT[0];
}

