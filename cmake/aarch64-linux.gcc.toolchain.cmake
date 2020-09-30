# set cross-compiled system type, it's better not use the type which cmake cannot recognized.
SET ( CMAKE_SYSTEM_NAME Linux )
SET ( CMAKE_SYSTEM_PROCESSOR aarch64 )

# if gcc/g++ was installed: 
SET( CROSS_COMPILE_TOOL aarch64-linux-gnu)
SET( CMAKE_C_COMPILER ${CROSS_COMPILE_TOOL}-gcc)
SET( CMAKE_CXX_COMPILER ${CROSS_COMPILE_TOOL}-g++)

# set searching rules
SET ( CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER )
SET ( CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY )
SET ( CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY )

# set compiler flags.
# note:
# -std=xxx is automatically set by CMAKE_CXX_STANDARD and compiler
# supprot will be checked since CMAKE_CXX_STANDARD_REQUIRED is set
#add_compile_options("-march=aarch64" "-Wall" "-Wno-unused-variable")
