#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>
#include <fcntl.h>

#define MAX_PATH 1024

typedef struct {
    char **paths;
    int size;
    int capacity;
} PathList;

void init_path_list(PathList *list) {
    list->size = 0;
    list->capacity = 128;
    list->paths = malloc(list->capacity * sizeof(char *));
}

void add_to_path_list(PathList *list, const char *path) {
    if (list->size >= list->capacity) {
        list->capacity *= 2;
        char **new_paths = realloc(list->paths, list->capacity * sizeof(char *));
        if (new_paths == NULL) {
            perror("Failed to realloc memory for paths");
            exit(EXIT_FAILURE);
        }
        list->paths = new_paths;
    }

    list->paths[list->size] = strdup(path);
    if (list->paths[list->size] == NULL) {
        perror("Failed to allocate memory for path");
        exit(EXIT_FAILURE);
    }
    list->size++;
}

void free_path_list(PathList *list) {
    for (int i = 0; i < list->size; i++) {
        free(list->paths[i]);
    }
    free(list->paths);
}

void create_directory(const char *path) {
    struct stat st = {0};
    if (stat(path, &st) == -1) {
        mkdir(path, 0700);
    }
}

void copy_file(const char *src, const char *dest) {
    int source_fd = open(src, O_RDONLY);
    int dest_fd = open(dest, O_WRONLY | O_CREAT, 0700);

    char buffer[4096];
    ssize_t bytes;
    while ((bytes = read(source_fd, buffer, sizeof(buffer))) > 0) {
        write(dest_fd, buffer, bytes);
    }

    close(source_fd);
    close(dest_fd);
}

void gather_paths(const char *base_path, PathList *dir_list, PathList *file_list, PathList *symlink_list) {
    DIR *dir;
    struct dirent *entry;
    char full_path[MAX_PATH];

    if ((dir = opendir(base_path)) == NULL) {
        perror("opendir");
        return;
    }

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        snprintf(full_path, sizeof(full_path), "%s/%s", base_path, entry->d_name);

        struct stat entry_stat;
        lstat(full_path, &entry_stat);

        if (S_ISDIR(entry_stat.st_mode)) {
            add_to_path_list(dir_list, full_path);
            gather_paths(full_path, dir_list, file_list, symlink_list);
        } else if (S_ISREG(entry_stat.st_mode)) {
            add_to_path_list(file_list, full_path);
        } else if (S_ISLNK(entry_stat.st_mode)) {
            add_to_path_list(symlink_list, full_path);
        }
    }

    closedir(dir);
}

void copy_directories(const PathList *dir_list, const char *base_path, const char *backup_path) {
    for (int i = 0; i < dir_list->size; i++) {
        char relative_path[MAX_PATH];
        const char *full_dir_path = dir_list->paths[i];
        snprintf(relative_path, MAX_PATH, "%s", full_dir_path + strlen(base_path));
        char dest_dir_path[MAX_PATH];
        snprintf(dest_dir_path, MAX_PATH, "%s%s", backup_path, relative_path);
        create_directory(dest_dir_path);
    }
}

void copy_files(const PathList *file_list, const char *base_path, const char *backup_path) {
    for (int i = 0; i < file_list->size; i++) {
        char relative_path[MAX_PATH];
        const char *full_file_path = file_list->paths[i];
        snprintf(relative_path, MAX_PATH, "%s", full_file_path + strlen(base_path));
        char dest_file_path[MAX_PATH];
        snprintf(dest_file_path, MAX_PATH, "%s%s", backup_path, relative_path);
        copy_file(full_file_path, dest_file_path);
    }
}

void save_symlink_list(const PathList *symlink_list, const char *base_path, const char *backup_path) {
    char symlink_list_path[MAX_PATH];
    snprintf(symlink_list_path, MAX_PATH, "%s/symlink_list.txt", backup_path);
    FILE *symlink_file = fopen(symlink_list_path, "w");

    if (symlink_file == NULL) {
        perror("fopen");
        return;
    }

    for (int i = 0; i < symlink_list->size; i++) {
        char relative_path[MAX_PATH];
        const char *full_symlink_path = symlink_list->paths[i];
        snprintf(relative_path, MAX_PATH, "%s", full_symlink_path + strlen(base_path));
        char link_target[MAX_PATH];
        readlink(full_symlink_path, link_target, sizeof(link_target) - 1);
        fprintf(symlink_file, "%s -> %s\n", relative_path, link_target);
    }

    fclose(symlink_file);
}

