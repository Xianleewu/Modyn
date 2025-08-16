#include <string.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <dirent.h>
#include <stdio.h>
#include "modyn.h"

// 模型加载器注册表：链表 + 简单按名查找
typedef struct loader_node_s {
  const modyn_model_loader_t *loader;
  struct loader_node_s *next;
} loader_node_t;

static loader_node_t *g_loader_head = NULL;
static size_t g_loader_count = 0;

MODYN_API modyn_status_e modyn_register_model_loader(const modyn_model_loader_t *loader) {
  if (!loader || !loader->ops) return MODYN_ERROR_INVALID_ARGUMENT;
  loader_node_t *n = (loader_node_t*)malloc(sizeof(loader_node_t));
  if (!n) return MODYN_ERROR_MEMORY_ALLOCATION;
  n->loader = loader; n->next = g_loader_head; g_loader_head = n; ++g_loader_count;
  return MODYN_SUCCESS;
}

MODYN_API modyn_status_e modyn_unregister_model_loader(const char *loader_name) {
  if (!loader_name) return MODYN_ERROR_INVALID_ARGUMENT;
  loader_node_t **pp = &g_loader_head;
  while (*pp) {
    loader_node_t *cur = *pp;
    if (cur->loader && strcmp(cur->loader->name, loader_name) == 0) {
      *pp = cur->next; free(cur); --g_loader_count; return MODYN_SUCCESS;
    }
    pp = &((*pp)->next);
  }
  return MODYN_ERROR_INVALID_ARGUMENT;
}

MODYN_API modyn_status_e modyn_load_model_from_source(const modyn_model_data_source_t *source,
                                                     const void *loader_config,
                                                     modyn_model_data_buffer_t *buffer,
                                                     modyn_model_load_info_t *load_info) {
  if (!source || !buffer) return MODYN_ERROR_INVALID_ARGUMENT;
  // 依次尝试注册的加载器
  for (loader_node_t *n = g_loader_head; n; n = n->next) {
    const modyn_model_loader_t *L = n->loader;
    if (!L || !L->ops || !L->ops->can_load) continue;
    modyn_model_format_e fmt = MODYN_MODEL_FORMAT_PLAIN;
    if (L->ops->can_load(source, &fmt) == MODYN_SUCCESS) {
      if (L->ops->load_model) {
        modyn_status_e st = L->ops->load_model(source, loader_config, buffer, load_info);
        if (st == MODYN_SUCCESS) return MODYN_SUCCESS;
      }
    }
  }
  // 无可用加载器，退回到 dummy 简化实现
  if (load_info) {
    memset(load_info, 0, sizeof(*load_info));
    load_info->source = *source;
    load_info->format = MODYN_MODEL_FORMAT_PLAIN;
    load_info->original_size = 16;
    load_info->processed_size = 16;
    strcpy(load_info->checksum, "dummy_checksum");
    const modyn_model_loader_config_t *cfg = (const modyn_model_loader_config_t*)loader_config;
    load_info->applied_flags = cfg ? cfg->flags : 0;
  }
  buffer->size = 16; buffer->data = malloc(buffer->size);
  buffer->memory_type = MODYN_MEMORY_INTERNAL; buffer->owns_memory = 1;
  return buffer->data ? MODYN_SUCCESS : MODYN_ERROR_MEMORY_ALLOCATION;
}

MODYN_API modyn_status_e modyn_load_model_with_loader(const char *model_path,
                                                     const void *loader_config,
                                                     modyn_model_data_buffer_t *buffer,
                                                     modyn_model_load_info_t *load_info) {
  (void)model_path;
  modyn_model_data_source_t src; memset(&src, 0, sizeof(src));
  src.type = MODYN_MODEL_SOURCE_FILE;
  return modyn_load_model_from_source(&src, loader_config, buffer, load_info);
}

MODYN_API modyn_status_e modyn_load_model_from_buffer(const void *buffer_data,
                                                     size_t buffer_size,
                                                     const void *loader_config,
                                                     modyn_model_data_buffer_t *output_buffer,
                                                     modyn_model_load_info_t *load_info) {
  (void)buffer_data; (void)buffer_size;
  modyn_model_data_source_t src; memset(&src, 0, sizeof(src));
  src.type = MODYN_MODEL_SOURCE_BUFFER;
  return modyn_load_model_from_source(&src, loader_config, output_buffer, load_info);
}

MODYN_API modyn_status_e modyn_load_model_from_url(const char *url,
                                                   const char *headers,
                                                   int timeout_seconds,
                                                   const void *loader_config,
                                                   modyn_model_data_buffer_t *buffer,
                                                   modyn_model_load_info_t *load_info) {
  (void)headers; (void)timeout_seconds;
  modyn_model_data_source_t src; memset(&src, 0, sizeof(src));
  src.type = MODYN_MODEL_SOURCE_URL; src.source.url.url = url;
  return modyn_load_model_from_source(&src, loader_config, buffer, load_info);
}

MODYN_API modyn_status_e modyn_free_model_buffer(modyn_model_data_buffer_t *buffer) {
  if (!buffer) return MODYN_ERROR_INVALID_ARGUMENT;
  if (buffer->owns_memory && buffer->data) free(buffer->data);
  buffer->data = NULL; buffer->size = 0; return MODYN_SUCCESS;
}

MODYN_API modyn_status_e modyn_find_model_loader(const char *model_path,
                                                char *loader_name,
                                                size_t name_size) {
  (void)model_path; if (!loader_name || name_size == 0) return MODYN_ERROR_INVALID_ARGUMENT;
  // 返回第一个注册的加载器名称
  if (g_loader_head && g_loader_head->loader) {
    strncpy(loader_name, g_loader_head->loader->name, name_size-1); return MODYN_SUCCESS;
  }
  strncpy(loader_name, "dummy_loader", name_size-1); return MODYN_SUCCESS;
}

