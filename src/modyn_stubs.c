#include <stdlib.h>
#include <string.h>
#include "modyn.h"

// 简易的多模态转换实现（示例）
MODYN_API modyn_status_e modyn_convert_to_tensor(modyn_data_modality_e modality, void *data,
                                                modyn_tensor_data_t **tensors, size_t *num_tensors) {
  if (!tensors || !num_tensors) return MODYN_ERROR_INVALID_ARGUMENT;
  *num_tensors = 1;
  *tensors = (modyn_tensor_data_t*)malloc(sizeof(modyn_tensor_data_t));
  if (!*tensors) return MODYN_ERROR_MEMORY_ALLOCATION;
  modyn_tensor_data_t *t = &(*tensors)[0];
  memset(t, 0, sizeof(*t));

  // 简化：根据模态设置一个固定形状，并尝试复制输入数据（若有）
  switch (modality) {
    case MODYN_MODALITY_IMAGE: {
      t->shape.num_dims = 3; t->shape.dims[0]=3; t->shape.dims[1]=224; t->shape.dims[2]=224;
      t->dtype = MODYN_DATA_TYPE_UINT8;
      t->size = MODYN_TENSOR_SIZE_BYTES(&t->shape, t->dtype);
      t->data = malloc(t->size);
      if (!t->data) return MODYN_ERROR_MEMORY_ALLOCATION;
      if (data) memcpy(t->data, data, t->size > 0 ? t->size : 0);
      break;
    }
    case MODYN_MODALITY_AUDIO: {
      t->shape.num_dims = 2; t->shape.dims[0]=1; t->shape.dims[1]=16000;
      t->dtype = MODYN_DATA_TYPE_INT16;
      t->size = MODYN_TENSOR_SIZE_BYTES(&t->shape, t->dtype);
      t->data = malloc(t->size);
      if (!t->data) return MODYN_ERROR_MEMORY_ALLOCATION;
      if (data) memcpy(t->data, data, t->size);
      break;
    }
    case MODYN_MODALITY_TEXT: {
      // 文本：简单映射为长度 128 的 uint8 序列
      t->shape.num_dims = 1; t->shape.dims[0]=128;
      t->dtype = MODYN_DATA_TYPE_UINT8;
      t->size = MODYN_TENSOR_SIZE_BYTES(&t->shape, t->dtype);
      t->data = malloc(t->size);
      if (!t->data) return MODYN_ERROR_MEMORY_ALLOCATION;
      memset(t->data, 0, t->size);
      if (data) {
        const char *s = (const char*)data;
        size_t len = strlen(s);
        if (len > 128) len = 128;
        memcpy(t->data, s, len);
      }
      break;
    }
    default: {
      // 其它模态返回空张量
      t->shape.num_dims = 1; t->shape.dims[0]=1;
      t->dtype = MODYN_DATA_TYPE_UINT8;
      t->size = 1; t->data = malloc(1); if (!t->data) return MODYN_ERROR_MEMORY_ALLOCATION;
      ((unsigned char*)t->data)[0]=0;
      break;
    }
  }
  t->mem_type = MODYN_MEMORY_INTERNAL;
  return MODYN_SUCCESS;
}

// Pipeline functions are now implemented in pipeline.c

// ===== 模型实例克隆（最小 stub） ===== //
typedef struct {
  int is_clone;
  int clone_count;
} modyn_clone_meta_t;

static modyn_clone_meta_t g_clone_meta_original = { .is_clone = 0, .clone_count = 0 };
static modyn_clone_meta_t g_clone_meta_clone     = { .is_clone = 1, .clone_count = 0 };

MODYN_API modyn_status_e modyn_check_clone_support(modyn_model_instance_handle_t instance,
                                                  int *supports_clone) {
  (void)instance; if (!supports_clone) return MODYN_ERROR_INVALID_ARGUMENT;
  *supports_clone = 1; return MODYN_SUCCESS;
}

