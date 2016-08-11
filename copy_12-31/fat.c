#include "fat.h"


bool read_BPB_value(byte fd, word *bpb_val, int offset, short cnt ){
    byte holder = 0;
    byte shift = 0;
    *bpb_val = 0;
    lseek(fd, offset, SEEK_SET);
    while(cnt > 0){
        read(fd, &holder, 1);
        *bpb_val += (holder << (8 * shift));
        shift++;
        cnt--;
    }
    return *bpb_val;
}

word fat_byte_from_clusn(FATFileSystem *fat, word clusn){

    word fatoffset = clusn * 2;
    word fat_sec_num = fat->RSVD_SEC_CNT + (fatoffset / fat->BYT_PER_SEC);
    word this_fatoffset = fatoffset % fat->BYT_PER_SEC;

    return ((fat_sec_num * fat->BYT_PER_SEC) + this_fatoffset);
}

void read_long_name(FATFileSystem *fat, const byte *sector, byte cnt){
    int i = 0;
    int inc = 0;
    while (i < 10){  
        inc = (13 * (cnt - 1)) + (i/2);
        fat->namet[inc] = *(sector + i + 1);
        i += 2;   
    }
    i = 13;
    while (i < 25){
        inc = (13 * (cnt - 1)) + ((i-3)/2);
        fat->namet[inc] = *(sector + i + 1);
        i += 2;
    }
    i = 27;
    while (i < 31){
        inc = (13 * (cnt - 1)) + ((i-4)/2);
        fat->namet[inc] = *(sector + i + 1);
        i += 2;
    }
    if (cnt  > 1){
        read_long_name(fat, sector + 32, cnt - 1);
    }
    else {
        strcat( fat->namet, "\0");
        //strcpy( fat->namet, namet);
        //printf("done reading :: %s --> ", fat->namet);
    }
    //return;
}

void read_short_name(const byte *src, char *buf){
    byte i;
    buf[0] = *src;
    char name[13];
    name[8] = '\0';
    char ext[5];
    ext[0] = '.';
    ext[1] = tolower(*(src  + 8));
    ext[2] = tolower(*(src + 9));
    ext[3] = tolower(*(src + 10));
    ext[4] = '\0';
    name[0] = *src;
    for(i = 1; i < 8; i++){
        if((*(src + i) == 0x20) && (*(src + 8) != 0x20)){
            strcat(name, ext);
            strcpy(buf, name);
            return;
        }
        if ((*(src + i) == 0x20) && (*(src + i) == 0x20)) {
            name[i] = '\0';
            strcpy(buf, name);
            return;
        }
        name[i] = tolower(*(src + i));
    }
    strcat(name, ext);
    strcpy(buf, name);
    return;
}

int contains(char *namev[], const char *str, word cnt){
    int i = 0;
    while(i < cnt){
        if(strcmp(namev[i], str) == 0){
            return i;
        }               
        i++;
    }
    return -1;
}

byte listdir(FATFileSystem *fat, dword clnum, char *namev[], byte *ptrvec[]){
    bool longname_switch = 0; //switch for start of long entry
    byte sector[fat->BYT_PER_SEC];  //buffer to hold sector of bytes
    byte entrycnt = 0; //number of entries in dir
    word secnum = ((clnum - 2) * fat->SEC_PER_CLUS) + fat->fir_data_sec;  //first sector of clus to read
    secnum = (clnum)? secnum: fat->fir_root_sec; //if clus less than 2, go to root

    lseek(fat->fd, secnum * fat->BYT_PER_SEC, SEEK_SET);  //set file pointer to sector

    while (read(fat->fd, sector, fat->BYT_PER_SEC) > 0){ //for each sector of dir
        
        byte *entry = sector; //directory entry pointer
        //printf("\n\n============================== NEW SECTOR ==============================\n\n");

        while(entry < (sector + fat->BYT_PER_SEC)){ 
            if (!(*entry)){ 
                //printf("\nThere were %i entries!!\n", entrycnt);
                return entrycnt;
            } //if first byte is 0x00

            if (*(entry + 11) == 0x0f){ //found long entry
                byte index = 0x0f & (*entry);
                longname_switch = 1;
                read_long_name(fat, entry, index);
                entry += (32 * index);
            } //end if: found long entry

            else { //found short entry
                
                if (*(entry + 11) == 0x08){  //volume id
                        entry +=32;
                        continue;
                }
                else if (longname_switch){ //short entry has long name
                    longname_switch = 0;
                } //end loop: short entry has long name
             
                else {
                        read_short_name(entry, fat->namet);      
                }
                namev[entrycnt] = malloc(sizeof(char) * (strlen(fat->namet) + 1));
                ptrvec[entrycnt] = malloc(sizeof(byte) * 32);
                strcpy(namev[entrycnt], fat->namet);
                int j = 0;
                

                //printf("\nbytes from vec ...\n");
                while (32 > j){
                    *(ptrvec[entrycnt] + j) = *(entry + j);
                    //printf("%02x\t", *(ptrvec[entrycnt] + j));
                    j++;
                }
                //printf("\ndone showing :: %s\n\n", fat->namet);

                //printf("%02x\n", entry);
                entry +=32;
                entrycnt++;
                word cnt = strlen(fat->namet);
                while (cnt--){
                    fat->namet[cnt] = '\0';
                }
            } 

        } 

    } 
    return entrycnt;
}

