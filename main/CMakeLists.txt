file(GLOB_RECURSE SOURCES "*.cpp" "http/*.cpp" "wifi/*.cpp")

set(include_dirs "." "http" "wifi")

idf_component_register(SRCS ${SOURCES}
                    INCLUDE_DIRS ${include_dirs})