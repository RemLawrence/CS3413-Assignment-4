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

void cleanupHead(fat32Head *h);

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
		int seek = lseek(fd, currentSector*h->bs->BPB_BytesPerSec + dirIndex*32, SEEK_SET);
		if(seek == -1) {
        	perror("Seek failed.\n");
    	}
		cluster[dirEntryNum] = malloc(sizeof(fat32Dir));
		int readd = read(fd, cluster[dirEntryNum], sizeof(fat32Dir));
		if(readd == -1) {
        	perror("Read failed.\n");
    	}
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
			}
			else {
				//archieve
				nameNoSpace[j] = '\0';
				printf("%s\t\t%d\n", nameNoSpace, dir->DIR_FileSize);
			}
		}

		free(cluster[dirEntryNum]);
		free(dir);
	}
	uint32_t FATContent = getFATEntryForClusterN(fd, 5, h);
	/* Check if FAT entry of this cluster contains EOC */
	if(FATContent > 0x0FFFFFF8) {
		/* EOC = TRUE */
	}
	else {
		/* Recursively call doDir with the next cluster # */
		printf("FATContent: %X\n", FATContent);
		doDir(fd, h, (int)FATContent);
	}
}

uint32_t doCD(int fd, fat32Head *h, uint32_t curDirClus, char *buffer) {
	/* Initialize folderName from buffer */
	char folderName[BUF_SIZE];
	int i = 0;
	while(buffer[i] != ' ') {
		i++;
	}
	int j = 0;
	i++; // Skip that space
	while(buffer[i] != '\0') {
		folderName[j] = buffer[i];
		i++;
		j++;
	}
	folderName[j] = '\0';
	
	uint32_t updatedCluster = curDirClus;
	int FirstDataSector = h->bs->BPB_RsvdSecCnt + h->bs->BPB_NumFATs * h->bs->BPB_FATSz32; // 1922+15423*2=32768
	int currentSector = findFirstDataSectorOfClusterN(h, curDirClus, FirstDataSector);

	if(curDirClus != 2 && strcmp(folderName, "..") == 0) {
		return 2;
	}

	// cluster[4096/32] = cluster[128] = 128 dir entries to go through in each cluster
	int dirEntryNum = h->bs->BPB_BytesPerSec*h->bs->BPB_SecPerClus/sizeof(fat32Dir);
	unsigned char *cluster[dirEntryNum];
	int dirIndex = 0; //  0-127
	// For each dir entry (32B) in the cluster (128 in total)
	for(dirIndex = 0; dirIndex < dirEntryNum; dirIndex++) {
		int seek = lseek(fd, currentSector*h->bs->BPB_BytesPerSec + dirIndex*32, SEEK_SET);
		if(seek == -1) {
        	perror("Seek failed.\n");
    	}
		cluster[dirEntryNum] = malloc(sizeof(fat32Dir));
		int readd = read(fd, cluster[dirEntryNum], sizeof(fat32Dir));
		if(readd == -1) {
        	perror("Read failed.\n");
    	}
		fat32Dir *dir = (fat32Dir*)(malloc(sizeof(fat32Dir)));
		memcpy(dir, cluster[dirEntryNum], sizeof(fat32Dir)); // Forgive me, I didn't use casting

		if(dir->DIR_Attr == 0x10) {
			char nameNoSpace[12];
			int i;
			int j = 0;

			for (i = 0; i < strlen(dir->DIR_Name); i++) {
				if (dir->DIR_Name[i] != ' ') {
					nameNoSpace[j] = dir->DIR_Name[i];
					j++;
				}
			}
			nameNoSpace[j-1] = '\0';

			/* folderName matches */
			if(strcmp(nameNoSpace, folderName) == 0) {
				updatedCluster = (dir->DIR_FstClusHI<<16) + dir->DIR_FstClusLO;
				free(cluster[dirEntryNum]);
				free(dir);
				return updatedCluster;
			}
		}

		free(cluster[dirEntryNum]);
		free(dir);
	}
	printf("Error: folder not found\n");
	return curDirClus;
}

