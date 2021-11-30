#define _FILE_OFFSET_BITS 64

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
		if(h->bs->BS_SigA == 0x55 && h->bs->BS_SigB == 0xAA) {
			printf("---- Device Info ----\n");
			printf("OEM Name: %s\n", h->bs->BS_OEMName);
			printf("Label: %*.*s\n", BS_VolLab_LENGTH, BS_VolLab_LENGTH, h->bs->BS_VolLab);
			printf("File System Type: %*.*s\n", BS_FilSysType_LENGTH, BS_FilSysType_LENGTH, h->bs->BS_FilSysType);
			printf("Media Type: 0x%X ", h->bs->BPB_Media);
			if(h->bs->BPB_Media == 0xF8) {
				printf("(fixed)\n");
			}
			else if(h->bs->BPB_Media == 0xF0) {
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
			printf("Volume ID: %s\n", h->dir->DIR_Name);
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

void doDir(int fd, fat32Head* h, int curDirClus) {
	int FirstDataSector = h->bs->BPB_RsvdSecCnt + h->bs->BPB_NumFATs * h->bs->BPB_FATSz32; // 1922+15423*2=32768
	int currentSector = findFirstDataSectorOfClusterN(h, curDirClus, FirstDataSector);
	
	// cluster[4096/32] = cluster[128] = 128 dir entries to go through in each cluster
	int dirEntryNum = h->bs->BPB_BytesPerSec*h->bs->BPB_SecPerClus/sizeof(fat32Dir);
	unsigned char *cluster[dirEntryNum];
	int dirIndex = 0; //  0-127
	// For each dir entry (32B) in the cluster (128 in total)
	for(dirIndex = 0; dirIndex < dirEntryNum; dirIndex++) {
		lseek(fd, currentSector*h->bs->BPB_BytesPerSec + dirIndex*32, SEEK_SET);
		cluster[dirEntryNum] = malloc(sizeof(fat32Dir));
		read(fd, cluster[dirEntryNum], sizeof(fat32Dir));
		fat32Dir *dir = (fat32Dir*)(malloc(sizeof(fat32Dir)));
		memcpy(dir, cluster[dirEntryNum],  sizeof(fat32Dir)); // Forgive me, I didn't use casting

		/* If we got an archieve, append dot to its name */
		if(dir->DIR_Attr == 0x20) {
			int nameIndex = 0;
			while(dir->DIR_Name[nameIndex] != ' ') {
				nameIndex++;
			}
			dir->DIR_Name[nameIndex] = '.';
		}

		/* If we got a directory or an archieve, remove all spaces from its name and print its info */
		if(dir->DIR_Attr == 0x10 || dir->DIR_Attr == 0x20) {
			char nameNoSpace[12];
			int i;
			int j = 0;

			for (i = 0; i < strlen(dir->DIR_Name); i++) {
				if (dir->DIR_Name[i] != ' ') {
					nameNoSpace[j] = dir->DIR_Name[i];
					j++;
				}
			}
			if(dir->DIR_Attr == 0x10) {
				//dir
				nameNoSpace[j-1] = '\0';
				printf("<%s>\t\t%d\n", nameNoSpace, dir->DIR_FileSize);
				printf("DIR_FstClusHI: %d\n",  dir->DIR_FstClusHI);
				printf("DIR_FstClusLO: %d\n",  dir->DIR_FstClusLO);
			}
			else {
				//archieve
				nameNoSpace[j] = '\0';
				printf("%s\t\t%d\n", nameNoSpace, dir->DIR_FileSize);
				printf("DIR_FstClusHI: %d\n",  dir->DIR_FstClusHI);
				printf("DIR_FstClusLO: %d\n",  dir->DIR_FstClusLO);
			}
		}

		free(cluster[dirEntryNum]);
		free(dir);
	}
	uint32_t FATContent = getFATEntryForClusterN(fd, 5, h);
	/* Check if FAT entry of this cluster contains EOC */
	if(FATContent > 0x0FFFFFF8) {
		/* EOC = TRUE */
		//printf("curDirClus: %X\n", curDirClus);
		printf("FATContent: %X\n", FATContent);
	}
	else {
		/* Recursively call doDir with the next cluster # */
		//printf("curDirClus: %X\n", curDirClus);
		printf("FATContent: %X\n", FATContent);
		doDir(fd, h, (int)FATContent);
	}

	uint64_t bytesFree = (uint64_t)h->fsi->FSI_Free_Count*(uint64_t)h->bs->BPB_BytesPerSec*(uint64_t)h->bs->BPB_SecPerClus;
	printf("---Bytes Free: %lu\n", bytesFree);
	printf("---DONE\n");
}

void shellLoop(int fd) 
{
	int running = true;
	uint32_t curDirClus;

	// Initialize fat32Head
	fat32Head *h = createHead(fd);

	if (h == NULL) {
		running = false;
	}
	else {// valid, grab the root cluster	
		;//TODO
		// Grab the root cluster
		curDirClus = h->bs->BPB_RootClus; // 2
	}
	
	char buffer[BUF_SIZE];
	char bufferRaw[BUF_SIZE];
	int CountofClusters; // Clusters in total
	int FirstDataSector = h->bs->BPB_RsvdSecCnt + h->bs->BPB_NumFATs * h->bs->BPB_FATSz32; // 1922+15423*2=32768

	/* Check if the total count of clusters (start from Cluster 2) 
		falls in the range of FAT32 */
	CountofClusters = checkIfFAT32(h);
	if(CountofClusters < 4085) {
		/* Volume is FAT12 */
		printf("Volume is FAT12\n");
		running = false;
	}
	else if (CountofClusters < 65525) {
		/* Volume is FAT16 */
		printf("Volume is FAT16\n");
		running = false;
	}
	else {
		/* Volume is FAT32 */
	}

	/* Load FSI */
	loadFSI(fd, h);
	if(h->fsi->FSI_LeadSig == 0x41615252 && h->fsi->FSI_StrucSig == 0x61417272 && h->fsi->FSI_TrailSig == 0xAA550000) {
		/* Check if all FSInfo's signatures are correct. */
	}
	else {
		running = false;
	}

	/* Load the Root Dir struct located in Cluster 2, Sector 0. */
	loadRootDir(fd, h, FirstDataSector);

	while(running) 
	{
		printf(">");

		if (fgets(bufferRaw, BUF_SIZE, stdin) == NULL) {
			running = false;
			continue;
		}
		bufferRaw[strlen(bufferRaw)-1] = '\0'; /* cut new line */
		for (int i=0; i < strlen(bufferRaw)+1; i++) {
			buffer[i] = toupper(bufferRaw[i]);
		}
		if (strncmp(buffer, CMD_INFO, strlen(CMD_INFO)) == 0) {
			printInfo(h);
		}
		else if (strncmp(buffer, CMD_DIR, strlen(CMD_DIR)) == 0) {
			// Starting sector after FAT
			printf("\nDIRECTORY LISTING\n");
			printf("VOL_ID: %s\n\n", h->dir->DIR_Name);
			doDir(fd, h, curDirClus);
		}
		else if (strncmp(buffer, CMD_CD, strlen(CMD_CD)) == 0) {
			curDirClus = 5;
			//curDirClus = doCD(h, curDirClus, buffer);
		}
		else if (strncmp(buffer, CMD_GET, strlen(CMD_GET)) == 0) {
			//doDownload(h, curDirClus, buffer);
		}
		else if (strncmp(buffer, CMD_PUT, strlen(CMD_PUT)) == 0) {
			//doUpload(h, curDirClus, buffer, bufferRaw);
			printf("Bonus marks!\n");
		}
		else {
			printf("\nCommand not found\n");
		}
	}
	printf("\nExited...\n");
	
	//cleanupHead(h);
}
