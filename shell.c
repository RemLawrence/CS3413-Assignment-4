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

#define BUF_SIZE 256
#define CMD_INFO "INFO"
#define CMD_DIR "DIR"
#define CMD_CD "CD"
#define CMD_GET "GET"
#define CMD_PUT "PUT"


void shellLoop(int fd) 
{
	int running = true;
	uint32_t curDirClus;
	char buffer[BUF_SIZE];
	char bufferRaw[BUF_SIZE];

	//TODO:
	fat32Head *h = createHead(fd);

	if (h == NULL)
		running = false;
	else // valid, grab the root cluster	
		;//TODO
		//curDirClus = h->bs->BPB_RootClus;

	while(running) 
	{
		printf(">");
		if (fgets(bufferRaw, BUF_SIZE, stdin) == NULL) 
		{
			running = false;
			continue;
		}
		bufferRaw[strlen(bufferRaw)-1] = '\0'; /* cut new line */
		for (int i=0; i < strlen(bufferRaw)+1; i++)
			buffer[i] = toupper(bufferRaw[i]);
	
		if (strncmp(buffer, CMD_INFO, strlen(CMD_INFO)) == 0)
			printInfo(h);	

		else if (strncmp(buffer, CMD_DIR, strlen(CMD_DIR)) == 0)
			doDir(h, curDirClus);	
	
		else if (strncmp(buffer, CMD_CD, strlen(CMD_CD)) == 0) 
			curDirClus = doCD(h, curDirClus, buffer);

		else if (strncmp(buffer, CMD_GET, strlen(CMD_GET)) == 0) 
			doDownload(h, curDirClus, buffer);

		else if (strncmp(buffer, CMD_PUT, strlen(CMD_PUT)) == 0)
			//doUpload(h, curDirClus, buffer, bufferRaw);
			printf("Bonus marks!\n");
		else 
			printf("\nCommand not found\n");
	}
	printf("\nExited...\n");
	
	cleanupHead(h);
}
