#define FUSE_USE_VERSION 26

#include <time.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <fuse.h>
#include <limits.h>
#include <sys/types.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>

typedef struct fileNode
{
    struct  fileNode    *parentDir;
    struct  fileNode    *childDir;
    struct  fileNode    *next;
    char    name[255];
    struct  stat        *statCont;
    char    *data;
    int     type; //0 - default/1 - dir/2 - file
} fsNode;

long memory;
fsNode *fsroot;

static int check_path(const char *path) {
    if(strlen(path)  > PATH_MAX)
        return 0;
    return 1;
}

void inc_memory_by_node() {
    memory = memory + sizeof(fsNode);
}
void inc_memory_by_stat() {
    memory = memory + sizeof(struct stat);
}
void dec_memory_by_node() {
    memory = memory - sizeof(fsNode);
}
void dec_memory_by_stat() {
    memory = memory - sizeof(struct stat);
}       

char currentNode[255];

int check_for_path(const char *path) {

    int check = 0;
    fsNode *tempPtr  = fsroot;
    fsNode *tempPtr2 = NULL;
    char tempPath[PATH_MAX];
    char *tempPath2;

    strcpy(tempPath, path);
    tempPath2 = strtok(tempPath, "/");

    if((tempPath2==NULL)&&(strcmp(path, "/")==0))
        return 0;

    while(tempPath2 != NULL) {
        for(tempPtr2 = tempPtr->childDir; tempPtr2 != NULL; tempPtr2 = tempPtr2->next) {
            if(strcmp(tempPtr2->name, tempPath2) == 0 ) {
                check = 1;
                break;
            }
        }
        tempPath2 = strtok(NULL, "/");
        if(check!=1){
            if(tempPath2==NULL)
                return 1;
            else
                return -1;
        } else {
            if((check==1)&&(tempPath2==NULL))
                return 0;
        }
        tempPtr = tempPtr2;
        check = 0;
    }
    return -1;
}

fsNode *find_file_node(const char *path) {

    int check;
    fsNode *tempPtr = fsroot;
    fsNode *tempPtr2 = NULL;
    char *tempPath2;
    char tempPath[PATH_MAX];

    strcpy(tempPath, path);
    tempPath2 = strtok(tempPath, "/");

    if((tempPath2==NULL)&&(strcmp(path, "/")==0))
        return fsroot;

    while(tempPath2!=NULL) {
        check = 0;
        for(tempPtr2=tempPtr->childDir; tempPtr2!=NULL; tempPtr2=tempPtr2->next) {
            if(strcmp(tempPtr2->name, tempPath2)==0){
                check = 1;
                break;
            }
        }
        if(check!=1) {
            strcpy(currentNode,tempPath2);
            return tempPtr;
        }else {
            tempPath2 = strtok(NULL, "/");
            if(tempPath2==NULL)
                return tempPtr2;
        }
        tempPtr=tempPtr2;
    }
    return NULL;
}

static int ramd_unlink(const char *path) {
    
    if (!check_path(path)) return -ENAMETOOLONG;
    
    if(check_for_path(path)==0) {
        fsNode *tempPtr = find_file_node(path);
        fsNode *tempPtr2 = tempPtr->parentDir;

        if(tempPtr2->childDir==tempPtr && tempPtr->next==NULL) {
            tempPtr2->childDir = NULL;
        } else if (tempPtr2->childDir==tempPtr) {
            tempPtr2->childDir = tempPtr->next;
        } else {
            fsNode *temporary;
            temporary = tempPtr2->childDir;
            while(temporary != NULL) {
                if(temporary->next == tempPtr) {
                    temporary->next = tempPtr->next;
                    break;
                } else {
                    temporary = temporary->next;
                }
            }    
        }

        if(tempPtr->statCont->st_size!=0) {
            memory = memory + tempPtr->statCont->st_size;
            free(tempPtr->data);
            free(tempPtr->statCont);
            free(tempPtr);
        } else {
            free(tempPtr->statCont);
            free(tempPtr);
        }
        inc_memory_by_node();
        inc_memory_by_stat();
        return 0;
    }
    return -ENOENT;
}

