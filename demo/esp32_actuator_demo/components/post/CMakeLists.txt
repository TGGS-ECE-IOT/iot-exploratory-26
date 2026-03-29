idf_component_register(
    SRCS "post.c"
    INCLUDE_DIRS "."
    REQUIRES led buzzer relay servo button oled freertos
)