MODYN_API modyn_status_e modyn_clone_model_instance(modyn_model_instance_handle_t source_instance,
                                                   const modyn_clone_config_t *config,
                                                   modyn_model_instance_handle_t *cloned_instance) {
  (void)config; if (!cloned_instance) return MODYN_ERROR_INVALID_ARGUMENT;
  // 简化：任意 source_instance 视为原始实例
  g_clone_meta_original.clone_count++;
  *cloned_instance = (modyn_model_instance_handle_t)&g_clone_meta_clone;
  return MODYN_SUCCESS;
}

MODYN_API modyn_status_e modyn_get_clone_info(modyn_model_instance_handle_t instance,
                                             int *is_cloned,
                                             int *clone_count) {
  if (!is_cloned || !clone_count) return MODYN_ERROR_INVALID_ARGUMENT;
  if (instance == (modyn_model_instance_handle_t)&g_clone_meta_clone) {
    *is_cloned = 1; *clone_count = 0; return MODYN_SUCCESS;
  }
  *is_cloned = 0; *clone_count = g_clone_meta_original.clone_count; return MODYN_SUCCESS;
}

// ===== 零拷贝缓冲池（最小 stub） ===== //
typedef struct {
  modyn_zero_copy_memory_region_t inputs[4];
  modyn_zero_copy_memory_region_t outputs[4];
  size_t num_inputs;
  size_t num_outputs;
} modyn_stub_pool_t;

MODYN_API modyn_status_e modyn_create_zero_copy_buffer_pool(modyn_model_instance_handle_t instance,
                                                           modyn_zero_copy_buffer_pool_t **pool) {
  (void)instance; if (!pool) return MODYN_ERROR_INVALID_ARGUMENT;
  modyn_stub_pool_t *p = (modyn_stub_pool_t*)malloc(sizeof(modyn_stub_pool_t));
  if (!p) return MODYN_ERROR_MEMORY_ALLOCATION;
  memset(p, 0, sizeof(*p)); p->num_inputs = 1; p->num_outputs = 1;
  *pool = (modyn_zero_copy_buffer_pool_t*)p; return MODYN_SUCCESS;
}

MODYN_API modyn_status_e modyn_destroy_zero_copy_buffer_pool(modyn_zero_copy_buffer_pool_t *pool) {
  if (!pool) return MODYN_ERROR_INVALID_ARGUMENT; free(pool); return MODYN_SUCCESS;
}

MODYN_API modyn_status_e modyn_get_input_buffer_region(modyn_zero_copy_buffer_pool_t *pool,
                                                      size_t input_index,
                                                      modyn_zero_copy_memory_region_t **region) {
  if (!pool || !region) return MODYN_ERROR_INVALID_ARGUMENT;
  modyn_stub_pool_t *p = (modyn_stub_pool_t*)pool; if (input_index >= p->num_inputs) return MODYN_ERROR_INVALID_ARGUMENT;
  *region = &p->inputs[input_index]; return MODYN_SUCCESS;
}

MODYN_API modyn_status_e modyn_get_output_buffer_region(modyn_zero_copy_buffer_pool_t *pool,
                                                       size_t output_index,
                                                       modyn_zero_copy_memory_region_t **region) {
  if (!pool || !region) return MODYN_ERROR_INVALID_ARGUMENT;
  modyn_stub_pool_t *p = (modyn_stub_pool_t*)pool; if (output_index >= p->num_outputs) return MODYN_ERROR_INVALID_ARGUMENT;
  *region = &p->outputs[output_index]; return MODYN_SUCCESS;
}

MODYN_API modyn_status_e modyn_run_inference_zero_copy(modyn_model_instance_handle_t instance,
                                                      modyn_zero_copy_buffer_pool_t *pool) {
  (void)instance; (void)pool; return MODYN_SUCCESS;
}

MODYN_API modyn_status_e modyn_sync_buffer_to_device(modyn_zero_copy_memory_region_t *region) {
  (void)region; return MODYN_SUCCESS;
}

MODYN_API modyn_status_e modyn_sync_buffer_to_cpu(modyn_zero_copy_memory_region_t *region) {
  (void)region; return MODYN_SUCCESS;
}

