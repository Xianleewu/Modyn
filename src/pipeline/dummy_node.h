#ifndef MODYN_DUMMY_NODE_H
#define MODYN_DUMMY_NODE_H

#include "modyn_pipeline.h"

#ifdef __cplusplus
extern "C" {
#endif

// ======================== Dummy Node Factory Functions ======================== //

/**
 * @brief Create a dummy preprocessing node
 * This is an example implementation showing how to create custom nodes
 *
 * @param name Node name
 * @param config_data Configuration data (can be NULL)
 * @param config_size Size of configuration data
 * @return modyn_pipeline_node_t* Node handle, or NULL on failure
 */
modyn_pipeline_node_t* modyn_create_dummy_preprocess_node(
  const char *name,
  const void *config_data,
  size_t config_size
);

/**
 * @brief Create a dummy postprocessing node
 *
 * @param name Node name
 * @param config_data Configuration data (can be NULL)
 * @param config_size Size of configuration data
 * @return modyn_pipeline_node_t* Node handle, or NULL on failure
 */
modyn_pipeline_node_t* modyn_create_dummy_postprocess_node(
  const char *name,
  const void *config_data,
  size_t config_size
);

/**
 * @brief Create a dummy conditional node
 *
 * @param name Node name
 * @param config_data Configuration data (can be NULL)
 * @param config_size Size of configuration data
 * @return modyn_pipeline_node_t* Node handle, or NULL on failure
 */
modyn_pipeline_node_t* modyn_create_dummy_conditional_node(
  const char *name,
  const void *config_data,
  size_t config_size
);

/**
 * @brief Create a dummy loop node
 *
 * @param name Node name
 * @param config_data Configuration data (can be NULL)
 * @param config_size Size of configuration data
 * @return modyn_pipeline_node_t* Node handle, or NULL on failure
 */
modyn_pipeline_node_t* modyn_create_dummy_loop_node(
  const char *name,
  const void *config_data,
  size_t config_size
);

/**
 * @brief Create a dummy model node
 *
 * @param name Node name
 * @param config_data Configuration data (can be NULL)
 * @param config_size Size of configuration data
 * @return modyn_pipeline_node_t* Node handle, or NULL on failure
 */
modyn_pipeline_node_t* modyn_create_dummy_model_node(
  const char *name,
  const void *config_data,
  size_t config_size
);

// ======================== Dummy Node Registration ======================== //

/**
 * @brief Register all dummy node types
 * This function registers all available dummy node types with the pipeline system
 *
 * @return modyn_status_e Operation status
 */
modyn_status_e modyn_register_dummy_node_types(void);

/**
 * @brief Unregister all dummy node types
 *
 * @return modyn_status_e Operation status
 */
modyn_status_e modyn_unregister_dummy_node_types(void);

#ifdef __cplusplus
}
#endif

#endif // MODYN_DUMMY_NODE_H