bool dr_ls(FATFileSystem *fat, const char *path){
    int i = 0;
    char *namev[256];
    byte *ptrvec[256];
    int w = -1;
    byte cnt = listdir(fat, fat->clusptr, namev, ptrvec);
    if (!fat->argv[1]){
        while (cnt > i){
            printf("%s\n", namev[i++]);
        }
        return 1;
    }
    else if (-1 < (w = contains(namev, fat->argv[1], cnt))){
        printf("foudn you @ %i!\n",  w);
        //printf("We found it :: %s, %i!!\n", fat->argv[i],  strlen(fat->argv[i]));
    }
    else{
        printf("did not find strng!!!\n");
    }

    return 1;
}

bool dr_chdir(FATFileSystem *fat, const char *path){
   // off_t reset = fat->offset;
    if (!(fat->argc == 2)){
        printf("format error: cd <dest>\n");
        return 0;
    }
    char *namev[256];
    byte *ptrvec[256];
    //byte cnt = listdir(fat, 2, namev, ptrvec);
    byte cnt = listdir(fat, fat->clusptr, namev, ptrvec);
    int w = 0;

    w = contains(namev, fat->argv[1], cnt);
    if(w == -1){
        printf("Error: Path %s doesnt exits\n", fat->argv[1]);
        return 0;

    }
   // else (-1 < (w = contains(namev, fat->argv[1], cnt))){
    else {
        if ( !((*(ptrvec[w] + 11)) & ATTR_DIRECTORY)){
            printf("Error: Path %s is not a directory\n", fat->argv[1]);
            return 0;
        }
        word clus = *(ptrvec[w] + 27);
        clus <<= 8;
        clus += *(ptrvec[w] + 26);

        fat->clusptr = clus;
        if (strcmp(fat->argv[1], ".") == 0){
            return 1;

        }
        if (strcmp(fat->argv[1], "..") == 0){
            int i = strlen(fat->path);
            while(--i){

                if (fat->path[i] == '/'){

                    fat->path[i] = '\0';
                    break;
                }
                fat->path[i] = '\0';
            }
        }
        else{
            if (strlen(fat->path) > 1) strcat(fat->path, "/");
            strcat(fat->path, fat->argv[1]);
        }
        return 1;
    }

} 

bool fl_cpout(FATFileSystem *fat, const char *src, const char *dst){
    if (!(fat->argc == 3)){
        printf("format error: cpout <src> <dst>\n");
        return 0;
    }
    off_t reset = lseek(fat->fd, 0, SEEK_CUR);
    char *namev[256];
    byte *ptrvec[256];
    //byte cnt = listdir(fat, 2, namev, ptrvec);
    byte cnt = listdir(fat, fat->clusptr, namev, ptrvec);
    int w = 0;
    w = contains(namev, fat->argv[1], cnt);
    if(w == -1){
        printf("Error: Path %s doesnt exits\n", src);
        //return 0;

    }
    else {


        word *clus = ptrvec[w] + 26;
        word fataddr = fat_byte_from_clusn(fat, *clus);

        lseek(fat->fd, fataddr, SEEK_SET);

        dword *sz = ptrvec[w] + 28;
        byte data[*sz + 1];
        dword bytperclus = fat->BYT_PER_SEC * fat->SEC_PER_CLUS;
        dword cluscnsmd = *sz % bytperclus ? *sz / bytperclus + 1: *sz/bytperclus;

        word fatentries[cluscnsmd];
        fatentries[0] = *clus; 

        int i = 0;
        while (++i < cluscnsmd){
            //fatentries[]
            read(fat->fd, fatentries + i, 2);
            word addr = fat_byte_from_clusn(fat, fatentries[i]);
           // printf("0x%02x::0x%02x, ", fatentries[i], addr);
            lseek(fat->fd, addr, SEEK_SET);
            //read(fat->fd, data, fat->BYT_PER_SEC * fat->SEC_PER_CLUS);


        }



        printf("found the source %i --> fat: 0x%04x, 0x%02x\n", *sz, fataddr, fatentries[0]);   

        i = 0;
        word remain = *sz;
        while (i < cluscnsmd){
            printf("0x%02x, ", fatentries[i]);
            word secnum = ((fatentries[i] - 2) * fat->SEC_PER_CLUS) + fat->fir_data_sec; 
            printf("sec = 0x%02x\n", secnum * fat->BYT_PER_SEC);
            lseek(fat->fd, secnum * fat->BYT_PER_SEC, SEEK_SET);  //set file pointer to sector
            if (i == cluscnsmd - 1){    
                read(fat->fd, data + (fat->BYT_PER_SEC * fat->SEC_PER_CLUS * i), remain);
            }
            else {
                read(fat->fd, data + (fat->BYT_PER_SEC * fat->SEC_PER_CLUS * i), fat->BYT_PER_SEC * fat->SEC_PER_CLUS);
            }
            remain -= (fat->BYT_PER_SEC * fat->SEC_PER_CLUS);
            ++i;
            //break;
        }
        printf("\n");
        int fd = open(src, O_WRONLY| O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH );
        if (fd < 0){printf("could not open\n");}
        //perror("Error!!!  ");
        printf("madie it \n");
        write(fd, data, *sz);
        close(fd);

    }
    lseek(fat->fd, reset, SEEK_SET);
    return 0;
} /**/

