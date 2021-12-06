/* fat32.c includes all the functions that creates/loads 
* FAT32 structures into the memory.
* Also the helper functions to calculate between 
* the position of FAT and cluster N.
* Author: Micah Hanmin Wang #3631308
*/

#define _FILE_OFFSET_BITS 64

#include "fat32.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>

#define BYTE_PER_FAT_ENTRY 4
#define FAT_FIRST_ENTRY 0x0FFFFFF8 
#define FAT_SECOND_ENTRY 0xFFFFFFFF

unsigned char *buffer;

/* Initialize FAT32's head struct, load in BPB */
fat32Head* createHead(int fd) {
    buffer = malloc(BUFFER_SIZE);
    if(buffer == NULL) {
        fprintf(stderr, "Fatal: failed to allocate %d bytes.\n", BUFFER_SIZE);
        abort();
    }
    fat32Head* h = (fat32Head*)(malloc(sizeof(fat32Head)));
    if(h == NULL) {
        fprintf(stderr, "Fatal: failed to allocate %lu bytes.\n", sizeof(fat32Head));
        abort();
    }
    fat32BS *bs = (fat32BS*)(malloc(sizeof(fat32BS)));
    if(bs == NULL) {
        fprintf(stderr, "Fatal: failed to allocate %lu bytes.\n", sizeof(fat32BS));
        abort();
    }
    
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

/* Return the total cluster number in the volume */
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
    uint32_t *FAT1 = malloc(BYTE_PER_FAT_ENTRY);
    if(FAT1 == NULL) {
        fprintf(stderr, "Fatal: failed to allocate %d bytes.\n", BYTE_PER_FAT_ENTRY);
        abort();
    }
    
    /* Read the signature in first FAT entry */
    int readd = read(fd, FAT1, BYTE_PER_FAT_ENTRY);
    if(readd == -1) {
        perror("Read failed.\n");
    }
    uint32_t *FAT2 = malloc(BYTE_PER_FAT_ENTRY);
    if(FAT2 == NULL) {
        fprintf(stderr, "Fatal: failed to allocate %d bytes.\n", BYTE_PER_FAT_ENTRY);
        abort();
    }
    seek = lseek(fd, h->bs->BPB_RsvdSecCnt*h->bs->BPB_BytesPerSec+BYTE_PER_FAT_ENTRY, SEEK_SET);
    if(seek == -1) {
        perror("Seek failed.\n");
    }
    /* Read the signature in second FAT entry */
    readd = read(fd, FAT2, BYTE_PER_FAT_ENTRY);
    if(readd == -1) {
        perror("Read failed.\n");
    }
    if(FAT1[0] == FAT_FIRST_ENTRY && FAT2[0] == FAT_SECOND_ENTRY) {
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
    if(fsi == NULL) {
        fprintf(stderr, "Fatal: failed to allocate %lu bytes.\n", sizeof(FSI));
        abort();
    }
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
    if(dir == NULL) {
        fprintf(stderr, "Fatal: failed to allocate %lu bytes.\n", sizeof(fat32Dir));
        abort();
    }
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
    int FATOffset = N*BYTE_PER_FAT_ENTRY;
    uint32_t *FAT = malloc(BYTE_PER_FAT_ENTRY);
    if(FAT == NULL) {
        fprintf(stderr, "Fatal: failed to allocate %d bytes.\n", BYTE_PER_FAT_ENTRY);
        abort();
    }
    int seek = lseek(fd, FATPosition*h->bs->BPB_BytesPerSec+FATOffset, SEEK_SET);
    if(seek == -1) {
        perror("Seek failed.\n");
    }
    int readd = read(fd, FAT, BYTE_PER_FAT_ENTRY);
    if(readd == -1) {
        perror("Read failed.\n");
    }
    //int ThisFATSecNum = h->bs->BPB_RsvdSecCnt + (FATOffset/h->bs->BPB_BytesPerSec);
    // TODO
    return FAT[0];
}

