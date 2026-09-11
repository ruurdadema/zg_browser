# CPM.cmake bootstrap -- downloads the real CPM.cmake on first configure.
# See https://github.com/cpm-cmake/CPM.cmake

set(CPM_DOWNLOAD_VERSION 0.43.1)

if(CPM_SOURCE_CACHE)
   set(CPM_DOWNLOAD_LOCATION "${CPM_SOURCE_CACHE}/cpm/CPM_${CPM_DOWNLOAD_VERSION}.cmake")
elseif(DEFINED ENV{CPM_SOURCE_CACHE})
   set(CPM_DOWNLOAD_LOCATION "$ENV{CPM_SOURCE_CACHE}/cpm/CPM_${CPM_DOWNLOAD_VERSION}.cmake")
else()
   set(CPM_DOWNLOAD_LOCATION "${CMAKE_BINARY_DIR}/cmake/CPM_${CPM_DOWNLOAD_VERSION}.cmake")
endif()

get_filename_component(CPM_DOWNLOAD_LOCATION "${CPM_DOWNLOAD_LOCATION}" ABSOLUTE)

if(NOT (EXISTS "${CPM_DOWNLOAD_LOCATION}"))
   message(STATUS "Downloading CPM.cmake v${CPM_DOWNLOAD_VERSION} to ${CPM_DOWNLOAD_LOCATION}")
   file(DOWNLOAD
      "https://github.com/cpm-cmake/CPM.cmake/releases/download/v${CPM_DOWNLOAD_VERSION}/CPM.cmake"
      "${CPM_DOWNLOAD_LOCATION}"
      STATUS CPM_DOWNLOAD_STATUS
      TLS_VERIFY ON)
   list(GET CPM_DOWNLOAD_STATUS 0 CPM_DOWNLOAD_RESULT)
   if(NOT CPM_DOWNLOAD_RESULT EQUAL 0)
      list(GET CPM_DOWNLOAD_STATUS 1 CPM_DOWNLOAD_ERROR)
      file(REMOVE "${CPM_DOWNLOAD_LOCATION}")
      message(FATAL_ERROR "Failed to download CPM.cmake: ${CPM_DOWNLOAD_ERROR}")
   endif()
endif()

include("${CPM_DOWNLOAD_LOCATION}")
