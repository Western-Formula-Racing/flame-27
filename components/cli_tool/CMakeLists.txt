idf_component_register( SRCS "cli_tool.c" 
                        INCLUDE_DIRS "." "../../include" 
                        REQUIRES ADBMS6830 statetask measuretask driver
)