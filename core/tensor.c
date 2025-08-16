#include "core/tensor.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

tensor_t tensor_create(const char* name, tensor_data_type_e dtype, const tensor_shape_t* shape, tensor_format_e format) {
    tensor_t tensor = {0};
    
    if (name) {
        tensor.name = strdup(name);
    }
    
    tensor.dtype = dtype;
    tensor.shape = *shape;
    tensor.format = format;
    tensor.memory_type = TENSOR_MEMORY_CPU;
    tensor.size = tensor_get_element_count(&tensor) * tensor_get_dtype_size(dtype);
    tensor.data = NULL;
    tensor.owns_data = false;
    tensor.ref_count = 1;
    
    return tensor;
}

tensor_t tensor_from_data(const char* name, tensor_data_type_e dtype, const tensor_shape_t* shape, 
                       tensor_format_e format, void* data, size_t size, bool owns_data) {
    tensor_t tensor = tensor_create(name, dtype, shape, format);
    tensor.data = data;
    tensor.size = size;
    tensor.owns_data = owns_data;
    
    return tensor;
}

tensor_t tensor_copy(const tensor_t* src) {
    if (!src) {
        return (tensor_t){0};
    }
    
    tensor_t dst = tensor_create(src->name, src->dtype, &src->shape, src->format);
    dst.memory_type = src->memory_type;
    dst.size = src->size;
    
    if (src->data && src->size > 0) {
        dst.data = malloc(src->size);
        if (dst.data) {
            memcpy(dst.data, src->data, src->size);
            dst.owns_data = true;
        }
    }
    
    return dst;
}

void tensor_free(tensor_t* tensor) {
    if (!tensor) {
        return;
    }
    
    if (tensor->ref_count > 1) {
        tensor->ref_count--;
        return;
    }
    
    if (tensor->name) {
        free(tensor->name);
        tensor->name = NULL;
    }
    
    if (tensor->data && tensor->owns_data) {
        free(tensor->data);
    }
    
    memset(tensor, 0, sizeof(tensor_t));
}

uint32_t tensor_get_element_count(const tensor_t* tensor) {
    if (!tensor || tensor->shape.ndim == 0) return 0;
    
    uint32_t count = 1;
    for (uint32_t i = 0; i < tensor->shape.ndim; i++) {
        count *= tensor->shape.dims[i];
    }
    return count;
}

size_t tensor_get_dtype_size(tensor_data_type_e dtype) {
    switch (dtype) {
        case TENSOR_TYPE_FLOAT32: return 4;
        case TENSOR_TYPE_FLOAT64: return 8;
        case TENSOR_TYPE_FLOAT16: return 2;
        case TENSOR_TYPE_INT32: return 4;
        case TENSOR_TYPE_INT64: return 8;
        case TENSOR_TYPE_INT16: return 2;
        case TENSOR_TYPE_INT8: return 1;
        case TENSOR_TYPE_UINT8: return 1;
        case TENSOR_TYPE_BOOL: return 1;
        case TENSOR_TYPE_STRING: return sizeof(char*);
        default: return 0;
    }
}

int tensor_reshape(tensor_t* tensor, const tensor_shape_t* new_shape) {
    if (!tensor || !new_shape) return -1;
    
    // 检查新形状的元素数量是否匹配
    uint32_t old_count = tensor_get_element_count(tensor);
    uint32_t new_count = 1;
    for (uint32_t i = 0; i < new_shape->ndim; i++) {
        new_count *= new_shape->dims[i];
    }
    
    if (old_count != new_count) {
        return -1; // 元素数量不匹配
    }
    
    tensor->shape = *new_shape;
    return 0;
}

int tensor_convert_format(tensor_t* tensor, tensor_format_e new_format) {
    if (!tensor) return -1;
    printf("[DEBUG] tensor_convert_format: 当前format=%d, 目标format=%d, ndim=%u\n", tensor->format, new_format, tensor->shape.ndim);
    
    // 如果格式相同，直接返回成功
    if (tensor->format == new_format) {
        return 0;
    }
    
    // 简化实现：只支持NCHW和NHWC之间的转换
    if (tensor->format == TENSOR_FORMAT_NCHW && new_format == TENSOR_FORMAT_NHWC) {
        // TODO: 实现NCHW到NHWC的转换
        tensor->format = new_format;
        return 0;
    } else if (tensor->format == TENSOR_FORMAT_NHWC && new_format == TENSOR_FORMAT_NCHW) {
        // TODO: 实现NHWC到NCHW的转换
        tensor->format = new_format;
        return 0;
    }
    
    printf("[DEBUG] 不支持的格式转换\n");
    return -1; // 不支持的转换
}

void tensor_print_info(const tensor_t* tensor) {
    if (!tensor) {
        printf("Tensor: NULL\n");
        return;
    }
    
    printf("Tensor: %s\n", tensor->name ? tensor->name : "unnamed");
    printf("  Type: %d\n", tensor->dtype);
    printf("  Shape: [");
    for (uint32_t i = 0; i < tensor->shape.ndim; i++) {
        printf("%u", tensor->shape.dims[i]);
        if (i < tensor->shape.ndim - 1) printf(", ");
    }
    printf("]\n");
    printf("  Format: %d\n", tensor->format);
    printf("  Size: %zu bytes\n", tensor->size);
    printf("  Ref count: %u\n", tensor->ref_count);
}

tensor_shape_t tensor_shape_create(const uint32_t* dims, uint32_t ndim) {
    tensor_shape_t shape = {0};
    
    if (ndim > 8) {
        ndim = 8; // 限制最大维度
    }
    
    shape.ndim = ndim;
    for (uint32_t i = 0; i < ndim; i++) {
        shape.dims[i] = dims[i];
    }
    
    return shape;
}

bool tensor_shape_equal(const tensor_shape_t* shape1, const tensor_shape_t* shape2) {
    if (!shape1 || !shape2) {
        return false;
    }
    
    if (shape1->ndim != shape2->ndim) {
        return false;
    }
    
    for (uint32_t i = 0; i < shape1->ndim; i++) {
        if (shape1->dims[i] != shape2->dims[i]) {
            return false;
        }
    }
    
    return true;
}

tensor_t prepare_tensor_from_image(const char* image_path, const tensor_shape_t* target_shape, tensor_format_e format) {
    // 简化实现，返回虚拟张量
    // 实际实现需要使用 OpenCV 或其他图像处理库
    
    printf("从图像创建张量: %s\n", image_path);
    
    tensor_t tensor = tensor_create("image_input", TENSOR_TYPE_FLOAT32, target_shape, format);
    
    // 分配内存
    tensor.data = malloc(tensor.size);
    if (!tensor.data) {
        printf("内存分配失败\n");
        return tensor;
    }
    
    tensor.owns_data = true;
    
    // 填充虚拟数据
    float* data = (float*)tensor.data;
    uint32_t element_count = tensor_get_element_count(&tensor);
    
    for (uint32_t i = 0; i < element_count; i++) {
        data[i] = (float)(rand() % 256) / 255.0f; // 模拟归一化的图像数据
    }
    
    printf("虚拟图像数据创建完成\n");
    
    return tensor;
} 