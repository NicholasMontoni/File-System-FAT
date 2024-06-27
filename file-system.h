#define MAX_NUM_FILES 100
#define NUM_BLOCKS 1024
#define BLOCK_SIZE 4096
#define FREE -1
#define EOF_BLOCK -2
#define TYPE_FILE 1
#define TYPE_DIRECTORY 2

typedef struct Entry{
    char* name;
    int start_index;
    int size;
    int type;
    int num_files;
    int num_directories;
    struct Entry* parent;
    struct Entry** directories;
    struct Entry** files;
} Entry;

typedef struct {
    int FAT[NUM_BLOCKS];
    Entry* current_dir;
    char* data;
} FATFileSystem;

typedef struct {
    char* parent_name;
    int pos;
} FileHandle;

FileHandle *createFile(FATFileSystem* fs, char *filename);
void eraseFile(FATFileSystem* fs, const char *filename);
void writeFile(FileHandle *fh, const void *buf, int size);
void readFile(FileHandle *fh, void *buf, int size);
void seekFile(FileHandle *fh, int pos);
void createDir(FATFileSystem* fs, const char *dirname);
void eraseDir(FATFileSystem* fs, const char *dirname);
void changeDir(FATFileSystem* fs, const char *dirname);
void listDir(FATFileSystem* fs);
