#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <stdbool.h>
#define MAX_COMMAND_SIZE 100
#define MAX_NUM_ARGUMENTS 5

struct FAT32_Boot           
{            
  char BS_OEMName[8];        
  int16_t BPB_BytesPerSec;    
  int8_t BPB_SecPerClus;     
  int16_t BPB_RsvdSecCnt;    
  int8_t BPB_NumFATS;        
  int16_t BPB_RootEntCnt;    
  int32_t BPB_TotSec32;      
  int16_t BPB_FATSz32;       
  int16_t BPB_ExtFlags;      
  int32_t BPB_RootClus;      
  int16_t BPB_FSInfo;       
  char BS_VolLab[11];       
  int32_t RootDirSectors;
  int32_t FirstDataSector;
  int32_t FirstSectorofCluster;
};

struct DirectoryEntry
{
  char DIR_Name[12];
  int8_t DIR_Attr;
  int8_t Unused[8];
  int16_t DIR_FirstClusterHigh;
  int8_t Unused2[4];
  int16_t DIR_FirstClusterLow;
  int32_t DIR_FileSize;
  int8_t DIR_SearchSuccess;
};
int LBAToOffset(int32_t sector,struct FAT32_Boot * b)
{
  return((sector - 2) * b->BPB_BytesPerSec) + (b->BPB_BytesPerSec * b->BPB_RsvdSecCnt) + (b->BPB_NumFATS * b->BPB_FATSz32 * b->BPB_BytesPerSec);
}

int16_t NextLB(uint32_t sector,struct FAT32_Boot * b,FILE *fp)
{
  uint32_t FATAddress = (b->BPB_BytesPerSec * b->BPB_RsvdSecCnt) + (sector * 4);
  int16_t val;
  fseek(fp, FATAddress,SEEK_SET);
  fread(&val, 2, 1, fp);
  if (val >= 0x0FFFFFF8) 
  {
        return -1; 
  }
  return val;
}


int* load_cluster_addresses(FILE *fp, struct FAT32_Boot *b, int root, int *total_entries, int *visited_clusters) 
{
  int *cluster_addresses = malloc(sizeof(int) * 10);  // start with 10 addresses, this is a random amount I chose
  int num_clusters = 1;      //Cluster array starts with the size for later use
  cluster_addresses[0] = 0;  // size is the total number of directory addr.'s
  int offset = LBAToOffset(root, b); // Read directory entries from the specified cluster
  fseek(fp, offset, SEEK_SET);
  struct DirectoryEntry entry; // Iterate over the directory entries
  int next_cluster = root;
  
  while (next_cluster != -1) 
  {
    for (int i = 0; i < 32; i++) 
    {
      fread(&entry.DIR_Name, 11, 1, fp);
      entry.DIR_Name[11] = '\0'; 
      fread(&entry.DIR_Attr, 1, 1, fp);
      fseek(fp, 8, SEEK_CUR); 
      fread(&entry.DIR_FirstClusterHigh, 2, 1, fp);
      fseek(fp, 4, SEEK_CUR); 
      fread(&entry.DIR_FirstClusterLow, 2, 1, fp);
      fread(&entry.DIR_FileSize, 4, 1, fp);

      if (entry.DIR_Attr == 0x10) 
      {
        int cluster = entry.DIR_FirstClusterLow + (entry.DIR_FirstClusterHigh << 16);// Full cluster
        bool already_visited = false;// Check if the cluster has been visited
        for (int j = 0; j < *visited_clusters; j++) 
        {
          if (cluster_addresses[j] == cluster) 
          {
            already_visited = true;
            break;
          }
        }
        if (!already_visited) // Add to list if not visited
        {
          if (num_clusters >= *total_entries) 
          {
              cluster_addresses = realloc(cluster_addresses, sizeof(int) * (num_clusters + 10));
          }
          cluster_addresses[num_clusters] = cluster;
          num_clusters++;
          (*visited_clusters)++;
          // Recursively load subdirectories
          int *subdir_clusters = load_cluster_addresses(fp, b, cluster, total_entries, visited_clusters);
          // Subdir clusters added to address array, starts from j = 1 due to storing the array size in the first element
          for (int j = 1; j < *total_entries; j++) 
          {
              cluster_addresses[num_clusters + j - 1] = subdir_clusters[j];
          }
          num_clusters += (*total_entries - 1);  
          free(subdir_clusters);
        }
      }
    }
    next_cluster = NextLB(next_cluster, b, fp);
    if (next_cluster == -1) 
    {
      break; 
    }
    offset = LBAToOffset(next_cluster, b);
    fseek(fp, offset, SEEK_SET);
  }
  *total_entries = num_clusters;  // Set the total number of entries read
  return cluster_addresses;
}
void load_records(struct DirectoryEntry *d,FILE *fp)
{
  for(int i = 0;i < 16;i++)
  {
    fread(&d[i].DIR_Name,11,1,fp);
    d[i].DIR_Name[11] = '\0';
    fread(&d[i].DIR_Attr,1,1,fp);
    fseek(fp,8,SEEK_CUR);
    fread(&d[i].DIR_FirstClusterHigh,2,1,fp);
    fseek(fp,4,SEEK_CUR);
    fread(&d[i].DIR_FirstClusterLow,2,1,fp);
    fread(&d[i].DIR_FileSize,4,1,fp);
  }
}

