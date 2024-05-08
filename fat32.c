#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <stdint.h>
#include "fat32.h"
#include <fcntl.h>


int f;
fat32BS MBR;
typedef struct FSInfo fsinfo;
fsinfo cluster;

typedef struct DirInfo directory;
directory dir;

typedef struct longDirectory longD;
longD long_directory;

uint32_t* FAT; //FAT Table

char* convertShort(char* name){
    char short_name_buffer[12] = {0};
    int i;
    for (i = 0; i < 7 && name[i] != ' '; i++) {
        short_name_buffer[i] = name[i];     //put the first 7 letter into an array if they are not empty.
    }
    short_name_buffer[i++] = '.';           // put a dot after extracting file name.
    int j;
    for (j = 8; j < 11 && name[j] != ' '; j++) {
        short_name_buffer[i++] = name[j];    //Put the file extension after the "."
    }
    for (int k = 0; k < 11; k++) {
        if (short_name_buffer[k] >= 'A' && short_name_buffer[k] <= 'Z') {
            short_name_buffer[k] += 32;     //if we have capital letters, convert it to small letter.         }
    }
    for (int k = 0; k < 11; k++) {
        if (short_name_buffer[k] == ' ') {
            short_name_buffer[k] = '\0';    //find the first empty and put "\0"
        }
    }
    return strdup(short_name_buffer);       //string duplicate.
}

int countDataCluster(fat32BS MBR ){
    uint16_t RootDirSectors = ((MBR.BPB_RootEntCnt * 32) + (MBR.BPB_BytesPerSec - 1)) / MBR.BPB_BytesPerSec; //from the root count the number of data cluster.
    uint16_t FATSz = MBR.BPB_FATSz32;       //Fat size
    uint32_t TotSec = MBR.BPB_TotSec32;     //total count of sectors
    uint32_t DataSec = TotSec - (MBR.BPB_RsvdSecCnt + (MBR.BPB_NumFATs * FATSz) + RootDirSectors); //total data bytes
    return DataSec / MBR.BPB_SecPerClus;    //total number of data clusters.

}

uint32_t findnextcluster(uint32_t cluster){
    //Validate FAT[0] = 0x0FFFFFF8
   assert(FAT[0] == 0x0FFFFFF8);

   //Validate (FAT[1] & 0x0FFFFFFF) == 0x0FFFFFFF
   assert((FAT[1] & 0x0FFFFFFF) == 0x0FFFFFFF);

   uint32_t nextCluster = FAT[cluster];     //find the entry of the cluster. remove the first 4 bits.

    return nextCluster & 0x0FFFFFFF;        //The last 28 bits are the next cluster of the cluster chain.
}

void convert(char *long_name, int *count,longD long_root){
    for (int i = 1; i >= 0; i--) {
        if(long_root.LDIR_Name3[i] > 0 && long_root.LDIR_Name3[i] <127){
            long_name[(*count)++] = (char) long_root.LDIR_Name3[i];
        }
    }
    for (int i = 5; i >= 0; i--) {
        if(long_root.LDIR_Name2[i] > 0 && long_root.LDIR_Name2[i] <127){
            long_name[(*count)++] = (char) long_root.LDIR_Name2[i];
        }
    }
    for (int i = 4; i >= 0; i--) {
        if(long_root.LDIR_Name1[i] > 0 && long_root.LDIR_Name1[i] <127){
            long_name[(*count)++] = (char) long_root.LDIR_Name1[i];
        }
    }
}

int c = 0;
int longVal = 0;
void getDirectory(uint32_t root_cluster, char *result[256]){
    longD long_root;
    directory short_root;
    uint32_t root_dir = (MBR.BPB_RsvdSecCnt + (MBR.BPB_FATSz32*MBR.BPB_NumFATs)) * MBR.BPB_BytesPerSec + (root_cluster - 2) * (MBR.BPB_BytesPerSec * MBR.BPB_SecPerClus);
    uint32_t dir_offset = 0;
    char long_name[256] = {0};
    int count = 0;
    while (1) {
        lseek(f, root_dir+ dir_offset, SEEK_SET);
        read(f, &short_root, sizeof(directory));
        if (short_root.dir_name[0] == 0x00) {
            // End of directory
            return;
        } //base case

        if( ( (unsigned char) short_root.dir_name[0] != 0xE5) && ( (unsigned char) short_root.dir_name[0] != 0x05) ){ //If the directory entry is not free.
            if(short_root.dir_attr == 0x0F){
                //ATTR_READ_ONLY
                //ATTR_HIDDEN
                //ATTR_SYSTEM
                //ATTR_VOLUME_ID


                //------------------------------------------------long directory--------------------------------------------------------------//.
                lseek(f, root_dir+ dir_offset, SEEK_SET);
                read(f, &long_root, sizeof(longD));
                if ((( long_root.LDIR_Attr & ATTR_LONG_NAME_MASK) == ATTR_LONG_NAME) && (long_root.LDIR_Type == 0) ){
                    /*   Found an active long name sub-component.   */

                    if (long_root.LDIR_Ord & ATTR_LONG_NAME_LAST) {  // If we are at the Last Long Directory Entry
                        long_name[count++] = 'L';   //Place a beginning "L" at the first element of the array to aid the reconstruction.
                    }

                    convert(long_name, &count,long_root); // convert long names funtion.

                    if (long_root.LDIR_Ord & 0x1) {  //If we are at the first long entry, reverse the string.
                        //convert long name
                        char name[256] = {0};
                        for(int i = count; i>= 0 ; i--){
                            name[count-i] = long_name[i-1]; //swapping.
                            
                        }
                        name[count-1] = '\0';
                        for(int i = 0; i < c+1; i++){
                            printf("-");
                        }
                        longVal = 1;
                        printf("%s \n",name);
                        strcpy(long_name,name);
                        count = 0;
                    }

                }
            }else{
                //8.3 convention
                char* short_name_value = convertShort(short_root.dir_name);
                for(int i = 0; i < c+1; i++){
                    printf("-");
                }
                if (short_root.dir_name[0] == 0x2E) {
                    //dot or dot-dot directory:
                    //printf("%.5s\n", short_root.dir_name);
                }else if(short_root.dir_attr == 0x10){
                    printf("Directory name: %.11s\n", short_root.dir_name);
                    int i = 0;
                    while (short_root.dir_name[i] != '\0') {
                        if (short_root.dir_name[i] == ' ') {
                            short_root.dir_name[i] = '\0';
                            break;
                        }
                        i++;
                    }
                    strcpy(long_name,short_root.dir_name);
                }else{
                    printf("%.11s\n",short_name_value);
                    strcpy(long_name,short_name_value);
                }

            }
        }
        
         if(strcmp(long_name,result[c])==0){ //is the file or directory the same.
            if(short_root.dir_attr == 0x10){ //if it is a directory
                if (short_root.dir_name[0] == 0x2E) {
                    //. and ..
                }else{
                    uint32_t cluster = ((uint32_t)short_root.dir_first_cluster_hi << 16) | short_root.dir_first_cluster_lo;//starting point of a file.
                    c++; //increase by c
                    getDirectory(cluster,result); //recurse to new folder.
                }
            }else{
                //"it is the file" - deal with it as such.
                printf("File name: %.11s\n", short_root.dir_name);
                int file = open(long_name,O_WRONLY | O_TRUNC | O_CREAT);
                if (file == -1) {
                    perror("Error opening file");
                    return ;
                }
                if(longVal == 1){
                    lseek(f, root_dir+ dir_offset + sizeof(directory), SEEK_SET); //find entry in cluster.
                }else{
                    lseek(f, root_dir+ dir_offset , SEEK_SET); //find entry in cluster.
                }

                read(f, &short_root, sizeof(directory));    //map it to short root struct,
                uint32_t cluster = ((uint32_t)short_root.dir_first_cluster_hi << 16) | short_root.dir_first_cluster_lo;//starting point of a file.

                while(cluster < EOC){
                    uint32_t firstData = (MBR.BPB_RsvdSecCnt + (MBR.BPB_FATSz32*MBR.BPB_NumFATs)) * MBR.BPB_BytesPerSec + (cluster - 2) * (MBR.BPB_BytesPerSec * MBR.BPB_SecPerClus);
                    lseek(f,firstData, SEEK_SET);
                    for(int i = 0; i < MBR.BPB_SecPerClus; i++){
                        char buffer[MBR.BPB_BytesPerSec]; //buffer
                        ssize_t bytes_read = read(f,buffer,MBR.BPB_BytesPerSec*MBR.BPB_SecPerClus);
                        if (bytes_read < 0) {
                            perror("read error");
                            exit(1);
                         }
                     write(file,buffer,bytes_read); //write to te file.
                    }
                    cluster = findnextcluster(cluster); //find the next cluster for continuation of file.
                }
                close(file); //close file.
                printf("%s created",long_name); //file was created successfully.
                exit(0);
        }
            }
        longVal = 0 ;

        dir_offset += sizeof(directory);

        if (dir_offset >= MBR.BPB_BytesPerSec * MBR.BPB_SecPerClus) {
            // We are at the end of our cluster.
            // Find the continuation in cluster chain in our FAT table
            uint32_t next_cluster = findnextcluster(root_cluster);
            if (next_cluster == EOC) {
                // End of chain
                return;
            }
            root_cluster = next_cluster; //new cluster value.
            root_dir = (MBR.BPB_RsvdSecCnt + (MBR.BPB_FATSz32*MBR.BPB_NumFATs)) * MBR.BPB_BytesPerSec + (root_cluster - 2) * (MBR.BPB_BytesPerSec * MBR.BPB_SecPerClus); //find the byte.
            dir_offset = 0; //reset offet.
        }
    }
}

void directoryPrint(uint32_t root_cluster)
{
    longD long_root;
    directory short_root;
    uint32_t root_dir = (MBR.BPB_RsvdSecCnt + (MBR.BPB_FATSz32*MBR.BPB_NumFATs)) * MBR.BPB_BytesPerSec + (root_cluster - 2) * (MBR.BPB_BytesPerSec * MBR.BPB_SecPerClus);
    uint32_t dir_offset = 0;
    char long_name[256] = {0};
    int count = 0;
    while (1) {
        lseek(f, root_dir+ dir_offset, SEEK_SET);
        read(f, &short_root, sizeof(directory));

        if (short_root.dir_name[0] == 0x00) {  //Base Case: End of directory
            return;
        }

        //PRINT THE NAME METHOD!
        if( ( (unsigned char) short_root.dir_name[0] != 0xE5) && ( (unsigned char) short_root.dir_name[0] != 0x05) ){ //If the directory entry is not free.
            if(short_root.dir_attr == 0x0F){
                //ATTR_READ_ONLY
                //ATTR_HIDDEN
                //ATTR_SYSTEM
                //ATTR_VOLUME_ID


                //------------------------------------------------long directory--------------------------------------------------------------//.
                lseek(f, root_dir+ dir_offset, SEEK_SET); //Go to the original position and map it using long struct
                read(f, &long_root, sizeof(longD));     // Map long struct.
                if ((( long_root.LDIR_Attr & ATTR_LONG_NAME_MASK) == ATTR_LONG_NAME) && (long_root.LDIR_Type == 0) ){ // This is to indicate directory entry is a sub component of a long name.
                    /*   Found an active long name sub-component.   */
                    if (long_root.LDIR_Ord & ATTR_LONG_NAME_LAST) {  // If we are at the Last Long Directory Entry
                        long_name[count++] = 'L'; //Place a beginning "L" at the first element of the array to aid the reconstruction.
                    }

                    convert(long_name, &count,long_root); // convert long names funtion.

                    if (long_root.LDIR_Ord & 0x1) {  //If we are at the first long entry, reverse the string.
                        char name[256] = {0};
                        for(int i = count; i>= 0 ; i--){
                            name[count-i] = long_name[i-1]; //swapping.

                        }
                                                name[count-1] = '\0';
                        for(int i = 0; i < c+1; i++){
                            printf("-");
                        }
                        printf("%s \n",name); //print the long name.
                        count = 0;
                    }
                }
            }else{
                //It is a short name so it is 8.3 convention.
                char* short_name_value = convertShort(short_root.dir_name); //convert to short name.
                for(int i = 0; i < c+1; i++){
                    printf("-");
                }
                if (short_root.dir_name[0] == 0x2E) {
                    //dot or dot-dot directory:
                    printf("%.5s\n", short_root.dir_name);
                }else if ( (short_root.dir_attr & ATTR_VOLUME_ID)  == ATTR_VOLUME_ID) {
                    //What is the volume ID?
                    printf("VOLUME ID: %s\n", short_root.dir_name);
                }else{
                    //print short name.
                    printf("%.11s\n",short_name_value);
                }
            }
            //-------------------------------------------------------------------------------------------------------------------------------------------//

            //If it is a directory, we recurse into it, if it is a file, we do nothing.
            if(short_root.dir_attr == 0x10){
                //Directory
                    if (short_root.dir_name[0] == 0x2E) {
                        //dot or dot-dot directory: DO NOTHING.
                    }else{
                        //Directory name
                        uint32_t cluster = ((uint32_t)short_root.dir_first_cluster_hi << 16) | short_root.dir_first_cluster_lo; //Find the next cluster from the directory struct.
                        directoryPrint(cluster);        //Go in recursively.
                    }
                                }
        }

        dir_offset += sizeof(directory);

        if (dir_offset >= MBR.BPB_BytesPerSec * MBR.BPB_SecPerClus) {
            // We are at the end of our cluster.
            // Find the continuation in cluster chain in our FAT table
            uint32_t next_cluster = findnextcluster(root_cluster);
            if (next_cluster == EOC) {
                // End of chain
                return;
            }
            root_cluster = next_cluster; //new cluster value.
            root_dir = (MBR.BPB_RsvdSecCnt + (MBR.BPB_FATSz32*MBR.BPB_NumFATs)) * MBR.BPB_BytesPerSec + (root_cluster - 2) * (MBR.BPB_BytesPerSec * MBR.BPB_SecPerClus);    //find the byte.
            dir_offset = 0; //reset offet.
        }

     }

}


int main(int argc, char *argv[])
{
    // make sure we got arguments.
    assert(argc > 0);

    //Variables
    int CountofClusters; //total number of data clusters.
    int freeSpaceClusters = 0;
    int badCluster = 0;


    // Open the disk.
    f = open("C:\\Users\\akint\\OneDrive\\Desktop\\COMP3430\\Assignment_3\\3430-good.img",O_RDONLY);    f = open(argv[1],O_RDONLY);
    if (f == -1) {
        perror("open");
        exit(1);
    }

    //Read BPB Sector.
    lseek(f, 0, SEEK_SET);
    read(f,&MBR,sizeof(fat32BS));

    //count the number data clusters.
    CountofClusters = countDataCluster(MBR);

    //Save the FAT table located in sector 2

    FAT = malloc(MBR.BPB_FATSz32 * MBR.BPB_BytesPerSec);
    lseek(f, MBR.BPB_RsvdSecCnt * MBR.BPB_BytesPerSec, SEEK_SET);
    read(f, FAT, MBR.BPB_FATSz32 * MBR.BPB_BytesPerSec);

    //Validate FAT[0] = 0x0FFFFFF8
   assert(FAT[0] == 0x0FFFFFF8);

   //Validate (FAT[1] & 0x0FFFFFFF) == 0x0FFFFFFF
   assert((FAT[1] & 0x0FFFFFFF) == 0x0FFFFFFF);

    // count the free clusters
    int i;
    for (i = 2; i < CountofClusters + 2; i++) {
        if (FAT[i] == 0) {
            freeSpaceClusters++;
        }
    }

    //The amount of usable storage on the drive.
    //Count the bad clusters and subtract from total data clusters.
        for (i = 2; i < CountofClusters+2; i++) {
        if((FAT[i] & 0x0FFFFFFF) == BAD_CLUSTER){
            badCluster++;
        }
    }


    if(strcmp("info","info")==0){
        //Drive name
        printf("\nDrive Name: %.11s",MBR.BS_OEMName);
        //Free space on the drive in kB
        printf("\nFree Space Available: %d KB",(freeSpaceClusters*MBR.BPB_BytesPerSec*MBR.BPB_SecPerClus )/ 1024);
        //The amount of usable storage on the drive (not free, but usable space)
        printf("\nThe amount of usable storage on the drive: %d KB\n",((CountofClusters - badCluster)*(MBR.BPB_BytesPerSec*MBR.BPB_SecPerClus))/1024);
        //The cluster size in number of sectors, and in KB.
        printf("Cluster Size: %.2f KB \n",((double)(MBR.BPB_BytesPerSec*MBR.BPB_SecPerClus)/1024));
    }

     if(strcmp("list","list")==0){ //print directory
          directoryPrint(MBR.BPB_RootClus);
      }

    if(strcmp(argv[2],"get")==0){ //fetch file.
         char *array[256];
         char *token = strtok(argv[3], "/");
         int i = 0;

         while (token != NULL) {
             array[i++] = token;
             token = strtok(NULL, "/");
         }
        getDirectory(MBR.BPB_RootClus,array);
    }

}