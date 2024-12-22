cmake_minimum_required(VERSION 3.10)
project(gbm-hybris VERSION 1.0 LANGUAGES C)
set(LIBRARY_NAME gbm)
set(SOURCE_FILES main.c)
include_directories(/usr/include/android/)
link_directories(/usr/lib/aarch64-linux-gnu)
add_library(${LIBRARY_NAME} SHARED ${SOURCE_FILES})
target_link_libraries(${LIBRARY_NAME} PRIVATE gralloc)
set_target_properties(${LIBRARY_NAME} PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
    OUTPUT_NAME "${LIBRARY_NAME}"
)
install(TARGETS ${LIBRARY_NAME}
    LIBRARY DESTINATION /usr/lib/aarch64-linux-gnu
)
