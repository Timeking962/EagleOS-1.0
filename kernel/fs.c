// EagleOS 1.0 Flat Filesystem Implementation.
#include <stdint.h>
#include "../include/fs.h"
#include "../include/disk.h"
#include "../include/version.h"

#define FS_MAX_FILES 8
#define FS_LEGACY_NAME_LEN 16
#define FS_MAX_FILE_SIZE 1024
#define FS_DISK_MAGIC 0x31534645u
#define FS_DISK_VERSION 2u
#define FS_SECTOR_SIZE 512u
#define FS_DISK_START_LBA 200u
#define FS_DISK_SECTORS 20u
#define FS_SYSTEM_DIR "EAGLEOS"
#define FS_DIR_MARKER_NAME ".DIR"

typedef struct fs_virtual_file {
    const char *name;
    const uint8_t *data;
    uint16_t size;
} fs_virtual_file_t;

typedef struct fs_list_item {
    char name[FS_NAME_LEN];
    uint16_t size;
    uint8_t kind;
} fs_list_item_t;

typedef struct __attribute__((packed)) fs_disk_header {
    uint32_t magic;
    uint16_t version;
    uint16_t file_count;
} fs_disk_header_t;

typedef struct fs_entry {
    uint8_t used;
    char name[FS_NAME_LEN];
    uint16_t size;
    uint8_t data[FS_MAX_FILE_SIZE];
} fs_entry_t;

static fs_entry_t entries[FS_MAX_FILES];

static uint8_t disk_blob[FS_DISK_SECTORS * FS_SECTOR_SIZE];
static uint8_t disk_probe_done = 0;
static uint8_t disk_persistence_enabled = 0;
static char pending_open_name[FS_NAME_LEN];
static uint8_t pending_open_valid = 0;

static const uint8_t boot_sys_data[] =
    "EAGLEOS BOOT SYSTEM\n"
    "STAGE-1 BOOTSTRAP IMAGE\n"
    "LOADS VGA MODE AND THE KERNEL.\n";

static const uint8_t kernel_sys_data[] =
    "EAGLEOS KERNEL\n"
    "PROTECTED MODE CORE\n"
    "PROVIDES EXEC, UI, FS, AND INPUT SERVICES.\n";

static const uint8_t desktop_sys_data[] =
    "EAGLEOS DESKTOP\n"
    "DESKTOP INTERFACE MANAGER LAYER\n"
    "WINDOW CHROME, DIALOGS, TASKBAR, AND SHARED UI.\n";

static const uint8_t exec_sys_data[] =
    "EAGLEOS EXECUTIVE\n"
    "PROGRAM REGISTRY AND WINDOW RUNTIME\n"
    "LAUNCHES BUILTIN APPS AND MANAGES OPEN WINDOWS.\n";

static const uint8_t version_txt_data[] =
    "EAGLEOS VERSION\n"
    "VERSION: " EAGLEOS_VERSION "\n"
    "BUILD: " EAGLEOS_BUILD_TAG "\n";

static const fs_virtual_file_t system_files[] = {
    {"BOOT.SYS", boot_sys_data, (uint16_t)(sizeof(boot_sys_data) - 1)},
    {"KERNEL.SYS", kernel_sys_data, (uint16_t)(sizeof(kernel_sys_data) - 1)},
    {"DESKTOP.SYS", desktop_sys_data, (uint16_t)(sizeof(desktop_sys_data) - 1)},
    {"EXEC.SYS", exec_sys_data, (uint16_t)(sizeof(exec_sys_data) - 1)},
    {"VERSION.TXT", version_txt_data, (uint16_t)(sizeof(version_txt_data) - 1)}
};

static uint8_t streq(const char *a, const char *b) {
    if (!a || !b) {
        return 0;
    }
    while (*a && *b) {
        if (*a != *b) {
            return 0;
        }
        ++a;
        ++b;
    }
    return (*a == '\0' && *b == '\0') ? 1 : 0;
}

