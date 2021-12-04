#define _FILE_OFFSET_BITS 64

#include "fat32.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>

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

/* Check the first two entries of FAT, which stores signatures */
bool checkFATSig(int fd, fat32Head *h) {
    /* Get to FAT position */
    int seek = lseek(fd, h->bs->BPB_RsvdSecCnt*h->bs->BPB_BytesPerSec, SEEK_SET);
    if(seek == -1) {
        perror("Seek failed.\n");
    }
    uint32_t *FAT1 = malloc(4);
    /* Read the signature in first FAT entry */
    int readd = read(fd, FAT1, 4);
    if(readd == -1) {
        perror("Read failed.\n");
    }
    uint32_t *FAT2 = malloc(4);
    seek = lseek(fd, h->bs->BPB_RsvdSecCnt*h->bs->BPB_BytesPerSec+4, SEEK_SET);
    if(seek == -1) {
        perror("Seek failed.\n");
    }
    /* Read the signature in second FAT entry */
    readd = read(fd, FAT2, 4);
    if(readd == -1) {
        perror("Read failed.\n");
    }
    if(FAT1[0] == 0x0FFFFFF8 && FAT2[0] == 0xFFFFFFFF) {
        free(FAT1);
        free(FAT2);
        return 1;
    }
    else {
        free(FAT1);
        free(FAT2);
        return 0;
    }
}

/* Given a file descriptor and a FAT32 header, load up FSInfo Secctor 
    aka. Sector 1 in Reserved Area */
void loadFSI(int fd, fat32Head* h) {
    FSI *fsi = (FSI*)(malloc(sizeof(FSI)));
    /* Skipping 512 Byte (Sector 0 aka BPB) */
    int seek = lseek(fd, h->bs->BPB_FSInfo*h->bs->BPB_BytesPerSec, SEEK_SET);
    if(seek == -1) {
        perror("Seek failed.\n");
    }
    int readd = read(fd, buffer, h->bs->BPB_BytesPerSec*1);
    if(readd == -1) {
        perror("Read failed.\n");
    }
    memcpy(fsi, buffer, sizeof(FSI));
    h->fsi = fsi;
}

/* Load in the root directory struct into the head,
    from the first sector of Cluster 2 */
void loadRootDir(int fd, fat32Head* h, int FirstDataSector) {
    // First, skip reserved area + 2*FAT in bytes
    int seek = lseek(fd, FirstDataSector*h->bs->BPB_BytesPerSec, SEEK_SET);
    if(seek == -1) {
        perror("Seek failed.\n");
    }
    fat32Dir *dir = (fat32Dir*)(malloc(sizeof(fat32Dir)));
    int readd = read(fd, buffer, sizeof(fat32Dir));
    if(readd == -1) {
        perror("Read failed.\n");
    }
    memcpy(dir, buffer,  sizeof(fat32Dir));
    h->dir = dir;
}



/*****************************HELPER FUNCTIONS**********************************/

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
    int seek = lseek(fd, FATPosition*h->bs->BPB_BytesPerSec+FATOffset, SEEK_SET);
    if(seek == -1) {
        perror("Seek failed.\n");
    }
    int readd = read(fd, FAT, 4);
    if(readd == -1) {
        perror("Read failed.\n");
    }
    //int ThisFATSecNum = h->bs->BPB_RsvdSecCnt + (FATOffset/h->bs->BPB_BytesPerSec);
    // TODO
    return FAT[0];
}

