#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "file-system.h"

///////////////////////////////////////UTILITIES///////////////////////////////////////

//Finding free block in O(1)
int allocate_block(FATFileSystem* fs) {
    if (fs->FAT->free_blocks == 0) return -1;

    int* pblock = (int*)fs->FAT->free_list; 
    int block = *pblock;
    fs->FAT->free_blocks--;
    fs->FAT->free_list++;

    return block;
}

void free_block(FATFileSystem* fs, int block) {
    fs->FAT->free_blocks++;
    fs->FAT->free_list--;
    int* pblock = (int*)fs->FAT->free_list; 
    *pblock = block;
}

//Printing FAT
void printFAT(FATFileSystem* fs) {
    int numblock = FILESYSTEM_SIZE/BLOCK_SIZE;
    
    printf("[");
    for (int i = 0; i < numblock; i++) {
        printf("%d, ", fs->FAT->FAT[i]);
    }
    printf("%d]\n", fs->FAT->FAT[numblock-1]);
}

//eraseFile and eraseDir defragmentation
void defragment(FileSystem* fs, int current_block, int previous_block, int pos) {

    while (1) {
        for (int i = (pos/32); i < (BLOCK_SIZE/32)-1; i++) {
            DirEntry* current_entry = (DirEntry*)(fs->FATfs->data + (current_block * BLOCK_SIZE) + i*32);
            DirEntry* next_entry = (DirEntry*)(fs->FATfs->data + (current_block * BLOCK_SIZE) + (i+1)*32);
            memcpy(current_entry, next_entry, 32);
        }
        memset(fs->FATfs->data + (current_block * BLOCK_SIZE) + (BLOCK_SIZE - 32), '\0', 32);

        if  ((*(fs->FATfs->data + (current_block * BLOCK_SIZE)) == '\0') && (*(fs->FATfs->data + (current_block * BLOCK_SIZE) + 32) == '\0')) {
            fs->FATfs->FAT->FAT[current_block] = FREE;
            fs->FATfs->FAT->FAT[previous_block] = EOF_BLOCK;
            free_block(fs->FATfs, current_block);
            int* parent_last_block = (int*)(fs->FATfs->data + (fs->current_dir * BLOCK_SIZE) + LAST_BLOCK_OFFSET);
            *parent_last_block = previous_block;
            return;
        }

        previous_block = current_block;
        current_block = fs->FATfs->FAT->FAT[previous_block];

        if(current_block == EOF_BLOCK) {
            return;
        }
        
        memcpy(fs->FATfs->data + (previous_block * BLOCK_SIZE) + (BLOCK_SIZE - 32), fs->FATfs->data + (current_block * BLOCK_SIZE), 32);
        memset(fs->FATfs->data + (current_block * BLOCK_SIZE), '\0', 32);
        pos = 0;
    }

    
}

///////////////////////////////////////FILESYSTEM FUNCTIONS///////////////////////////////////////