static uint8_t starts_with(const char *text, const char *prefix) {
    uint16_t i = 0;
    if (!text || !prefix) {
        return 0;
    }
    while (prefix[i]) {
        if (text[i] != prefix[i]) {
            return 0;
        }
        ++i;
    }
    return 1;
}

static uint8_t text_len(const char *text) {
    uint16_t len = 0;
    while (text && text[len]) {
        ++len;
    }
    return (uint8_t)len;
}

static void copy_name(char *dst, const char *src) {
    uint16_t i = 0;
    if (!dst) {
        return;
    }
    if (!src) {
        dst[0] = '\0';
        return;
    }

    while (src[i] && i + 1 < FS_NAME_LEN) {
        dst[i] = src[i];
        ++i;
    }
    dst[i] = '\0';
}

static void normalize_path(char *dst, const char *src) {
    uint16_t out = 0;
    uint16_t in = 0;

    if (!dst) {
        return;
    }
    if (!src || !src[0]) {
        dst[0] = '\0';
        return;
    }

    while (src[in] == '/') {
        ++in;
    }
    while (src[in] && out + 1 < FS_NAME_LEN) {
        dst[out++] = src[in++];
    }
    while (out > 0 && dst[out - 1] == '/') {
        --out;
    }
    dst[out] = '\0';
}

static uint8_t is_root_path(const char *path) {
    return (!path || !path[0]) ? 1u : 0u;
}

static const fs_virtual_file_t *find_virtual_file(const char *path) {
    char normalized[FS_NAME_LEN];
    const char *leaf;
    uint16_t prefix_len = (uint16_t)text_len(FS_SYSTEM_DIR);

    normalize_path(normalized, path);
    if (!starts_with(normalized, FS_SYSTEM_DIR) || normalized[prefix_len] != '/') {
        return 0;
    }

    leaf = normalized + prefix_len + 1;
    if (!leaf[0]) {
        return 0;
    }

    for (uint16_t i = 0; i < (uint16_t)(sizeof(system_files) / sizeof(system_files[0])); ++i) {
        if (streq(leaf, system_files[i].name)) {
            return &system_files[i];
        }
    }
    return 0;
}

static uint8_t is_reserved_path(const char *path) {
    char normalized[FS_NAME_LEN];
    uint16_t prefix_len = (uint16_t)text_len(FS_SYSTEM_DIR);
    normalize_path(normalized, path);
    if (streq(normalized, FS_SYSTEM_DIR)) {
        return 1;
    }
    if (starts_with(normalized, FS_SYSTEM_DIR) && normalized[prefix_len] == '/') {
        return 1;
    }
    return find_virtual_file(normalized) ? 1u : 0u;
}

static int find_entry(const char *name) {
    for (int i = 0; i < FS_MAX_FILES; ++i) {
        if (entries[i].used && streq(entries[i].name, name)) {
            return i;
        }
    }
    return -1;
}

static uint8_t child_from_entry_name(const char *entry_name, const char *dir_path, char *child_out, uint8_t *is_dir_out) {
    const char *remainder = 0;
    uint16_t i = 0;
    uint16_t prefix_len = 0;

    if (!entry_name || !child_out) {
        return 0;
    }

    if (is_root_path(dir_path)) {
        remainder = entry_name;
    } else {
        prefix_len = (uint16_t)text_len(dir_path);
        if (!starts_with(entry_name, dir_path) || entry_name[prefix_len] != '/') {
            return 0;
        }
        remainder = entry_name + prefix_len + 1;
    }

    if (!remainder[0]) {
        return 0;
    }

    while (remainder[i] && remainder[i] != '/' && i + 1 < FS_NAME_LEN) {
        child_out[i] = remainder[i];
        ++i;
    }
    child_out[i] = '\0';
    if (is_dir_out) {
        *is_dir_out = (remainder[i] == '/') ? 1u : 0u;
    }
    return i > 0 ? 1u : 0u;
}

