#include "fat32.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

fat32Head* createHead(int fd)
{
    unsigned char *buffer = malloc(BUFFER_SIZE);

    fat32Head* h = (fat32Head*)(malloc(sizeof(fat32Head)));
    fat32BS *bs = (fat32BS*)(malloc(sizeof(fat32BS)));
    
    // Read first 512 bytes -> boot sector
    int bs_count = read(fd, buffer, 512);
    if (!bs_count) {
        printf("Error (%d) - Boot Sector \n", bs_count);
        return NULL;
    }
    memcpy(bs, buffer, sizeof(fat32BS));
    h->bs = bs;

    return h;
}