//Load file system
FileSystem* loadFileSystem(char* name, int size_requested) {
    int blocks_required = (size_requested + BLOCK_SIZE -1) /BLOCK_SIZE;
    int total_size = sizeof(FATFileSystem) + sizeof(FATStruct) + blocks_required*8 + blocks_required*BLOCK_SIZE; 
    
    FileSystem* fs = (FileSystem*) malloc(sizeof(FileSystem));
    fs->current_dir = 0;

    int fd = open (name, O_RDWR | O_CREAT, 0666);
    
    if (fd == -1) {
        perror("Error opening filesystem");
        exit(EXIT_FAILURE);
    }

    int len = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);

    //Filesystem is not empty
    if (len > 0) {
        fs->FATfs = (FATFileSystem*) mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);   
        if (fs->FATfs == MAP_FAILED) {
            perror("Error mapping filesystem");
            exit(EXIT_FAILURE);
        }        

        fs->total_size = len;

        char* start_ptr = (char*)fs->FATfs + sizeof(FATFileSystem);
        fs->FATfs->FAT = (FATStruct*) start_ptr;
        int free_size = *((int*)(start_ptr + 16));
        fs->FATfs->FAT->free_list = (int*)(start_ptr + sizeof(FATStruct) + (blocks_required - free_size)*4);
        fs->FATfs->FAT->FAT = (int*)(start_ptr + sizeof(FATStruct) + blocks_required*4);
        fs->FATfs->data = start_ptr + sizeof(FATStruct) + blocks_required*8;
        
        //Not enough space left in filesystem        
        if (fs->FATfs->FAT->free_blocks < blocks_required) {
            printf ("Space left in this file system is not enough. Bytes left = %d\n", fs->FATfs->FAT->free_blocks * BLOCK_SIZE);    
        }
    }

    //Empty filesystem
    else {
        if (ftruncate(fd, total_size) == -1) {
            perror("Error truncating filesystem");
            exit(EXIT_FAILURE);
        }

        fs->FATfs = (FATFileSystem*) mmap(NULL, total_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        if (fs->FATfs == MAP_FAILED) {
            perror("Error mapping filesystem");
            exit(EXIT_FAILURE);
        }

        fs->total_size = total_size;

        char* start_ptr = (char*)fs->FATfs + sizeof(FATFileSystem);
        fs->FATfs->FAT = (FATStruct*) start_ptr;
        fs->FATfs->FAT->free_list = (int*)(start_ptr + sizeof(FATStruct));
        fs->FATfs->FAT->FAT = (int*)(start_ptr + sizeof(FATStruct) + blocks_required*4);
        fs->FATfs->FAT->free_blocks = blocks_required;
        fs->FATfs->data = start_ptr + sizeof(FATStruct) + blocks_required*8;

        for (int i=0; i<blocks_required; i++) {
            fs->FATfs->FAT->free_list[i] = i;
        }

        memset(fs->FATfs->FAT->FAT, FREE, blocks_required*4);

        //Setting root directory (by default in block 0 of FAT)
        allocate_block(fs->FATfs);
        fs->FATfs->FAT->FAT[0] = EOF_BLOCK;

        DirEntry* root = (DirEntry*)fs->FATfs->data;
        strncpy(root->name, "root", NAME_MAX_LEN);
        root->parent_start = 0;
        root->start_block = 0;
        root->last_block = 0;
        root->type = TYPE_DIRECTORY;
        root->size = 32;
    }

    close(fd);
    return fs;
}

//Unload file system
void unloadFileSystem(FileSystem* fs) { 
    if (munmap(fs->FATfs, fs->total_size) == -1) {
        perror("Error unmapping filesystem");
        exit(EXIT_FAILURE);
    }
    free(fs);
}

//Create a new file
void createFile(FileSystem* fs, char *filename) {
    int* parent_size = (int*)(fs->FATfs->data + (fs->current_dir * BLOCK_SIZE) + SIZE_OFFSET);
    int block = fs->current_dir;
    int* parent_last_block = (int*)(fs->FATfs->data + (fs->current_dir * BLOCK_SIZE) + LAST_BLOCK_OFFSET);

    //Checking if file already exist
    while (block != EOF_BLOCK) {
        for (int i = 0; i < BLOCK_SIZE; i += 32) {
            int type = *((int*)(fs->FATfs->data + (block * BLOCK_SIZE) + i + TYPE_OFFSET));

            if (!type) break;
            if (type == TYPE_DIRECTORY) continue;

            char name[NAME_MAX_LEN];
            strncpy(name, fs->FATfs->data + (block * BLOCK_SIZE) + i, NAME_MAX_LEN);

            if(strcmp(filename, name) == 0) {
                printf("File '%s' already exist in this directory!\n", filename);
                return;
            } 
        }
        block = fs->FATfs->FAT->FAT[block];
    }
    
    block = 0;
    int offset = (*parent_size) % BLOCK_SIZE;

    if (offset == 0) {
        block = allocate_block(fs->FATfs);
        
        if (block == -1) {
            printf("No more space left, file '%s' cannot be created!\n", filename);
            return;
        }

        fs->FATfs->FAT->FAT[*parent_last_block] = block;
        fs->FATfs->FAT->FAT[block] = EOF_BLOCK;
        *parent_last_block = block;
    }
    else {
        block = *parent_last_block;
    }

    DirEntry* new_file = (DirEntry*)(fs->FATfs->data + (block * BLOCK_SIZE) + offset);
    strncpy(new_file->name, filename, NAME_MAX_LEN);
    new_file->name[NAME_MAX_LEN-1] = '\0';
    new_file->parent_start = fs->current_dir;
    new_file->type = TYPE_FILE;
    new_file->size = 0;
    *parent_size += 32;

    //Size increase propagation
    char current_dirname[NAME_MAX_LEN];
    strncpy(current_dirname, fs->FATfs->data + (fs->current_dir * BLOCK_SIZE), NAME_MAX_LEN);

    if (strncmp(current_dirname, "root", NAME_MAX_LEN) == 0) return;

    int pblock = *((int*)(fs->FATfs->data + (fs->current_dir * BLOCK_SIZE) + PARENT_START_OFFSET));

    int ok = 0;
    while (pblock != EOF_BLOCK) {
        for (int i = 0; i < BLOCK_SIZE; i += 32) {
            int type = *((int*)(fs->FATfs->data + (pblock * BLOCK_SIZE) + i + TYPE_OFFSET));

            if (!type) break;
            if (type == TYPE_FILE) continue;

            char name[NAME_MAX_LEN];
            strncpy(name, fs->FATfs->data + (pblock * BLOCK_SIZE) + i, NAME_MAX_LEN);

            if(strcmp(current_dirname, name) == 0) {
                int* size = (int*)(fs->FATfs->data + (pblock * BLOCK_SIZE) + i + SIZE_OFFSET);
                *size += 32;
                ok = 1;
                break;
            } 
        }
        if (ok) break;

        pblock = fs->FATfs->FAT->FAT[pblock];            
    }

}