static uint8_t listing_contains(const fs_list_item_t *items, uint16_t count, const char *name) {
    for (uint16_t i = 0; i < count; ++i) {
        if (streq(items[i].name, name)) {
            return 1;
        }
    }
    return 0;
}

static void build_dir_marker_path(char *dst, const char *dir_path) {
    uint16_t len = 0;
    if (!dst) {
        return;
    }

    normalize_path(dst, dir_path);
    while (dst[len]) {
        ++len;
    }
    if (len + 1 < FS_NAME_LEN) {
        dst[len++] = '/';
        dst[len] = '\0';
    }
    copy_name(dst + len, FS_DIR_MARKER_NAME);
}

static uint8_t path_has_child_prefix(const char *path) {
    char prefix[FS_NAME_LEN];
    uint16_t len = 0;

    normalize_path(prefix, path);
    while (prefix[len]) {
        ++len;
    }
    if (len + 1 < FS_NAME_LEN) {
        prefix[len++] = '/';
        prefix[len] = '\0';
    }

    for (uint16_t i = 0; i < FS_MAX_FILES; ++i) {
        if (!entries[i].used) {
            continue;
        }
        if (starts_with(entries[i].name, prefix)) {
            return 1;
        }
    }
    return 0;
}

static uint8_t replace_path_prefix(char *dst, const char *src, const char *old_prefix, const char *new_prefix) {
    uint16_t src_pos = 0;
    uint16_t dst_pos = 0;
    uint16_t old_len = (uint16_t)text_len(old_prefix);
    uint16_t new_len = (uint16_t)text_len(new_prefix);

    if (!dst || !src || !old_prefix || !new_prefix) {
        return 0;
    }
    if (!starts_with(src, old_prefix)) {
        return 0;
    }
    if (old_len + 1 < (uint16_t)FS_NAME_LEN && src[old_len] && src[old_len] != '/') {
        return 0;
    }
    if (new_len + 1 >= (uint16_t)FS_NAME_LEN) {
        return 0;
    }

    while (new_prefix[dst_pos] && dst_pos + 1 < FS_NAME_LEN) {
        dst[dst_pos] = new_prefix[dst_pos];
        ++dst_pos;
    }
    src_pos = old_len;
    while (src[src_pos] && dst_pos + 1 < FS_NAME_LEN) {
        dst[dst_pos++] = src[src_pos++];
    }
    dst[dst_pos] = '\0';
    return src[src_pos] == '\0' ? 1u : 0u;
}

static uint8_t directory_exists(const char *path) {
    char normalized[FS_NAME_LEN];
    char marker[FS_NAME_LEN];

    normalize_path(normalized, path);
    if (is_root_path(normalized) || streq(normalized, FS_SYSTEM_DIR)) {
        return 1;
    }
    if (!normalized[0]) {
        return 0;
    }

    build_dir_marker_path(marker, normalized);
    if (find_entry(marker) >= 0) {
        return 1;
    }
    return path_has_child_prefix(normalized);
}

static uint8_t parent_directory_exists(const char *path) {
    char parent[FS_NAME_LEN];
    uint16_t len = 0;

    normalize_path(parent, path);
    while (parent[len]) {
        ++len;
    }
    while (len > 0 && parent[len - 1] != '/') {
        --len;
    }
    if (len == 0) {
        return 1;
    }
    parent[len - 1] = '\0';
    return directory_exists(parent);
}

