idf_component_register( SRCS "coretask.c" 
                        INCLUDE_DIRS "." "../../include" 
                        REQUIRES ADBMS6830 statetask gpio
)