//Erase a file
void eraseFile(FileSystem* fs, char* filename) {
    int block = fs->current_dir;
    int previous_block = block;

    while (block != EOF_BLOCK) {
        for (int i = 0; i < BLOCK_SIZE; i += 32) {
            int type = *((int*)(fs->FATfs->data + (block * BLOCK_SIZE) + i + TYPE_OFFSET));

            if (!type) break;
            if (type == TYPE_DIRECTORY) continue;

            char name[NAME_MAX_LEN];
            strncpy(name, fs->FATfs->data + (block * BLOCK_SIZE) + i, NAME_MAX_LEN);

            if(strcmp(filename, name) == 0) {
                int file_size = *((int*)(fs->FATfs->data + (block * BLOCK_SIZE) + i + SIZE_OFFSET));
                
                if (file_size) {
                    //Cleaning FAT and freeing blocks
                    int start = *((int*)(fs->FATfs->data + (block * BLOCK_SIZE) + i + START_BLOCK_OFFSET));

                    while (start != EOF_BLOCK) {
                        memset(fs->FATfs->data + (start * BLOCK_SIZE), '\0', BLOCK_SIZE);
                        free_block(fs->FATfs, start);
                        int temp = fs->FATfs->FAT->FAT[start];
                        fs->FATfs->FAT->FAT[start] = FREE;
                        start = temp;
                    }
                }

                defragment(fs, block, previous_block, i);
                int* parent_size = (int*)(fs->FATfs->data + (fs->current_dir * BLOCK_SIZE) + SIZE_OFFSET);
                *parent_size -= 32;

                //Size decrease propagation 
                char current_dirname[NAME_MAX_LEN];
                strncpy(current_dirname, fs->FATfs->data + (fs->current_dir * BLOCK_SIZE), NAME_MAX_LEN);

                if (strncmp(current_dirname, "root", NAME_MAX_LEN) == 0) return;

                int pblock = *((int*)(fs->FATfs->data + (fs->current_dir * BLOCK_SIZE) + PARENT_START_OFFSET));

                while (pblock != EOF_BLOCK) {
                    for (int i = 0; i < BLOCK_SIZE; i += 32) {
                        int type = *((int*)(fs->FATfs->data + (pblock * BLOCK_SIZE) + i + TYPE_OFFSET));

                        if (!type) break;
                        if (type == TYPE_FILE) continue;

                        char name[NAME_MAX_LEN];
                        strncpy(name, fs->FATfs->data + (pblock * BLOCK_SIZE) + i, NAME_MAX_LEN);

                        if(strcmp(current_dirname, name) == 0) {
                            int* size = (int*)(fs->FATfs->data + (pblock * BLOCK_SIZE) + i + SIZE_OFFSET);
                            *size -= 32;
                            return;
                        } 
                    }
                    pblock = fs->FATfs->FAT->FAT[pblock];            
                }
            } 
        }
        previous_block = block;
        block = fs->FATfs->FAT->FAT[previous_block];
    }

    printf("No file named %s in this directory\n", filename);
}