static uint16_t build_listing(const char *dir_path, fs_list_item_t *items, uint16_t max_items) {
    char normalized[FS_NAME_LEN];
    uint16_t count = 0;

    normalize_path(normalized, dir_path);

    if (is_root_path(normalized)) {
        if (items && count < max_items) {
            copy_name(items[count].name, FS_SYSTEM_DIR);
            items[count].size = 0;
            items[count].kind = FS_ENTRY_KIND_DIR;
        }
        ++count;
    }

    if (streq(normalized, FS_SYSTEM_DIR)) {
        for (uint16_t i = 0; i < (uint16_t)(sizeof(system_files) / sizeof(system_files[0])); ++i) {
            if (items && count < max_items) {
                copy_name(items[count].name, system_files[i].name);
                items[count].size = system_files[i].size;
                items[count].kind = FS_ENTRY_KIND_FILE;
            }
            ++count;
        }
        return count;
    }

    for (uint16_t pass = 0; pass < 2; ++pass) {
        for (uint16_t i = 0; i < FS_MAX_FILES; ++i) {
            char child[FS_NAME_LEN];
            uint8_t is_dir = 0;

            if (!entries[i].used) {
                continue;
            }
            if (!child_from_entry_name(entries[i].name, normalized, child, &is_dir)) {
                continue;
            }
            if ((pass == 0 && !is_dir) || (pass == 1 && is_dir)) {
                continue;
            }
            if (!is_dir && streq(child, FS_DIR_MARKER_NAME)) {
                continue;
            }
            if (items && listing_contains(items, count < max_items ? count : max_items, child)) {
                continue;
            }

            if (items && count < max_items) {
                copy_name(items[count].name, child);
                items[count].size = is_dir ? 0 : entries[i].size;
                items[count].kind = is_dir ? FS_ENTRY_KIND_DIR : FS_ENTRY_KIND_FILE;
            }
            ++count;
        }
    }

    return count;
}

static int find_free_entry(void) {
    for (int i = 0; i < FS_MAX_FILES; ++i) {
        if (!entries[i].used) {
            return i;
        }
    }
    return -1;
}

static void clear_blob(void) {
    for (uint16_t i = 0; i < (uint16_t)sizeof(disk_blob); ++i) {
        disk_blob[i] = 0;
    }
}

static uint8_t write_u16(uint8_t *blob, uint16_t size, uint16_t *pos, uint16_t value) {
    if (!blob || !pos || (uint16_t)(*pos + 2) > size) {
        return 0;
    }
    blob[*pos] = (uint8_t)(value & 0xFFu);
    blob[*pos + 1] = (uint8_t)((value >> 8) & 0xFFu);
    *pos = (uint16_t)(*pos + 2);
    return 1;
}

static uint8_t write_u32(uint8_t *blob, uint16_t size, uint16_t *pos, uint32_t value) {
    if (!blob || !pos || (uint16_t)(*pos + 4) > size) {
        return 0;
    }
    blob[*pos] = (uint8_t)(value & 0xFFu);
    blob[*pos + 1] = (uint8_t)((value >> 8) & 0xFFu);
    blob[*pos + 2] = (uint8_t)((value >> 16) & 0xFFu);
    blob[*pos + 3] = (uint8_t)((value >> 24) & 0xFFu);
    *pos = (uint16_t)(*pos + 4);
    return 1;
}

static uint8_t read_u16(const uint8_t *blob, uint16_t size, uint16_t *pos, uint16_t *out) {
    if (!blob || !pos || !out || (uint16_t)(*pos + 2) > size) {
        return 0;
    }
    *out = (uint16_t)(blob[*pos] | ((uint16_t)blob[*pos + 1] << 8));
    *pos = (uint16_t)(*pos + 2);
    return 1;
}

static uint8_t read_u32(const uint8_t *blob, uint16_t size, uint16_t *pos, uint32_t *out) {
    if (!blob || !pos || !out || (uint16_t)(*pos + 4) > size) {
        return 0;
    }
    *out = (uint32_t)blob[*pos]
         | ((uint32_t)blob[*pos + 1] << 8)
         | ((uint32_t)blob[*pos + 2] << 16)
         | ((uint32_t)blob[*pos + 3] << 24);
    *pos = (uint16_t)(*pos + 4);
    return 1;
}

