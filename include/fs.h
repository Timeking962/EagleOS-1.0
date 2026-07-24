// EagleOS 1.0 Flat Filesystem Interface.
#ifndef FS_H
#define FS_H

#include <stdint.h>

#define FS_NAME_LEN 32

#define FS_ENTRY_KIND_FILE 1
#define FS_ENTRY_KIND_DIR  2

void fs_init(void);
int fs_write_file(const char *name, const uint8_t *data, uint16_t size);
int fs_read_file(const char *name, uint8_t *out, uint16_t max_size, uint16_t *out_size);
int fs_delete_file(const char *name);
int fs_rename_file(const char *old_name, const char *new_name);
int fs_create_dir(const char *path);
int fs_delete_dir(const char *path);
int fs_rename_dir(const char *old_path, const char *new_path);
int fs_persistence_active(void);
uint16_t fs_file_count(void);
int fs_file_info(uint16_t visible_index, char *name_out, uint16_t name_out_size, uint16_t *size_out);
uint16_t fs_list_count(const char *dir_path);
int fs_list_entry(const char *dir_path, uint16_t visible_index, char *name_out, uint16_t name_out_size, uint16_t *size_out, uint8_t *kind_out);
int fs_path_is_read_only(const char *path);
void fs_request_open_file(const char *name);
int fs_take_open_request(char *name_out, uint16_t name_out_size);

#endif // FS_H
