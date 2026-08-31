#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "Poco::DataMySQL" for configuration "Release"
set_property(TARGET Poco::DataMySQL APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(Poco::DataMySQL PROPERTIES
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/libPocoDataMySQL.so.112"
  IMPORTED_SONAME_RELEASE "libPocoDataMySQL.so.112"
  )

list(APPEND _cmake_import_check_targets Poco::DataMySQL )
list(APPEND _cmake_import_check_files_for_Poco::DataMySQL "${_IMPORT_PREFIX}/lib/libPocoDataMySQL.so.112" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