//Open a file (returning FileHandle)
FileHandle* openFile(FileSystem* fs, char* filename) {
    int block = fs->current_dir;
    
    while (block != EOF_BLOCK) {
        for (int i = 0; i < BLOCK_SIZE; i += 32) {
            int type = *((int*)(fs->FATfs->data + (block * BLOCK_SIZE) + i + TYPE_OFFSET));

            if (!type) break;            
            if (type == TYPE_DIRECTORY) continue;

            char name[NAME_MAX_LEN];
            strncpy(name, fs->FATfs->data + (block * BLOCK_SIZE) + i, NAME_MAX_LEN);

            if(strcmp(filename, name) == 0) {
                FileHandle* fh = (FileHandle*) malloc (sizeof(FileHandle));
                strncpy(fh->name, filename, NAME_MAX_LEN);
                fh->parent_block = block;
                fh->parent_offset = i;

                int* file_size = (int*)(fs->FATfs->data + (block * BLOCK_SIZE) + i + SIZE_OFFSET);
                if (*file_size) {
                    int start = *((int*)(fs->FATfs->data + (block * BLOCK_SIZE) + i + START_BLOCK_OFFSET));
                    fh->first_block = start;
                    fh->current_block = start;
                    fh->last_block = start;
                    fh->pos = 0; 
                }

                return fh;
            } 
        }
        block = fs->FATfs->FAT->FAT[block];
    }
    printf("No file named %s in this directory\n", filename);
    return NULL;
}

//Close a file
void closeFile(FileSystem* fs, FileHandle* fh) {
    free(fh);
}

//Rewriting a file from the beginning
void writeFile(FileSystem* fs, FileHandle *fh, char *buf, int size) {
    if (fh == NULL) {
        printf("Invalid file\n");
        return;
    }

    //Cleaning the previous contents of the file
    int* file_size = (int*)(fs->FATfs->data + (fh->parent_block * BLOCK_SIZE) + fh->parent_offset + SIZE_OFFSET);

    if (*file_size) {
        int i = fh->first_block;
        while (i != EOF_BLOCK) {
            memset(fs->FATfs->data + (i * BLOCK_SIZE), '\0', BLOCK_SIZE);
            free_block(fs->FATfs,i);
            int temp = fs->FATfs->FAT->FAT[i];
            fs->FATfs->FAT->FAT[i] = FREE;
            *file_size -= BLOCK_SIZE;
            i = temp;
        }
    }

    //Writing from the beginning
    if(size == 0) return;
 
    fh->pos = 0;
    int written_bytes = 0;
    int b = allocate_block(fs->FATfs);

    if (b == -1) {
        printf("No more space left, file '%s' cannot be written!\n", fh->name);
        return;
    }

    int* file_start_block = (int*)(fs->FATfs->data + (fh->parent_block * BLOCK_SIZE) + fh->parent_offset + START_BLOCK_OFFSET);
    int* file_last_block = (int*)(fs->FATfs->data + (fh->parent_block * BLOCK_SIZE) + fh->parent_offset + LAST_BLOCK_OFFSET);

    *file_start_block = b;
    *file_last_block = b;
    *file_size += BLOCK_SIZE;
    fh->first_block = b;
    fh->current_block = b;
    fh->last_block = b;
    fs->FATfs->FAT->FAT[b] = EOF_BLOCK;

    while (size > 0) {
        int left_bytes = (size > BLOCK_SIZE) ? BLOCK_SIZE : size;
        memcpy (fs->FATfs->data + (fh->last_block * BLOCK_SIZE), buf + written_bytes, left_bytes);
        written_bytes += left_bytes;
        size -= left_bytes;
        fh->pos += left_bytes;

        if (size != 0) {
            b = allocate_block(fs->FATfs);

            if (b == -1) {
                printf("Buf was too big, no more space left. Only first %d bytes of buf have been written!\n", fh->pos);
                return;
            }

            *file_size += BLOCK_SIZE;
            fs->FATfs->FAT->FAT[fh->last_block] = b;
            fs->FATfs->FAT->FAT[b] = EOF_BLOCK;
            fh->last_block = b;
            *file_last_block = b;
            fh->current_block = b;
        }
    }
}

