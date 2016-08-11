#include "fat.h"


/*read_BPB_value(byte fd, word *bpb_val, int offset, short cnt )
@param byte fd :: file description to read from
@param word *bpb_val :: pointer to store value
@param int offset :: location to seek in file 
@param short cnt :: number of bytes to read
purpose :: this function reads takes a pointer in which to read the given
    number of bytes from the file
*/
void read_BPB_value(byte fd, word *bpb_val, int offset, short cnt ){
    *bpb_val = 0;
    lseek(fd, offset, SEEK_SET);
    read(fd, bpb_val, cnt);
}

/*fat_byte_from_clusn(FATFileSystem *fat, word clusn){
@param FATFileSystem *fat :: pointer to fat data structure
@param word clusn :: cluster value 
purpose :: this function takes a clus num and determines the the byte location
    of the corresponding location in the fat table 
return :: byte location of fat that matches clusn
*/
word fat_byte_from_clusn(FATFileSystem *fat, word clusn){

    word fatoffset = clusn * 2;
    word fat_sec_num = fat->RSVD_SEC_CNT + (fatoffset / fat->BYT_PER_SEC);
    word this_fatoffset = fatoffset % fat->BYT_PER_SEC;

    return ((fat_sec_num * fat->BYT_PER_SEC) + this_fatoffset);
}

/*read_long_name(FATFileSystem *fat, const byte *sector, byte cnt){
@param FATFileSystem *fat :: pointer to fat data structure
@param const byte *sector :: array containing 32 bits of a directory entry
@param byte  cnt :: index of the first long name entry
purpose :: this function starts at the initial long name entry and stores 
    and continues reading and storing the values in char array contained by fat    
*/
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

