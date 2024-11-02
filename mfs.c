#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>

#define MAX_COMMAND_SIZE 100
#define MAX_NUM_ARGUMENTS 3

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
};

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


void print_records(struct DirectoryEntry *d)
{
  for(int i = 0;i<16;i++)
  {
    if(d[i].DIR_Attr == 0x01 || d[i].DIR_Attr == 0x10 || d[i].DIR_Attr == 0x20 )
    {
      printf("%d. DIR_FileName: |%s|\n",i,d[i].DIR_Name);
      printf("%d. DIR_Attr: %X\n",i,d[i].DIR_Attr);
      printf("%d. DIR_FirstClusterHigh: %X\n",i,d[i].DIR_FirstClusterHigh);
      printf("%d. DIR_FirstClusterLow: %X\n",i,d[i].DIR_FirstClusterLow);
      printf("%d. DIR_FileSize: %d\n",i,d[i].DIR_FileSize);
    }
  }
}

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
  return val;
}
void get_stat_info(struct DirectoryEntry *d,char * searchName)
{
  for(int i = 0;i<16;i++)
  {
    if(strcmp(d[i].DIR_Name,searchName) == 0)
    {
      printf("Attribute: %X\nStarting Cluster Number: %X%X\n",d[i].DIR_Attr,d[i].DIR_FirstClusterHigh,d[i].DIR_FirstClusterLow);
      return;
    }
  }
}


int main( int argc, char * argv[] )
{
  FILE *fp = NULL;
  struct FAT32_Boot boot;
  struct DirectoryEntry dir[16]; 
  char *currentImage = NULL;
  int imageSize; 
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


    if(strcmp(token[0],"exit") == 0 || strcmp(token[0],"quit") == 0)
    {
      return 0;
    }


    if(token[1] != NULL)
    {
      strcpy(secondArg,token[1]);
    }

    if(strcmp(token[0],"open") == 0)
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
        
        fseek(fp,((boot.BPB_NumFATS * boot.BPB_FATSz32 * boot.BPB_BytesPerSec) 
            + (boot.BPB_RsvdSecCnt * boot.BPB_BytesPerSec)),SEEK_SET);
        load_records(dir,fp);
        print_records(dir);
        imageSize = ((boot.BPB_NumFATS * boot.BPB_FATSz32 * boot.BPB_BytesPerSec) //Size of rsvd sec + size of fats + (size of sec * num clusters)
                  + (boot.BPB_RsvdSecCnt * boot.BPB_BytesPerSec) + (boot.BPB_SecPerClus * boot.BPB_BytesPerSec) //spent at least an hour figuring this out
                  * (boot.BPB_TotSec32 - (boot.BPB_RsvdSecCnt + (boot.BPB_NumFATS * boot.BPB_FATSz32))));
        printf("%d\n",imageSize);
      }
      else
      {
        printf("File already open.");
      }
    }
    else if(strcmp(token[0],"save") == 0)
    {
      if(strlen(token[1]) != 0)
      {
        printf("%s\n",currentImage);
        FILE *outputFile = fopen(secondArg,"wb");
        FILE *inputFile = fopen(currentImage,"rb");
        char *buffer = malloc(imageSize);
        fseek(fp,0,SEEK_SET);
        fread(buffer,1,imageSize,inputFile);
        size_t written = fwrite(buffer, 1, imageSize, outputFile);
        if(written == imageSize)
        {
          fclose(inputFile);
          fclose(outputFile);
        }
        
      }
      else
      {

      }
    }
    else if(strcmp(token[0],"info") == 0)
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
    else if(strcmp(token[0],"stat") == 0)
    {
      //strcpy(secondArg,token[1]);
      strcpy(secondArg,get_fat_file_name(token[1]));
      get_stat_info(dir,secondArg);
    }
    else if(strcmp(token[0],"get") == 0)
    {
      
    }
    else if(strcmp(token[0],"put") == 0)
    {
      
    }
    else if(strcmp(token[0],"cd") == 0)
    {
      
    }
    else if(strcmp(token[0],"ls") == 0)
    {
      for(int i = 0;i < 16;i++)
      {
        if(dir[i].DIR_Attr == 0x01 || dir[i].DIR_Attr == 0x10 || dir[i].DIR_Attr == 0x20 )
        {
          printf("%s\n",dir[i].DIR_Name);
        }
      }
    }
    else if(strcmp(token[0],"read") == 0)
    {
      
    }
    else if(strcmp(token[0],"del") == 0)
    {
      
    }
    else if(strcmp(token[0],"undel") == 0)
    {
      
    }
  } 
  return 0;
}