static uint8_t fs_flush_to_disk(void) {
    uint16_t pos = 0;
    uint16_t used_files = 0;

    clear_blob();

    for (uint16_t i = 0; i < FS_MAX_FILES; ++i) {
        if (entries[i].used) {
            used_files++;
        }
    }

    if (!write_u32(disk_blob, (uint16_t)sizeof(disk_blob), &pos, FS_DISK_MAGIC)) {
        return 0;
    }
    if (!write_u16(disk_blob, (uint16_t)sizeof(disk_blob), &pos, FS_DISK_VERSION)) {
        return 0;
    }
    if (!write_u16(disk_blob, (uint16_t)sizeof(disk_blob), &pos, used_files)) {
        return 0;
    }

    for (uint16_t i = 0; i < FS_MAX_FILES; ++i) {
        if ((uint16_t)(pos + 1 + FS_NAME_LEN + 2 + FS_MAX_FILE_SIZE) > (uint16_t)sizeof(disk_blob)) {
            return 0;
        }

        disk_blob[pos++] = entries[i].used;
        for (uint16_t n = 0; n < FS_NAME_LEN; ++n) {
            disk_blob[pos++] = (uint8_t)entries[i].name[n];
        }
        disk_blob[pos++] = (uint8_t)(entries[i].size & 0xFFu);
        disk_blob[pos++] = (uint8_t)((entries[i].size >> 8) & 0xFFu);
        for (uint16_t d = 0; d < FS_MAX_FILE_SIZE; ++d) {
            disk_blob[pos++] = entries[i].data[d];
        }
    }

    for (uint16_t s = 0; s < FS_DISK_SECTORS; ++s) {
        if (!disk_write_sector((uint32_t)(FS_DISK_START_LBA + s), &disk_blob[s * FS_SECTOR_SIZE])) {
            return 0;
        }
    }
    return 1;
}

static uint8_t fs_load_from_disk(void) {
    uint16_t pos = 0;
    uint32_t magic = 0;
    uint16_t version = 0;
    uint16_t file_count = 0;
    (void)file_count;

    for (uint16_t s = 0; s < FS_DISK_SECTORS; ++s) {
        if (!disk_read_sector((uint32_t)(FS_DISK_START_LBA + s), &disk_blob[s * FS_SECTOR_SIZE])) {
            return 0;
        }
    }

    if (!read_u32(disk_blob, (uint16_t)sizeof(disk_blob), &pos, &magic)) {
        return 0;
    }
    if (!read_u16(disk_blob, (uint16_t)sizeof(disk_blob), &pos, &version)) {
        return 0;
    }
    if (!read_u16(disk_blob, (uint16_t)sizeof(disk_blob), &pos, &file_count)) {
        return 0;
    }

    if (magic != FS_DISK_MAGIC || (version != 1u && version != FS_DISK_VERSION)) {
        return 0;
    }

    for (uint16_t i = 0; i < FS_MAX_FILES; ++i) {
        uint16_t name_len = (version == 1u) ? FS_LEGACY_NAME_LEN : FS_NAME_LEN;
        if ((uint16_t)(pos + 1 + name_len + 2 + FS_MAX_FILE_SIZE) > (uint16_t)sizeof(disk_blob)) {
            return 0;
        }

        entries[i].used = disk_blob[pos++];
        for (uint16_t n = 0; n < name_len; ++n) {
            entries[i].name[n] = (char)disk_blob[pos++];
        }
        for (uint16_t n = name_len; n < FS_NAME_LEN; ++n) {
            entries[i].name[n] = '\0';
        }
        entries[i].name[FS_NAME_LEN - 1] = '\0';

        entries[i].size = (uint16_t)disk_blob[pos] | ((uint16_t)disk_blob[pos + 1] << 8);
        pos = (uint16_t)(pos + 2);
        if (entries[i].size > FS_MAX_FILE_SIZE) {
            entries[i].size = FS_MAX_FILE_SIZE;
        }

        for (uint16_t d = 0; d < FS_MAX_FILE_SIZE; ++d) {
            entries[i].data[d] = disk_blob[pos++];
        }
    }

    return 1;
}

