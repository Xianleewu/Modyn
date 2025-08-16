PIPELINE_BUILD_START("my_tts") {
    PIPELINE_ADD_CUSTOM("preprocess", "文本预处理", my_preprocess, NULL);
    PIPELINE_ADD_MODEL("acoustic", QUICK_ONNX_MODEL("acoustic", "model.onnx"));
    PIPELINE_ADD_CUSTOM("postprocess", "音频优化", my_postprocess, NULL);
    
    PIPELINE_CONNECT("preprocess", "acoustic");
    PIPELINE_CONNECT("acoustic", "postprocess");
} PIPELINE_BUILD_END(); 