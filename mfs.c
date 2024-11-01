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
  int16_t BPB_FATSz32;       // 36
  int16_t BPB_ExtFlags;      // 40
  int32_t BPB_RootClus;      // 44
  int16_t BPB_FSInfo;        // 48
  char BS_VolLab[11];         // 71
  int32_t RootDirSectors;
  int32_t FirstDataSector;
  int32_t FirstSectorofCluster;
};


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


int main( int argc, char * argv[] )
{
  FILE *fp = NULL;
  struct FAT32_Boot boot;
  char name[9],ext[4];
  for( ; ; )
  {
    if(fp != NULL)
    {
      fseek(fp,((boot.BPB_NumFATS * boot.BPB_FATSz32 * boot.BPB_BytesPerSec) 
            + (boot.BPB_RsvdSecCnt * boot.BPB_BytesPerSec)),SEEK_SET);
    }
    char imgName[12];
    char * command_string = (char*) malloc( MAX_COMMAND_SIZE );
    char *argument_pointer;
    char *token[MAX_NUM_ARGUMENTS];
    //char executePath[MAX_COMMAND_SIZE];
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
      strcpy(imgName,token[1]);
    }
    if(strcmp(token[0],"open") == 0)
    {
      if(fp == NULL)
      {
        fp = fopen(imgName,"r");
        if(fp == NULL)
        {
          printf("File not found");
          continue;
        }
        load_bootSector_values(&boot,fp);
      }
      else
      {
        printf("File already open.");
        continue;
      }
    }
    else if(strcmp(token[0],"save") == 0)
    {
      
    }
    else if(strcmp(token[0],"info") == 0)
    {
      
      printf("BPB_BytesPerSec: \t%d\t%X\n",boot.BPB_BytesPerSec,boot.BPB_BytesPerSec);
      printf("BPB_SecPerClus: \t%d\t%X\n",boot.BPB_SecPerClus,boot.BPB_SecPerClus);
      printf("BPB_RsvdSecCnt: \t%d\t%X\n",boot.BPB_RsvdSecCnt,boot.BPB_RsvdSecCnt);
      printf("BPB_NumFATS: \t\t%d\t%X\n",boot.BPB_NumFATS,boot.BPB_NumFATS);
      printf("BPB_FATSz32: \t\t%d\t%X\n",boot.BPB_FATSz32,boot.BPB_FATSz32);
      printf("BPB_ExtFlags: \t\t%d\t%X\n",boot.BPB_ExtFlags,boot.BPB_ExtFlags);
      printf("BPB_RootClus: \t\t%d\t%X\n",boot.BPB_RootClus,boot.BPB_RootClus);
      printf("BPB_BPB_FSInfo: \t%d\t%X\n",boot.BPB_FSInfo,boot.BPB_FSInfo);
    }
    else if(strcmp(token[0],"stat") == 0)
    {
      
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
      fseek(fp,((boot.BPB_NumFATS * boot.BPB_FATSz32 * boot.BPB_BytesPerSec) 
            + (boot.BPB_RsvdSecCnt * boot.BPB_BytesPerSec)),SEEK_SET);
      for(int i = 0;i<16;i++)
      {
        int8_t attr;
        fseek(fp,11,SEEK_CUR);
        fread(&attr,1,1,fp);
        fseek(fp,-12,SEEK_CUR);
        if(attr == 0x01 || attr == 0x10 || attr == 0x20)
        {
          fread(name,8,1,fp);
          fread(ext,3,1,fp);
          name[8] = '\0';
          ext[3] = '\0';
          printf("%8s%3s\n",name,ext);
        }
        fseek(fp,21,SEEK_CUR);
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