static void fs_try_enable_persistence(uint8_t allow_init_write) {
    if (disk_persistence_enabled) {
        return;
    }

    // Read/list paths probe only once to avoid repeated long timeouts.
    // Write paths are allowed to retry so an early transient probe failure
    // does not permanently lock the filesystem in RAM-only mode.
    if (disk_probe_done && !allow_init_write) {
        return;
    }
    disk_probe_done = 1;

    if (fs_load_from_disk()) {
        disk_persistence_enabled = 1;
        return;
    }

    if (allow_init_write && fs_flush_to_disk()) {
        disk_persistence_enabled = 1;
        return;
    }

    disk_persistence_enabled = 0;
}

void fs_init(void) {
    for (int i = 0; i < FS_MAX_FILES; ++i) {
        entries[i].used = 0;
        entries[i].name[0] = '\0';
        entries[i].size = 0;
    }

    // Boot-safety fallback: do not touch disk during startup.
    disk_probe_done = 0;
    disk_persistence_enabled = 0;
    pending_open_valid = 0;
}

int fs_write_file(const char *name, const uint8_t *data, uint16_t size) {
    int idx;
    char normalized[FS_NAME_LEN];

    fs_try_enable_persistence(1);

    if (!name || !data) {
        return 0;
    }
    if (size > FS_MAX_FILE_SIZE) {
        return 0;
    }

    normalize_path(normalized, name);
    if (!normalized[0] || is_reserved_path(normalized)) {
        return 0;
    }

    idx = find_entry(normalized);
    if (idx < 0) {
        idx = find_free_entry();
        if (idx < 0) {
            return 0;
        }
        entries[idx].used = 1;
        copy_name(entries[idx].name, normalized);
    }

    entries[idx].size = size;
    for (uint16_t i = 0; i < size; ++i) {
        entries[idx].data[i] = data[i];
    }

    if (!disk_persistence_enabled) {
        return 1;
    }

    return fs_flush_to_disk() ? 1 : 0;
}

int fs_read_file(const char *name, uint8_t *out, uint16_t max_size, uint16_t *out_size) {
    char normalized[FS_NAME_LEN];
    const fs_virtual_file_t *virtual_file;
    fs_try_enable_persistence(0);

    normalize_path(normalized, name);
    virtual_file = find_virtual_file(normalized);
    if (virtual_file) {
        if (!out || virtual_file->size > max_size) {
            return 0;
        }
        for (uint16_t i = 0; i < virtual_file->size; ++i) {
            out[i] = virtual_file->data[i];
        }
        if (out_size) {
            *out_size = virtual_file->size;
        }
        return 1;
    }

    int idx = find_entry(normalized);
    uint16_t size;

    if (out_size) {
        *out_size = 0;
    }

    if (idx < 0 || !out) {
        return 0;
    }

    size = entries[idx].size;
    if (size > max_size) {
        return 0;
    }

    for (uint16_t i = 0; i < size; ++i) {
        out[i] = entries[idx].data[i];
    }

    if (out_size) {
        *out_size = size;
    }
    return 1;
}

int fs_delete_file(const char *name) {
    int idx;
    char normalized[FS_NAME_LEN];

    fs_try_enable_persistence(1);

    if (!name || !name[0]) {
        return 0;
    }

    normalize_path(normalized, name);
    if (is_reserved_path(normalized)) {
        return 0;
    }

    idx = find_entry(normalized);
    if (idx < 0) {
        return 0;
    }

    entries[idx].used = 0;
    entries[idx].name[0] = '\0';
    entries[idx].size = 0;
    for (uint16_t i = 0; i < FS_MAX_FILE_SIZE; ++i) {
        entries[idx].data[i] = 0;
    }

    if (!disk_persistence_enabled) {
        return 1;
    }

    return fs_flush_to_disk() ? 1 : 0;
}

