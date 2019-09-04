# Macros for SIP
# ~~~~~~~~~~~~~~
# Copyright (c) 2007, Simon Edwards <simon@simonzone.com>
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.
#
# SIP website: http://www.riverbankcomputing.co.uk/sip/index.php
#
# This file defines the following macros:
#
# ADD_SIP_PYTHON_MODULE (MODULE_NAME MODULE_SIP [library1, libaray2, ...])
#     Specifies a SIP file to be built into a Python module and installed.
#     MODULE_NAME is the name of Python module including any path name. (e.g.
#     os.sys, Foo.bar etc). MODULE_SIP the path and filename of the .sip file
#     to process and compile. libraryN are libraries that the Python module,
#     which is typically a shared library, should be linked to. The built
#     module will also be install into Python's site-packages directory.
#
# The behavior of the ADD_SIP_PYTHON_MODULE macro can be controlled by a
# number of variables:
#
# SIP_INCLUDES - List of directories which SIP will scan through when looking
#     for included .sip files. (Corresponds to the -I option for SIP.)
#
# SIP_TAGS - List of tags to define when running SIP. (Corresponds to the -t
#     option for SIP.)
#
# SIP_CONCAT_PARTS - An integer which defines the number of parts the C++ code
#     of each module should be split into. Defaults to 8. (Corresponds to the
#     -j option for SIP.)
#
# SIP_DISABLE_FEATURES - List of feature names which should be disabled
#     running SIP. (Corresponds to the -x option for SIP.)
#
# SIP_EXTRA_OPTIONS - Extra command line options which should be passed on to
#     SIP.

SET(SIP_INCLUDES)
SET(SIP_TAGS)
SET(SIP_CONCAT_PARTS 16)
SET(SIP_DISABLE_FEATURES)
SET(SIP_EXTRA_OPTIONS)
SET(SIP_EXTRA_OBJECTS)

MACRO(GENERATE_SIP_PYTHON_MODULE_CODE MODULE_NAME MODULE_SIP SIP_FILES CPP_FILES)
  STRING(REPLACE "." "/" _x ${MODULE_NAME})
  GET_FILENAME_COMPONENT(_parent_module_path ${_x} PATH)
  GET_FILENAME_COMPONENT(_child_module_name ${_x} NAME)

  GET_FILENAME_COMPONENT(_module_path ${MODULE_SIP} PATH)
  GET_FILENAME_COMPONENT(_abs_module_sip ${MODULE_SIP} ABSOLUTE)

  FILE(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/${_module_path})    # Output goes in this dir.

  # If this is not need anymore (using input configuration file for SIP files)
  # SIP could be run in the source rather than in binary directory
  SET(_configured_module_sip ${CMAKE_CURRENT_BINARY_DIR}/${_module_path}/${_module_path}.sip)
  FOREACH (_sip_file ${SIP_FILES})
    GET_FILENAME_COMPONENT(_sip_file_path ${_sip_file} PATH)
    GET_FILENAME_COMPONENT(_sip_file_name_we ${_sip_file} NAME_WE)
    FILE(RELATIVE_PATH _sip_file_relpath ${CMAKE_CURRENT_SOURCE_DIR} "${_sip_file_path}/${_sip_file_name_we}")
    SET(_out_sip_file "${CMAKE_CURRENT_BINARY_DIR}/${_sip_file_relpath}.sip")
    CONFIGURE_FILE(${_sip_file} ${_out_sip_file})
  ENDFOREACH (_sip_file)


  SET(_sip_includes)
  FOREACH (_inc ${SIP_INCLUDES})
    GET_FILENAME_COMPONENT(_abs_inc ${_inc} ABSOLUTE)
    LIST(APPEND _sip_includes -I ${_abs_inc})
  ENDFOREACH (_inc )

  SET(_sip_tags)
  FOREACH (_tag ${SIP_TAGS})
    LIST(APPEND _sip_tags -t ${_tag})
  ENDFOREACH (_tag)

  SET(_sip_x)
  FOREACH (_x ${SIP_DISABLE_FEATURES})
    LIST(APPEND _sip_x -x ${_x})
  ENDFOREACH (_x ${SIP_DISABLE_FEATURES})

  SET(_message "-DMESSAGE=Generating CPP code for module ${MODULE_NAME}")
  SET(_sip_output_files)
  FOREACH(CONCAT_NUM RANGE 0 ${SIP_CONCAT_PARTS} )
    IF( ${CONCAT_NUM} LESS ${SIP_CONCAT_PARTS} )
      SET(_sip_output_files ${_sip_output_files} ${CMAKE_CURRENT_BINARY_DIR}/${_module_path}/sip${_child_module_name}part${CONCAT_NUM}.cpp )
    ENDIF( ${CONCAT_NUM} LESS ${SIP_CONCAT_PARTS} )
  ENDFOREACH(CONCAT_NUM RANGE 0 ${SIP_CONCAT_PARTS} )

  # Suppress warnings
  IF(PEDANTIC)
    IF(MSVC)
      ADD_DEFINITIONS(
        /wd4189  # local variable is initialized but not referenced
        /wd4996  # deprecation warnings (bindings re-export deprecated methods)
        /wd4701  # potentially uninitialized variable used (sip generated code)
        /wd4702  # unreachable code (sip generated code)
        /wd4703  # potentially uninitialized local pointer variable 'sipType' used
      )
    ELSE(MSVC)
      # disable all warnings
      ADD_DEFINITIONS( -w -Wno-deprecated-declarations )
      IF(NOT APPLE)
        ADD_DEFINITIONS( -fpermissive )
      ENDIF(NOT APPLE)
    ENDIF(MSVC)
  ENDIF(PEDANTIC)

  IF(MSVC)
    ADD_DEFINITIONS( /bigobj )
  ENDIF(MSVC)

  SET(SIPCMD ${SIP_BINARY_PATH} ${_sip_tags} -w -e ${_sip_x} ${SIP_EXTRA_OPTIONS} -j ${SIP_CONCAT_PARTS} -c ${CMAKE_CURRENT_BINARY_DIR}/${_module_path} -I ${CMAKE_CURRENT_BINARY_DIR}/${_module_path} ${_sip_includes} ${_configured_module_sip})
  ADD_CUSTOM_COMMAND(
    OUTPUT ${_sip_output_files}
    COMMAND ${CMAKE_COMMAND} -E echo ${message}
    COMMAND ${CMAKE_COMMAND} -E touch ${_sip_output_files}
    COMMAND ${SIPCMD}
    MAIN_DEPENDENCY ${_configured_module_sip}
    DEPENDS ${SIP_EXTRA_FILES_DEPEND}
    VERBATIM
  )

  ADD_CUSTOM_TARGET(generate_sip_${MODULE_NAME}_cpp_files DEPENDS ${_sip_output_files})

  SET(CPP_FILES ${sip_output_files})
