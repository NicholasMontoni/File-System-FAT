#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#include "file-system.h"

FATFileSystem* startFileSystem() {
    FATFileSystem* fs = (FATFileSystem*) malloc (sizeof (FATFileSystem));
    memset (fs->FAT, FREE, sizeof(fs->FAT));
    int fd = open ("filesystem", O_RDWR | O_CREAT, 0666);
    
    if (fd == -1) {
        perror("Error opening filesystem");
        exit(EXIT_FAILURE);
    }

    if (ftruncate(fd, BLOCK_SIZE*NUM_BLOCKS) == -1) {
        perror("Error truncating filesystem");
        exit(EXIT_FAILURE);
    }

    fs->data = mmap(NULL, NUM_BLOCKS*BLOCK_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (fs->data == MAP_FAILED) {
        perror("Error mapping filesystem");
        exit(EXIT_FAILURE);
    }

    memset(fs->data, '\0', BLOCK_SIZE*NUM_BLOCKS);
    
    if (close(fd) == -1) {
        perror("Error closing filesystem");
        exit(EXIT_FAILURE);
    }

    fs->current_dir = (Entry*) malloc (sizeof(Entry));
    fs->current_dir->name = "root";
    fs->current_dir->num_files = 0;
    fs->current_dir->num_directories = 0;
    fs->current_dir->parent = fs->current_dir;
    fs->current_dir->directories = (Entry**) malloc(MAX_NUM_FILES * sizeof(Entry*));
    fs->current_dir->files_handlers = (FileHandle**) malloc(MAX_NUM_FILES * sizeof(FileHandle*));

    return fs;
}

void endFileSystem(FATFileSystem* fs) {
    free(fs->current_dir->directories);
    free(fs->current_dir->files_handlers);
    free(fs->current_dir);
    
    if (munmap(fs->data, NUM_BLOCKS*BLOCK_SIZE) == -1) {
        perror("Error unmapping filesystem");
        exit(EXIT_FAILURE);
    }

    free(fs);
}
 
FileHandle* createFile(FATFileSystem* fs, char *filename) {
    Entry* current_dir = fs->current_dir;
    if (current_dir->num_files == MAX_NUM_FILES) {
        printf("Current directory is full\n");
        return NULL;
    };

    for (int i=0; i<current_dir->num_files; i++) {
        if (strcmp(current_dir->files_handlers[i]->name, filename)==0) {
            printf("File '%s' already exists in current directory\n", filename);
            return NULL;
        }
    }

    //Finding free block
    int block_found = -1;
    for (int i=0; i<NUM_BLOCKS; i++) {
        if (fs->FAT[i]==FREE) {
            block_found = i;
            fs->FAT[i]=EOF_BLOCK;
            break;
        }
    }

    if (block_found == -1) {
        printf("No more space\n");
        return NULL;
    }

    //Creating file handler
    FileHandle* entryfh = (FileHandle*) malloc (sizeof (FileHandle));
    entryfh->name = filename;
    entryfh->parent_name = current_dir->name;
    entryfh->first_block = block_found;
    entryfh->current_block = block_found;
    entryfh->last_block = block_found;
    entryfh->pos = 0;

    fs->current_dir->files_handlers[fs->current_dir->num_files] = entryfh;
    fs->current_dir->num_files++;

    return entryfh;
}

void eraseFile(FATFileSystem* fs, FileHandle* fh) {
    if (fh == NULL) {
        printf("Invalid file\n");
        return;
    }

    Entry* current_dir = fs->current_dir;
    
    if (strcmp(current_dir->name, fh->parent_name) != 0) {
        printf("The file is not in this directory. Change to '%s' directory and try again\n", fh->parent_name);
        return;
    }

    for (int i=0; i<current_dir->num_files; i++) {
        if (strcmp(current_dir->files_handlers[i]->name, fh->name)==0) {
            int b = current_dir->files_handlers[i]->first_block;
            
            //Freeing all blocks
            do {
                int temp = fs->FAT[b];
                memset(fs->data+(b*BLOCK_SIZE),'\0', BLOCK_SIZE);
                fs->FAT[b] = FREE;
                b = temp;
            } while (b != EOF_BLOCK);

            //Freeing file handler
            for (int j = i; j < fs->current_dir->num_files - 1; j++) {
                fs->current_dir->files_handlers[j] = fs->current_dir->files_handlers[j + 1];
            }
            fs->current_dir->files_handlers[fs->current_dir->num_files-1] = NULL;
            current_dir->num_files--;
            
            free(fh);

            return;
        }
    }
}

void writeFile(FATFileSystem* fs, FileHandle *fh, const void *buf, int size) {
    if (fh == NULL) {
        printf("Invalid file\n");
        return;
    }

    if (strcmp(fs->current_dir->name, fh->parent_name) != 0) {
        printf("The file is not in this directory. Change to '%s' directory and try again\n", fh->parent_name);
        return;
    }

    //Cleaning the previous contents of the file
    int i = fh->first_block;
    while (i != EOF_BLOCK) {
        memset(fs->data + (i * BLOCK_SIZE), '\0', BLOCK_SIZE);
        int temp = fs->FAT[i];
        fs->FAT[i] = (i == fh->first_block) ? EOF_BLOCK : FREE;
        i = temp;
    }

    //Writing from the beginning 
    fh->pos = 0;
    int initial_position = fh->pos;
    fh->last_block = fh->first_block;
    int written_bytes = 0;

    while (size > 0) {
        int left_bytes = (size > BLOCK_SIZE) ? BLOCK_SIZE : size;
        memcpy (fs->data + (fh->last_block * BLOCK_SIZE), buf + written_bytes, left_bytes);
        written_bytes += left_bytes;
        size -= left_bytes;
        fh->pos += left_bytes;

        if (size != 0) {
            int found = 0;
            for (int i=0; i<NUM_BLOCKS; i++) {
                if (fs->FAT[i] == FREE) {
                    found = 1;
                    fs->FAT[fh->last_block] = i;
                    fs->FAT[i] = EOF_BLOCK;
                    fh->current_block = i;
                    fh->last_block = i;
                    break;
                }
            }
            
            if (!found) {
                printf ("Buf was too big, no more space left. Only first %d bytes of buf have been written!\n",(fh->pos-initial_position));
                return;
            }
        }
    }
}

void appendFile(FATFileSystem* fs, FileHandle *fh, const void *buf, int size) {

    if (fh == NULL) {
        printf("Invalid file\n");
        return;
    }

    if (strcmp(fs->current_dir->name, fh->parent_name) != 0) {
        printf("The file is not in this directory. Change to '%s' directory and try again\n", fh->parent_name);
        return;
    }

    int initial_position = fh->pos;
    int offset = fh->pos % BLOCK_SIZE;

    //Cleaning the previous contents of the file after current position
    int i = fh->current_block;
    while (i != EOF_BLOCK) {
        (i == fh->current_block)? memset(fs->data + (i * BLOCK_SIZE) + offset, '\0', BLOCK_SIZE - offset) : memset(fs->data + (i * BLOCK_SIZE), '\0', BLOCK_SIZE);
        int temp = fs->FAT[i];
        fs->FAT[i] = (i == fh->current_block) ? EOF_BLOCK : FREE;
        i = temp;
    }

    //Writing from current position
    if (size < BLOCK_SIZE - offset) {
        memcpy (fs->data + (fh->current_block * BLOCK_SIZE) + offset-1, buf, size);
        fh->pos += size;
        return;
    }

    memcpy (fs->data + (fh->current_block * BLOCK_SIZE) + offset-1, buf, BLOCK_SIZE - offset+1);
    int written_bytes = (BLOCK_SIZE - offset + 1);
    fh->pos += written_bytes;
    size -= written_bytes;

    while (size > 0) {
        int found = 0;
        for (int i=0; i<NUM_BLOCKS; i++) {
            if (fs->FAT[i] == FREE) {
                found = 1;
                fs->FAT[fh->last_block] = i;
                fs->FAT[i] = EOF_BLOCK;
                fh->current_block = i;
                fh->last_block = i;
                break;
            }
        }
        
        if (!found) {
            printf ("Buf was too big, no more space left. Only first %d bytes of buf have been written!\n",(fh->pos-initial_position));
            return;
        }

        int left_bytes = (size > BLOCK_SIZE) ? BLOCK_SIZE : size;
        memcpy (fs->data + (fh->last_block * BLOCK_SIZE), buf + written_bytes, left_bytes);
        written_bytes += left_bytes;
        size -= left_bytes;
        fh->pos += left_bytes;
    }
}

void readFile(FATFileSystem* fs, FileHandle *fh, void *buf, int size) {

    if (fh == NULL) {
        printf("Invalid file\n");
        return;
    }

    if (strcmp(fs->current_dir->name, fh->parent_name) != 0) {
        printf("The file is not in this directory. Change to '%s' directory and try again\n", fh->parent_name);
        return;
    }
    
    int offset = fh->pos % BLOCK_SIZE;
    if (*(fs->data + (fh->current_block * BLOCK_SIZE) + offset) == '\0') {
        printf("Nothing to read\n");
        return;
    }

    if (size < BLOCK_SIZE - offset) {
        memcpy(buf, fs->data + (fh->current_block * BLOCK_SIZE) + offset, size);
        fh->pos += size;
        return;
    }

    memcpy(buf, fs->data + (fh->current_block * BLOCK_SIZE) + offset, BLOCK_SIZE - offset);
    int read_bytes = BLOCK_SIZE - offset; 
    fh->pos += read_bytes;
    size -= read_bytes;

    while (size > 0) {      
        int block = fh->current_block;
        fh->current_block = fs->FAT[block];

        int left_bytes = (size > BLOCK_SIZE) ? BLOCK_SIZE : size;
        memcpy (buf + read_bytes, fs->data + (fh->current_block * BLOCK_SIZE), left_bytes);
        read_bytes += left_bytes;
        size -= left_bytes;
        fh->pos += left_bytes;

        if (fh->current_block == fh->last_block) return;
    }
}

void seekFile(FATFileSystem* fs, FileHandle *fh, int offset, int whence) {

    if (fh == NULL) {
        printf("Invalid file\n");
        return;
    }

    if (strcmp(fs->current_dir->name, fh->parent_name) != 0) {
        printf("The file is not in this directory. Change to '%s' directory and try again\n", fh->parent_name);
        return;
    }

    if (*(fs->data + fh->first_block * BLOCK_SIZE) == '\0') {
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
            
            if (offset > BLOCK_SIZE) fh->current_block = fs->FAT[block];
            int val = (offset > BLOCK_SIZE) ? BLOCK_SIZE+1 : offset;
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
            block = fs->FAT[block];
        }

        char* temp = fs->data + (fh->last_block * BLOCK_SIZE);
        while (*temp) {
            fh->pos++;
            temp++;
        }

        if (offset >= 0) return;

        seekFile(fs, fh, fh->pos + offset, 0);
    }
}