int fs_rename_file(const char *old_name, const char *new_name) {
    int idx;
    char normalized_old[FS_NAME_LEN];
    char normalized_new[FS_NAME_LEN];

    fs_try_enable_persistence(1);

    if (!old_name || !old_name[0] || !new_name || !new_name[0]) {
        return 0;
    }

    normalize_path(normalized_old, old_name);
    normalize_path(normalized_new, new_name);
    if (!normalized_old[0] || !normalized_new[0] || is_reserved_path(normalized_old) || is_reserved_path(normalized_new)) {
        return 0;
    }

    idx = find_entry(normalized_old);
    if (idx < 0) {
        return 0;
    }

    if (!streq(normalized_old, normalized_new) && find_entry(normalized_new) >= 0) {
        return 0;
    }

    copy_name(entries[idx].name, normalized_new);

    if (!disk_persistence_enabled) {
        return 1;
    }

    return fs_flush_to_disk() ? 1 : 0;
}

int fs_create_dir(const char *path) {
    int idx;
    char normalized[FS_NAME_LEN];
    char marker[FS_NAME_LEN];

    fs_try_enable_persistence(1);

    if (!path || !path[0]) {
        return 0;
    }

    normalize_path(normalized, path);
    if (!normalized[0] || is_reserved_path(normalized) || find_entry(normalized) >= 0) {
        return 0;
    }
    if (directory_exists(normalized) || !parent_directory_exists(normalized)) {
        return 0;
    }

    build_dir_marker_path(marker, normalized);
    idx = find_free_entry();
    if (idx < 0) {
        return 0;
    }

    entries[idx].used = 1;
    copy_name(entries[idx].name, marker);
    entries[idx].size = 0;
    for (uint16_t i = 0; i < FS_MAX_FILE_SIZE; ++i) {
        entries[idx].data[i] = 0;
    }

    if (!disk_persistence_enabled) {
        return 1;
    }
    return fs_flush_to_disk() ? 1 : 0;
}

int fs_delete_dir(const char *path) {
    char normalized[FS_NAME_LEN];
    char marker[FS_NAME_LEN];
    int marker_idx;

    fs_try_enable_persistence(1);

    if (!path || !path[0]) {
        return 0;
    }

    normalize_path(normalized, path);
    if (!normalized[0] || is_root_path(normalized) || is_reserved_path(normalized)) {
        return 0;
    }

    build_dir_marker_path(marker, normalized);
    marker_idx = find_entry(marker);

    for (uint16_t i = 0; i < FS_MAX_FILES; ++i) {
        if (!entries[i].used) {
            continue;
        }
        if (starts_with(entries[i].name, marker) && streq(entries[i].name, marker)) {
            continue;
        }
        if (starts_with(entries[i].name, normalized) && entries[i].name[text_len(normalized)] == '/') {
            return 0;
        }
    }

    if (marker_idx < 0) {
        return 0;
    }

    entries[marker_idx].used = 0;
    entries[marker_idx].name[0] = '\0';
    entries[marker_idx].size = 0;
    for (uint16_t i = 0; i < FS_MAX_FILE_SIZE; ++i) {
        entries[marker_idx].data[i] = 0;
    }

    if (!disk_persistence_enabled) {
        return 1;
    }
    return fs_flush_to_disk() ? 1 : 0;
}

