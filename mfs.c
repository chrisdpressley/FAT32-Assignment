#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <stdbool.h>
#define MAX_COMMAND_SIZE 100
#define MAX_NUM_ARGUMENTS 5

struct FAT32_Boot{            //Offsets:
  char BS_OEMName[8];         // 3
  int16_t BPB_BytesPerSec;   // 11 
  int8_t BPB_SecPerClus;     // 13 
  int16_t BPB_RsvdSecCnt;    // 14
  int8_t BPB_NumFATS;        // 16
  int16_t BPB_RootEntCnt;    // 17
  int32_t BPB_TotSec32;      // 34
  int16_t BPB_FATSz32;       // 36
  int16_t BPB_ExtFlags;      // 40
  int32_t BPB_RootClus;      // 44
  int16_t BPB_FSInfo;        // 48
  char BS_VolLab[11];         // 71
  int32_t RootDirSectors;
  int32_t FirstDataSector;
  int32_t FirstSectorofCluster;
};

struct DirectoryEntry {
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
        return -1; // End of chain marker
  }
  return val;
}


int* load_cluster_addresses(FILE *fp, struct FAT32_Boot *b, int root, int *total_entries, int *visited_clusters) 
{
  int *cluster_addresses = malloc(sizeof(int) * 10);  // start with 10 addresses, this is a random amount I chose
  int num_clusters = 1;      //Cluster array starts with the size for later use
  cluster_addresses[0] = 0;  // size is the total number of directory addr.'s

  // Read directory entries from the specified cluster
  int offset = LBAToOffset(root, b);
  fseek(fp, offset, SEEK_SET);

  // Iterate over the directory entries
  struct DirectoryEntry entry;
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
        // Full cluster
        int cluster = entry.DIR_FirstClusterLow + (entry.DIR_FirstClusterHigh << 16);

        // Check if the cluster has been visited
        bool already_visited = false;
        for (int j = 0; j < *visited_clusters; j++) {
            if (cluster_addresses[j] == cluster) {
                already_visited = true;
                break;
            }
        }
        printf("%s: %X: %X\n",entry.DIR_Name,entry.DIR_Attr,cluster);
        // Add to list if not visited
        if (!already_visited) 
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
          
          // Append the subdir clusters to the main array, starts from j = 1 due to storing the size in the first element
          for (int j = 1; j < *total_entries; j++) 
          {
              cluster_addresses[num_clusters + j - 1] = subdir_clusters[j];
          }
          num_clusters += (*total_entries - 1);  // addr.'s  
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

void load_bootSector_values(struct FAT32_Boot * b,FILE *fp)
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
  //char IMG_Name[11] = "FAT32   IMG";
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
  /*if( strncmp( newName, IMG_Name, 11 ) == 0 )
  {
    printf("They matched\n");
  }*/
  return newName;
}
int undelete_file(struct DirectoryEntry *d, char *fileName,char firstLetter)
{
  for(int i = 0;i<16;i++)
  {
    //printf("%s\n",d[i].DIR_Name);
    if(!strcmp(d[i].DIR_Name,fileName))
    {
      d[i].DIR_Name[0] = firstLetter; 
    }
  }
  return -1;
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
void print_records(struct DirectoryEntry *d) 
{
  for (int i = 0; i < 16; i++) 
  {
    if (d[i].DIR_Attr == 0x01 || d[i].DIR_Attr == 0x10 || d[i].DIR_Attr == 0x20)
    { 
      if(d[i].DIR_Name[0] == 0xe5 || d[i].DIR_Name[0] == 0x00)
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

void write_binary_cluster_to_file(FILE *outputFile,FILE *inputFile)
{
  int i = 0;
  char c;

  while(i < 512)
  {
    c = fgetc(inputFile);
    if (feof(inputFile)) 
    {
      break; // Stop when end of file is reached
    }
    fwrite(&c,1,1,outputFile);
    i++;
  }
}
void read_print_to_hex(FILE *fp,int pos,int numBytes,int remainingSpace,struct FAT32_Boot *b,int nextCluster)
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
    printf("%X",c);
    i++;
  }
}
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

int main( int argc, char * argv[] )
{
  FILE *fp = NULL;
  struct FAT32_Boot boot;
  struct DirectoryEntry *dir = malloc(16* sizeof(struct DirectoryEntry));
  int visited_clusters = 0;
  int *clusterAdresses = NULL;
  int nextCluster = 0;
  int total_entries = 0;
  char *currentImage = NULL;
  char *ptr = NULL;
  int imageSize; 
  printf("%d, %d, %d, %d\n",(10/512),(616/512),(10 % 512),(614 % 512));
  for( ; ; )
  {
    if(fp != NULL)
    {
      fseek(fp,((boot.BPB_NumFATS * boot.BPB_FATSz32 * boot.BPB_BytesPerSec) 
            + (boot.BPB_RsvdSecCnt * boot.BPB_BytesPerSec)),SEEK_SET);
    }
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
      token_count++;
    }


    if((!strcmp(token[0],"exit")) || (!strcmp(token[0],"quit")))
    {
      return 0;
    }


    if(token[1] != NULL)
    {
      strcpy(secondArg,token[1]);
    }

    if(!strcmp(token[0],"open"))
    {
      if(fp == NULL)
      {
        fp = fopen(secondArg,"r");
        if(fp == NULL)
        {
          printf("File not found");
          continue;
        }
        currentImage = strdup(secondArg);
        load_bootSector_values(&boot,fp);
        imageSize = ((boot.BPB_NumFATS * boot.BPB_FATSz32 * boot.BPB_BytesPerSec) //Size of rsvd sec + size of fats + (size of sec * num clusters)
                  + (boot.BPB_RsvdSecCnt * boot.BPB_BytesPerSec) + (boot.BPB_SecPerClus * boot.BPB_BytesPerSec) //spent at least an hour figuring this out
                  * (boot.BPB_TotSec32 - (boot.BPB_RsvdSecCnt + (boot.BPB_NumFATS * boot.BPB_FATSz32))));
        printf("%d\n",imageSize);
        clusterAdresses = load_cluster_addresses(fp,&boot,boot.BPB_RootClus,&total_entries, &visited_clusters);

        for(int i = 1;i < total_entries;i++)
        {
          printf("%X, ",clusterAdresses[i]);
        }
      }
      else
      {
        printf("File already open.");
      }
    }
    else if(!strcmp(token[0],"save"))
    {
      if(token_count > 1)
      {
        printf("%s\n",currentImage);
        FILE *outputFile = fopen(secondArg,"wb");
        FILE *inputFile = fopen(currentImage,"rb");
        char *buffer = malloc(imageSize);
        fread(buffer,1,imageSize,inputFile);
        size_t written = fwrite(buffer, 1, imageSize, outputFile);
        if(written == imageSize)
        {
          printf("Successfully written to new file");
        }
        fclose(inputFile);
        fclose(outputFile);
      }
      else
      {
        FILE *inputFile = fopen(currentImage,"rb");
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
    else if(!strcmp(token[0],"info"))
    {
      printf("BPB_BytesPerSec:\t%d\t%X\n",boot.BPB_BytesPerSec,boot.BPB_BytesPerSec);
      printf("BPB_SecPerClus:\t%d\t%X\n",boot.BPB_SecPerClus,boot.BPB_SecPerClus);
      printf("BPB_RsvdSecCnt:\t%d\t%X\n",boot.BPB_RsvdSecCnt,boot.BPB_RsvdSecCnt);
      printf("BPB_NumFATS:\t\t%d\t%X\n",boot.BPB_NumFATS,boot.BPB_NumFATS);
      printf("BPB_FATSz32:\t\t%d\t%X\n",boot.BPB_FATSz32,boot.BPB_FATSz32);
      printf("BPB_ExtFlags:\t\t%d\t%X\n",boot.BPB_ExtFlags,boot.BPB_ExtFlags);
      printf("BPB_RootClus:\t\t%d\t%X\n",boot.BPB_RootClus,boot.BPB_RootClus);
      printf("BPB_FSInfo:\t\t%d\t%X\n",boot.BPB_FSInfo,boot.BPB_FSInfo);
    }
    else if(!strcmp(token[0],"stat"))
    {
      int offset;
      int nextCluster = 0;
      printf("|%s|",token[1]);
      strcpy(secondArg,get_fat_file_name(token[1]));
      offset = LBAToOffset(boot.BPB_RootClus,&boot);
      fseek(fp,offset,SEEK_SET); //Search root dir
      load_records(dir,fp);
      int searchCluster = get_stat_info(dir,secondArg);
      
      if(searchCluster == -1)     // This is how I am handling searching directories, multiple commands make use 
      {                           // of a version of this code, it has some nested loops and is probably not very efficient
        nextCluster = NextLB(boot.BPB_RootClus,&boot,fp);
        printf("%d\n",nextCluster);
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
    else if(!strcmp(token[0],"get"))
    {
      int offset;
      int nextCluster = 0;
      char *newFileName = strdup(token[1]);
      printf("|%s|",token[1]);
      strcpy(secondArg,get_fat_file_name(token[1]));
      printf("|%s|",secondArg);
      offset = LBAToOffset(boot.BPB_RootClus,&boot);
      fseek(fp,offset,SEEK_SET); //Search root dir
      load_records(dir,fp);
      int searchCluster = get_file_cluster(dir,secondArg);
      
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
      printf("%d\n",searchCluster);
      FILE *outf = fopen(newFileName,"w");
      FILE *inpf = fopen(currentImage,"rb");
      offset = LBAToOffset(searchCluster,&boot);  //Opens cluster address from the successfully found file
      printf("%d | %d\n",nextCluster,offset);
      fseek(inpf,offset,SEEK_SET);
      write_binary_cluster_to_file(outf,inpf);

      nextCluster = NextLB(searchCluster,&boot,fp); //Get next cluster address
      printf("%d | %d\n",nextCluster,offset);
      while(nextCluster != -1)                    //If not eof loop to get rest of file
      {
        offset = LBAToOffset(nextCluster,&boot);
        
        fseek(inpf,offset,SEEK_SET);
        write_binary_cluster_to_file(outf,inpf);
        nextCluster = NextLB(nextCluster,&boot,fp);
      }
      fclose(inpf);
      fclose(outf);
        
    }
    else if(!strcmp(token[0],"put"))
    {
      
    }
    else if(!strcmp(token[0],"cd"))
    {
      char *path = malloc(MAX_COMMAND_SIZE*sizeof(char));
      if(chdir(token[1]) != 0)
      {
        printf("Directory not found.\n");
      }
      else
      {
        ptr = getcwd(path,MAX_COMMAND_SIZE);
        printf("%s\n",ptr);
      }
    }
    else if (!strcmp(token[0], "ls")) 
    {
      // Start from the root directory (BPB_RootClus is the starting cluster for the root)
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
    else if(!strcmp(token[0],"read"))
    {//read <filename> <position> <number of bytes> <OPTION>
      int offset = 0;
      char *optionFlag = NULL;
      int position = atoi(token[2]);
      int numBytes = atoi(token[3]);
      int positionCluster = 0;
      int remainingSpace = 0;
      strcpy(secondArg,get_fat_file_name(token[1]));
      printf("|%s|",secondArg);
      offset = LBAToOffset(boot.BPB_RootClus,&boot);
      fseek(fp,offset,SEEK_SET); //Search root dir
      load_records(dir,fp);
      int searchCluster = get_file_cluster(dir,secondArg);
      
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
      nextCluster = searchCluster;
      positionCluster = (position / 512);
      position = (position % 512);
      remainingSpace = (512 - position);
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
        if(!strcmp(optionFlag,"-ascii"))
        {
          read_print_to_ascii(fp,position,numBytes,remainingSpace,&boot,nextCluster);
        }
        else if(!strcmp(optionFlag,"-dec"))
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
    }
    else if(!strcmp(token[0],"del"))
    {
      
    }
    else if(!strcmp(token[0],"undel"))
    {
      int offset = 0;
      
      strcpy(secondArg,get_fat_file_name(token[1]));
      char firstLetter = secondArg[0];
      secondArg[0] = 0xe5;
      printf("|%s|",secondArg);
      offset = LBAToOffset(boot.BPB_RootClus,&boot);
      fseek(fp,offset,SEEK_SET); //Search root dir
      load_records(dir,fp);
      int searchCluster = undelete_file(dir,secondArg,firstLetter);
      
      if(searchCluster == -1)
      {
        nextCluster = NextLB(boot.BPB_RootClus,&boot,fp);
        printf("%d\n",nextCluster);
        while(nextCluster != -1)
        {
          offset = LBAToOffset(nextCluster,&boot);
          fseek(fp,offset,SEEK_SET);
          load_records(dir,fp);
          searchCluster = undelete_file(dir,secondArg,firstLetter);
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
              searchCluster = undelete_file(dir,secondArg,firstLetter);
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
  } 
  return 0;
}