static int ramd_mkdir(const char *path, mode_t mode) {

    if (!check_path(path)) return -ENAMETOOLONG;

    fsNode *tempPtr2 = (fsNode *)malloc(sizeof(fsNode));
    if(tempPtr2==NULL)
        return -ENOSPC;
    dec_memory_by_node();
    tempPtr2->statCont=(struct stat *)malloc(sizeof(struct stat));
    dec_memory_by_stat();


    fsNode *tempPtr = find_file_node(path);

    if(memory < 0) {
        return -ENOSPC;
    }

    time_t current;
    time(&current);
    strcpy(tempPtr2->name, currentNode);

    tempPtr2->statCont->st_nlink = 2;
    tempPtr2->statCont->st_mode = S_IFDIR | 0755;
    tempPtr2->statCont->st_uid = getuid();
    tempPtr2->statCont->st_gid = getgid();
    tempPtr2->statCont->st_atime = current;
    tempPtr2->statCont->st_mtime = current;
    tempPtr2->statCont->st_ctime = current;
    tempPtr2->parentDir = tempPtr;
    tempPtr2->childDir = NULL;
    tempPtr2->next = NULL;
    tempPtr2->statCont->st_size = 4096;
    tempPtr2->type = 1;

    if(tempPtr->childDir==NULL) {
        tempPtr->childDir = tempPtr2;
        tempPtr->statCont->st_nlink += 1;
    } else {
        fsNode *temporary = tempPtr->childDir;
        while(temporary->next != NULL) {
            temporary = temporary->next;
        }
        temporary->next = tempPtr2;
        tempPtr->statCont->st_nlink += 1;
    }
    return 0;
}

static int ramd_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {
    
    if (!check_path(path)) return -ENAMETOOLONG;

    if(check_for_path(path) != -1) {
        fsNode *tempPtr=find_file_node(path);
        filler(buf, ".", NULL, 0);
        filler(buf, "..", NULL, 0);
        
        fsNode *temporary;
        for(temporary=tempPtr->childDir;temporary!=NULL; temporary=temporary->next)
            filler(buf, temporary->name, NULL, 0);

        time_t current;
        time(&current);
        tempPtr->statCont->st_atime = current;
        return 0;
    }
    return -ENOENT;
}

static int ramd_rmdir(const char *path) {
    
    if (!check_path(path)) return -ENAMETOOLONG;

    if(check_for_path(path)==0) {
        fsNode *tempPtr = find_file_node(path);
        if(tempPtr->childDir!=NULL)
            return -ENOTEMPTY;
        
        fsNode *temporary = tempPtr->parentDir;
        if(temporary->childDir==tempPtr && tempPtr->next==NULL)
            temporary->childDir = NULL;
        else if(temporary->childDir==tempPtr)
            temporary->childDir = tempPtr->next;
        else {
            fsNode *temporary2;
            for(temporary2=temporary->childDir; temporary2!=NULL; temporary2=temporary2->next) {
                if(temporary2->next==tempPtr) {
                    temporary2->next = tempPtr->next;
                    break;
                }
            }
        }
        temporary->statCont->st_nlink--;

        free(tempPtr->statCont);
        inc_memory_by_stat();
        free(tempPtr);
        inc_memory_by_node();
        return 0;
    }

    return -ENOENT;
}

static int ramd_create(const char *path, mode_t mode, struct fuse_file_info *fi) {

    if (!check_path(path)) return -ENAMETOOLONG;

    if(memory < 0) {
        return -ENOMEM;
    }

    fsNode *tempPtr=(fsNode *)malloc(sizeof(fsNode));
    fsNode *tempPtr2 = find_file_node(path);
    tempPtr->statCont = (struct stat *)malloc(sizeof(struct stat));
    if(tempPtr == NULL)
        return -ENOMEM;

    dec_memory_by_node();
    dec_memory_by_stat();

    strcpy(tempPtr->name, currentNode);
    tempPtr->statCont->st_mode = S_IFREG | mode;
    tempPtr->statCont->st_nlink = 1;
    tempPtr->statCont->st_uid = getuid();
    tempPtr->statCont->st_gid = getgid();
    
    time_t current;
    time(&current);
    tempPtr->statCont->st_atime = current;
    tempPtr->statCont->st_mtime = current;
    tempPtr->statCont->st_ctime = current;
    
    tempPtr->statCont->st_size = 0;    
    tempPtr->parentDir = tempPtr2;
    tempPtr->childDir = NULL;
    tempPtr->next = NULL;
    tempPtr->data = NULL;
    tempPtr->type = 2;

    if(tempPtr2->childDir == NULL) {
        tempPtr2->childDir = tempPtr;
    } else {
        fsNode *temporary = tempPtr2->childDir;
        while(temporary->next != NULL) {
            temporary = temporary->next;
        }
        temporary->next = tempPtr;
    }
    return 0;
}

