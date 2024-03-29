#################################################################
#								#
# Copyright (c) 2012-2020 Fidelity National Information		#
# Services, Inc. and/or its subsidiaries. All rights reserved.	#
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################

# CMake 2.8.4 introduced LINK_DEPENDS target property
# CMake 2.8.4 introduced generator expressions in custom commands
# CMake 2.8.5 introduced use of C compiler as the assembler
cmake_minimum_required(VERSION 2.8.5)
project(GTM C ASM)

# Max optimization level is -O2
get_property(languages GLOBAL PROPERTY ENABLED_LANGUAGES)
foreach(lang ${languages})
  get_property(flags CACHE CMAKE_${lang}_FLAGS_RELEASE PROPERTY VALUE)
  if("${flags}" MATCHES "-O[3-9]")
    string(REGEX REPLACE "-O[3-9]" "-O2" flags "${flags}")
    set_property(CACHE CMAKE_${lang}_FLAGS_RELEASE PROPERTY VALUE "${flags}")
  endif()
endforeach()

# Defaults
set(version ${V7.0-000})
if("${version}" STREQUAL "")
        set(version V9.9-0)
endif()

# If CMAKE_BUILD_TYPE is not defined make it a Release build
if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

if(GTM_RELEASE_NAME)
  add_definitions(-DGTM_RELEASE_NAME="${GTM_RELEASE_NAME}")
endif()

# Enable GT.M debug options unless directed not to enable them. Added to build without whitebox tests.
set(GTM_ENABLE_DEBUG 1 CACHE BOOL "Enable GT.M debug options")
if(GTM_ENABLE_DEBUG)
  set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -DDEBUG")
endif()

if ( CMAKE_COMPILER_IS_GNUCC )
    set(CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} -Wall -Wno-unused-result -Wno-parentheses -Wno-unused-value -Wno-unused-variable")
    set(CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} -Wno-maybe-uninitialized -Wno-char-subscripts -Wno-unused-but-set-variable")
    set(CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} -Wno-unused-function")
endif()

set(install_permissions_script
  OWNER_READ OWNER_EXECUTE OWNER_WRITE
  GROUP_READ GROUP_EXECUTE
  WORLD_READ WORLD_EXECUTE
  )

if(CMAKE_VERSION VERSION_LESS 3.0.0)
    set_property(DIRECTORY APPEND PROPERTY COMPILE_DEFINITIONS_DEBUG DEBUG)
else()
    set_property(DIRECTORY APPEND PROPERTY COMPILE_DEFINITIONS $<$<CONFIG:Debug>:DEBUG>)
endif()

# Only IA64 and x86_64 architectures use this option
set(gen_xfer_desc 0)

# Platform specific libs
set(gtm_osarch_libs "")

# Define these ahead of establishing platforms
set(gt_src_list gen)
set(sources_used "")
set(extralibs "")
message("--> OS = ${CMAKE_SYSTEM_NAME} / ARCH = ${CMAKE_SYSTEM_PROCESSOR}")
# Establish platform
# Except for Solaris, CMAKE_COMPILER_IS_GNUCC is true
if(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
  include(sr_linux/platform.cmake)
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "AIX")
  include(sr_aix/platform.cmake)
else()
  message(FATAL_ERROR "--> OS = ${CMAKE_SYSTEM_NAME} / ARCH = ${CMAKE_SYSTEM_PROCESSOR}")
endif()

# Choose where to get bootstrap sources.
set(GTM_DIST "" CACHE PATH "Existing GT.M Distribution")
if(GTM_DIST)
  # Bootstrap using already-installed mumps.
  message(STATUS "Using existing gtm_dist=${GTM_DIST} to generate sources.")
  set(gen_bootstrap 1)
  set(gen_bootstrap_dist "${GTM_DIST}")
  set(gen_bootstrap_mumps "${GTM_DIST}/mumps")
  set(gen_bootstrap_depend "${gen_bootstrap_mumps}")
else()
  # Bootstrap using pre-generated bootstrap sources.
  # We generate them again only for comparison.
  message(STATUS "Bootstraping from pre-generated sources.")
  set(gen_bootstrap 0)
  set(gen_bootstrap_dist "${GTM_BINARY_DIR}")
  set(gen_bootstrap_mumps "$<TARGET_FILE:mumps>")
  set(gen_bootstrap_depend mumps)
endif()

set(GTM_INSTALL_DIR lib/fis-gtm/${version}_${arch} CACHE STRING "Install directory (under CMAKE_PREFIX_PATH if relative)")
if(NOT GTM_INSTALL_DIR)
  set(GTM_INSTALL_DIR .)
endif()

#-----------------------------------------------------------------------------

# Source files are organized into subdirectories named by platform.
# Store in 'gt_src_list' a list of directory names selected and
# ordered based on the target platform.  A given source file name may
# appear in multiple directories but only the instance found earliest
# in the search path will be used.  Later instances of the same source
# file name will be ignored.
list(APPEND gt_src_list
  sr_unix_gnp
  sr_unix_cm
  sr_unix
  sr_port_cm
  sr_port
  )

