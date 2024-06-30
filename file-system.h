#define MAX_NUM_FILES 100
#define NUM_BLOCKS 1024
#define BLOCK_SIZE 40
#define FREE -1
#define EOF_BLOCK -2
#define TYPE_FILE 1
#define TYPE_DIRECTORY 2

typedef struct Entry{
    char* name;
    int start_index;
    int type;
    int num_files;
    int num_directories;
    struct Entry* parent;
    struct Entry** directories;
    FileHandle** files_handlers;
} Entry;

typedef struct {
    int FAT[NUM_BLOCKS];
    Entry* current_dir;
    char* data;
} FATFileSystem;

typedef struct {
    char* name;
    char* parent_name;
    int first_block;
    int current_block;
    int last_block;
    int pos;
} FileHandle;

FileHandle *createFile(FATFileSystem* fs, char *filename);
void eraseFile(FATFileSystem* fs, FileHandle* fh);
void writeFile(FATFileSystem* fs, FileHandle *fh, const void *buf, int size);
void appendFile(FATFileSystem* fs, FileHandle* fh, const void *buf, int size);
void readFile(FATFileSystem* fs, FileHandle *fh, void *buf, int size);
void seekFile(FATFileSystem* fs, FileHandle *fh, int offset, int whence);
void createDir(FATFileSystem* fs, const char *dirname);
void eraseDir(FATFileSystem* fs, const char *dirname);
void changeDir(FATFileSystem* fs, const char *dirname);
void listDir(FATFileSystem* fs);
