#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "libprojectM::projectM" for configuration "Release"
set_property(TARGET libprojectM::projectM APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(libprojectM::projectM PROPERTIES
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/libprojectM-4.so.4.2.0"
  IMPORTED_SONAME_RELEASE "libprojectM-4.so.4"
  )

list(APPEND _cmake_import_check_targets libprojectM::projectM )
list(APPEND _cmake_import_check_files_for_libprojectM::projectM "${_IMPORT_PREFIX}/lib/libprojectM-4.so.4.2.0" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