void load_boot_sector(struct FAT32_Boot * b,FILE *fp)
{
  fseek(fp,3,SEEK_SET);
  fread(&b->BS_OEMName,8,1,fp);
  fseek(fp,11,SEEK_SET);
  fread(&b->BPB_BytesPerSec,2,1,fp);
  fseek(fp,13,SEEK_SET);
  fread(&b->BPB_SecPerClus,1,1,fp);
  fseek(fp,14,SEEK_SET);
  fread(&b->BPB_RsvdSecCnt,2,1,fp);
  fseek(fp,16,SEEK_SET);
  fread(&b->BPB_NumFATS,1,1,fp);
  fseek(fp,17,SEEK_SET);
  fread(&b->BPB_RootEntCnt,2,1,fp);
  fseek(fp,32,SEEK_SET);
  fread(&b->BPB_TotSec32,4,1,fp);
  fseek(fp,36,SEEK_SET);
  fread(&b->BPB_FATSz32,2,1,fp);
  fseek(fp,40,SEEK_SET);
  fread(&b->BPB_ExtFlags,2,1,fp);
  fseek(fp,44,SEEK_SET);
  fread(&b->BPB_RootClus,4,1,fp);
  fseek(fp,48,SEEK_SET);
  fread(&b->BPB_FSInfo,2,1,fp);
  fseek(fp,71,SEEK_SET);
  fread(&b->BS_VolLab,11,1,fp);
}

