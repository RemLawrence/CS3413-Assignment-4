#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "shell.h"
#include "fat32.h"
#include <stdbool.h>
#include <inttypes.h>

#define BUF_SIZE 256
#define CMD_INFO "INFO"
#define CMD_DIR "DIR"
#define CMD_CD "CD"
#define CMD_GET "GET"
#define CMD_PUT "PUT"

#define BYTE_TO_MB 1000000
#define MB_TO_GB 1000


void printInfo(fat32Head* h) {
	// Check if both Sz16 fields are equal to 0
	if(h->bs->BPB_FATSz16 == 0 && h->bs-> BPB_TotSec16 == 0) {
		// Then check the two signature bytes (0x55 0xAA), this tells you if you load it correctly.
		if(h->bs->BS_SigA == 85 && h->bs->BS_SigB == 170) {
			printf("---- Device Info ----\n");
			printf("OEM Name: %s\n", h->bs->BS_OEMName);
			printf("Label: %*.*s\n", BS_VolLab_LENGTH, BS_VolLab_LENGTH, h->bs->BS_VolLab);
			printf("File System Type: %*.*s\n", BS_FilSysType_LENGTH, BS_FilSysType_LENGTH, h->bs->BS_FilSysType);
			printf("Media Type: 0x%X ", h->bs->BPB_Media);
			if(h->bs->BPB_Media == 248) {
				//BPB_Media = 248 = 0xF8
				printf("(fixed)\n");
			}
			else if(h->bs->BPB_Media == 240) {
				//BPB_Media = 240 = 0xF0
				printf("not fixed\n");
			}
			else {
				printf("\n");
			}
			unsigned long total_byte = (long)h->bs->BPB_BytesPerSec*(long)h->bs->BPB_TotSec32;
			double total_mb = (double)total_byte/BYTE_TO_MB;
			double total_gb = total_mb/MB_TO_GB;
			printf("Size: %lu (%dMB, %.3fGB)\n", total_byte, (int)total_mb, total_gb);
			printf("Drive Number: %d (hard disk)\n\n", h->bs->BS_DrvNum);

			printf("--- Geometry ---\n");
			printf("Bytes per Sector: %d\n", h->bs->BPB_BytesPerSec);
			printf("Sectors per Cluster: %d\n", h->bs->BPB_SecPerClus);
			printf("Total Sectors: %d\n", h->bs->BPB_TotSec32);
			printf("Geom: Sectors per Track: %d\n", h->bs->BPB_SecPerTrk);
			printf("Geom: Heads: %d\n", h->bs->BPB_NumHeads);
			printf("Hidden Sectors: %d\n\n", h->bs->BPB_HiddSec);

			printf("--- FS Info ---\n");
			printf("Volume ID: %d\n", h->bs->BS_VolID); //
			printf("Version: %d:%d\n", h->bs->BPB_FSVerLow, h->bs->BPB_FSVerLow);
			printf("Reserved Sectors: %d\n", h->bs->BPB_RsvdSecCnt);
			printf("Number of FATs: %d\n", h->bs->BPB_NumFATs);
			printf("FAT Size: %d\n", h->bs->BPB_FATSz32);
			printf("Mirrored FAT: %d ", h->bs->BPB_ExtFlags);
			if(h->bs->BPB_ExtFlags == 0){
				printf("(yes)\n");
			}
			else {
				printf("\n");
			}
			printf("Boot Sector Backup Sector No: %d\n", h->bs->BPB_BkBootSec);
		}
	}
	else {
		printf("Disk does not have FAT32\n");
	}
}

void shellLoop(int fd) 
{
	int running = true;
	uint32_t curDirClus;
	char buffer[BUF_SIZE];
	char bufferRaw[BUF_SIZE];

	//TODO:
	fat32Head *h = createHead(fd);

	if (h == NULL) {
		running = false;
	}
	else {// valid, grab the root cluster	
		;//TODO
		curDirClus = h->bs->BPB_RootClus; // 2
	}

	while(running) 
	{
		printf(">");

		

		if (fgets(bufferRaw, BUF_SIZE, stdin) == NULL) 
		{
			running = false;
			continue;
		}
		bufferRaw[strlen(bufferRaw)-1] = '\0'; /* cut new line */
		for (int i=0; i < strlen(bufferRaw)+1; i++){
			buffer[i] = toupper(bufferRaw[i]);
		}
		if (strncmp(buffer, CMD_INFO, strlen(CMD_INFO)) == 0){
			printInfo(h);	
		}
		else if (strncmp(buffer, CMD_DIR, strlen(CMD_DIR)) == 0){
			printf("%s", buffer);
			//doDir(h, curDirClus);	
		}
		else if (strncmp(buffer, CMD_CD, strlen(CMD_CD)) == 0){
			printf("%s", buffer); 
			//curDirClus = doCD(h, curDirClus, buffer);
		}
		else if (strncmp(buffer, CMD_GET, strlen(CMD_GET)) == 0){
			printf("%s", buffer);
			//doDownload(h, curDirClus, buffer);
		}
		else if (strncmp(buffer, CMD_PUT, strlen(CMD_PUT)) == 0){
			printf("%s", buffer);
			//doUpload(h, curDirClus, buffer, bufferRaw);
			printf("Bonus marks!\n");
		}
		else 
			printf("\nCommand not found\n");
	}
	printf("\nExited...\n");
	
	//cleanupHead(h);
}
