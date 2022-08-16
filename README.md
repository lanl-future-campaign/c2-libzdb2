**A helper library able to translate ZFS filenames to the underlying disk LBAs that are mapped to the files**

C2 LibZDB 2
================

```
XX              XXXXX XXX         XX XX           XX       XX XX XXX         XXX
XX             XXX XX XXXX        XX XX           XX       XX XX    XX     XX   XX
XX            XX   XX XX XX       XX XX           XX       XX XX      XX XX       XX
XX           XX    XX XX  XX      XX XX           XX       XX XX      XX XX       XX
XX          XX     XX XX   XX     XX XX           XX XXXXX XX XX      XX XX       XX
XX         XX      XX XX    XX    XX XX           XX       XX XX     XX  XX
XX        XX       XX XX     XX   XX XX           XX       XX XX    XX   XX
XX       XX XX XX XXX XX      XX  XX XX           XX XXXXX XX XX XXX     XX       XX
XX      XX         XX XX       XX XX XX           XX       XX XX         XX       XX
XX     XX          XX XX        X XX XX           XX       XX XX         XX       XX
XX    XX           XX XX          XX XX           XX       XX XX          XX     XX
XXXX XX            XX XX          XX XXXXXXXXXX   XX       XX XX            XXXXXX
```

LibZDB is developed as part of C2 under U.S. Government contract 89233218CNA000001 for Los Alamos National Laboratory (
LANL), which is operated by Triad National Security, LLC for the U.S. Department of Energy/National Nuclear Security
Administration. See the accompanying LICENSE.txt for further information. ZDB is a component of OpenZFS, an advanced
file system and volume manager which was originally developed for Solaris and is now maintained by the OpenZFS
community. Visit [openzfs.org](https://openzfs.org/) for more information on this open source file system. LibZDB
includes modified OpenZFS ZDB source code released under the CDDL-1.0 license. See the accompanying OPENSOLARIS.LICENSE
for more information.

# ZFS

This codebase targets ZFS 2.1.5.

# Software requirements

Compiling LibZDB currently requires zfs, g++, cmake, and make. On CentOS 8, one may use the following commands to
prepare
the programming environment for LibZDB.

```bash
sudo yum install libzfs5-devel
```

## ZFS headers

Installing libzfs5-devel will not install the zfs_ioctl.h that is required by libZDB. To resolve this issue,
one can manually install the header from the zfs source tree. Make sure to use zfs 2.1.5.

```bash
cd /usr/include/libzfs/sys/
sudo wget https://raw.githubusercontent.com/openzfs/zfs/zfs-2.1.5/include/sys/zfs_ioctl.h
```

# Building LibZDB

After all software requirements are installed, one can use the following to configure and build libZDB.

```bash
git clone https://github.com/lanl-future-campaign/c2-libzdb2.git
cd c2-libzdb2
mkdir build
cd build
cmake -DSPL_INCLUDE_DIR=/usr/include/libspl \
-DZFS_INCLUDE_DIR=/usr/include/libzfs \
-DZFS_LIBRARY_DIR=/lib64 \
-DBUILD_SHARED_LIBS=ON \
-DCMAKE_BUILD_TYPE=Release ..
make
```