/*read_short_name(const byte *src, char *buf)
@param const byte *src :: pointer to 32 bytes of directory entry
@param char *buf :: buf to store long name
purpose :: this function reads the values of a short name entry and 
    stores it in the buffer
*/
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
        if((*(src + i) == 0x20) && (*(src + 8) != 0x20)){ //found a space but there a period meaning extention is present
            strcat(name, ext);
            strcpy(buf, name);
            return;
        }
        if ((*(src + i) == 0x20) && (*(src + i) == 0x20)) { //found a space and no extention
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

/*int contains(char *namev[], const char *str, word cnt)
@param char *namev[] :: an array or string contianing the names of each 
    entry in a directory
@param cconst har *str :: string to compare
@param word cnt :: number of values in namev
purpose :: this function take an array of strings and a string and checks
    to see if the string is in the array
returns the index of string or -1 if it is not found
*/
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

/*istdir(FATFileSystem *fat, dword clnum, char *namev[], byte *ptrvec[]){
@param FATFileSystem *fat :: pointer to fat data structure
@param dword clnum :: pointer to store value
@param char *namev[] :: array of strings of each directory entry name
@param byte *bytev[] :: array of pointer to 32 bits of each directory entry
purpose :: this funtion reads the directory that starts at clnum and stores the names
    in namev and the 32 bytes in bytev
return :: this function returns the number of values read in the from the directory
*/
byte listdir(FATFileSystem *fat, dword clnum, char *namev[], byte bytev[][32]){
    bool longname_switch = 0; //switch for start of long entry
    byte sector[fat->BYT_PER_SEC];  //buffer to hold sector of bytes
    byte entrycnt = 0; //number of entries in dir
    word secnum = ((clnum - 2) * fat->SEC_PER_CLUS) + fat->fir_data_sec;  //first sector of clus to read
    secnum = (clnum)? secnum: fat->fir_root_sec; //if clus less than 2, go to root

    lseek(fat->fd, secnum * fat->BYT_PER_SEC, SEEK_SET);  //set file pointer to sector

    while (read(fat->fd, sector, fat->BYT_PER_SEC) > 0){ //for each sector of dir
        
        byte *entry = sector; //directory entry pointer

        while(entry < (sector + fat->BYT_PER_SEC)){ 
            if (!(*entry)){ 
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
                } 
             
                else {
                        read_short_name(entry, fat->namet);      
                }
                namev[entrycnt] = malloc(sizeof(char) * (strlen(fat->namet) + 1));
                //bytev[entrycnt] = malloc(sizeof(byte) * 32);
                strcpy(namev[entrycnt], fat->namet);
                int j = 0;
                
                while (32 > j){
                    *(bytev[entrycnt] + j) = *(entry + j);
                    j++;
                }
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

/*dr_ls(FATFileSystem *fat, const char *path){
@param FATFileSystem *fat :: pointer to fat data structure
@param const char *path:: the path to the directory to list
purpose :: this function prints the name of each entry in the directory
    specified by path
*/
bool dr_ls(FATFileSystem *fat, const char *path){

    int i = 0;
    char *namev[256];
    byte bytev[256][32];
    byte cnt = 0;
    
    if (fat->argc == 1){
        cnt = listdir(fat, fat->clusptr, namev, bytev);
        while (cnt > i){
            printf("%s\n", namev[i++]);
        }
        return 1;
    }
    char pathreset[256];
    strcpy(pathreset, fat->path);
    word clusreset = fat->clusptr;
    if (!dr_chdir(fat, path)){
        ;
    }
    else {
        cnt = listdir(fat, fat->clusptr, namev, bytev);
        while (cnt > i){
            printf("%s\n", namev[i++]);
        }
    }
    //strcpy(fat->path, pathreset);
    fat->clusptr = clusreset;
    while (cnt--){
        free(namev[cnt]);
    }
    strcpy(fat->path, pathreset);
    return 1;
}

/*dr_chdir(FATFileSystem *fat, const char *path){
@param FATFileSystem *fat :: pointer to fat data structure
@param const char *path:: the path to the directory to change to
purpose :: this function switched directories by updating the path 
    and current cluster pointer of fat
*/
bool dr_chdir(FATFileSystem *fat, const char *path){
   // off_t reset = fat->offset;
    if ((fat->argc < 2)){
        printf("format error: cd <dest>\n");
        return 0;
    }
    char *namev[256];
    byte ptrvec[256][32];
    char nm[256];
    strcpy(nm, path);
    char *token = strtok(nm, "/");

   
    //return 1;
    //byte cnt = listdir(fat, 2, namev, ptrvec);
    while(token != NULL){
        byte cnt = listdir(fat, fat->clusptr, namev, ptrvec);
        //int entry = 0;

        int index = contains(namev, token, cnt);
        if(index == -1){
            printf("Error: Path %s doesnt exits\n", token);
            return 0;

        }
   // else (-1 < (w = contains(namev, fat->argv[1], cnt))){
        else {
            if ( !((*(ptrvec[index] + 11)) & ATTR_DIRECTORY)){
                printf("Error: Path %s is not a directory\n", token);
                return 0;
            }
            word clus = *(ptrvec[index] + 27);
            clus <<= 8;
            clus += *(ptrvec[index] + 26);

            fat->clusptr = clus;
            if (strcmp(token, ".") == 0){
                //return 1;
                break;

            }
            if (strcmp(token, "..") == 0){
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
                strcat(fat->path, token);
            }
            //return 1;
        }
        token = strtok(NULL, "/");
        while (cnt--){
            free(namev[cnt]);
        }

    }
    return 1;

} 

/*fl_cpout(FATFileSystem *fat, const char *src, const char *dst){
@param FATFileSystem *fat :: pointer to fat data structure
@param const char *src:: name of file to read from
@param const char *dst:: name of new file
purpose :: this function searches for the file src, copies the data
    into an array, and creates a file dst unified file system used
    to launch the program
*/
bool fl_cpout(FATFileSystem *fat,  char *src, const char *dst){
    if (!(fat->argc == 3)){
        printf("format error: cpout <src> <dst>\n");
        return 0;
    }
    char *namev[256];
    byte bytev[256][32];
    char *filename = strrchr(src, '/');
    char pathreset[256];
    strcpy(pathreset, fat->path);
    word clusreset = fat->clusptr;
    if (filename ){ //if forward
        ++filename;
        *(filename - 1) = '\0';
        dr_chdir(fat, src);
    }

    byte cnt = listdir(fat, fat->clusptr, namev, bytev);
    int index = 0;
    index = (filename)? contains(namev, filename, cnt): contains(namev, src, cnt);
    if(index == -1){
        printf("Errrrrror: Path %s doesnt exits\n", filename);
        return 0;

    }
    else {

        word *clus = bytev[index] + 26;
        word fataddr = fat_byte_from_clusn(fat, *clus);

        lseek(fat->fd, fataddr, SEEK_SET);

        dword *sz = bytev[index] + 28;
        byte data[*sz + 1];
        dword bytperclus = fat->BYT_PER_SEC * fat->SEC_PER_CLUS;
        dword cluscnsmd = *sz % bytperclus ? *sz / bytperclus + 1: *sz/bytperclus;

        word fatentries[cluscnsmd];
        fatentries[0] = *clus; 

        int i = 0;
        while (++i < cluscnsmd){
            read(fat->fd, fatentries + i, 2);
            word addr = fat_byte_from_clusn(fat, fatentries[i]);
            lseek(fat->fd, addr, SEEK_SET);
        }

        i = 0;
        word remain = *sz;
        while (i < cluscnsmd){
           // printf("0x%02x, ", fatentries[i]);
            word secnum = ((fatentries[i] - 2) * fat->SEC_PER_CLUS) + fat->fir_data_sec; 
            //printf("sec = 0x%02x\n", secnum * fat->BYT_PER_SEC);
            lseek(fat->fd, secnum * fat->BYT_PER_SEC, SEEK_SET);  //set file pointer to sector
            if (i == cluscnsmd - 1){    
                read(fat->fd, data + (fat->BYT_PER_SEC * fat->SEC_PER_CLUS * i), remain);
            }
            else {
                read(fat->fd, data + (fat->BYT_PER_SEC * fat->SEC_PER_CLUS * i), fat->BYT_PER_SEC * fat->SEC_PER_CLUS);
            }
            remain -= (fat->BYT_PER_SEC * fat->SEC_PER_CLUS);
            ++i;
        }
        printf("\n");
        int fd = open(dst, O_WRONLY| O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH );
        if (fd < 0){printf("could not open\n");}
        //perror("Error!!!  ");
        write(fd, data, *sz);
        close(fd);

    }
    while (cnt--){
        free(namev[cnt]);
    }
    fat->clusptr = clusreset;
    strcpy(fat->path, pathreset);
    return 1;
} 


/*fat_mount(FATFileSystem *fat, const char *filename)
@param FATFileSystem *fat :: pointer to fat data structure
@param const char *filename:: name of FAT16 file system to load

purpose :: this function loads the file system and records initial 
    information in the FATFileSystem structure
*/
bool fat_mount(FATFileSystem *fat, const char *filename){
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

/*execute_cmd_line(FATFileSystem *fat)
@param FATFileSystem *fat :: pointer to fat data structure
purpose :: this function compares the user input with the available
    commands and executes them if possible
*/
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


/*parse_args(FATFileSystem *fat)
@param FATFileSystem *fat :: pointer to fat data structure
purpose :: this function reads user input updates fat's
    array of strings that holds the command and its arguments
return :: the number of arguments given
*/
byte parse_args(FATFileSystem *fat){
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
    return fat->argc;
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
