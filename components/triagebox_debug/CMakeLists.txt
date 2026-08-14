# Debug console: frame injection + CPU/heap/stack stats. The whole component
# compiles to nothing unless CONFIG_TB_DEBUG_CONSOLE is set, so a production
# build cannot accidentally ship a way to fake patient vitals over USB.
if(CONFIG_TB_DEBUG_CONSOLE)
    set(srcs "tb_debug.c")
else()
    set(srcs "")
endif()

idf_component_register(
    SRCS ${srcs}
    INCLUDE_DIRS "include"
    REQUIRES
        console
        esp_timer
        triagebox_link
        triagebox_ml
)
