file(GLOB_RECURSE SOURCES "*.cpp" "http/*.cpp")

set(include_dirs "." "http")

idf_component_register(SRCS ${SOURCES}
                    INCLUDE_DIRS ${include_dirs})