static int ramd_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi){

    if (!check_path(path)) return -ENAMETOOLONG;

    if(memory<size){
        return -ENOSPC;
    }
    fsNode *tempPtr = find_file_node(path);
    size_t curr_size = tempPtr->statCont->st_size;
    
    if(tempPtr->type == 1) {
        return -EISDIR;
    }
    if(size==0) {
        return size;
    } else if(size>0&&curr_size!=0) {
        if(offset>curr_size) {
            offset=curr_size;
        }
        char *new_file_data = (char *)realloc(tempPtr->data, sizeof(char) * (offset+size));
        if(new_file_data == NULL) {
            return -ENOSPC;
        } else {
            tempPtr->data = new_file_data;
            memcpy(tempPtr->data + offset, buf, size);
            tempPtr->statCont->st_size = offset + size;
            time_t current;
            time(&current);
            tempPtr->statCont->st_ctime = current;
            tempPtr->statCont->st_mtime = current;
            memory = memory+curr_size-(offset + size);
            return size;
        }
    } else if(size>0&&curr_size==0) {
        tempPtr->data = (char *)malloc(sizeof(char) * size);
        offset = 0;
        memory = memory - size;
        memcpy(tempPtr->data + offset, buf, size);
        tempPtr->statCont->st_size = offset + size;
        time_t current;
        time(&current);
        tempPtr->statCont->st_ctime = current;
        tempPtr->statCont->st_mtime = current;
        return size;
    }
    return size;
}

static int ramd_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    
    if (!check_path(path)) return -ENAMETOOLONG;
    
    fsNode *tempPtr = find_file_node(path);
    size_t curr_size = tempPtr->statCont->st_size;
    
    if(tempPtr->type==1)
        return -EISDIR;
    if(offset<curr_size){
        if(offset+size>curr_size) {
            size=curr_size-offset;
        }
        memcpy(buf, tempPtr->data + offset, size);
    } else 
        size = 0;

    if(size > 0) {
        time_t current_time;
        time(&current_time);
        tempPtr->statCont->st_atime = current_time;
    }
    return size;
}

static int ramd_open(const char *path, struct fuse_file_info *fi) {
    
    if (!check_path(path)) return -ENAMETOOLONG;

    if(check_for_path(path)==0)
        return 0;
    else
        return -ENOENT;
}

static int ramd_opendir(const char *path, struct fuse_file_info *fi) {
    
    if (!check_path(path)) return -ENAMETOOLONG;
    return 0;
}

static int ramd_getattr(const char *path, struct stat *stbuf) {

    if (!check_path(path)) return -ENAMETOOLONG;

    if(check_for_path(path)==0) {
        fsNode *tempPtr = find_file_node(path);
        stbuf->st_nlink = tempPtr->statCont->st_nlink; 
        stbuf->st_mode  = tempPtr->statCont->st_mode;
        stbuf->st_uid   = tempPtr->statCont->st_uid;   
        stbuf->st_gid   = tempPtr->statCont->st_gid;
        stbuf->st_atime = tempPtr->statCont->st_atime;
        stbuf->st_mtime = tempPtr->statCont->st_mtime;
        stbuf->st_ctime = tempPtr->statCont->st_ctime; 
        stbuf->st_size = tempPtr->statCont->st_size;
        return 0;
    }
    return -ENOENT; 
}