bool fl_cpin(FATFileSystem *fat, const char *src, const char *dst){
    return 0;
} /**/
bool fat_mount(FATFileSystem *fat, char *filename){
    fat->fd = open(filename, O_RDWR);
    if(!fat->fd){
        perror("Error: ");
        exit(EXIT_FAILURE);
    }
    fat->argc = 0;
    fat->argv[0] = NULL;
/** Record Boot Sector values**/
    read_BPB_value(fat->fd, &fat->BYT_PER_SEC, 11, 2);
    read_BPB_value(fat->fd, (word *)&fat->SEC_PER_CLUS, 13, 1);
    read_BPB_value(fat->fd, &fat->RSVD_SEC_CNT, 14, 2);
    read_BPB_value(fat->fd, (word *)&fat->NUM_FAT, 16, 1);
    read_BPB_value(fat->fd, (word *)&fat->ROOT_ENT_CNT, 17, 2);
    read_BPB_value(fat->fd, &fat->FATsz, 22, 2);

/** Set pointer to address in loaded file system **/
    word root_ds = ((fat->ROOT_ENT_CNT * 32) + (fat->BYT_PER_SEC - 1)) / fat->BYT_PER_SEC;
    fat->fir_root_sec = fat->RSVD_SEC_CNT + (fat->NUM_FAT * fat->FATsz);
    fat->fir_data_sec = fat->fir_root_sec + root_ds; 
    fat->offset = lseek(fat->fd, (fat->fir_data_sec - root_ds) * fat->BYT_PER_SEC, SEEK_SET);
    //printf("root ds: 0x%02x\n", fat->cur_byte_ptr);
    
    fat->long_name = NULL;
    fat->cmd_line_input = NULL;
 
 /** NOTE FIX CLUS AVAIL**/ 
    fat->clusptr = 0;  
    fat->clus_avail_cnt = 0;
    fat->clus_last_alloc = 0;

    //fat->path = "/";
    strcat(fat->path, "/");
    return 1;
    //return test(fat);

}

bool execute_cmd_line(FATFileSystem *fat){
    if (!fat->argc) return 0;
    byte i = 0;
    byte ret = 0;
    
    if(strcmp(fat->argv[0], "ls") == 0){
        ret = dr_ls(fat, fat->argv[1]);

    }
    else if(strcmp(fat->argv[0], "cd") == 0){
        //printf("cd\n");
        ret = dr_chdir(fat, fat->argv[1]);
    }
    else if((strcmp(fat->argv[0], "cpout") == 0)){
        ret = fl_cpout(fat, fat->argv[1], fat->argv[2]);

    }
    else if((strcmp(fat->argv[0], "cpin") == 0)){

    }
    else if((strcmp(fat->argv[0], "exit") == 0)){
        printf("Goodbye!\n");
        exit(EXIT_SUCCESS);
    }
    else{
        printf("command %s not found\n", fat->argv[0]);
        ret = 0;
    }
    while(i < fat->argc) free(fat->argv[i++]);
    fat->argc = 0;
    return ret;
}

bool parse_args(FATFileSystem *fat){
    char line[256];
    line[254] = '\n';
    line[255] = '\0';
    int i = 0;
    if(!fgets(line, 256, stdin)) return 0;
    if(line[254] != '\n') return 0;
    char *str = strtok(line, DELIMS);
    if(!str) return 0;
    fat->argv[0] = (char *)malloc(sizeof(char) * strlen(str + 1));
    strcpy(fat->argv[0], str);
    while (fat->argv[i++]){
        fat->argc = i;
        if ((str = strtok(NULL, DELIMS))) {
            fat->argv[i] = (char *)malloc(sizeof(char) * (strlen(str) + 1) );
            strcpy(fat->argv[i], str);
        }
        else{break;}
    }
    fat->argv[fat->argc] = NULL;
    return 1;
}

int main(int argc, char *argv[]){
    if (argc != 2){
	   perror("Error:  " );
	   exit(EXIT_FAILURE);
    }
    FATFileSystem fat;
    fat_mount(&fat, argv[1]);
    while(1){
        printf(":%s> ", fat.path);
        parse_args(&fat);
        execute_cmd_line(&fat);
    }
    close(fat.fd);
    return 0;
}