void restore_directories(const PathList *dir_list, const char *base_path, const char *restore_path) {
    for (int i = 0; i < dir_list->size; i++) {
        char relative_path[MAX_PATH];
        const char *full_dir_path = dir_list->paths[i];
        snprintf(relative_path, MAX_PATH, "%s", full_dir_path + strlen(base_path));
        char dest_dir_path[MAX_PATH];
        snprintf(dest_dir_path, MAX_PATH, "%s%s", restore_path, relative_path);
        create_directory(dest_dir_path);
    }
}

void restore_files(const PathList *file_list, const char *base_path, const char *restore_path) {
    for (int i = 0; i < file_list->size; i++) {
        char relative_path[MAX_PATH];
        const char *full_file_path = file_list->paths[i];
        snprintf(relative_path, MAX_PATH, "%s", full_file_path + strlen(base_path));
        char dest_file_path[MAX_PATH];
        snprintf(dest_file_path, MAX_PATH, "%s%s", restore_path, relative_path);
        copy_file(full_file_path, dest_file_path);
    }
}

void restore_symlinks(const char *symlink_list_path, const char *restore_path) {
    FILE *symlink_file = fopen(symlink_list_path, "r");
    if (symlink_file == NULL) {
        perror("fopen");
        return;
    }

    char line[MAX_PATH];
    while (fgets(line, sizeof(line), symlink_file)) {
        char relative_path[MAX_PATH], link_target[MAX_PATH];
        sscanf(line, "%s -> %s", relative_path, link_target);

        char symlink_path[MAX_PATH];
        snprintf(symlink_path, MAX_PATH, "%s%s", restore_path, relative_path);

        symlink(link_target, symlink_path);
    }

    fclose(symlink_file);
}

void generate_backup_id(char *buffer, size_t len) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    strftime(buffer, len, "%Y%m%d%H%M%S_FULL", t);
}

void full_backup(const char *repository_path, const char *database_path) {
    char backup_id[64];
    generate_backup_id(backup_id, sizeof(backup_id));

    char backup_path[MAX_PATH];
    snprintf(backup_path, MAX_PATH, "%s/%s", repository_path, backup_id);
    create_directory(backup_path);

    PathList dir_list, file_list, symlink_list;

    init_path_list(&dir_list);
    init_path_list(&file_list);
    init_path_list(&symlink_list);

    gather_paths(database_path, &dir_list, &file_list, &symlink_list);

    copy_directories(&dir_list, database_path, backup_path);

    copy_files(&file_list, database_path, backup_path);

    save_symlink_list(&symlink_list, database_path, backup_path);

    free_path_list(&dir_list);
    free_path_list(&file_list);
    free_path_list(&symlink_list);

    printf("Backup completed successfully in %s\n", backup_path);
}

void restore(const char *repository_path, const char *restore_path, const char *backup_id) {
    char backup_path[MAX_PATH];
    snprintf(backup_path, MAX_PATH, "%s/%s", repository_path, backup_id);

    PathList dir_list, file_list, symlink_list;

    init_path_list(&dir_list);
    init_path_list(&file_list);
    init_path_list(&symlink_list);

    gather_paths(backup_path, &dir_list, &file_list, &symlink_list);

    restore_directories(&dir_list, backup_path, restore_path);

    restore_files(&file_list, backup_path, restore_path);

    char symlink_list_file[MAX_PATH];
    snprintf(symlink_list_file, MAX_PATH, "%s/symlink_list.txt", backup_path);
    restore_symlinks(symlink_list_file, restore_path);

    free_path_list(&dir_list);
    free_path_list(&file_list);
    free_path_list(&symlink_list);

    printf("Restore completed successfully from %s to %s\n", backup_path, restore_path);
}

int main(int argc, char *argv[]) {
    if (argc < 4) {
        fprintf(stderr, "Usage: %s <operation> <repository_path> <database_path> [backup_id]\n", argv[0]);
        return 1;
    }

    const char *repository_path = argv[1];
    const char *operation = argv[2];
    const char *database_path = argv[3];

    if (strcmp(operation, "full_backup") == 0) {
        full_backup(repository_path, database_path);
    } else if (strcmp(operation, "restore") == 0 && argc == 5) {
        const char *backup_id = argv[4];
        restore(repository_path, database_path, backup_id);
    } else {
        fprintf(stderr, "Invalid operation or arguments.\n");
        return 1;
    }

    return 0;
}