MODYN_API modyn_status_e modyn_get_buffer_pool_info(modyn_zero_copy_buffer_pool_t *pool,
                                                   size_t *num_inputs,
                                                   size_t *num_outputs,
                                                   size_t *total_size) {
  if (!pool || !num_inputs || !num_outputs || !total_size) return MODYN_ERROR_INVALID_ARGUMENT;
  modyn_stub_pool_t *p = (modyn_stub_pool_t*)pool;
  *num_inputs = p->num_inputs; *num_outputs = p->num_outputs; *total_size = 0; return MODYN_SUCCESS;
}

MODYN_API modyn_status_e modyn_check_zero_copy_support(modyn_model_instance_handle_t instance,
                                                      int *supports_zero_copy) {
  (void)instance; if (!supports_zero_copy) return MODYN_ERROR_INVALID_ARGUMENT; *supports_zero_copy = 1; return MODYN_SUCCESS;
}

// -------- 内存池提供者注册（链表）与动态加载 --------
#include <dlfcn.h>
#include <dirent.h>
#include <stdio.h>

typedef struct mp_node_s { const modyn_mempool_ops_t *ops; struct mp_node_s *next; } mp_node_t;
static mp_node_t *g_mp_head = NULL; static size_t g_mp_count = 0;

MODYN_API modyn_status_e modyn_register_mempool_provider(const modyn_mempool_ops_t *ops) {
  if (!ops || !ops->create || !ops->destroy || !ops->name) return MODYN_ERROR_INVALID_ARGUMENT;
  mp_node_t *n = (mp_node_t*)malloc(sizeof(mp_node_t)); if (!n) return MODYN_ERROR_MEMORY_ALLOCATION;
  n->ops = ops; n->next = g_mp_head; g_mp_head = n; ++g_mp_count; return MODYN_SUCCESS;
}

MODYN_API modyn_status_e modyn_get_registered_mempool_count(size_t *count) { if (!count) return MODYN_ERROR_INVALID_ARGUMENT; *count = g_mp_count; return MODYN_SUCCESS; }

MODYN_API modyn_status_e modyn_list_registered_mempools(const modyn_mempool_ops_t *ops[], size_t max, size_t *out) {
  if (!out) return MODYN_ERROR_INVALID_ARGUMENT; size_t i=0; for (mp_node_t *n=g_mp_head; n && i<max; n=n->next){ if (ops) ops[i]=n->ops; ++i;} *out=i; return MODYN_SUCCESS;
}

MODYN_API modyn_status_e modyn_find_mempool_by_name(const char *name, const modyn_mempool_ops_t **ops) {
  if (!name || !ops) return MODYN_ERROR_INVALID_ARGUMENT; for (mp_node_t *n=g_mp_head; n; n=n->next){ if (n->ops && n->ops->name && strcmp(n->ops->name,name)==0){ *ops=n->ops; return MODYN_SUCCESS; }} return MODYN_ERROR_INVALID_ARGUMENT;
}

MODYN_API modyn_status_e modyn_load_mempool_from_file(const char *so_path) { if (!so_path) return MODYN_ERROR_INVALID_ARGUMENT; void *h=dlopen(so_path, RTLD_NOW|RTLD_GLOBAL); return h?MODYN_SUCCESS:MODYN_ERROR_INVALID_ARGUMENT; }

MODYN_API modyn_status_e modyn_load_mempools_from_directory(const char *dir_path) {
  if (!dir_path) return MODYN_ERROR_INVALID_ARGUMENT; DIR *d=opendir(dir_path); if(!d) return MODYN_ERROR_INVALID_ARGUMENT; struct dirent* e; char full[1024]; while((e=readdir(d))){ size_t len=strlen(e->d_name); if(len>3 && strcmp(e->d_name+(len-3), ".so")==0){ snprintf(full,sizeof(full), "%s/%s", dir_path, e->d_name); (void)modyn_load_mempool_from_file(full);} } closedir(d); return MODYN_SUCCESS;
}

// Pipeline functions are now implemented in pipeline.c


