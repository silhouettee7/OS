#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>

#define MAX_FILES 10000
#define MAX_PATH_LEN 4096

typedef struct {
    char path[MAX_PATH_LEN];
    char name[NAME_MAX];
    off_t size;
} FileInfo;

int compare_by_size(const void *a, const void *b) {
    const FileInfo *file1 = (const FileInfo *)a;
    const FileInfo *file2 = (const FileInfo *)b;
    if (file1->size < file2->size) return -1;
    if (file1->size > file2->size) return 1;
    return 0;
}

int compare_by_name(const void *a, const void *b) {
    const FileInfo *file1 = (const FileInfo *)a;
    const FileInfo *file2 = (const FileInfo *)b;
    return strcmp(file1->name, file2->name);
}

void collect_files(const char *base_path, FileInfo *files, int *count) {
    char path[MAX_PATH_LEN];
    struct dirent *entry;
    DIR *dir = opendir(base_path);
    
    if (!dir) {
        fprintf(stderr, "Cannot open directory %s: %s\n", base_path, strerror(errno));
        return;
    }
    
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        int path_len = snprintf(path, sizeof(path), "%s/%s", base_path, entry->d_name);
        if (path_len >= sizeof(path)) {
            fprintf(stderr, "Path too long, skipping: %s/%s\n", base_path, entry->d_name);
            continue;
        }
        
        struct stat statbuf;
        if (lstat(path, &statbuf) == -1) {
            fprintf(stderr, "Cannot stat %s: %s\n", path, strerror(errno));
            continue;
        }
        
        if (S_ISDIR(statbuf.st_mode)) {
            collect_files(path, files, count);
        } else if (S_ISREG(statbuf.st_mode)) {
            if (*count >= MAX_FILES) {
                fprintf(stderr, "Too many files, maximum is %d\n", MAX_FILES);
                closedir(dir);
                return;
            }
            
            strncpy(files[*count].path, path, MAX_PATH_LEN - 1);
            files[*count].path[MAX_PATH_LEN - 1] = '\0';
            strncpy(files[*count].name, entry->d_name, NAME_MAX - 1);
            files[*count].name[NAME_MAX - 1] = '\0';
            files[*count].size = statbuf.st_size;
            (*count)++;
        }
    }
    
    closedir(dir);
}

int copy_file(const char *src_path, const char *dest_path) {
    FILE *src = fopen(src_path, "rb");
    if (!src) {
        fprintf(stderr, "Cannot open source file %s: %s\n", src_path, strerror(errno));
        return 0;
    }
    
    FILE *dest = fopen(dest_path, "wb");
    if (!dest) {
        fprintf(stderr, "Cannot create destination file %s: %s\n", dest_path, strerror(errno));
        fclose(src);
        return 0;
    }
    
    char buffer[8192];
    size_t bytes;
    int success = 1;
    
    while ((bytes = fread(buffer, 1, sizeof(buffer), src)) > 0) {
        if (fwrite(buffer, 1, bytes, dest) != bytes) {
            fprintf(stderr, "Error writing to %s: %s\n", dest_path, strerror(errno));
            success = 0;
            break;
        }
    }
    
    if (ferror(src)) {
        fprintf(stderr, "Error reading from %s: %s\n", src_path, strerror(errno));
        success = 0;
    }
    
    fclose(src);
    fclose(dest);
    return success;
}

