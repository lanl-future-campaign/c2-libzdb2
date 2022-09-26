# Copyright (c) 2021 Triad National Security, LLC, as operator of Los Alamos
# National Laboratory with the U.S. Department of Energy/National Nuclear
# Security Administration. All Rights Reserved.
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# with the Software without restriction, including without limitation the
# rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
# sell copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# 1. Redistributions of source code must retain the above copyright notice,
#    this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright notice,
#    this list of conditions and the following disclaimer in the documentation
#    and/or other materials provided with the distribution.
# 3. Neither the name of TRIAD, Los Alamos National Laboratory, LANL, the
#    U.S. Government, nor the names of its contributors may be used to endorse
#    or promote products derived from this software without specific prior
#    written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS "AS IS" AND ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
# EVENT SHALL THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
# INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
set(zdb_srcs
  c2_libnvpair.c
  c2_vdev_raidz.c
  libzdb.c
  list.c
  )

add_library(zdb ${zdb_srcs})

target_compile_definitions(zdb PRIVATE LARGEFILE64_SOURCE)
target_compile_definitions(zdb PRIVATE _GNU_SOURCE)

if (ZFS_DEBUG)
    target_compile_definitions(zdb PRIVATE ZFS_DEBUG)
endif ()

target_include_directories(zdb PRIVATE ${CMAKE_SOURCE_DIR}/include)

# find libraries inside ZFS that are expected to always exist
function(find_zfs_lib VAR NAME)
  set(LIBNAME "lib${NAME}.so")
  execute_process(COMMAND find "${ZFS_SOURCE_HOME}" -name "${LIBNAME}"
    OUTPUT_VARIABLE ${VAR}
    OUTPUT_STRIP_TRAILING_WHITESPACE)
  if (NOT "${${VAR}}" STREQUAL "")
    set(${VAR} "${${VAR}}" PARENT_SCOPE)
    message(STATUS "${LIBNAME} found at ${${VAR}}")
  else()
    message(FATAL_ERROR "${LIBNAME} not found")
  endif()
endfunction()

if (ZFS_SOURCE_HOME STREQUAL "")
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(ZFS REQUIRED libzfs)

    target_include_directories(zdb PRIVATE ${ZFS_INCLUDE_DIRS})
    target_link_libraries(zdb ${ZFS_LDFLAGS} zpool Threads::Threads)
else ()
    target_include_directories(zdb PRIVATE ${ZFS_SOURCE_HOME}/include)
    target_include_directories(zdb PRIVATE ${ZFS_SOURCE_HOME}/lib/libspl/include)
    target_include_directories(zdb PRIVATE ${ZFS_SOURCE_HOME}/lib/libspl/include/os/linux)

    find_zfs_lib(ZPOOL    zpool)
    find_zfs_lib(ZFS_CORE zfs_core)
    find_zfs_lib(NVPAIR   nvpair)

    target_link_libraries(zdb
            ${ZPOOL}
            ${ZFS_CORE}
            ${NVPAIR}
            Threads::Threads
            )
endif ()

add_executable(dump_dvas dump_dvas.c)
target_include_directories(dump_dvas PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(dump_dvas zdb)

add_executable(reconstruct reconstruct.cpp)