int fs_rename_dir(const char *old_path, const char *new_path) {
    char normalized_old[FS_NAME_LEN];
    char normalized_new[FS_NAME_LEN];
    uint8_t touched = 0;

    fs_try_enable_persistence(1);

    if (!old_path || !old_path[0] || !new_path || !new_path[0]) {
        return 0;
    }

    normalize_path(normalized_old, old_path);
    normalize_path(normalized_new, new_path);
    if (!normalized_old[0] || !normalized_new[0]) {
        return 0;
    }
    if (is_root_path(normalized_old) || is_root_path(normalized_new)) {
        return 0;
    }
    if (is_reserved_path(normalized_old) || is_reserved_path(normalized_new)) {
        return 0;
    }
    if (!directory_exists(normalized_old) || directory_exists(normalized_new) || find_entry(normalized_new) >= 0 || !parent_directory_exists(normalized_new)) {
        return 0;
    }

    for (uint16_t i = 0; i < FS_MAX_FILES; ++i) {
        char renamed[FS_NAME_LEN];
        if (!entries[i].used) {
            continue;
        }
        if (!replace_path_prefix(renamed, entries[i].name, normalized_old, normalized_new)) {
            continue;
        }
        touched = 1;
        copy_name(entries[i].name, renamed);
    }

    if (!touched) {
        return 0;
    }

    if (!disk_persistence_enabled) {
        return 1;
    }
    return fs_flush_to_disk() ? 1 : 0;
}

int fs_persistence_active(void) {
    return disk_persistence_enabled ? 1 : 0;
}

uint16_t fs_list_count(const char *dir_path) {
    fs_try_enable_persistence(0);
    return build_listing(dir_path, 0, 0);
}

int fs_list_entry(const char *dir_path, uint16_t visible_index, char *name_out, uint16_t name_out_size, uint16_t *size_out, uint8_t *kind_out) {
    fs_list_item_t items[FS_MAX_FILES + 8];
    uint16_t count;

    fs_try_enable_persistence(0);

    if (!name_out || name_out_size == 0) {
        return 0;
    }

    count = build_listing(dir_path, items, (uint16_t)(sizeof(items) / sizeof(items[0])));
    if (visible_index >= count) {
        return 0;
    }

    copy_name(name_out, items[visible_index].name);
    if (size_out) {
        *size_out = items[visible_index].size;
    }
    if (kind_out) {
        *kind_out = items[visible_index].kind;
    }
    return 1;
}

int fs_path_is_read_only(const char *path) {
    char normalized[FS_NAME_LEN];
    normalize_path(normalized, path);
    return is_reserved_path(normalized) ? 1 : 0;
}

uint16_t fs_file_count(void) {
    fs_try_enable_persistence(0);

    uint16_t count = 0;
    for (uint16_t i = 0; i < FS_MAX_FILES; ++i) {
        if (entries[i].used) {
            count++;
        }
    }
    return count;
}

int fs_file_info(uint16_t visible_index, char *name_out, uint16_t name_out_size, uint16_t *size_out) {
    fs_try_enable_persistence(0);

    uint16_t cursor = 0;

    if (!name_out || name_out_size == 0) {
        return 0;
    }

    for (uint16_t i = 0; i < FS_MAX_FILES; ++i) {
        if (!entries[i].used) {
            continue;
        }

        if (cursor == visible_index) {
            uint16_t n = 0;
            while (entries[i].name[n] && n + 1 < name_out_size) {
                name_out[n] = entries[i].name[n];
                n++;
            }
            name_out[n] = '\0';
            if (size_out) {
                *size_out = entries[i].size;
            }
            return 1;
        }
        cursor++;
    }

    return 0;
}

void fs_request_open_file(const char *name) {
    if (!name || !name[0]) {
        pending_open_valid = 0;
        pending_open_name[0] = '\0';
        return;
    }

    normalize_path(pending_open_name, name);
    pending_open_valid = 1;
}

int fs_take_open_request(char *name_out, uint16_t name_out_size) {
    uint16_t i = 0;

    if (!name_out || name_out_size == 0 || !pending_open_valid) {
        return 0;
    }

    while (pending_open_name[i] && i + 1 < name_out_size) {
        name_out[i] = pending_open_name[i];
        i++;
    }
    name_out[i] = '\0';

    pending_open_valid = 0;
    pending_open_name[0] = '\0';
    return 1;
}
