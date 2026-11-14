# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "Release")
  file(REMOVE_RECURSE
  "CMakeFiles/deezer-desktop_autogen.dir/AutogenUsed.txt"
  "CMakeFiles/deezer-desktop_autogen.dir/ParseCache.txt"
  "deezer-desktop_autogen"
  )
endif()