//Writing in a file from current position
void appendFile(FileSystem* fs, FileHandle *fh, char *buf, int size) {

    if (fh == NULL) {
        printf("Invalid file\n");
        return;
    }

    int initial_position;
    int offset;

    //Cleaning the previous contents of the file after current position
    int* file_size = (int*)(fs->FATfs->data + (fh->parent_block * BLOCK_SIZE) + fh->parent_offset + SIZE_OFFSET);

    if (*file_size) {
        initial_position = fh->pos;
        offset = fh->pos % BLOCK_SIZE;

        int i = fh->current_block;
        while (i != EOF_BLOCK) {
            int temp = fs->FATfs->FAT->FAT[i];

            if (i == fh->current_block) { 
                memset(fs->FATfs->data + (i * BLOCK_SIZE) + offset, '\0', BLOCK_SIZE - offset);
                fs->FATfs->FAT->FAT[i] = EOF_BLOCK;
            }
            else {
                memset(fs->FATfs->data + (i * BLOCK_SIZE), '\0', BLOCK_SIZE);
                free_block(fs->FATfs, i);
                fs->FATfs->FAT->FAT[i] = FREE;
                *file_size -= BLOCK_SIZE;
            }
            i = temp;
        }
    }
    else {
        writeFile(fs, fh, buf, size);
        return;
    }

    //Writing from current position
    if (size < BLOCK_SIZE - offset) {
        memcpy (fs->FATfs->data + (fh->current_block * BLOCK_SIZE) + offset-1, buf, size);
        fh->pos += size;
        return;
    }

    memcpy (fs->FATfs->data + (fh->current_block * BLOCK_SIZE) + offset-1, buf, BLOCK_SIZE - offset+1);
    int written_bytes = (BLOCK_SIZE - offset + 1);
    fh->pos += written_bytes;
    size -= written_bytes;
    int* file_last_block = (int*)(fs->FATfs->data + (fh->parent_block * BLOCK_SIZE) + fh->parent_offset + LAST_BLOCK_OFFSET);

    while (size > 0) {
        int b = allocate_block(fs->FATfs);
        
        if (b == -1) {
            printf ("Buf was too big, no more space left. Only first %d bytes of buf have been written!\n",(fh->pos-initial_position));
            return;
        }

        *file_size += BLOCK_SIZE;
        fs->FATfs->FAT->FAT[fh->last_block] = b;
        fs->FATfs->FAT->FAT[b] = EOF_BLOCK;
        fh->last_block = b;
        *file_last_block = b;
        fh->current_block = b;

        int left_bytes = (size > BLOCK_SIZE) ? BLOCK_SIZE : size;
        memcpy (fs->FATfs->data + (fh->last_block * BLOCK_SIZE), buf + written_bytes, left_bytes);
        written_bytes += left_bytes;
        size -= left_bytes;
        fh->pos += left_bytes;
    }
}

//Read the content of a file
void readFile(FileSystem* fs, FileHandle *fh, char *buf, int size) {

    if (fh == NULL) {
        printf("Invalid file\n");
        return;
    }

    int* file_size = (int*)(fs->FATfs->data + (fh->parent_block * BLOCK_SIZE) + fh->parent_offset + SIZE_OFFSET);

    if (!*file_size) {
        printf("File %s is empty\n", fh->name);
        return;
    }
    
    int offset = fh->pos % BLOCK_SIZE;

    if (*(fs->FATfs->data + (fh->current_block * BLOCK_SIZE) + offset) == '\0') {
        printf("Nothing to read in %s\n", fh->name);
        return;
    }

    int read_bytes = 0;

    if (fh->current_block != fh->last_block) {

        if (size < BLOCK_SIZE - offset) {
            memcpy(buf, fs->FATfs->data + (fh->current_block * BLOCK_SIZE) + offset, size);
            fh->pos += size;
            return;
        }

        memcpy(buf, fs->FATfs->data + (fh->current_block * BLOCK_SIZE) + offset, BLOCK_SIZE - offset);
        read_bytes = BLOCK_SIZE - offset;
        offset = 0; 
        fh->pos += read_bytes;
        size -= read_bytes;

        while (size > 0) {      
            int block = fh->current_block;
            fh->current_block = fs->FATfs->FAT->FAT[block];

            if (fh->current_block == fh->last_block) break;

            int left_bytes = (size > BLOCK_SIZE) ? BLOCK_SIZE : size;
            memcpy (buf + read_bytes, fs->FATfs->data + (fh->current_block * BLOCK_SIZE), left_bytes);
            read_bytes += left_bytes;
            size -= left_bytes;
            fh->pos += left_bytes;
        }
        if (!size){ 
            buf[fh->pos] = '\0';
            return;
        }
    }

    //Reading in last_block
    char* c = fs->FATfs->data + (fh->last_block * BLOCK_SIZE) + offset;

    for (int i = 0; *c; i++) {
        memcpy(buf + read_bytes, fs->FATfs->data + (fh->last_block * BLOCK_SIZE) + offset + i, 1);
        read_bytes++;
        fh->pos++;
        size--;
        c++;
        if (!size) {
            memset(buf + read_bytes, '\0', 1);
            return;
        }
    }
    memset(buf + read_bytes, '\0', 1);
}