void createDir(FATFileSystem* fs, char *dirname) {
    if (fs->current_dir->num_directories == MAX_NUM_FILES) {
        printf("Current directory is full\n");
        return;
    };

    for (int i=0; i<fs->current_dir->num_directories; i++) {
        if (strcmp(fs->current_dir->directories[i]->name, dirname)==0) {
            printf("Directory '%s' already exists in current directory\n", dirname);
            return;
        }
    }

    //Creating new directory
    Entry* new_entry = (Entry*) malloc(sizeof(Entry));
    new_entry->name = dirname;
    new_entry->num_files = 0;
    new_entry->num_directories = 0;
    new_entry->parent = fs->current_dir;
    new_entry->directories = (Entry**) malloc(MAX_NUM_FILES * sizeof(Entry*));
    new_entry->files_handlers = (FileHandle**) malloc(MAX_NUM_FILES * sizeof(FileHandle*));


    fs->current_dir->directories[fs->current_dir->num_directories] = new_entry;
    fs->current_dir->num_directories++;
}

void eraseDir(FATFileSystem* fs, char *dirname) {
    Entry* current_dir = fs->current_dir;

    int ok = 0;
    for (int i=0; i<current_dir->num_directories; i++) {
        if (strcmp(current_dir->directories[i]->name, dirname)==0) {
            Entry* dir = current_dir->directories[i];
            fs->current_dir = dir;
        
            //Deleting files inside the directory
            while (fs->current_dir->num_files > 0) {
                eraseFile(fs, fs->current_dir->files_handlers[0]);
            }
            
            //Deleting directories inside the directory
            while (fs->current_dir->num_directories > 0) {
                eraseDir(fs, fs->current_dir->directories[0]->name);
            }

            //Freeing the directory
            free(dir->directories);
            free(dir->files_handlers);
            free(dir);

            fs->current_dir = current_dir;

            for (int k = i; k < fs->current_dir->num_directories - 1; k++) {
                fs->current_dir->directories[k] = fs->current_dir->directories[k + 1];
            }
            fs->current_dir->directories[fs->current_dir->num_directories-1] = NULL;
            fs->current_dir->num_directories--;
            
            ok = 1;
            break;
        }
    }

    if (!ok) {
        printf("No directory called '%s' in current directory!\n", dirname);
        return;
    }
}