void doDownload(int fd, fat32Head* h, int curDirClus, char *buffer) {
	int nextClus = 0;
	int fileSize = 0;
	bool found = false;
	/* Initialize folderName from buffer */
	char fileName[BUF_SIZE];
	int i = 0;
	while(buffer[i] != ' ') {
		i++;
	}
	int j = 0;
	i++; // Skip that space
	while(buffer[i] != '\0') {
		fileName[j] = buffer[i];
		i++;
		j++;
	}
	fileName[j] = '\0';

	int FirstDataSector = h->bs->BPB_RsvdSecCnt + h->bs->BPB_NumFATs *h->bs->BPB_FATSz32; // 1922+15423*2=32768
	int currentSector = findFirstDataSectorOfClusterN(h, curDirClus, FirstDataSector);

	// cluster[4096/32] = cluster[128] = 128 dir entries to go through in each cluster
	int dirEntryNum = h->bs->BPB_BytesPerSec*h->bs->BPB_SecPerClus/sizeof(fat32Dir);
	unsigned char *cluster[dirEntryNum];
	int dirIndex = 0; //  0-127
	// For each dir entry (32B) in the cluster (128 in total)
	for(dirIndex = 0; dirIndex < dirEntryNum; dirIndex++) {
		int seek = lseek(fd, currentSector*h->bs->BPB_BytesPerSec + dirIndex*32, SEEK_SET);
		if(seek == -1) {
        	perror("Seek failed.\n");
    	}
		cluster[dirEntryNum] = malloc(sizeof(fat32Dir));
		int readd = read(fd, cluster[dirEntryNum], sizeof(fat32Dir));
		if(readd == -1) {
        	perror("Read failed.\n");
    	}
		fat32Dir *dir = (fat32Dir*)(malloc(sizeof(fat32Dir)));
		memcpy(dir, cluster[dirEntryNum], sizeof(fat32Dir)); // Forgive me, I didn't use casting

		/* If the entry represents a file */
		/* If we got an archieve, append dot to its name */
		if(dir->DIR_Attr == 0x20) {
			int nameIndex = 0;
			while(dir->DIR_Name[nameIndex] != ' ') {
				nameIndex++;
			}
			dir->DIR_Name[nameIndex] = '.';

			char nameNoSpace[12];
			int i;
			int j = 0;

			for (i = 0; i < strlen(dir->DIR_Name); i++) {
				if (dir->DIR_Name[i] != ' ') {
					nameNoSpace[j] = dir->DIR_Name[i];
					j++;
				}
			}
			nameNoSpace[j] = '\0';
			if(strcmp(nameNoSpace, fileName) == 0) {
				nextClus = (dir->DIR_FstClusHI<<16) + dir->DIR_FstClusLO;
				fileSize = dir->DIR_FileSize; // Store the fileSize here for later use (read & write!)
				found = true;
				break;
			}
		}
		free(cluster[dirEntryNum]);
		free(dir);
	}

	/* If the file name searched is found, then find all its clusters by reading the FAT entries */
	if(found) {
		/* Read each cluster until EOC */
		uint32_t totalBytes = 0; //DIR_FileSize
		uint32_t sector = findFirstDataSectorOfClusterN(h, nextClus, FirstDataSector);
		while(getFATEntryForClusterN(fd, nextClus, h) != 0x0FFFFFFF) {
			totalBytes += h->bs->BPB_BytesPerSec*h->bs->BPB_SecPerClus;
			nextClus = getFATEntryForClusterN(fd, nextClus, h);
		}
		/* This operation adds the last cluster size to the totalBytes */
		totalBytes += h->bs->BPB_BytesPerSec*h->bs->BPB_SecPerClus;
		char* buffer = malloc(totalBytes);
		/* Seek to the first sector of the first cluster the file begins with */
		int seek = lseek(fd, sector*h->bs->BPB_BytesPerSec, SEEK_SET);
		if(seek == -1) {
        	perror("Seek failed.\n");
    	}
		if(totalBytes >= fileSize) {
			/* Bonus 2: Just 1 read! */
			int readd = read(fd, buffer, fileSize);
			if(readd == -1) {
        		perror("Read failed.\n");
    		}
			int new_file = open(fileName, O_WRONLY | O_APPEND | O_CREAT | O_EXCL, 0777);
			if(new_file < 0) {
				printf("Failed to create file '%s'\n", fileName);
			}
			write(new_file, buffer, fileSize);
			printf("Done.\n");
		}
		else {
			printf("There's some error reading the file '%s'\n", fileName);
		}
		free(buffer);
	}
	else {
		printf("Error: file not found\n");
	}
}

void shellLoop(int fd) 
{
	int running = true;
	uint32_t curDirClus;

	// Step 1: Initialize fat32Head
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

	/* Step 2: Check if the total count of clusters (start from Cluster 2) 
		falls in the range of FAT32 */
	CountofClusters = checkIfFAT32(h);
	if(CountofClusters < FAT12_TOTAL_CLUSTERS) {
		/* Volume is FAT12 */
		printf("Volume is FAT12\n");
		running = false;
	}
	else if (CountofClusters < FAT16_TOTAL_CLUSTERS) {
		/* Volume is FAT16 */
		printf("Volume is FAT16\n");
		running = false;
	}
	else {
		/* Volume is FAT32 */
	}

	/* Step 3: Check FAT signature */
	if(!checkFATSig(fd, h)) {
		printf("The FAT has incorrect signatures. Exiting now...\n");
		running = false;
	}

	/* Step 4: Load FSI and check its signature entries */
	loadFSI(fd, h);
	if(h->fsi->FSI_LeadSig == 0x41615252 && h->fsi->FSI_StrucSig == 0x61417272 && h->fsi->FSI_TrailSig == 0xAA550000) {
		/* Check if all FSInfo's signatures are correct. */
	}
	else {
		printf("AT least one FSInfo signature is incorrect! Exiting...\n");
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
			uint64_t bytesFree = (uint64_t)h->fsi->FSI_Free_Count*(uint64_t)h->bs->BPB_BytesPerSec*(uint64_t)h->bs->BPB_SecPerClus;
			printf("---Bytes Free: %lu\n", bytesFree);
			printf("---DONE\n");
		}
		else if (strncmp(buffer, CMD_CD, strlen(CMD_CD)) == 0) {
			if(strcmp(buffer, "CD") == 0){
				printf("Error: folder not found\n");
			}
			else if (strcmp(buffer, "CD ") == 0) {
				printf("Error: folder not found\n");
			}
			else {
				curDirClus = doCD(fd, h, curDirClus, buffer);
			}
		}
		else if (strncmp(buffer, CMD_GET, strlen(CMD_GET)) == 0) {
			printf("\n");
			if(strcmp(buffer, "GET") == 0) {
				printf("Error: file not found\n");
			}
			else if (strcmp(buffer, "GET ") == 0) {
				printf("Error: file not found\n");
			}
			else {
				doDownload(fd, h, curDirClus, buffer);
			}
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
	
	cleanupHead(h);
}

void cleanupHead(fat32Head *h) {
	free(h->bs);
	free(h);
}