//Move current position within the file
void seekFile(FileSystem* fs, FileHandle *fh, int offset, int whence) {

    if (fh == NULL) {
        printf("Invalid file\n");
        return;
    }

    int* file_size = (int*)(fs->FATfs->data + (fh->parent_block * BLOCK_SIZE) + fh->parent_offset + SIZE_OFFSET);

    if (!*file_size) {
        printf("File %s is empty\n", fh->name);
        return;
    }

    //whence == 0 -> initial_position
    if (whence == 0) {   
        fh->pos = 0;
        fh->current_block = fh->first_block;

        if (offset <= 0) return;
        
        while (offset > 0) {
            int block = fh->current_block;
            if (block == EOF_BLOCK) return;
            
            if (offset >= BLOCK_SIZE) fh->current_block = fs->FATfs->FAT->FAT[block];
            int val = (offset >= BLOCK_SIZE) ? BLOCK_SIZE : offset;
            offset -= val;
            fh->pos += val;
        }
    }

    //whence == 1 -> current_position
    else if (whence == 1) {
        seekFile(fs, fh, fh->pos + offset, 0);
    }

    //whence == 2 -> end_position
    else if (whence == 2) {
        fh->pos = 0;
        fh-> current_block = fh->last_block;
        int block = fh->first_block;
        
        while (block != fh->last_block) {
            fh->pos += BLOCK_SIZE;
            block = fs->FATfs->FAT->FAT[block];
        }

        char* temp = fs->FATfs->data + (fh->last_block * BLOCK_SIZE);
        while (*temp) {
            fh->pos++;
            temp++;
        }

        if (offset >= 0) return;

        seekFile(fs, fh, fh->pos + offset, 0);
    }
}