void create_directory_recursive(const char *path) {
    char tmp[MAX_PATH_LEN];
    
    if (strlen(path) >= MAX_PATH_LEN) {
        fprintf(stderr, "Path too long: %s\n", path);
        return;
    }
    
    strncpy(tmp, path, sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = '\0';
    
    size_t len = strlen(tmp);
    if (len > 0 && tmp[len - 1] == '/') {
        tmp[len - 1] = '\0';
    }
    
    char *p = tmp;
    if (*p == '/') {
        p++;
    }
    
    char *slash;
    while ((slash = strchr(p, '/')) != NULL) {
        *slash = '\0';
        
        char dir_path[MAX_PATH_LEN];
        if (tmp[0] == '/') {
            snprintf(dir_path, sizeof(dir_path), "/%s", tmp);
        } else {
            snprintf(dir_path, sizeof(dir_path), "%s", tmp);
        }
        
        if (mkdir(dir_path, 0755) != 0 && errno != EEXIST) {
            fprintf(stderr, "Cannot create directory %s: %s\n", dir_path, strerror(errno));
        }
        
        *slash = '/';
        p = slash + 1;
    }
    
    if (mkdir(path, 0755) != 0 && errno != EEXIST) {
        fprintf(stderr, "Cannot create directory %s: %s\n", path, strerror(errno));
    }
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <source_dir> <sort_criteria> <dest_dir>\n", argv[0]);
        fprintf(stderr, "Sort criteria: 1 - by size, 2 - by name\n");
        return 1;
    }
    
    struct stat statbuf;
    if (stat(argv[1], &statbuf) == -1 || !S_ISDIR(statbuf.st_mode)) {
        fprintf(stderr, "Source directory %s does not exist or is not a directory\n", argv[1]);
        return 1;
    }
    
    int sort_criteria = atoi(argv[2]);
    if (sort_criteria != 1 && sort_criteria != 2) {
        fprintf(stderr, "Invalid sort criteria. Use 1 (by size) or 2 (by name)\n");
        return 1;
    }
    
    FileInfo *files = malloc(MAX_FILES * sizeof(FileInfo));
    if (!files) {
        fprintf(stderr, "Memory allocation failed\n");
        return 1;
    }
    
    int file_count = 0;
    
    printf("Collecting files from %s and subdirectories...\n", argv[1]);
    collect_files(argv[1], files, &file_count);
    
    if (file_count == 0) {
        printf("No files found in %s\n", argv[1]);
        free(files);
        return 0;
    }
    
    printf("Found %d files. Sorting...\n", file_count);
    
    if (sort_criteria == 1) {
        qsort(files, file_count, sizeof(FileInfo), compare_by_size);
    } else {
        qsort(files, file_count, sizeof(FileInfo), compare_by_name);
    }
    
    printf("Creating destination directory %s\n", argv[3]);
    create_directory_recursive(argv[3]);
    
    printf("Copying files to %s\n\n", argv[3]);
    
    int copied_count = 0;
    for (int i = 0; i < file_count; i++) {
        char dest_path[MAX_PATH_LEN];
        int needed = snprintf(dest_path, sizeof(dest_path), "%s/%s", argv[3], files[i].name);
        if (needed >= sizeof(dest_path)) {
            fprintf(stderr, "Destination path too long, skipping: %s\n", files[i].path);
            continue;
        }
        
        char unique_name[MAX_PATH_LEN];
        strncpy(unique_name, dest_path, sizeof(unique_name) - 1);
        unique_name[sizeof(unique_name) - 1] = '\0';
        
        int counter = 1;
        while (access(unique_name, F_OK) == 0) {
            char *dot = strrchr(files[i].name, '.');
            if (dot) {
                char base[NAME_MAX];
                char ext[NAME_MAX];
                
                size_t base_len = dot - files[i].name;
                if (base_len >= sizeof(base)) base_len = sizeof(base) - 1;
                strncpy(base, files[i].name, base_len);
                base[base_len] = '\0';
                
                strncpy(ext, dot, sizeof(ext) - 1);
                ext[sizeof(ext) - 1] = '\0';
                
                needed = snprintf(unique_name, sizeof(unique_name), 
                                 "%s/%s_%d%s", argv[3], base, counter, ext);
            } else {
                needed = snprintf(unique_name, sizeof(unique_name), 
                                 "%s/%s_%d", argv[3], files[i].name, counter);
            }
            
            if (needed >= sizeof(unique_name)) {
                fprintf(stderr, "Unique name too long, skipping: %s\n", files[i].path);
                unique_name[0] = '\0';
                break;
            }
            
            counter++;
        }
        
        if (unique_name[0] == '\0') {
            continue;
        }
        
        if (copy_file(files[i].path, unique_name)) {
            printf("Copied: %s\n", files[i].path);
            printf("  Name: %s\n", files[i].name);
            printf("  Size: %ld bytes\n", (long)files[i].size);
            printf("  Destination: %s\n\n", unique_name);
            copied_count++;
        } else {
            fprintf(stderr, "Failed to copy: %s\n\n", files[i].path);
        }
    }
    
    free(files);
    
    printf("Done! Successfully copied %d of %d files to %s\n", 
           copied_count, file_count, argv[3]);
    
    return 0;
}