# Collect the source file names belonging to each directory.  Store
# the full path to a directory in 'source_dir_${d}' and the list of
# source file names in the directory in 'sources_${d}'.
foreach(d ${gt_src_list})
  set(source_dir_${d} ${GTM_SOURCE_DIR}/${d})
  file(GLOB sources_${d} RELATIVE ${source_dir_${d}}
	  ${d}/*.c ${d}/*.s ${d}/*.si ${d}/*.txt)
endforeach()
# Prefer generated sources over all other locations.
file(MAKE_DIRECTORY ${GTM_BINARY_DIR}/gen)
set(source_dir_gen ${GTM_BINARY_DIR}/gen)
# Hard-code list since we cannot glob files that do not exist.
set(sources_gen gtm_threadgbl_deftypes.h gtm_threadgbl_deftypes_asm.si)
if(gen_bootstrap)
  set(sources_gen ${sources_gen} cmerrors_ctl.c cmierrors_ctl.c gdeerrors_ctl.c merrors_ctl.c ttt.c)
endif()

# The C preprocessor include search path consists of every source
# directory ordered by 'gt_src_list'.
set(includes ${GTM_BINARY_DIR}/gen)
foreach(d ${gt_src_list})
  list(APPEND includes ${source_dir_${d}})
endforeach()
include_directories(${includes})
# We generate some uniquely-named headers in the top of the build tree.
include_directories(${GTM_BINARY_DIR})

# Function to select the first instance of a given source file name
# found in the ordered 'gt_src_list' search path.
function(select_file src_var name)
  set(found "")
  foreach(d ${gt_src_list})
    set(src "${source_dir_${d}}/${name}")
    if(EXISTS "${src}")
      set(found "${src}")
      break()
    endif()
  endforeach()
  if(NOT found)
    message(FATAL_ERROR "Cannot find \"${name}\" as ${ARGN}")
  endif()
  set("${src_var}" "${found}" PARENT_SCOPE)
endfunction()

# Macro to search directories ordered by 'gt_src_list' for a given
# list of source file names, assign them to a target, and exclude the
# source file names from inclusion in libmumps.
macro(set_source_list target)
  foreach(name ${ARGN})
    set(src "")
    foreach(d ${gt_src_list})
      if(";${sources_${d}};" MATCHES ";(${name}\\.(c|s|si));")
      	set(fname ${CMAKE_MATCH_1})
	set(src ${d}/${fname})
	set("source_used_${fname}" 1)
	list(APPEND sources_used ${source_dir_${d}}/${fname})
	break()
      endif()
    endforeach()
    if(src)
      list(APPEND ${target}_SOURCES ${src})
    else()
      message(FATAL_ERROR "No source \"${name}\" available!")
    endif()
  endforeach()
endmacro()

# Macro like set_source_list but that loads the list of source file
# names from a file.
macro(load_source_list target listfile)
  file(STRINGS "${listfile}" names)
  set_source_list(${target} ${names})
endmacro()

#-----------------------------------------------------------------------------

# Assign sources to libraries.
load_source_list(libcmisockettcp sr_unix_gnp/libcmisockettcp.list)
load_source_list(libdse          sr_unix/libdse.list)
load_source_list(libgnpclient    sr_unix_gnp/libgnpclient.list)
load_source_list(libgnpserver    sr_unix_gnp/libgnpserver.list)
load_source_list(libgtcm         sr_unix_cm/libgtcm.list)
load_source_list(liblke          sr_unix/liblke.list)
load_source_list(libmupip        sr_unix/libmupip.list)
load_source_list(libstub         sr_unix/libstub.list)

# Assign sources to executables.
set_source_list(gtm_threadgbl_deftypes gtm_threadgbl_deftypes)
set_source_list(dse              dse dse_cmd)
set_source_list(gtcm_gnp_server  gtcm_gnp_server)
set_source_list(gtcm_pkdisp      gtcm_pkdisp)
set_source_list(gtcm_play        gtcm_play omi_srvc_xct)
set_source_list(gtcm_server      gtcm_main omi_srvc_xct)
set_source_list(gtcm_shmclean    gtcm_shmclean)
set_source_list(gtmsecshr        gtmsecshr_wrapper)
set_source_list(gtmsecshr_real   gtmsecshr)
set_source_list(libgtmcrypt      gtmcrypt_ref gtmcrypt_pk_ref gtmcrypt_dbk_ref gtmcrypt_sym_ref gtmcrypt_util)
set_source_list(libgtmtls        gtm_tls_impl gtmcrypt_util)
set_source_list(libgtmshr        gtm_main)
set_source_list(lke              lke lke_cmd)
set_source_list(maskpass         maskpass gtmcrypt_util)
set_source_list(mumps            gtm)
set_source_list(mupip            mupip mupip_cmd)

#-----------------------------------------------------------------------------
# libmumps gets leftover sources, so compute the remaining list.
set(source_used_dtgbldir.c 1) # exclude unused source
set(libmumps_SOURCES "")
foreach(d ${gt_src_list})
  foreach(s ${sources_${d}})
    if(NOT source_used_${s} AND "${s}" MATCHES "\\.(c|s|si)$")
      list(APPEND libmumps_SOURCES ${d}/${s})
      set(source_used_${s} 1)
      list(APPEND sources_used ${source_dir_${d}}/${s})
    endif()
  endforeach()
endforeach()

#-----------------------------------------------------------------------------
# Generate files depending on gtm

# Copy generation routines to a working directory.
foreach(m chk2lev.m chkop.m gendash.m genout.m loadop.m loadvx.m msg.m tttgen.m tttscan.m)
  add_custom_command(
    OUTPUT gen/${m}
    DEPENDS ${GTM_SOURCE_DIR}/sr_port/${m}
    COMMAND ${CMAKE_COMMAND} -E copy ${GTM_SOURCE_DIR}/sr_port/${m} ${GTM_BINARY_DIR}/gen/${m}
    )
endforeach()

foreach(f ttt.txt opcode_def.h vxi.h)
  select_file(src ${f})
  list(APPEND tttfiles ${src})
endforeach()
set(mumps_ttt_args -run tttgen ${tttfiles})
add_custom_command(
  OUTPUT gen/ttt.c
  DEPENDS ${tttfiles}
    gen/chk2lev.m gen/chkop.m gen/gendash.m gen/genout.m gen/loadop.m
    gen/loadvx.m gen/tttgen.m gen/tttscan.m
    ${GTM_SOURCE_DIR}/sr_unix/mumps.cmake ${gen_bootstrap_depend}
  WORKING_DIRECTORY ${GTM_BINARY_DIR}/gen
  COMMAND ${CMAKE_COMMAND}
    -D gtm_dist=${gen_bootstrap_dist}
    -D gtm_tools=${GTM_SOURCE_DIR}/sr_port
    -D gtmroutines=.
    -D mumps=${gen_bootstrap_mumps}
    -D "args=${mumps_ttt_args}"
    -D output_file=ttt.log
    -P ${GTM_SOURCE_DIR}/sr_unix/mumps.cmake
  VERBATIM
  )
list(APPEND gen_bootstrap_files gen/ttt.c)

set(gen_merrors_extra gen/merrors_ansi.h)
foreach(msg
    sr_port/cmerrors.msg
    sr_port/gdeerrors.msg
    sr_port/merrors.msg
    sr_unix_gnp/cmierrors.msg
    )
  get_filename_component(name ${msg} NAME_WE)
  set(mumps_msg_args -run msg ${GTM_SOURCE_DIR}/${msg} unix)
  set(outputs gen/${name}_ctl.c ${gen_${name}_extra})
  add_custom_command(
    OUTPUT ${outputs}
    DEPENDS gen/msg.m ${GTM_SOURCE_DIR}/${msg}
            ${GTM_SOURCE_DIR}/sr_unix/mumps.cmake ${gen_bootstrap_depend}
    WORKING_DIRECTORY ${GTM_BINARY_DIR}/gen
    COMMAND ${CMAKE_COMMAND}
      -D gtm_dist=${gen_bootstrap_dist}
      -D gtm_tools=${GTM_SOURCE_DIR}/sr_port
      -D gtmroutines=.
      -D mumps=${gen_bootstrap_mumps}
      -D "args=${mumps_msg_args}"
      -P ${GTM_SOURCE_DIR}/sr_unix/mumps.cmake
    VERBATIM
    )
  list(APPEND gen_bootstrap_files ${outputs})
endforeach()

add_custom_target(gen_bootstrap ALL DEPENDS ${gen_bootstrap_files})

#-----------------------------------------------------------------------------

if(gen_xfer_desc)
  list(SORT sources_used)
  set(CMAKE_CONFIGURABLE_FILE_CONTENT "")
  foreach(src ${sources_used})
    set(CMAKE_CONFIGURABLE_FILE_CONTENT "${CMAKE_CONFIGURABLE_FILE_CONTENT}${src}\n")
  endforeach()
  configure_file(${CMAKE_ROOT}/Modules/CMakeConfigurableFile.in ${GTM_BINARY_DIR}/sources.list)
  add_custom_command(
    OUTPUT xfer_desc.i
    WORKING_DIRECTORY ${GTM_BINARY_DIR}
    DEPENDS ${GTM_SOURCE_DIR}/sr_unix/gen_xfer_desc.cmake
            ${GTM_BINARY_DIR}/sources.list
    COMMAND ${CMAKE_COMMAND}
      -D CMAKE_C_COMPILER=${CMAKE_C_COMPILER}
      -D "arch=${arch}"
      -D "includes=${includes}"
      -P ${GTM_SOURCE_DIR}/sr_unix/gen_xfer_desc.cmake
    VERBATIM
    )
  add_custom_target(gen_xfer_desc DEPENDS xfer_desc.i)
  if(gen_bootstrap)
    add_dependencies(gen_xfer_desc gen_bootstrap)
  endif()
endif()

#-----------------------------------------------------------------------------

add_executable(gtm_threadgbl_deftypes ${gtm_threadgbl_deftypes_SOURCES})
if(gen_xfer_desc)
  add_dependencies(gtm_threadgbl_deftypes gen_xfer_desc)
endif()

# gtm_threadgbl_deftypes
set(gtmthreadgblasmhdr   "gen/gtm_threadgbl_deftypes.h")
add_custom_command(
  OUTPUT ${gtmthreadgblasmhdr}
  DEPENDS gtm_threadgbl_deftypes
  COMMAND gtm_threadgbl_deftypes > ${gtmthreadgblasmhdr}
  )
# gtm_threadgbl_deftypes_asm.si
set(gtmthreadgblasmfile   "gen/gtm_threadgbl_deftypes_asm.si")
select_file(gtmthreadgblasmaccess "gtm_threadgbl_asm_access.txt")
add_custom_command(
  OUTPUT ${gtmthreadgblasmfile}
  DEPENDS ${gtmthreadgblasmhdr}
  COMMAND ${CMAKE_COMMAND}
    -D "gtmthreadgblasmfile=${gtmthreadgblasmfile}"
    -D "gtmthreadgblasmhdr=${gtmthreadgblasmhdr}"
    -D "gtmthreadgblasmaccess=${gtmthreadgblasmaccess}"
    -P ${GTM_SOURCE_DIR}/${srdir}/gen_threadgbl_asm.cmake
  VERBATIM
  )

add_custom_target(gen_gtm_threadgbl_deftypes
  DEPENDS ${GTM_BINARY_DIR}/${gtmthreadgblasmhdr} ${GTM_BINARY_DIR}/${gtmthreadgblasmfile})

foreach(exp exe shr)
  set(out gtm${exp}_symbols.export)
  set(in ${GTM_SOURCE_DIR}/sr_unix/gtm${exp}_symbols.exp)
  add_custom_command(
    OUTPUT ${out}
    DEPENDS ${in} gen_gtm_threadgbl_deftypes
    COMMAND tcsh -f ${GTM_SOURCE_DIR}/${srdir}/genexport.csh ${in} ${out}
    )
endforeach()

add_custom_target(gen_export DEPENDS gtmexe_symbols.export gtmshr_symbols.export)

foreach(lib
  gtcm
  cmisockettcp
  gnpclient
  gnpserver
  dse
  lke
  mupip
  stub
  mumps
  ${extralibs}
  )
  add_library(lib${lib} STATIC ${lib${lib}_SOURCES})
  set_property(TARGET lib${lib} PROPERTY OUTPUT_NAME ${lib})
  add_dependencies(lib${lib} gen_gtm_threadgbl_deftypes)
endforeach()

# TODO: find_package or find_library for system libs?
include_directories (/usr/local/include)
target_link_libraries(libmumps		${libmumpslibs})
if(gen_xfer_desc)
  add_dependencies(libmumps gen_xfer_desc)
endif()

add_executable(mumps			${mumps_SOURCES})
target_link_libraries(mumps		libmumps)

add_executable(dse			${dse_SOURCES})
target_link_libraries(dse		libdse libmumps libstub)
list(APPEND with_export dse)

add_executable(gtmsecshr		${gtmsecshr_SOURCES})
add_dependencies(gtmsecshr		gen_gtm_threadgbl_deftypes)

add_executable(gtmsecshr_real		${gtmsecshr_real_SOURCES})
target_link_libraries(gtmsecshr_real libmumps)
set_target_properties(gtmsecshr_real PROPERTIES
  OUTPUT_NAME gtmsecshr
  RUNTIME_OUTPUT_DIRECTORY ${GTM_BINARY_DIR}/gtmsecshrdir
  )
add_dependencies(gtmsecshr_real		gen_gtm_threadgbl_deftypes)

add_executable(mupip			${mupip_SOURCES})
target_link_libraries(mupip		libmupip libmumps libstub)
list(APPEND with_export mupip)

add_executable(lke			${lke_SOURCES})
target_link_libraries(lke		liblke libmumps libgnpclient libmumps libgnpclient libcmisockettcp)
list(APPEND with_export lke)


add_executable(gtcm_server		${gtcm_server_SOURCES})
target_link_libraries(gtcm_server	libgtcm libmumps libstub)
list(APPEND with_export gtcm_server)

add_executable(gtcm_gnp_server		${gtcm_gnp_server_SOURCES})
target_link_libraries(gtcm_gnp_server	libgnpserver liblke libmumps libcmisockettcp libstub)
list(APPEND with_export gtcm_gnp_server)

add_executable(gtcm_play		${gtcm_play_SOURCES})
target_link_libraries(gtcm_play		libgtcm libmumps libstub)
list(APPEND with_export gtcm_play)

add_executable(gtcm_pkdisp		${gtcm_pkdisp_SOURCES})
target_link_libraries(gtcm_pkdisp	libgtcm libmumps libstub)

add_executable(gtcm_shmclean 		${gtcm_shmclean_SOURCES})
target_link_libraries(gtcm_shmclean	libgtcm libmumps libstub)

add_library(libgtmshr MODULE ${libgtmshr_SOURCES})
set_property(TARGET libgtmshr PROPERTY OUTPUT_NAME gtmshr)
target_link_libraries(libgtmshr libmumps libgnpclient libcmisockettcp)
set_target_properties(libgtmshr PROPERTIES
  LINK_FLAGS "${libgtmshr_link}"
  LINK_DEPENDS "${libgtmshr_dep}"
  )
add_dependencies(libgtmshr gen_export)
add_dependencies(mumps libgtmshr)

# Iterate over the list of GPG related libraries
foreach(gpglib gpg-error gpgme gcrypt config)
  # For each library, we need a new CMake variable, hence GPGLIB_${gpglib}
  find_library(GPGLIB_${gpglib} NAME ${gpglib} PATHS ${CMAKE_LIBRARY_PATH})
  # Append the found library to the list
  set(GPG_LIBRARIES ${GPG_LIBRARIES} ${GPGLIB_${gpglib}})
endforeach()

# Iterate over the list of SSL related libraries
foreach(tlslib ssl crypto config)
  # For each library, we need a new CMake variable, hence TLSLIB_${tlslib}
  find_library(TLSLIB_${tlslib} NAME ${tlslib} PATHS ${CMAKE_LIBRARY_PATH})
  # Append the found library to the list
  set(TLS_LIBRARIES ${TLS_LIBRARIES} ${TLSLIB_${tlslib}})
endforeach()

# Building the three encryption libraries could by a loop of some sort, but
# manually creating each target is way easier.

# Library=GCRYPT Algorithm=AES256CFB
include_directories (/usr/local/ssl/include)
add_library(libgtmcrypt_gcrypt_AES256CFB.so MODULE ${libgtmcrypt_SOURCES})
set_target_properties(libgtmcrypt_gcrypt_AES256CFB.so PROPERTIES
  OUTPUT_NAME gtmcrypt_gcrypt_AES256CFB
  COMPILE_DEFINITIONS "USE_GCRYPT -DUSE_AES256CFB"
  LIBRARY_OUTPUT_DIRECTORY ${GTM_BINARY_DIR}/plugin
  )
add_dependencies(libgtmcrypt_gcrypt_AES256CFB.so gen_gtm_threadgbl_deftypes)
target_link_libraries(libgtmcrypt_gcrypt_AES256CFB.so ${GPG_LIBRARIES})
install(TARGETS libgtmcrypt_gcrypt_AES256CFB.so DESTINATION ${GTM_INSTALL_DIR}/plugin)

# Establish the default encryption link
set(default_encr_link   ${GTM_BINARY_DIR}/plugin/libgtmcrypt.so)
set(default_encr_target libgtmcrypt_gcrypt_AES256CFB.so)
add_custom_command(
	OUTPUT ${default_encr_link}
	COMMAND ${CMAKE_COMMAND} -E create_symlink "${default_encr_target}" "${default_encr_link}"
	DEPENDS ${GTM_BINARY_DIR}/plugin/${default_encr_target}
	COMMENT "Generating default_encr symbolic link"
	)
add_custom_target(install_default_encr ALL DEPENDS ${default_encr_link})
install(FILES ${default_encr_link} DESTINATION ${GTM_INSTALL_DIR}/plugin)

# Library=OPENSSL Algorithm=AES256CFB
add_library(libgtmcrypt_openssl_AES256CFB MODULE ${libgtmcrypt_SOURCES})
set_target_properties(libgtmcrypt_openssl_AES256CFB PROPERTIES
  OUTPUT_NAME gtmcrypt_openssl_AES256CFB
  COMPILE_DEFINITIONS "USE_OPENSSL -DUSE_AES256CFB"
  LIBRARY_OUTPUT_DIRECTORY ${GTM_BINARY_DIR}/plugin
  )
target_link_libraries(libgtmcrypt_openssl_AES256CFB ${GPG_LIBRARIES} ${TLS_LIBRARIES})
add_dependencies(libgtmcrypt_openssl_AES256CFB gen_gtm_threadgbl_deftypes)
install(TARGETS libgtmcrypt_openssl_AES256CFB DESTINATION ${GTM_INSTALL_DIR}/plugin)

add_library(libgtmtls MODULE ${libgtmtls_SOURCES})
set_target_properties(libgtmtls PROPERTIES
  OUTPUT_NAME gtmtls
  COMPILE_DEFINITIONS "USE_OPENSSL"
  LIBRARY_OUTPUT_DIRECTORY ${GTM_BINARY_DIR}/plugin
  )
target_link_libraries(libgtmtls ${TLS_LIBRARIES})
add_dependencies(libgtmtls gen_gtm_threadgbl_deftypes)
install(TARGETS libgtmtls DESTINATION ${GTM_INSTALL_DIR}/plugin)

add_executable(maskpass ${maskpass_SOURCES})
target_link_libraries(maskpass ${GPG_LIBRARIES} ${TLS_LIBRARIES})
set_target_properties(maskpass PROPERTIES
  COMPILE_DEFINITIONS "USE_GCRYPT -DUSE_SYSLIB_FUNCS"
  RUNTIME_OUTPUT_DIRECTORY ${GTM_BINARY_DIR}/plugin/gtmcrypt
  )
install(TARGETS maskpass DESTINATION ${GTM_INSTALL_DIR}/plugin/gtmcrypt)

# Always copy files into the plugin directory
foreach(f
      Makefile.mk
      encrypt_sign_db_key.sh
      gen_keypair.sh
      gen_sym_hash.sh
      gen_sym_key.sh
      gtm_tls_impl.c
      gtm_tls_impl.h
      gtm_tls_interface.h
      gtmcrypt_dbk_ref.c
      gtmcrypt_dbk_ref.h
      gtmcrypt_interface.h
      gtmcrypt_pk_ref.c
      gtmcrypt_pk_ref.h
      gtmcrypt_ref.c
      gtmcrypt_ref.h
      gtmcrypt_sym_ref.c
      gtmcrypt_sym_ref.h
      gtmcrypt_util.c
      gtmcrypt_util.h
      import_and_sign_key.sh
      maskpass.c
      pinentry-gtm.sh
      pinentry.m
      show_install_config.sh
    )
  set(f_in "${GTM_SOURCE_DIR}/sr_unix/${f}")
  string(REGEX REPLACE ".mk$" "" f_mod "${f}")
  set(f_out "${GTM_BINARY_DIR}/plugin/gtmcrypt/${f_mod}")
  add_custom_command(
    OUTPUT "${f_out}"
    DEPENDS "${f_in}"
    COMMAND ${CMAKE_COMMAND} -E copy "${f_in}" "${f_out}"
    )
  if("${f}" MATCHES "\\.sh$")
    set(permissions PERMISSIONS ${install_permissions_script})
  else()
    set(permissions "")
  endif()
  install(FILES "${f_out}" DESTINATION ${GTM_INSTALL_DIR}/plugin/gtmcrypt ${permissions})
  list(APPEND files_to_place "${f_out}")
endforeach()

install(TARGETS
  mumps
  dse
  lke
  gtcm_server
  gtcm_gnp_server
  gtcm_pkdisp
  gtcm_play
  gtcm_shmclean
  gtmsecshr
  mupip
  libgtmshr
  DESTINATION ${GTM_INSTALL_DIR}
  )
install(TARGETS gtmsecshr_real DESTINATION ${GTM_INSTALL_DIR}/gtmsecshrdir)

# <r>.m -> <R>.m
#file(GLOB m_files_sr_port RELATIVE ${GTM_SOURCE_DIR}/sr_port ${GTM_SOURCE_DIR}/sr_port/*.m)
set(m_files_sr_port
  gde.m
  gdeadd.m
  gdechang.m
  gdedelet.m
  gdeexit.m
  gdehelp.m
  gdeinit.m
  gdelocks.m
  gdelog.m
  gdemap.m
  gdemsgin.m
  gdeparse.m
  gdequit.m
  gderenam.m
  gdescan.m
  gdesetgd.m
  gdeshow.m
  gdespawn.m
  gdetempl.m
  )
file(GLOB m_files_sr_unix RELATIVE ${GTM_SOURCE_DIR}/sr_unix ${GTM_SOURCE_DIR}/sr_unix/*.m)
file(GLOB m_files_sr_x86_64 RELATIVE ${GTM_SOURCE_DIR}/sr_x86_64 ${GTM_SOURCE_DIR}/sr_x86_64/*.m)
file(GLOB m_files_sr_i386 RELATIVE ${GTM_SOURCE_DIR}/sr_i386 ${GTM_SOURCE_DIR}/sr_i386/*.m)
file(GLOB mpt_files_sr_port RELATIVE ${GTM_SOURCE_DIR}/sr_port ${GTM_SOURCE_DIR}/sr_port/*.mpt)
file(GLOB mpt_files_sr_unix RELATIVE ${GTM_SOURCE_DIR}/sr_unix ${GTM_SOURCE_DIR}/sr_unix/*.mpt)

set(gtm_chset_dir_ "")
set(gtm_chset_dir_UTF-8 "/utf8")

foreach(d ${gt_src_list})
  foreach(m ${m_files_${d}})
    get_filename_component(m_name "${m}" NAME_WE)
    string(TOUPPER "${m_name}" m_upper)
    if("${m}" MATCHES "^gde")
      foreach(gtm_chset "" "UTF-8")
        set(m_out "${GTM_BINARY_DIR}${gtm_chset_dir_${gtm_chset}}/${m_upper}.m")
        add_custom_command(
          OUTPUT "${m_out}"
          DEPENDS ${GTM_SOURCE_DIR}/${d}/${m}
          COMMAND ${CMAKE_COMMAND} -E copy ${GTM_SOURCE_DIR}/${d}/${m} "${m_out}"
          )
        list(APPEND files_to_place "${m_out}")
        list(APPEND gtm_chset_routines_${gtm_chset} "${m_out}")
      endforeach()
    else()
      if(";${m_name};" MATCHES ";GTMDefinedTypesInit[a-zA-Z]*;")
	set(m_out "${GTM_BINARY_DIR}/${m_name}.m")
	set(${m_name}_found 1)
      else()
        set(m_out "${GTM_BINARY_DIR}/${m_upper}.m")
      endif()
      add_custom_command(
        OUTPUT "${m_out}"
        DEPENDS ${GTM_SOURCE_DIR}/${d}/${m}
        COMMAND ${CMAKE_COMMAND} -E copy ${GTM_SOURCE_DIR}/${d}/${m} "${m_out}"
        )
      install(FILES "${m_out}" DESTINATION ${GTM_INSTALL_DIR})
      list(APPEND files_to_place "${m_out}")
    endif()
  endforeach()
  foreach(m ${mpt_files_${d}})
    string(TOUPPER "_${m}" m_upper)
    string(REGEX REPLACE "MPT$" "m" m_out "${GTM_BINARY_DIR}/${m_upper}")
    add_custom_command(
      OUTPUT "${m_out}"
      DEPENDS ${GTM_SOURCE_DIR}/${d}/${m}
      COMMAND ${CMAKE_COMMAND} -E copy ${GTM_SOURCE_DIR}/${d}/${m} "${m_out}"
      )
    install(FILES "${m_out}" DESTINATION ${GTM_INSTALL_DIR})
    list(APPEND files_to_place "${m_out}")
  endforeach()
endforeach()

set(files)
foreach(f
  gtm_common_defs.h
  gtm_descript.h
  gtm_limits.h
  gtm_sizeof.h
  gtm_stdio.h
  gtm_stdlib.h
  gtm_string.h
  gtm_strings.h
  gtmgblstat.xc
  gtmxc_types.h
  main_pragma.h
  arch.gtc
  gtcm_run.gtc
  gtcm_slist.gtc
  gdedefaults.gtc
  gtmcshrc.gtc
  gtmprofile.gtc
  gtmprofile_preV54000.gtc
  gtmstart.gtc
  gtmstop.gtc
  dse.hlp
  gde.hlp
  lke.hlp
  mumps.hlp
  mupip.hlp
  custom_errors_sample.txt
  )
  select_file(src ${f})
  get_filename_component(srcfname "${src}" NAME)
  add_custom_command(
    OUTPUT "${GTM_BINARY_DIR}/${srcfname}"
    DEPENDS "${src}"
    COMMAND ${CMAKE_COMMAND} -E copy "${src}" "${GTM_BINARY_DIR}/${srcfname}"
    )
  list(APPEND files_to_place "${GTM_BINARY_DIR}/${srcfname}")
  list(APPEND files ${src})
endforeach()
install(FILES ${files} DESTINATION ${GTM_INSTALL_DIR})

set(scripts)
foreach(f
  gtm.gtc
  gtmbase.gtc
  )
  select_file(src ${f})
  list(APPEND scripts ${src})
endforeach()
install(FILES ${scripts}
  DESTINATION ${GTM_INSTALL_DIR}
  PERMISSIONS ${install_permissions_script}
  )

if(${CMAKE_SYSTEM_NAME} STREQUAL "AIX")
  set(gtm_icu_version "")
else()
  find_program(ICUCONFIG NAMES icu-config)
  if(ICUCONFIG)
    execute_process(
      COMMAND ${ICUCONFIG} --version
      OUTPUT_VARIABLE icu_version
      RESULT_VARIABLE icu_failed
      OUTPUT_STRIP_TRAILING_WHITESPACE
      )
    if(icu_failed)
      message(FATAL_ERROR "Command\n ${ICUCONFIG} --version\nfailed (${icu_failed}).")
    elseif("x${icu_version}" MATCHES "^x([0-9]+\\.[0-9]+)")
      set(gtm_icu_version "${CMAKE_MATCH_1}")
    else()
      message(FATAL_ERROR "Command\n ${ICUCONFIG} --version\nproduced unrecognized output:\n ${icu_version}")
    endif()
  else()
    include(FindPkgConfig)
    pkg_check_modules(ICUIO icu-io)
    if(ICUIO_FOUND)
      if("x${ICUIO_VERSION}" MATCHES "^x([0-9]+\\.[0-9]+)")
	set(gtm_icu_version "${CMAKE_MATCH_1}")
      else()
	message(FATAL_ERROR "Command\n pkg-config icu-io\nproduced unrecognized output:\n ${ICUIO_VERSION}")
      endif()
    else()
      pkg_check_modules(ICU icu)
      if(ICU_FOUND)
	if("x${ICU_VERSION}" MATCHES "^x([0-9]+\\.[0-9]+)")
	  set(gtm_icu_version "${CMAKE_MATCH_1}")
	else()
	  message(FATAL_ERROR "Command\n pkg-config icu\nproduced unrecognized output:\n ${ICU_VERSION}")
	endif()
      else()
	message(FATAL_ERROR "Unable to find 'icu-config'.  Set ICUCONFIG in CMake cache.")
      endif()
    endif()
  endif()
endif()

find_program(LOCALECFG NAMES locale)
if(LOCALECFG)
  execute_process(
  COMMAND ${LOCALECFG} -a
    OUTPUT_VARIABLE locale_list
    RESULT_VARIABLE locale_failed
    OUTPUT_STRIP_TRAILING_WHITESPACE
    )
  if(locale_failed)
    message(FATAL_ERROR "Command\n ${LOCALECFG} -a\nfailed (${locale_failed}).")
  endif()
  STRING(REGEX REPLACE "\n" ";" locale_list "${locale_list}")
  foreach(lc ${locale_list})
    string(TOLOWER "${lc}" lc_lower)
    if("x${lc_lower}" MATCHES "^x[a-zA-Z_]+\\.?utf-?8")
      set(LC_ALL ${lc})
      message("-- Setting locale to ${LC_ALL}")
      break()
    endif()
  endforeach(lc)
  if("${LC_ALL}" STREQUAL "")
    message("Locale undefined. Expect to see NONUTF8LOCALE during MUMPS routine compilation: ${locale_list}\n")
  endif()
else()
  message(FATAL_ERROR "Unable to find 'locale'.  Set LOCALECFG in CMake cache.")
endif()

foreach(gtm_chset "" "UTF-8")
  foreach(in ${gtm_chset_routines_${gtm_chset}})
    string(REPLACE ".m" ".o" out "${in}")
    get_filename_component(out_dir "${out}" PATH)
    add_custom_command(
      OUTPUT ${out}
      DEPENDS ${in} ${GTM_SOURCE_DIR}/sr_unix/mumps.cmake mumps
      COMMAND ${CMAKE_COMMAND}
        -D gtm_dist=${GTM_BINARY_DIR}
        -D gtmroutines=.
        -D gtm_chset=${gtm_chset}
        -D gtm_icu_version=${gtm_icu_version}
        -D LC_ALL=${LC_ALL}
        -D mumps=$<TARGET_FILE:mumps>
        -D "args=${in}"
        -P ${GTM_SOURCE_DIR}/sr_unix/mumps.cmake
      WORKING_DIRECTORY ${out_dir}
      VERBATIM
      )
    list(APPEND files_to_place ${out})
    install(FILES "${out}" DESTINATION ${GTM_INSTALL_DIR}${gtm_chset_dir_${gtm_chset}})
  endforeach()
endforeach()

#-----------------------------------------------------------------------------
set(gtm_hlp mumps.hlp)
set(gde_hlp gde.hlp)
set(mupip_hlp mupip.hlp)
set(dse_hlp dse.hlp)
set(lke_hlp lke.hlp)
foreach(help gtm gde mupip dse lke)
  set(CMAKE_CONFIGURABLE_FILE_CONTENT
    "Change -segment DEFAULT -block=2048 -file=\$gtm_dist/${help}help.dat
Change -region DEFAULT -record=1020 -key=255 -qdbrundown -nostats
exit")
  configure_file(${CMAKE_ROOT}/Modules/CMakeConfigurableFile.in
                 ${GTM_BINARY_DIR}/${help}help.in1)
  set(CMAKE_CONFIGURABLE_FILE_CONTENT
    "Do ^GTMHLPLD
${GTM_SOURCE_DIR}/sr_port/${${help}_hlp}
Halt")
  configure_file(${CMAKE_ROOT}/Modules/CMakeConfigurableFile.in
                 ${GTM_BINARY_DIR}/${help}help.in2)
  set(env
    -D gtm_dist=${GTM_BINARY_DIR}
    -D gtmroutines=.
    -D gtmgbldir=${GTM_BINARY_DIR}/${help}help
    )
  add_custom_command(
    OUTPUT ${help}help.dat ${help}help.gld
    DEPENDS ${GTM_SOURCE_DIR}/sr_unix/mumps.cmake mumps mupip
            ${GTM_BINARY_DIR}/${help}help.in1
            ${GTM_BINARY_DIR}/${help}help.in2
    COMMAND ${CMAKE_COMMAND} -E remove ${help}help.dat ${help}help.gld
    COMMAND ${CMAKE_COMMAND} ${env}
      -D mumps=$<TARGET_FILE:mumps> -D "args=-run;GDE"
      -D input_file=${GTM_BINARY_DIR}/${help}help.in1
      -P ${GTM_SOURCE_DIR}/sr_unix/mumps.cmake
    COMMAND ${CMAKE_COMMAND} ${env}
      -D mumps=$<TARGET_FILE:mupip> -D "args=create"
      -P ${GTM_SOURCE_DIR}/sr_unix/mumps.cmake
    COMMAND ${CMAKE_COMMAND} ${env}
      -D mumps=$<TARGET_FILE:mumps> -D "args=-direct"
      -D input_file=${GTM_BINARY_DIR}/${help}help.in2
      -P ${GTM_SOURCE_DIR}/sr_unix/mumps.cmake
    VERBATIM
    )
  list(APPEND files_to_place ${help}help.dat)
  install(FILES ${GTM_BINARY_DIR}/${help}help.dat DESTINATION ${GTM_INSTALL_DIR})
  install(FILES ${GTM_BINARY_DIR}/${help}help.gld DESTINATION ${GTM_INSTALL_DIR})
endforeach()
if (GTMDefinedTypesInit${CMAKE_BUILD_TYPE}_found)
add_custom_command(
  OUTPUT GTMDEFINEDTYPESTODB.o
  DEPENDS gtmhelp.dat gtmhelp.gld GTMDefinedTypesInit${CMAKE_BUILD_TYPE}.m ${GTM_BINARY_DIR}/gtmhelp.dat_noro
  COMMAND ${CMAKE_COMMAND} -E copy GTMDefinedTypesInit${CMAKE_BUILD_TYPE}.m ${GTM_BINARY_DIR}/GTMDefinedTypesInit.m
  COMMAND ${CMAKE_COMMAND}
    -D gtm_dist=${GTM_BINARY_DIR}
    -D gtmroutines=.
    -D gtmgbldir=${GTM_BINARY_DIR}/gtmhelp
    -D mumps=$<TARGET_FILE:mumps>
    -D "args=-run;GTMDEFINEDTYPESTODB"
    -P ${GTM_SOURCE_DIR}/sr_unix/mumps.cmake
    VERBATIM
  )
list(APPEND files_to_place GTMDEFINEDTYPESTODB.o)
endif(GTMDefinedTypesInit${CMAKE_BUILD_TYPE}_found)

foreach(help gtm gde mupip dse lke)
  set(CMAKE_CONFIGURABLE_FILE_CONTENT
	  "SET -ACC=MM -READ_ONLY -reg \"*\"
exit")
  configure_file(${CMAKE_ROOT}/Modules/CMakeConfigurableFile.in
                 ${GTM_BINARY_DIR}/${help}help.in3)
  set(env
    -D gtm_dist=${GTM_BINARY_DIR}
    -D gtmroutines=.
    -D gtmgbldir=${GTM_BINARY_DIR}/${help}help
    )
  add_custom_command(
    OUTPUT ${GTM_BINARY_DIR}/${help}help.dat_ro
    DEPENDS ${GTM_SOURCE_DIR}/sr_unix/mumps.cmake mumps mupip
            ${GTM_BINARY_DIR}/${help}help.in3
	    ${help}help.dat ${help}help.gld ${help}help.dat_noro
    COMMAND ${CMAKE_COMMAND} ${env}
      -D mumps=$<TARGET_FILE:mupip>
      -D input_file=${GTM_BINARY_DIR}/${help}help.in3
      -P ${GTM_SOURCE_DIR}/sr_unix/mumps.cmake
    VERBATIM
    )
    list(APPEND targets_to_run ${GTM_BINARY_DIR}/${help}help.dat_ro)
endforeach()

foreach(help gtm gde mupip dse lke)
  set(CMAKE_CONFIGURABLE_FILE_CONTENT
	  "SET -ACC=MM -NOREAD_ONLY -reg \"*\"
exit")
  configure_file(${CMAKE_ROOT}/Modules/CMakeConfigurableFile.in
                 ${GTM_BINARY_DIR}/${help}help.in4)
  set(env
    -D gtm_dist=${GTM_BINARY_DIR}
    -D gtmroutines=.
    -D gtmgbldir=${GTM_BINARY_DIR}/${help}help
    )
  add_custom_command(
    OUTPUT ${GTM_BINARY_DIR}/${help}help.dat_noro
    DEPENDS ${GTM_SOURCE_DIR}/sr_unix/mumps.cmake mumps mupip
            ${GTM_BINARY_DIR}/${help}help.in4
	    ${help}help.dat ${help}help.gld
    COMMAND ${CMAKE_COMMAND} ${env}
      -D mumps=$<TARGET_FILE:mupip>
      -D input_file=${GTM_BINARY_DIR}/${help}help.in4
      -P ${GTM_SOURCE_DIR}/sr_unix/mumps.cmake
    VERBATIM
    )
    list(APPEND targets_to_run ${GTM_BINARY_DIR}/${help}help.dat_noro)
endforeach()
#-----------------------------------------------------------------------------

set(GTM_TOP "${GTM_BINARY_DIR}")
configure_file(sr_unix/gpgagent.tab.in plugin/gpgagent.tab @ONLY)
set(GTM_TOP "${CMAKE_INSTALL_PREFIX}/${GTM_INSTALL_DIR}")
configure_file(sr_unix/gpgagent.tab.in CMakeFiles/plugin/gpgagent.tab @ONLY)
install(FILES ${GTM_BINARY_DIR}/CMakeFiles/plugin/gpgagent.tab DESTINATION ${GTM_INSTALL_DIR}/plugin)

install(FILES sr_unix/configure.gtc DESTINATION ${GTM_INSTALL_DIR}
  PERMISSIONS ${install_permissions_script}
  RENAME configure
  )

install(FILES sr_unix/gtminstall.sh DESTINATION ${GTM_INSTALL_DIR}
  PERMISSIONS ${install_permissions_script}
  RENAME gtminstall
  )

install(FILES sr_unix/lowerc_cp.sh DESTINATION ${GTM_INSTALL_DIR}
  RENAME lowerc_cp
  )

if (EXISTS ${GTM_SOURCE_DIR}/COPYING)
  install(FILES COPYING DESTINATION ${GTM_INSTALL_DIR})
endif()

add_custom_target(place_files ALL DEPENDS ${files_to_place})
add_custom_target(make_read_only ALL DEPENDS ${targets_to_run} place_files)