void changeDir(FATFileSystem* fs, char *dirname) {
    Entry* current_dir = fs->current_dir;

    // dirname == "." -> remain in current directory
    if (strcmp(dirname, ".") == 0) {
        return;
    }

    // dirname == ".." -> move up to parent directory
    if (strcmp(dirname, "..") == 0) {
        fs->current_dir = current_dir->parent;
        return;
    }
    
    int ok = 0;
    for (int i=0; i<current_dir->num_directories; i++) {
        if (strcmp(current_dir->directories[i]->name, dirname)==0) {
            fs->current_dir = current_dir->directories[i];
            ok = 1;
            break;
        }
    }

    if (!ok) {
        printf("No directory called '%s' in current directory!\n", dirname);
        return;
    }
}

void listDir(FATFileSystem* fs) {
    Entry* current_dir = fs->current_dir;

    //Listing directories inside current directory
    printf("Directories inside '%s' directory:\n", current_dir->name);

    for (int i = 0; i < current_dir->num_directories; i++) {
        printf("%s\n", current_dir->directories[i]->name);
    }

    //Listing files inside current directory
    printf("Files inside '%s' directory:\n", current_dir->name);

    for (int j = 0; j < current_dir->num_files; j++) {
        printf("%s\n", current_dir->files_handlers[j]->name);
    }
}