ENDMACRO(GENERATE_SIP_PYTHON_MODULE_CODE)

# Will compile and link the module
MACRO(BUILD_SIP_PYTHON_MODULE MODULE_NAME SIP_FILES EXTRA_OBJECTS)
  SET(EXTRA_LINK_LIBRARIES ${ARGN})

  # We give this target a long logical target name.
  # (This is to avoid having the library name clash with any already
  # install library names. If that happens then cmake dependency
  # tracking get confused.)
  STRING(REPLACE "." "_" _logical_name ${MODULE_NAME})
  SET(_logical_name "python_module_${_logical_name}")

  ADD_LIBRARY(${_logical_name} MODULE ${_sip_output_files} ${EXTRA_OBJECTS})
  SET_TARGET_PROPERTIES(${_logical_name} PROPERTIES CXX_VISIBILITY_PRESET default)
  IF (NOT APPLE)
    TARGET_LINK_LIBRARIES(${_logical_name} ${PYTHON_LIBRARY})
  ENDIF (NOT APPLE)
  TARGET_LINK_LIBRARIES(${_logical_name} ${EXTRA_LINK_LIBRARIES})
  IF (APPLE)
    SET_TARGET_PROPERTIES(${_logical_name} PROPERTIES LINK_FLAGS "-undefined dynamic_lookup")
  ENDIF (APPLE)
  SET_TARGET_PROPERTIES(${_logical_name} PROPERTIES PREFIX "" OUTPUT_NAME ${_child_module_name})

  IF (WIN32)
    SET_TARGET_PROPERTIES(${_logical_name} PROPERTIES SUFFIX ".pyd")
  ENDIF (WIN32)

  IF(WIN32)
    GET_TARGET_PROPERTY(_runtime_output ${_logical_name} RUNTIME_OUTPUT_DIRECTORY)
    ADD_CUSTOM_COMMAND(TARGET ${_logical_name} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E echo "Copying extension ${_child_module_name}"
      COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:${_logical_name}>" "${_runtime_output}/${_child_module_name}.pyd"
      DEPENDS ${_logical_name}
      )
  ENDIF(WIN32)

  INSTALL(TARGETS ${_logical_name} DESTINATION "${PYTHON_SITE_PACKAGES_DIR}/${_parent_module_path}")
ENDMACRO(BUILD_SIP_PYTHON_MODULE MODULE_NAME SIP_FILES EXTRA_OBJECTS)