char * get_fat_file_name(char * token)
{
  char * newName = malloc(12 * sizeof(char));
  memset( newName, ' ', 12 );
  char *fileName = strtok( token, "." );
  strncpy( newName, fileName, strlen( fileName ) );
  fileName = strtok( NULL, "." );
  if( fileName )
  {
    strncpy( (char*)(newName+8), fileName, strlen(fileName ) );
  }
  newName[11] = '\0';
  for(int i = 0; i < 11; i++ )
  {
    newName[i] = toupper( newName[i] );
  }
  return newName;
}
// Finds file and then undeletes it using the first letter from the specified file to be deleted
int undelete_file(FILE *fp, struct FAT32_Boot *b, struct DirectoryEntry *d, char *fileName,int clusterAddress, char firstLetter) 
{
  fseek(fp,clusterAddress,SEEK_SET);
  for (int i = 0; i < 16; i++) 
  {
    if (strcmp(d[i].DIR_Name, fileName) == 0) 
    {
      if (d[i].DIR_Name[0] == 0xffffffe5) 
      {
        fwrite(&firstLetter,1,1,fp);          // Sets first element to whatever was given, (num.txt and tum.txt would both undelete num.txt)
        printf("File '%s' has been undeleted.\n", d[i].DIR_Name);
        return 0; 
      }
      else 
      {
        printf("File '%s' is not marked as deleted.\n", d[i].DIR_Name);
        return -1; 
      }
    }
    fseek(fp,32,SEEK_CUR);
  }
  return -1; // File not found
}
// Deletes the file by finding it and then changing first element of name
int delete_file(FILE *fp, struct FAT32_Boot *b, struct DirectoryEntry *d, char *fileName, int clusterAddress) 
{
  char c = 0xe5;
  fseek(fp,clusterAddress,SEEK_SET);
  for (int i = 0; i < 16; i++) 
  {
    if (strcmp(d[i].DIR_Name, fileName) == 0) 
    {
      fwrite(&c,1,1,fp);                          // Sets first element to deleted
      printf("File '%s' has been deleted.\n", d[i].DIR_Name);
      return 0; 
    }
    fseek(fp,32,SEEK_CUR);
  }
  return -1; // File not found
}
//Checks a cluster for the desired file and returns the cluster of the file
int get_file_cluster(struct DirectoryEntry *d, char *fileName)
{
  for(int i = 0;i<16;i++)
  {
    //printf("%s\n",d[i].DIR_Name);
    if(!strcmp(d[i].DIR_Name,fileName))
    {
      return (d[i].DIR_FirstClusterLow + (d[i].DIR_FirstClusterHigh << 16));
    }
  }
  return -1;
}
// Finds empty directory are sets the values using the second struct
int get_empty_directory_entry(FILE *fp,struct DirectoryEntry *d,struct DirectoryEntry *newD)
{
  for(int i = 0;i < 16;i++)
  {
    if(d[i].DIR_Name[0] == 0x000000)
    {
      fwrite(&newD->DIR_Name,11,1,fp);
      newD->DIR_Name[11] = '\0';
      fwrite(&newD->DIR_Attr,1,1,fp);
      fseek(fp,8,SEEK_CUR);
      fwrite(&newD->DIR_FirstClusterHigh,2,1,fp);
      fseek(fp,4,SEEK_CUR);
      fwrite(&newD->DIR_FirstClusterLow,2,1,fp);
      fwrite(&newD->DIR_FileSize,4,1,fp);
      return 1;
    }
    fseek(fp,4,SEEK_CUR);
  }
  return 0;
}
// Prints given struct which is loaded from a directory
void print_records(struct DirectoryEntry *d) 
{
  for (int i = 0; i < 16; i++) 
  {
    if (d[i].DIR_Attr == 0x01 || d[i].DIR_Attr == 0x10 || d[i].DIR_Attr == 0x20)
    { 
      if(d[i].DIR_Name[0] == 0xffffffe5 || d[i].DIR_Name[0] == 0x00)
      {
        continue;
      }
      printf("%s\n", d[i].DIR_Name);
    } 
  }
}
//Checks a cluster for the desired file and prints the stat info
int get_stat_info(struct DirectoryEntry *d,char * searchName)
{
  for(int i = 0;i<16;i++)
  {
    if(!strcmp(d[i].DIR_Name,searchName))
    {
      printf("Attribute: %X\nStarting Cluster Number: %X\n",d[i].DIR_Attr,d[i].DIR_FirstClusterLow + (d[i].DIR_FirstClusterHigh << 16));
      return 0;
    }
  }
  return -1;
}
//Writes 512 bytes from the second file to the first(interchangeable so that i don't repeat more code)
void write_to_cluster_or_file(FILE *outputFile,FILE *inputFile)
{
  int i = 0;
  char c;

  while(i < 512)
  {
    c = fgetc(inputFile);
    if (feof(inputFile))      
    {
      break; 
    }
    fwrite(&c,1,1,outputFile);
    i++;
  }
}
// Prints from the image to terminal
void read_print_to_hex(FILE *fp,int pos,int numBytes,int remainingSpace,struct FAT32_Boot *b,int nextCluster)
{
  int i = 0;
  char c;
  fseek(fp,pos,SEEK_SET); // Seeks to the starting cluster position

  while(i < numBytes) // I < total bytes to be read
  {
    if(remainingSpace == 0) // Out of space in current cluster (if first iteration it is 512 - specified offset into)
    {
      nextCluster = NextLB(nextCluster,b,fp);
      if(nextCluster == -1)
      {
        continue;
      }
      else
      {
        pos = LBAToOffset(nextCluster,b);
        fseek(fp,pos,SEEK_SET);
        remainingSpace = 512;
      }
    }
    c = fgetc(fp);
    printf("%X",c);
    i++;
  }
}
// Prints the same way as previous except to ascii
void read_print_to_dec(FILE *fp,int pos,int numBytes,int remainingSpace,struct FAT32_Boot *b,int nextCluster)
{
  int i = 0;
  char c;
  fseek(fp,pos,SEEK_SET);

  while(i < numBytes)
  {
    if(remainingSpace == 0)
    {
      nextCluster = NextLB(nextCluster,b,fp);
      if(nextCluster == -1)
      {
        continue;
      }
      else
      {
        pos = LBAToOffset(nextCluster,b);
        fseek(fp,pos,SEEK_SET);
        remainingSpace = 512;
      }
    }
    c = fgetc(fp);
    printf("%d",c);
    i++;
  }
}
// Prints the same way as previous except to ascii
void read_print_to_ascii(FILE *fp,int pos,int numBytes,int remainingSpace,struct FAT32_Boot *b,int nextCluster)
{
  int i = 0;
  char c;
  fseek(fp,pos,SEEK_SET);

  while(i < numBytes)
  {
    if(remainingSpace == 0)
    {
      nextCluster = NextLB(nextCluster,b,fp);
      if(nextCluster == -1)
      {
        continue;
      }
      else
      {
        pos = LBAToOffset(nextCluster,b);
        fseek(fp,pos,SEEK_SET);
        remainingSpace = 512;
      }
    }
    c = fgetc(fp);
    printf("%c",c);
    remainingSpace--;
    i++;
  }
}
// Gets the file size yo
int get_file_size(FILE *fp)
{
  fseek(fp, 0, SEEK_END); 
  long size = ftell(fp); 
  fclose(fp);
  return size;
}
// Finds and empty FAT entry and returns it
int find_empty_cluster(FILE *fp, struct FAT32_Boot *b)
{
  int offset = (b->BPB_RsvdSecCnt * b->BPB_BytesPerSec);
  int clusterNumber = 0;
  fseek(fp, offset, SEEK_SET);
  int fatEntry;
  fread(&fatEntry,2, 1, fp);
  int numClusters = (b->BPB_TotSec32 - (b->BPB_RsvdSecCnt + (b->BPB_NumFATS * b->BPB_FATSz32)));
  while(clusterNumber < numClusters)
  {
    if (fatEntry == 0x00) 
    {
      return clusterNumber;   // returns LBA
    }
    offset += 4;
    fseek(fp,offset,SEEK_SET);
    clusterNumber++;
  }
  return -1;
}