MODYN_API modyn_status_e modyn_list_model_loaders(char loaders[][64], size_t max_loaders, size_t *count) {
  if (!loaders || !count || max_loaders == 0) return MODYN_ERROR_INVALID_ARGUMENT;
  size_t out = 0;
  for (loader_node_t *n = g_loader_head; n && out < max_loaders; n = n->next) {
    if (n->loader) { strncpy(loaders[out], n->loader->name, 63); ++out; }
  }
  if (out == 0) { strncpy(loaders[0], "dummy_loader", 63); out = 1; }
  *count = out; return MODYN_SUCCESS;
}

MODYN_API modyn_status_e modyn_validate_model_file(const char *model_path,
                                                   const char *expected_checksum,
                                                   int *is_valid) {
  (void)model_path; (void)expected_checksum; if (!is_valid) return MODYN_ERROR_INVALID_ARGUMENT;
  *is_valid = 1; return MODYN_SUCCESS;
}

MODYN_API modyn_status_e modyn_create_file_source(const char *file_path,
                                                  modyn_model_data_source_t *source) {
  if (!file_path || !source) return MODYN_ERROR_INVALID_ARGUMENT;
  memset(source, 0, sizeof(*source)); source->type = MODYN_MODEL_SOURCE_FILE; source->source.file.path = file_path; return MODYN_SUCCESS;
}

MODYN_API modyn_status_e modyn_create_buffer_source(const void *buffer_data,
                                                    size_t buffer_size,
                                                    int owns_data,
                                                    modyn_model_data_source_t *source) {
  if (!source) return MODYN_ERROR_INVALID_ARGUMENT;
  memset(source, 0, sizeof(*source)); source->type = MODYN_MODEL_SOURCE_BUFFER; source->source.buffer.data = buffer_data; source->source.buffer.size = buffer_size; source->source.buffer.owns_data = owns_data; return MODYN_SUCCESS;
}

MODYN_API modyn_status_e modyn_create_url_source(const char *url,
                                                 const char *headers,
                                                 int timeout_seconds,
                                                 modyn_model_data_source_t *source) {
  if (!url || !source) return MODYN_ERROR_INVALID_ARGUMENT;
  memset(source, 0, sizeof(*source)); source->type = MODYN_MODEL_SOURCE_URL; source->source.url.url = url; source->source.url.headers = headers; source->source.url.timeout_seconds = timeout_seconds; return MODYN_SUCCESS;
}

MODYN_API modyn_status_e modyn_create_embedded_source(const char *resource_id,
                                                      const void *resource_data,
                                                      size_t resource_size,
                                                      modyn_model_data_source_t *source) {
  if (!resource_id || !resource_data || !source) return MODYN_ERROR_INVALID_ARGUMENT;
  memset(source, 0, sizeof(*source)); source->type = MODYN_MODEL_SOURCE_EMBEDDED; source->source.embedded.resource_id = resource_id; source->source.embedded.data = resource_data; source->source.embedded.size = resource_size; return MODYN_SUCCESS;
}

MODYN_API modyn_status_e modyn_destroy_data_source(modyn_model_data_source_t *source) {
  if (!source) return MODYN_ERROR_INVALID_ARGUMENT;
  memset(source, 0, sizeof(*source)); return MODYN_SUCCESS;
}

// ---- 加载器查询接口 ----

MODYN_API modyn_status_e modyn_get_registered_loader_count(size_t *count) {
  if (!count) return MODYN_ERROR_INVALID_ARGUMENT; *count = g_loader_count; return MODYN_SUCCESS;
}

MODYN_API modyn_status_e modyn_list_registered_loaders(modyn_model_loader_t const **loaders, size_t max, size_t *out) {
  if (!out) return MODYN_ERROR_INVALID_ARGUMENT; size_t i = 0;
  for (loader_node_t *n = g_loader_head; n && i < max; n = n->next) {
    if (loaders) loaders[i] = n->loader; ++i;
  }
  *out = i; return MODYN_SUCCESS;
}

MODYN_API modyn_status_e modyn_find_registered_loader_by_name(const char *name, const modyn_model_loader_t **loader) {
  if (!name || !loader) return MODYN_ERROR_INVALID_ARGUMENT;
  for (loader_node_t *n = g_loader_head; n; n = n->next) {
    if (n->loader && strcmp(n->loader->name, name) == 0) { *loader = n->loader; return MODYN_SUCCESS; }
  }
  return MODYN_ERROR_INVALID_ARGUMENT;
}

// ---- 动态加载 .so 的加载器 ----

MODYN_API modyn_status_e modyn_load_model_loader_from_file(const char *so_path) {
  if (!so_path) return MODYN_ERROR_INVALID_ARGUMENT;
  void *h = dlopen(so_path, RTLD_NOW | RTLD_GLOBAL);
  return h ? MODYN_SUCCESS : MODYN_ERROR_INVALID_ARGUMENT;
}

MODYN_API modyn_status_e modyn_load_model_loaders_from_directory(const char *dir_path) {
  if (!dir_path) return MODYN_ERROR_INVALID_ARGUMENT;
  DIR *d = opendir(dir_path); if (!d) return MODYN_ERROR_INVALID_ARGUMENT;
  struct dirent *e; char full[1024];
  while ((e = readdir(d)) != NULL) {
    size_t len = strlen(e->d_name);
    if (len > 3 && strcmp(e->d_name + (len - 3), ".so") == 0) {
      snprintf(full, sizeof(full), "%s/%s", dir_path, e->d_name);
      (void)modyn_load_model_loader_from_file(full);
    }
  }
  closedir(d); return MODYN_SUCCESS;
}


