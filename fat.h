#ifndef _FAT_H_
#define _FAT_H_
#define DELIMS " \t\r\n"

/*directory entry attributes*/
#define ATTR_READ_ONLY 	1
#define ATTR_HIDDEN 	2
#define ATTR_SYSTEM 	4
#define ATTR_VOLUME_ID 	8
#define ATTR_DIRECTORY 	16 
#define ATTR_ARCHIVE 	32

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

/*type definitions*/
typedef unsigned char  byte;
typedef signed char bool;
typedef unsigned short word;
typedef unsigned int dword;

typedef struct {    
    //struct Dir *wrking_dir;
    off_t offset;
    short argc;
    byte fd;
    
    char *long_name;
    char *cmd_line_input;
    char path[512];

    char *argv[16];
    char namet[100];
    char **namelist;
    //bool has_longname;
    
    byte SEC_PER_CLUS; 
    byte NUM_FAT;
    word FATsz;
    word BYT_PER_SEC;
    word RSVD_SEC_CNT;
    word ROOT_ENT_CNT;
    
    word clus_avail_cnt;
    word clus_last_alloc;
    word clusptr;

    dword fir_root_sec;
    dword fir_data_sec;
} FATFileSystem;



bool fat_mount(FATFileSystem *fat, const char *filename);
void read_BPB_value(byte fd, word *bpb_val, int offset, short cnt );

byte parse_args(FATFileSystem *fat); /*returns number of command arguments*/
dword fatscan(FATFileSystem *fat, char *longname_holder, const char *target);
void find_longname(FATFileSystem *fat, byte *bytes, char *buf, byte cnt);

bool execute_cmd_line(FATFileSystem *fat);
bool fl_cpout(FATFileSystem *fat,  char *src, const char *dst); /**/
bool fl_cpin(FATFileSystem *fat, const char *src, const char *dst); /**/
bool dr_ls(FATFileSystem *fat, const char *path); /**/
bool dr_chdir(FATFileSystem *fat, const char *path); /**/


#endif