static int ramd_rename(const char *path, const char *to) {
    
    fsNode *tempPtr, *tempPtr2;
    int size1, size2;

    if(strlen(path)>PATH_MAX || strlen(to)>PATH_MAX)
        return -ENAMETOOLONG;

    if((check_for_path(path)==0)&&(check_for_path(to)==0)) {
        tempPtr = find_file_node(path);
        tempPtr2 = find_file_node(to);

        if(tempPtr2->type==2) {
            tempPtr2->statCont->st_mode = tempPtr->statCont->st_mode;
            tempPtr2->statCont->st_nlink = tempPtr->statCont->st_nlink;
            tempPtr2->statCont->st_uid = tempPtr->statCont->st_uid;
            tempPtr2->statCont->st_gid = tempPtr->statCont->st_gid;
            tempPtr2->statCont->st_atime = tempPtr->statCont->st_atime;
            tempPtr2->statCont->st_mtime = tempPtr->statCont->st_mtime;
            tempPtr2->statCont->st_ctime = tempPtr->statCont->st_ctime;

            size1 = tempPtr->statCont->st_size;
            size2 = tempPtr2->statCont->st_size;

            memset(tempPtr2->data, 0, size2);
            if(size1!=0) {
                char *rename_data = (char *)realloc(tempPtr2->data, sizeof(char)*(size1));
                if(rename_data == NULL)
                    return -ENOSPC;
                else {
                    tempPtr2->data = rename_data;
                    strcpy(tempPtr2->data, tempPtr->data);
                    memory = memory - size1;
                }
            }
            tempPtr2->statCont->st_size = size1;
            memset(tempPtr2->data, 0, size2);

            ramd_unlink(path);
            return 0;
        }
    } else if((check_for_path(path)==0)&&(check_for_path(to)==1)) {
        tempPtr = find_file_node(path);
        tempPtr2 = find_file_node(to);

        char rep_file[255];
        strcpy(rep_file, currentNode);

        if(tempPtr2->type==1) {
            ramd_create(to, tempPtr->statCont->st_mode, NULL);
            
            fsNode *temp  = find_file_node(to);
            temp->statCont->st_mode = tempPtr->statCont->st_mode;
            temp->statCont->st_uid = tempPtr->statCont->st_uid;
            temp->statCont->st_gid = tempPtr->statCont->st_gid; 
            temp->statCont->st_size = tempPtr->statCont->st_size; 
            temp->statCont->st_nlink = tempPtr->statCont->st_nlink;
            temp->statCont->st_atime = tempPtr->statCont->st_atime;
            temp->statCont->st_mtime = tempPtr->statCont->st_mtime;
            temp->statCont->st_ctime = tempPtr->statCont->st_ctime;
            if(tempPtr->statCont->st_size > 0) {
                temp->data = (char *) malloc(sizeof(char)* tempPtr->statCont->st_size);
                if(temp->data == NULL) {
                    return -ENOSPC;
                } else {
                    strcpy(temp->data, tempPtr->data);
                    memory = memory - tempPtr->statCont->st_size;
                }
            }
            ramd_unlink(path);
            return 0;
        } else if(tempPtr2->type==2 ) {
            memset(tempPtr->name, 0, 255);
            strcpy(tempPtr->name, rep_file);
            return 0;
        }
        return -ENOENT;
    }
    return -ENOENT;
}

static struct fuse_operations ramd_oper = {
    .open     = ramd_open,
    .read     = ramd_read,
    .write    = ramd_write,
    .create   = ramd_create,
    .rename   = ramd_rename,
    .mkdir    = ramd_mkdir,
    .unlink   = ramd_unlink,
    .rmdir    = ramd_rmdir,
    .opendir  = ramd_opendir,
    .readdir  = ramd_readdir,
    .getattr  = ramd_getattr
};

int main(int argc, char **argv) {


    if( argc != 3 ) {
        printf("Usage: ramdisk /path/to/dir <MB>\n");
        return -EINVAL;
    }

    memory = (long)atoi(argv[2]);
    memory = memory * 1024 * 1024; 

    fsroot = (fsNode *)malloc(sizeof(fsNode));
    dec_memory_by_node();
    fsroot->statCont = (struct stat *)malloc(sizeof(struct stat));
    dec_memory_by_stat();

    strcpy(fsroot->name, "/");
    time_t current_time;
    time(&current_time);
    fsroot->statCont->st_atime = current_time;
    fsroot->statCont->st_mtime = current_time;
    fsroot->statCont->st_ctime = current_time;
    
    fsroot->statCont->st_mode = S_IFDIR | 0755;
    fsroot->statCont->st_nlink = 2;
    fsroot->statCont->st_uid = 0;
    fsroot->statCont->st_gid = 0;

    fsroot->parentDir   = NULL;
    fsroot->childDir     = NULL;
    fsroot->next    = NULL;
    fsroot->data    = NULL;
    fsroot->type    = 1;

    argc--;
    return fuse_main(argc, argv, &ramd_oper, NULL);
}