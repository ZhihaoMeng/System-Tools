#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <stdint.h>
#include <sys/mman.h>
// sector size
#define SECTOR_SIZE 0x200
// number of sectors
#define NUM_SECTOR 0xB40
// volume of fat12 file system
#define VOLUME 0x200 * 0xB40
// each entry in root directory is 32 bytes 0x20
#define ENTRY_SIZE 0x20
#define FILE_NAME_LEN 0x8
#define FILE_EXT_LEN 0x3
#define fIRST_ROOT_SECTOR 0x13
#define FAT_SECTOR_NUM 0x9

// helper functions
// decode the fat into logical_sector_num array
uint16_t* decodeFAT12(uint8_t* image);
// recover the files
void recFiles(uint8_t* image, u_int32_t offset, char* curPath, uint16_t* arr, char* recPath);
// write files to recovery directory
void writeFile(uint8_t* image, uint16_t firstLogicalCluster, uint16_t* arr, u_int32_t fileSize, char* fileExt, char* recPath, int deleted);

// global variable for number of files
int numFiles;

int main(int argc, char **argv) {
    // not enough command line arguments
    if (argc != 3){
        exit(EXIT_FAILURE);
    }
    // map the whole disk into memory
    int fd = open(argv[1], O_RDWR);
    void *image = mmap(NULL, VOLUME, PROT_READ, MAP_PRIVATE, fd, 0);
    // decode the FAT first, we dont care first two entry
    uint16_t* arr = decodeFAT12((uint8_t*) image);
    // path to store recovered files
    char*  recPath = strcat(argv[2],"/");
    // current print out path
    char*  curPath = "";
    // offset of root dir on image 
    uint32_t offset = fIRST_ROOT_SECTOR * SECTOR_SIZE;
    recFiles((uint8_t*) image, offset, curPath, arr, recPath);
    free(arr);
    munmap(image, VOLUME);
    return 0;
}

uint16_t* decodeFAT12(uint8_t* image){
    // num of entries in FAT12
    uint16_t numEntry = FAT_SECTOR_NUM * SECTOR_SIZE * 2 /3 ;
    uint16_t* arr = (uint16_t*) malloc(numEntry * sizeof(uint16_t));
    // start address of first FAT12
    uint8_t* ptr = (uint8_t*)image + SECTOR_SIZE ;
    for(int i= 0; i< (numEntry/2);i++){
        uint16_t firstByte = (uint16_t) *ptr; ptr++;
        uint16_t secondByte = (uint16_t) *ptr; ptr++;
        uint16_t thirdByte = (uint16_t) *ptr; ptr++;
        uint16_t firstEntry = firstByte + ((secondByte & 0x000F) << 8);
        uint16_t secondEntry = ((secondByte & 0x00F0)>>4) + (thirdByte << 4);
        *(arr + 2 * i) = firstEntry;
        *(arr + 2 * i + 1) = secondEntry;
    }
    return arr;
}

void recFiles(uint8_t* image, u_int32_t offset, char* curPath, uint16_t* arr, char* recPath){
    uint8_t entry[ENTRY_SIZE];
    uint8_t* entryAddr = image + offset;
	// add null termination to the end, so length + 1
    char fileName[FILE_NAME_LEN+1];
    fileName[FILE_NAME_LEN] = 0x00;
    char fileExt[FILE_EXT_LEN+1];
    fileExt[FILE_EXT_LEN] = 0x00;
    while(1) {
        // copy next entry in dir
        memcpy(entry, entryAddr, ENTRY_SIZE);
        if(entry[0] == 0){
            break;
        }
        // if this entry is not a link to current or parent directory
        if (entry[0] != 0x2E) {

            if(entry[0] == 0xE5) {
                fileName[0] = '_';
            } else {
                fileName[0] = (char)entry[0];
            }
            for (int i = 1; i < FILE_NAME_LEN;i++) {
                if(entry[i] == 0x20){
                    fileName[i] = 0x00;
                    break;
                }else{
                    fileName[i] = (char)entry[i];
                }
            }
            for(int i = 0; i < FILE_EXT_LEN; i++) {
                fileExt[i] = (char)entry[i+8];
            }
        }
        // first logical cluster of this file or sub directory sector
        uint16_t firstLogicalCluster = ((uint16_t)entry[27] << 8) + (uint16_t)entry[26];
        // get the size of this file, 0 for sub directory
        uint32_t fileSize = (uint32_t)entry[28] + ((uint32_t)entry[29] << 0x8)  + ((uint32_t)entry[30] << 0x10) + ((uint32_t)entry[31] << 0x18);
        // get attribute to see if it's a directory
        uint8_t attr = entry[11];
        if (attr & 0x10) {
            if(entry[0] != 0x2E){
                // the length of subdirectory,include '/' and null termination
                uint8_t lenSubDir = strlen(curPath) + strlen(fileName) + 2;
                char subDir[lenSubDir];
                // write the new path to the buffer
                snprintf(subDir, lenSubDir,"%s%s/", curPath, fileName);
                uint32_t newOffset = (firstLogicalCluster + 33 - 2) * SECTOR_SIZE;
                recFiles(image, newOffset, subDir, arr, recPath);
            }
        }else{
            if (entry[0] == 0xE5) {
                printf("FILE\tDELETED\t%s%s.%s\t%u\n", curPath, fileName, fileExt, fileSize);
                writeFile(image, firstLogicalCluster, arr, fileSize, fileExt, recPath, 1);
            }else{
                printf("FILE\tNORMAL\t%s%s.%s\t%u\n", curPath, fileName, fileExt, fileSize);
                writeFile(image, firstLogicalCluster, arr, fileSize, fileExt,recPath, 0);
            }
        }
        entryAddr += ENTRY_SIZE;
    }
}

void writeFile(uint8_t* image, uint16_t firstLogicalCluster, uint16_t* arr, u_int32_t fileSize, char* fileExt, char* recPath, int deleted) {
    uint16_t numSectors = fileSize == 0 ? 0 : ((fileSize-1)/SECTOR_SIZE + 1);
    uint16_t curSector = firstLogicalCluster;
    uint8_t* buffer = (uint8_t*)malloc(numSectors*SECTOR_SIZE);
    if(!deleted) {
        for(int i = 0; i < numSectors;i++){
            memcpy(buffer+i*SECTOR_SIZE, image+((31+curSector)*SECTOR_SIZE),SECTOR_SIZE);
            curSector = arr[curSector];
        }
    }else{
        uint16_t i = 0;
        while(arr[curSector] == 0 && i < numSectors){
            memcpy(buffer+i*SECTOR_SIZE,image+((31+curSector)*SECTOR_SIZE),SECTOR_SIZE);
            curSector++;
            i++;
        }
        if (i != numSectors){
            fileSize = i * SECTOR_SIZE;
        }
    }
    // 4 for strlen('file'), 5 for strlen(.ext\0)
    uint8_t lenPath = strlen(recPath)+ 4 +sizeof(int) + 5;
    char fileName[lenPath];
    snprintf(fileName, lenPath, "%sfile%d.%s",recPath, numFiles++, fileExt);
    FILE *fp = fopen(fileName, "w");
    if(fileSize != 0) {
        fwrite(buffer, sizeof(uint8_t), fileSize, fp);
    }
    fclose(fp);
    free(buffer);
}
