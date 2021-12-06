/* This header file stores all types of the FAT32 structs
* Including: Bios parameter block (BPB) Structure, FSInfo Sector Structure, and
* Directory Entry Structure.
* And also a header including them.
* Author: Micah Hanmin Wang #3631308
*/

#ifndef FAT32_H
#define FAT32_H

#include <inttypes.h>
#include <stdbool.h>

/* boot sector constants */
#define BS_OEMName_LENGTH 8
#define BS_VolLab_LENGTH 11
#define BS_FilSysType_LENGTH 8 

#define BUFFER_SIZE 512

#pragma pack(push)
#pragma pack(1)
struct fat32BS_struct {
	char BS_jmpBoot[3];
	char BS_OEMName[BS_OEMName_LENGTH];
	uint16_t BPB_BytesPerSec;
	uint8_t BPB_SecPerClus;
	uint16_t BPB_RsvdSecCnt;
	uint8_t BPB_NumFATs;
	uint16_t BPB_RootEntCnt;
	uint16_t BPB_TotSec16;
	uint8_t BPB_Media;
	uint16_t BPB_FATSz16;
	uint16_t BPB_SecPerTrk;
	uint16_t BPB_NumHeads;
	uint32_t BPB_HiddSec;
	uint32_t BPB_TotSec32;

	uint32_t BPB_FATSz32;
	uint16_t BPB_ExtFlags;
	uint8_t BPB_FSVerLow;
	uint8_t BPB_FSVerHigh;
	uint32_t BPB_RootClus;
	uint16_t BPB_FSInfo;
	uint16_t BPB_BkBootSec;
	char BPB_reserved[12];
	uint8_t BS_DrvNum;
	uint8_t BS_Reserved1;
	uint8_t BS_BootSig;
	uint32_t BS_VolID;
	char BS_VolLab[BS_VolLab_LENGTH];
	char BS_FilSysType[BS_FilSysType_LENGTH];
	
	char BS_CodeReserved[420];
	uint8_t BS_SigA;
	uint8_t BS_SigB;
};
#pragma pack(pop)
typedef struct fat32BS_struct fat32BS;

#pragma pack(push)
#pragma pack(1)
struct fsi_struct {
	uint32_t FSI_LeadSig;
	char FSI_Reserved1[480];
	uint32_t FSI_StrucSig;
	uint32_t FSI_Free_Count;
	uint32_t FSI_Nxt_Free;
	char FSI_Reserved2[12];
	uint32_t FSI_TrailSig;
};
#pragma pack(pop)
typedef struct fsi_struct FSI;

#pragma pack(push)
#pragma pack(1)
struct dir_struct {
	char DIR_Name[11];
	uint8_t DIR_Attr;
	uint8_t DIR_NTRes;
	uint8_t DIR_CrtTimeTenth;
	uint16_t DIR_CrtTime;
	uint16_t DIR_CrtDate;
	uint16_t DIR_LstAccDate;
	uint16_t DIR_FstClusHI;
	uint16_t DIR_WrtTime;
	uint16_t DIR_WrtDate;
	uint16_t DIR_FstClusLO;
    uint32_t DIR_FileSize;
};
#pragma pack(pop)
typedef struct dir_struct fat32Dir;




#pragma pack(push)
#pragma pack(1)
struct fat32Head {
	fat32BS *bs;
	FSI *fsi;
	fat32Dir *dir; // The ROOT DIR entry, specifically
};
#pragma pack(pop)
typedef struct fat32Head fat32Head;


fat32Head *createHead(int fd);
int checkIfFAT32(fat32Head* h);
void loadFSI(int fd, fat32Head* h);
void loadRootDir(int fd, fat32Head* h, int FirstDataSector);
int findFirstDataSectorOfClusterN(fat32Head* h, int N, int FirstDataSector);
uint32_t getFATEntryForClusterN(int fd, int N, fat32Head* h);
bool checkFATSig(int fd, fat32Head *h);

#endif