//Create a new directory
void createDir(FileSystem* fs, char *dirname) {
    int* parent_size = (int*)(fs->FATfs->data + (fs->current_dir * BLOCK_SIZE) + SIZE_OFFSET);
    int block = fs->current_dir;
    int* parent_last_block = (int*)(fs->FATfs->data + (fs->current_dir * BLOCK_SIZE) + LAST_BLOCK_OFFSET);

    while (block != EOF_BLOCK) {
        for (int i = 0; i < BLOCK_SIZE; i += 32) {
            int type = *((int*)(fs->FATfs->data + (block * BLOCK_SIZE) + i + TYPE_OFFSET));

            if (!type) break;   
            if (type == TYPE_FILE) continue;

            char name[NAME_MAX_LEN];
            strncpy(name, fs->FATfs->data + (block * BLOCK_SIZE) + i, NAME_MAX_LEN);

            if(strcmp(dirname, name) == 0) {
                printf("Directory '%s' already exist in this directory!\n", dirname);
                return;
            } 
        }
        block = fs->FATfs->FAT->FAT[block];
    }
    
    block = 0;
    int block_for_newdir;
    int offset = (*parent_size) % BLOCK_SIZE;

    if (offset == 0) {
        block = allocate_block(fs->FATfs);
        
        if (block == -1) {
            printf("No more space left, file '%s' cannot be created!\n", dirname);
            return;
        }

        block_for_newdir = allocate_block(fs->FATfs);
        if (block_for_newdir == -1) {
            free_block(fs->FATfs, block);
            printf("No more space left, file '%s' cannot be created!\n", dirname);
            return;
        }

        fs->FATfs->FAT->FAT[*parent_last_block] = block;
        fs->FATfs->FAT->FAT[block] = EOF_BLOCK;
        *parent_last_block = block;

        fs->FATfs->FAT->FAT[block_for_newdir] = EOF_BLOCK;
    }
    else {
        block = *parent_last_block;

        block_for_newdir = allocate_block(fs->FATfs);
        if (block_for_newdir == -1) {
            printf("No more space left, file '%s' cannot be created!\n", dirname);
            return;
        }

        fs->FATfs->FAT->FAT[block_for_newdir] = EOF_BLOCK;
    }

    DirEntry* new_dir = (DirEntry*)(fs->FATfs->data + (block * BLOCK_SIZE) + offset);
    strncpy(new_dir->name, dirname, NAME_MAX_LEN);
    new_dir->name[NAME_MAX_LEN-1] = '\0';
    new_dir->parent_start = fs->current_dir;
    new_dir->type = TYPE_DIRECTORY;
    new_dir->start_block = block_for_newdir;
    new_dir->last_block = block_for_newdir;
    new_dir->size = 32;
    *parent_size += 32;

    //Creating dirEntry for newdir
    DirEntry* dir = (DirEntry*)(fs->FATfs->data + (block_for_newdir * BLOCK_SIZE));
    memcpy(dir, new_dir, 32);

    //Size increase propagation
    char current_dirname[NAME_MAX_LEN];
    strncpy(current_dirname, fs->FATfs->data + (fs->current_dir * BLOCK_SIZE), NAME_MAX_LEN);

    if (strncmp(current_dirname, "root", NAME_MAX_LEN) == 0) return;

    int pblock = *((int*)(fs->FATfs->data + (fs->current_dir * BLOCK_SIZE) + PARENT_START_OFFSET));

    int ok = 0;
    while (pblock != EOF_BLOCK) {
        for (int i = 0; i < BLOCK_SIZE; i += 32) {
            int type = *((int*)(fs->FATfs->data + (pblock * BLOCK_SIZE) + i + TYPE_OFFSET));

            if (!type) break;   
            if (type == TYPE_FILE) continue;

            char name[NAME_MAX_LEN];
            strncpy(name, fs->FATfs->data + (pblock * BLOCK_SIZE) + i, NAME_MAX_LEN);

            if(strcmp(current_dirname, name) == 0) {
                int* size = (int*)(fs->FATfs->data + (pblock * BLOCK_SIZE) + i + SIZE_OFFSET);
                *size += 32;
                ok = 1;
                break;
            } 
        }
        if(ok) break;

        pblock = fs->FATfs->FAT->FAT[pblock];            
    }
}

//Erase a directory
void eraseDir(FileSystem* fs, char *dirname) {
    int block = fs->current_dir;
    int previous_block = block;

    while (block != EOF_BLOCK) {
        for (int i = 0; i < BLOCK_SIZE; i += 32) {
            if (block == fs->current_dir && i == 0) continue;

            int type = *((int*)(fs->FATfs->data + (block * BLOCK_SIZE) + i + TYPE_OFFSET));

            if (!type) break;   
            if (type == TYPE_FILE) continue;

            char name[NAME_MAX_LEN];
            strncpy(name, fs->FATfs->data + (block * BLOCK_SIZE) + i, NAME_MAX_LEN);

            if(strcmp(dirname, name) == 0) {
                int dir_size = *((int*)(fs->FATfs->data + (block * BLOCK_SIZE) + i + SIZE_OFFSET));
                int start = *((int*)(fs->FATfs->data + (block * BLOCK_SIZE) + i + START_BLOCK_OFFSET));
                int temp = fs->current_dir;
                
                //Erasing all files and directories inside the directory
                fs->current_dir = start;

                while (dir_size > 32) {
                    type = *((int*)(fs->FATfs->data + (start * BLOCK_SIZE) + 32 + TYPE_OFFSET));
                    char name[NAME_MAX_LEN];
                    strncpy(name, fs->FATfs->data + (start * BLOCK_SIZE) + 32, NAME_MAX_LEN);

                    if (type == TYPE_FILE) {
                        eraseFile(fs, name);
                    }
                    else {
                        eraseDir(fs, name);
                    }
                    
                    dir_size -= 32;
                }

                memset(fs->FATfs->data + (start * BLOCK_SIZE), '\0', 32);
                free_block(fs->FATfs, start);
                fs->FATfs->FAT->FAT[start] = FREE;

                fs->current_dir = temp;
                defragment(fs, block, previous_block, i);
                int* parent_size = (int*)(fs->FATfs->data + (fs->current_dir * BLOCK_SIZE) + SIZE_OFFSET);
                *parent_size -= 32;

                //Size decrease propagation 
                char current_dirname[NAME_MAX_LEN];
                strncpy(current_dirname, fs->FATfs->data + (fs->current_dir * BLOCK_SIZE), NAME_MAX_LEN);
            
                if (strncmp(current_dirname, "root", NAME_MAX_LEN) == 0) return;

                int pblock = *((int*)(fs->FATfs->data + (fs->current_dir * BLOCK_SIZE) + PARENT_START_OFFSET));

                while (pblock != EOF_BLOCK) {
                    for (int i = 0; i < BLOCK_SIZE; i += 32) {
                        int type = *((int*)(fs->FATfs->data + (pblock * BLOCK_SIZE) + i + TYPE_OFFSET));

                        if (!type) break;   
                        if (type == TYPE_FILE) continue;

                        char name[NAME_MAX_LEN];
                        strncpy(name, fs->FATfs->data + (pblock * BLOCK_SIZE) + i, NAME_MAX_LEN);

                        if(strcmp(current_dirname, name) == 0) {
                            int* size = (int*)(fs->FATfs->data + (pblock * BLOCK_SIZE) + i + SIZE_OFFSET);
                            *size -= 32;
                            return;
                        } 
                    }
                    pblock = fs->FATfs->FAT->FAT[pblock];            
                }    
            } 
        }
        previous_block = block;
        block = fs->FATfs->FAT->FAT[previous_block];
    }

    printf("No directory named %s in this directory\n", dirname);
}