int main( int argc, char * argv[] )
{
  FILE *fp = NULL;
  struct FAT32_Boot boot;
  struct DirectoryEntry *dir = malloc(16* sizeof(struct DirectoryEntry));
  bool closeFlag = false;
  int visited_clusters = 0;
  int nextCluster = 0;
  int total_entries = 0;
  int imageSize = 0; 
  int *clusterAdresses = NULL;
  char *currentImage = NULL;
  char *ptr = NULL;

  for( ; ; )
  {
    char secondArg[12];
    char * command_string = (char*) malloc( MAX_COMMAND_SIZE );
    char *argument_pointer;
    char *token[MAX_NUM_ARGUMENTS];
    int token_count = 0;

    printf("mfs> ");
    fgets (command_string, MAX_COMMAND_SIZE, stdin);
    while ( ( (argument_pointer = strsep(&command_string, " \t\n" ) ) != NULL) &&
                (token_count<MAX_NUM_ARGUMENTS))
    {
      if(argument_pointer[0] == '\0') // Skip past whitespace
      {
        continue;
      }
      token[token_count] = strndup( argument_pointer, MAX_COMMAND_SIZE );
      ptr = NULL;
      token_count++;
    }

    if(token[0] == NULL)
    {
      continue;
    }

    if(closeFlag)
    {
      ptr = token[0];
      while(*ptr)
      {
        *ptr = toupper((unsigned char)*ptr);
        ptr++;
      }if(!strcmp(token[0],"QUIT") || !strcmp(token[0],"EXIT") )
      {
        return 0;
      }
      else if(strcmp(token[0],"OPEN"))
      {
        printf("Error: File system image must be opened first.\n");
        continue;
      }
    }
    else
    {
      ptr = token[0];
      while(*ptr)
      {
        *ptr = toupper((unsigned char)*ptr);
        ptr++;
      }
    }

    if((!strcmp(token[0],"EXIT")) || (!strcmp(token[0],"QUIT")))
    {
      return 0;
    }
    if(token[1] != NULL)
    {
      strcpy(secondArg,token[1]);
    }

    if(!strcmp(token[0],"OPEN"))
    {
      if(fp == NULL)
      {
        fp = fopen(secondArg,"r");
        if(fp == NULL)
        {
          printf("Error: File system image not found.");
          continue;
        }
        currentImage = strdup(secondArg); // Used in multiple functionalities
        load_boot_sector(&boot,fp);
        imageSize = ((boot.BPB_NumFATS * boot.BPB_FATSz32 * boot.BPB_BytesPerSec) //Size of rsvd sec + size of fats + (size of sec * num clusters)
                  + (boot.BPB_RsvdSecCnt * boot.BPB_BytesPerSec) + (boot.BPB_SecPerClus * boot.BPB_BytesPerSec) 
                  * (boot.BPB_TotSec32 - (boot.BPB_RsvdSecCnt + (boot.BPB_NumFATS * boot.BPB_FATSz32))));
        clusterAdresses = load_cluster_addresses(fp,&boot,boot.BPB_RootClus,&total_entries, &visited_clusters); // Load up addresses of subdirectories
        closeFlag = false; 
      }
      else
      {
        printf("File already open.");
      }
    }
    else if(!strcmp(token[0],"CLOSE"))
    {
      fclose(fp);
      fp = NULL;
      closeFlag = true;
    }
    else if(!strcmp(token[0],"SAVE"))
    {
      if(token_count > 1)
      {
        printf("%s\n",currentImage);
        FILE *outputFile = fopen(secondArg,"wb");   // Opens file with specified name
        FILE *inputFile = fopen(currentImage,"rb");
        char *buffer = malloc(imageSize);           // Loads buffer size of image with image contents
        fread(buffer,1,imageSize,inputFile);
        size_t written = fwrite(buffer, 1, imageSize, outputFile); // Writes buffer to new file
        if(written == imageSize)
        {
          printf("Successfully written to new file");
        }
        fclose(inputFile);
        fclose(outputFile);
      }
      else
      {
        FILE *inputFile = fopen(currentImage,"rb"); // Opens file with same name
        char *buffer = malloc(imageSize);
        fread(buffer,1,imageSize,inputFile);
        fclose(inputFile);
        FILE *outputFile = fopen(currentImage,"wb");
        size_t written = fwrite(buffer, 1, imageSize, outputFile);
        if(written == imageSize)
        {
          printf("Successfully written to same file");
        }
      }
    }
    else if(!strcmp(token[0],"INFO"))
    {
      printf("BPB_BytesPerSec:\t%d\t%X\n",boot.BPB_BytesPerSec,boot.BPB_BytesPerSec);
      printf("BPB_SecPerClus:\t\t%d\t%X\n",boot.BPB_SecPerClus,boot.BPB_SecPerClus);
      printf("BPB_RsvdSecCnt:\t\t%d\t%X\n",boot.BPB_RsvdSecCnt,boot.BPB_RsvdSecCnt);
      printf("BPB_NumFATS:\t\t%d\t%X\n",boot.BPB_NumFATS,boot.BPB_NumFATS);
      printf("BPB_FATSz32:\t\t%d\t%X\n",boot.BPB_FATSz32,boot.BPB_FATSz32);
      printf("BPB_ExtFlags:\t\t%d\t%X\n",boot.BPB_ExtFlags,boot.BPB_ExtFlags);
      printf("BPB_RootClus:\t\t%d\t%X\n",boot.BPB_RootClus,boot.BPB_RootClus);
      printf("BPB_FSInfo:\t\t%d\t%X\n",boot.BPB_FSInfo,boot.BPB_FSInfo);
    }
    else if(!strcmp(token[0],"STAT"))
    {
      int offset;
      int nextCluster = 0;
      strcpy(secondArg,get_fat_file_name(token[1]));
      offset = LBAToOffset(boot.BPB_RootClus,&boot);
      fseek(fp,offset,SEEK_SET); //Search root dir
      load_records(dir,fp);
      int searchCluster = get_stat_info(dir,secondArg); // Search for desired file in root directory
      
      if(searchCluster == -1)     // ********This is how I am handling searching directories, multiple commands make use ******
      {                           // ********of a version of this code, it has some nested loops and is probably not very efficient******
        nextCluster = NextLB(boot.BPB_RootClus,&boot,fp); // Wasn't found in first clust of root so next cluster of root
        while(nextCluster != -1)  // If the next cluster of root isn't -1 we loop till the end of it
        {
          offset = LBAToOffset(nextCluster,&boot);
          fseek(fp,offset,SEEK_SET);
          load_records(dir,fp); // Load records from this one
          searchCluster = get_stat_info(dir,secondArg); // Search again
          if(searchCluster == -1 )
          {
            nextCluster = NextLB(nextCluster,&boot,fp);
          }
          else
          {
            break;  // we found it 
          }
        }
        if(nextCluster == -1) // Now we search the rest of the folders using the clusterAdresses array that was loaded upon opening
        {
          for(int i = 1;i < total_entries;i++)
          {
            nextCluster = clusterAdresses[i];
            while(nextCluster != -1)
            {
              offset = LBAToOffset(nextCluster,&boot);
              fseek(fp,offset,SEEK_SET);
              load_records(dir,fp);
              searchCluster = get_stat_info(dir,secondArg);
              if(searchCluster == -1 )
              {
                nextCluster = NextLB(nextCluster,&boot,fp);
              }
              else
              {
                break;
              }
            }
        }
      }
    }
    }
    else if(!strcmp(token[0],"GET"))
    {
      int offset;
      int nextCluster = 0;
      char *newFileName = strdup(token[1]);
      strcpy(secondArg,get_fat_file_name(token[1]));
      offset = LBAToOffset(boot.BPB_RootClus,&boot);
      fseek(fp,offset,SEEK_SET); 
      load_records(dir,fp);
      int searchCluster = get_file_cluster(dir,secondArg); // Gets the file by searching each directory starting from root
      
      if(searchCluster == -1)
      {
        nextCluster = NextLB(boot.BPB_RootClus,&boot,fp);
        printf("%d\n",nextCluster);
        while(nextCluster != -1)
        {
          offset = LBAToOffset(nextCluster,&boot);
          fseek(fp,offset,SEEK_SET);
          load_records(dir,fp);
          searchCluster = get_file_cluster(dir,secondArg);
          if(searchCluster == -1 )
          {
            nextCluster = NextLB(nextCluster,&boot,fp);
          }
          else
          {
            break;
          }
        }
        if(nextCluster == -1)
        {
          for(int i = 1;i < total_entries;i++)
          {
            nextCluster = clusterAdresses[i];
            while(nextCluster != -1)
            {
              offset = LBAToOffset(nextCluster,&boot);
              fseek(fp,offset,SEEK_SET);
              load_records(dir,fp);
              searchCluster = get_file_cluster(dir,secondArg);
              if(searchCluster == -1 )
              {
                nextCluster = NextLB(nextCluster,&boot,fp);
              }
              else
              {
                break;
              }
            }
        }
      }
    }
      FILE *outf = fopen(newFileName,"w");
      FILE *inpf = fopen(currentImage,"rb");
      offset = LBAToOffset(searchCluster,&boot);  //Opens cluster address from the successfully found file
      fseek(inpf,offset,SEEK_SET);
      write_to_cluster_or_file(outf,inpf);

      nextCluster = NextLB(searchCluster,&boot,fp); //Get next cluster address
      while(nextCluster != -1)                    //If not eof loop to get rest of file
      {
        offset = LBAToOffset(nextCluster,&boot);
        
        fseek(inpf,offset,SEEK_SET);
        write_to_cluster_or_file(outf,inpf);
        nextCluster = NextLB(nextCluster,&boot,fp);
      }
      fclose(inpf);
      fclose(outf);  
    }
    else if(!strcmp(token[0],"PUT")) // I DON'T THINK THIS WORKS AS INTENDED
    {
      int previousFat = 0;
      struct DirectoryEntry *newD = malloc(sizeof(struct DirectoryEntry));
      int offset = 0;
      int emptyCluster = find_empty_cluster(fp,&boot);
      if(emptyCluster == -1)
      {
        printf("Error: No empty clusters.\n");
      }
      if(token_count == 2)
      {
        char *fileName = strdup(token[1]);              
        strcpy(secondArg,get_fat_file_name(token[1]));
        FILE *inpf = fopen(fileName,"r+b");
        FILE *outf = fopen(currentImage,"r+b");
        int fileSize = get_file_size(inpf);
        int amountClustersNeeded = (fileSize / 512);
        offset = LBAToOffset(boot.BPB_RootClus,&boot);
        fseek(fp,offset,SEEK_SET); //Search root dir
        load_records(dir,fp);
    
        strncpy(newD->DIR_Name,secondArg,11);     // Load new directory entry with specified values
        newD->DIR_Attr = 0x01;
        newD->DIR_FirstClusterHigh = (int16_t)(emptyCluster >> 16);
        newD->DIR_FirstClusterLow = (int16_t)(emptyCluster & 0xFFFF);
        newD->DIR_FileSize = fileSize;

        int found = get_empty_directory_entry(fp,dir,newD); // Gets the empty directory entyr by searching each directory starting from root
        if(!found)
        {
          nextCluster = NextLB(boot.BPB_RootClus,&boot,fp);
          printf("%d\n",nextCluster);
          while(nextCluster != -1)
          {
            offset = LBAToOffset(nextCluster,&boot);
            fseek(fp,offset,SEEK_SET);
            load_records(dir,fp);
            found = get_empty_directory_entry(fp,dir,newD);
            if(!found)
            {
              nextCluster = NextLB(nextCluster,&boot,fp);
            }
            else
            {
              break;
            }
          }
          if(nextCluster == -1)
          {
            for(int i = 1;i < total_entries;i++)
            {
              nextCluster = clusterAdresses[i];
              while(nextCluster != -1)
              {
                offset = LBAToOffset(nextCluster,&boot);
                fseek(fp,offset,SEEK_SET);
                load_records(dir,fp);
                found = get_empty_directory_entry(fp,dir,newD);
                if(!found)
                {
                  nextCluster = NextLB(nextCluster,&boot,fp);
                }
                else
                {
                  break;
                }
              }
            }
          }
        }
        previousFat = ((emptyCluster * 4) + (boot.BPB_BytesPerSec * boot.BPB_RsvdSecCnt));//Go to empty address
        fseek(outf,previousFat,SEEK_SET);
        fwrite(&boot.BPB_RootClus,4,1,outf);  // Write root LBA to it
        offset = LBAToOffset(emptyCluster,&boot);
        fseek(outf,offset,SEEK_SET);
        write_to_cluster_or_file(outf,inpf);  // Write data in the cluster of the first empty fat
        nextCluster = find_empty_cluster(fp,&boot);
        previousFat = ((nextCluster * 4) + (boot.BPB_BytesPerSec * boot.BPB_RsvdSecCnt));
        fseek(outf,previousFat,SEEK_SET);
        fwrite(&emptyCluster,4,1,outf);              // Write to previous fat address the new fat address
        for(int i = 0;i < amountClustersNeeded;i++) // Repeat until all clusters are loaded
        {
          emptyCluster = find_empty_cluster(fp,&boot);
          offset = LBAToOffset(emptyCluster,&boot);
          fseek(outf,offset,SEEK_SET);
          write_to_cluster_or_file(outf,inpf);
          if(i == (amountClustersNeeded - 1))
          {
            int j = -1;
            previousFat = ((emptyCluster * 4) + (boot.BPB_BytesPerSec * boot.BPB_RsvdSecCnt));
            fseek(outf,previousFat,SEEK_SET);
            fwrite(&j,4,1,outf);
          }
          else
          {
            nextCluster = find_empty_cluster(fp,&boot);
            previousFat = ((nextCluster * 4) + (boot.BPB_BytesPerSec * boot.BPB_RsvdSecCnt));
            fseek(outf,previousFat,SEEK_SET);
            fwrite(&emptyCluster,4,1,outf);
          }
          fclose(inpf);
          fclose(outf);
        }
        
      }
      else if(token_count == 3)
      {
        char *fileName = strdup(token[1]);
        strcpy(secondArg,get_fat_file_name(token[2]));
        FILE *inpf = fopen(fileName,"r+b");
        FILE *outf = fopen(currentImage,"r+b");
        int fileSize = get_file_size(inpf);
        int amountClustersNeeded = (fileSize / 512);
        offset = LBAToOffset(boot.BPB_RootClus,&boot);
        fseek(fp,offset,SEEK_SET); //Search root dir
        load_records(dir,fp);
    
        strncpy(newD->DIR_Name,secondArg,11);     // Load new directory entry with specified values
        newD->DIR_Attr = 0x01;
        newD->DIR_FirstClusterHigh = (int16_t)(emptyCluster >> 16);
        newD->DIR_FirstClusterLow = (int16_t)(emptyCluster & 0xFFFF);
        newD->DIR_FileSize = fileSize;

        int found = get_empty_directory_entry(fp,dir,newD); // Gets the empty directory entyr by searching each directory starting from root
        if(!found)
        {
          nextCluster = NextLB(boot.BPB_RootClus,&boot,fp);
          printf("%d\n",nextCluster);
          while(nextCluster != -1)
          {
            offset = LBAToOffset(nextCluster,&boot);
            fseek(fp,offset,SEEK_SET);
            load_records(dir,fp);
            found = get_empty_directory_entry(fp,dir,newD);
            if(!found)
            {
              nextCluster = NextLB(nextCluster,&boot,fp);
            }
            else
            {
              break;
            }
          }
          if(nextCluster == -1)
          {
            for(int i = 1;i < total_entries;i++)
            {
              nextCluster = clusterAdresses[i];
              while(nextCluster != -1)
              {
                offset = LBAToOffset(nextCluster,&boot);
                fseek(fp,offset,SEEK_SET);
                load_records(dir,fp);
                found = get_empty_directory_entry(fp,dir,newD);
                if(!found)
                {
                  nextCluster = NextLB(nextCluster,&boot,fp);
                }
                else
                {
                  break;
                }
              }
            }
          }
        }
        previousFat = ((emptyCluster * 4) + (boot.BPB_BytesPerSec * boot.BPB_RsvdSecCnt));//Go to empty address
        fseek(outf,previousFat,SEEK_SET);
        fwrite(&boot.BPB_RootClus,4,1,outf);  // Write root LBA to it
        offset = LBAToOffset(emptyCluster,&boot);
        fseek(outf,offset,SEEK_SET);
        write_to_cluster_or_file(outf,inpf);  // Write data in the cluster of the first empty fat
        nextCluster = find_empty_cluster(fp,&boot);
        previousFat = ((nextCluster * 4) + (boot.BPB_BytesPerSec * boot.BPB_RsvdSecCnt));
        fseek(outf,previousFat,SEEK_SET);
        fwrite(&emptyCluster,4,1,outf);              // Write to previous fat address the new fat address
        for(int i = 0;i < amountClustersNeeded;i++) // Repeat until all clusters are loaded
        {
          emptyCluster = find_empty_cluster(fp,&boot);
          offset = LBAToOffset(emptyCluster,&boot);
          fseek(outf,offset,SEEK_SET);
          write_to_cluster_or_file(outf,inpf);
          if(i == (amountClustersNeeded - 1))
          {
            int j = -1;
            previousFat = ((emptyCluster * 4) + (boot.BPB_BytesPerSec * boot.BPB_RsvdSecCnt));
            fseek(outf,previousFat,SEEK_SET);
            fwrite(&j,4,1,outf);
          }
          else
          {
            nextCluster = find_empty_cluster(fp,&boot);
            previousFat = ((nextCluster * 4) + (boot.BPB_BytesPerSec * boot.BPB_RsvdSecCnt));
            fseek(outf,previousFat,SEEK_SET);
            fwrite(&emptyCluster,4,1,outf);
          }
          fclose(inpf);
          fclose(outf);
        } 
      }
    }
    else if(!strcmp(token[0],"CD"))
    {
      char *path = malloc(MAX_COMMAND_SIZE*sizeof(char));
      if(chdir(token[1]) != 0)
      {
        printf("Directory not found.\n");
      }
      else
      {
        ptr = getcwd(path,MAX_COMMAND_SIZE);
        printf("New Directory: %s\n",ptr);  
      }
    }
    else if (!strcmp(token[0], "LS")) 
    {
      nextCluster = boot.BPB_RootClus;
      int offset = 0;
      while(nextCluster != -1)
      {
        offset = LBAToOffset(nextCluster,&boot);
        fseek(fp,offset,SEEK_SET);
        load_records(dir,fp);
        print_records(dir);
        nextCluster = NextLB(nextCluster,&boot,fp);
      }
      for(int i = 1;i < total_entries;i++)
      {
        nextCluster = clusterAdresses[i];
        while(nextCluster != -1)
        {
          offset = LBAToOffset(nextCluster,&boot);
          fseek(fp,offset,SEEK_SET);
          load_records(dir,fp);
          print_records(dir);
          nextCluster = NextLB(nextCluster,&boot,fp);
        }
      }
    }
    else if(!strcmp(token[0],"READ"))
    {//read <filename> <position> <number of bytes> <OPTION>
      int offset = 0;
      char *optionFlag = NULL;
      int position = atoi(token[2]);
      int numBytes = atoi(token[3]);
      int positionCluster = 0;
      int remainingSpace = 0;
      strcpy(secondArg,get_fat_file_name(token[1]));
      offset = LBAToOffset(boot.BPB_RootClus,&boot);
      fseek(fp,offset,SEEK_SET); //Search root dir
      load_records(dir,fp);
      int searchCluster = get_file_cluster(dir,secondArg);
      
      if(searchCluster == -1)
      {
        nextCluster = NextLB(boot.BPB_RootClus,&boot,fp);
        while(nextCluster != -1)
        {
          offset = LBAToOffset(nextCluster,&boot);
          fseek(fp,offset,SEEK_SET);
          load_records(dir,fp);
          searchCluster = get_file_cluster(dir,secondArg);
          if(searchCluster == -1 )
          {
            nextCluster = NextLB(nextCluster,&boot,fp);
          }
          else
          {
            break;
          }
        }
        if(nextCluster == -1)
        {
          for(int i = 1;i < total_entries;i++)
          {
            nextCluster = clusterAdresses[i];
            while(nextCluster != -1)
            {
              offset = LBAToOffset(nextCluster,&boot);
              fseek(fp,offset,SEEK_SET);
              load_records(dir,fp);
              searchCluster = get_file_cluster(dir,secondArg);
              if(searchCluster == -1 )
              {
                nextCluster = NextLB(nextCluster,&boot,fp);
              }
              else
              {
                break;
              }
            }
        }
      }
    }
      nextCluster = searchCluster;
      positionCluster = (position / 512); // Calculate how many clusters needed
      position = (position % 512);        // Calculate the offset into the last cluster
      remainingSpace = (512 - position);  // Get remaining space of last cluster
      if(positionCluster > 0)
      {
        for(int j = 0;j < positionCluster;j++)
        {
          if(nextCluster == -1)
          {
            break;
          }
          nextCluster = NextLB(nextCluster,&boot,fp);
        }
        if(nextCluster == -1)
        {
          continue;
        }
        position += LBAToOffset(nextCluster,&boot); 
      }
      else
      {
        position += LBAToOffset(nextCluster,&boot); 
      }
      
      if(token_count == 5)
      {
        optionFlag = strdup(token[4]);
        ptr = optionFlag;
        while(*ptr)
        {
          *ptr = toupper((unsigned char)*ptr);
          ptr++;
        }
        if(!strcmp(optionFlag,"-ASCII"))
        {
          read_print_to_ascii(fp,position,numBytes,remainingSpace,&boot,nextCluster);
        }
        else if(!strcmp(optionFlag,"-DEC"))
        {
          read_print_to_dec(fp,position,numBytes,remainingSpace,&boot,nextCluster);
        }
        else
        {
          printf("Error: Incorrect flag");
        }
      }
      else
      {
        read_print_to_hex(fp,position,numBytes,remainingSpace,&boot,nextCluster);
      }
      printf("\n");
    }
    else if(!strcmp(token[0],"DEL"))
    {
      int offset = 0;
      FILE *editPtr = fopen(currentImage,"r+b");
      strcpy(secondArg,get_fat_file_name(token[1]));
      offset = LBAToOffset(boot.BPB_RootClus,&boot);
      fseek(fp,offset,SEEK_SET); //Search root dir
      load_records(dir,fp);
      int searchCluster = delete_file(editPtr,&boot,dir,secondArg,offset); // Searches for the specified file and the function deletes
      
      if(searchCluster == -1)
      {
        nextCluster = NextLB(boot.BPB_RootClus,&boot,fp);
        while(nextCluster != -1)
        {
          offset = LBAToOffset(nextCluster,&boot);
          fseek(fp,offset,SEEK_SET);
          load_records(dir,fp);
          searchCluster = delete_file(editPtr,&boot,dir,secondArg,offset);
          if(searchCluster == -1 )
          {
            nextCluster = NextLB(nextCluster,&boot,fp);
          }
          else
          {
            break;
          }
        }
        if(nextCluster == -1)
        {
          for(int i = 1;i < total_entries;i++)
          {
            nextCluster = clusterAdresses[i];
            while(nextCluster != -1)
            {
              offset = LBAToOffset(nextCluster,&boot);
              fseek(fp,offset,SEEK_SET);
              load_records(dir,fp);
              searchCluster = delete_file(editPtr,&boot,dir,secondArg,offset);
              if(searchCluster == -1 )
              {
                nextCluster = NextLB(nextCluster,&boot,fp);
              }
              else
              {

                break;
              }
            }
          }
        }
      }
      fclose(editPtr);
    }
    else if(!strcmp(token[0],"UNDEL"))  
    {
      int offset = 0;
      FILE *editPtr = fopen(currentImage,"r+b");
      strcpy(secondArg,get_fat_file_name(token[1]));
      char firstLetter = secondArg[0];
      secondArg[0] = 0xe5;
      offset = LBAToOffset(boot.BPB_RootClus,&boot);
      fseek(fp,offset,SEEK_SET); //Search root dir
      load_records(dir,fp);
      int searchCluster = undelete_file(editPtr,&boot,dir,secondArg,offset,firstLetter); // Searches for the specified file and the function undeletes
      
      if(searchCluster == -1)
      {
        nextCluster = NextLB(boot.BPB_RootClus,&boot,fp);
        while(nextCluster != -1)
        {
          offset = LBAToOffset(nextCluster,&boot);
          fseek(fp,offset,SEEK_SET);
          load_records(dir,fp);
          searchCluster = undelete_file(editPtr,&boot,dir,secondArg,offset,firstLetter);
          if(searchCluster == -1 )
          {
            nextCluster = NextLB(nextCluster,&boot,fp);
          }
          else
          {
            break;
          }
        }
        if(nextCluster == -1)
        {
          for(int i = 1;i < total_entries;i++)
          {
            nextCluster = clusterAdresses[i];
            while(nextCluster != -1)
            {
              offset = LBAToOffset(nextCluster,&boot);
              fseek(fp,offset,SEEK_SET);
              load_records(dir,fp);
              searchCluster = undelete_file(editPtr,&boot,dir,secondArg,offset,firstLetter);
              if(searchCluster == -1 )
              {
                nextCluster = NextLB(nextCluster,&boot,fp);
              }
              else
              {
                break;
              }
            }
          }
        }
      }
      fclose(editPtr);
    }
    for(int i = 0;i < MAX_NUM_ARGUMENTS;i++)
    {
      token[i] = NULL;
    }
    memset(secondArg,0,12);
  } 
  return 0;
}


