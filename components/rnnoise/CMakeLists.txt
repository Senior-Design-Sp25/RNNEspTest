idf_component_register(
    SRCS 
        "celt_lpc.c"
        "denoise.c"
        "kiss_fft.c"
        "pitch.c"
        "rnn.c"
        "rnn_data.c"
    INCLUDE_DIRS 
        "include")

target_compile_options(${COMPONENT_LIB} PRIVATE 
    -O3
    -ffast-math
)