//Navigate through the directories of the filesystem
void changeDir(FileSystem* fs, char *dirname) {

    // dirname == "." -> remain in current directory
    if (strcmp(dirname, ".") == 0) {
        return;
    }

    // dirname == ".." -> move up to parent directory
    if (strcmp(dirname, "..") == 0) {
        int parent = *((int*)(fs->FATfs->data + (fs->current_dir * BLOCK_SIZE) + PARENT_START_OFFSET));
        fs->current_dir = parent;
        return;
    }

    int block = fs->current_dir;
    
    while (block != EOF_BLOCK) {
        for (int i = 0; i < BLOCK_SIZE; i += 32) {
            int type = *((int*)(fs->FATfs->data + (block * BLOCK_SIZE) + i + TYPE_OFFSET));

            if (!type) break;   
            if (type == TYPE_FILE) continue;

            char name[NAME_MAX_LEN];
            strncpy(name, fs->FATfs->data + (block * BLOCK_SIZE) + i, NAME_MAX_LEN);

            if(strcmp(dirname, name) == 0) {
                int start = *((int*)(fs->FATfs->data + (block * BLOCK_SIZE) + i + START_BLOCK_OFFSET));
                fs->current_dir = start;
                return;
            } 
        }
        block = fs->FATfs->FAT->FAT[block];
    }

    printf("No directory named %s in this directory\n", dirname);
}

//List all files and directories contained within the current directory
void listDir(FileSystem* fs) {
    int block = fs->current_dir;
    int dir_size = *((int*)(fs->FATfs->data + (fs->current_dir * BLOCK_SIZE) + SIZE_OFFSET));
    dir_size /= 32;
    char directories[dir_size][NAME_MAX_LEN];
    char files[dir_size][NAME_MAX_LEN];

    int dir_index = 0;
    int file_index = 0;
    while (block != EOF_BLOCK) {
        for (int i = 0; i < BLOCK_SIZE; i += 32) {
            int type = *((int*)(fs->FATfs->data + (block * BLOCK_SIZE) + i + TYPE_OFFSET));

            if (!type) break;   

            if (type == TYPE_FILE) {
                strncpy(files[file_index], fs->FATfs->data + (block * BLOCK_SIZE) + i, NAME_MAX_LEN);
                file_index++;
            }
            else {
                strncpy(directories[dir_index], fs->FATfs->data + (block * BLOCK_SIZE) + i, NAME_MAX_LEN);
                dir_index++;                
            }
        }
        block = fs->FATfs->FAT->FAT[block];
    }

    //Printing
    printf("Files inside current directory (%s):\n", directories[0]);
    for (int i = 0; i < file_index; i++) {
        printf("%s\n", files[i]);
    }

    printf("Directories inside current directory (%s):\n", directories[0]);
    for (int i = 1; i < dir_index; i++) {
        printf("%s\n", directories[i]);
    }

}