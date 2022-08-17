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

This codebase targets the latest ZFS master branch (ZFS 2.1.99 at the moment).

# Software requirements

Compiling LibZDB currently requires the ZFS source tree, gcc, cmake, and make. Compiling the ZFS source tree further
requires autoconf, automake, libtool, and an additional set of packages listed
at [openzfs.github.io](https://openzfs.github.io/openzfs-docs/Developer%20Resources/Building%20ZFS.html)
. On CentOS 8, one can use the following commands to install all required packages.

```bash
sudo dnf install gcc make cmake autoconf automake libtool rpm-build \
libtirpc-devel libblkid-devel libuuid-devel libudev-devel \
openssl-devel zlib-devel libaio-devel libattr-devel elfutils-libelf-devel \
python3 python3-devel python3-setuptools python3-cffi \
libffi-devel git ncompress libcurl-devel
sudo dnf install kernel-devel-$(uname -r)
sudo dnf install epel-release
sudo dnf install --enablerepo=epel --enablerepo=powertools python3-packaging dkms
```

# Build ZFS

Next, download the latest ZFS source tree and build it.

```bash
cd ${HOME}
git clone https://github.com/openzfs/zfs
cd zfs
./autogen.sh
./configure --prefix=/opt/zfs
make
sudo make install
```

# Build LibZDB

There are two options to build LibZDB. The first is to build LibZDB against the ZFS source tree we just built.

```bash
git clone https://github.com/lanl-future-campaign/c2-libzdb2.git
cd c2-libzdb2
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release \
-DZFS_SOURCE_HOME=${HOME}/zfs ..
make
```

The other is to build LibZDB against the ZFS we just installed to /opt/zfs.

```bash
git clone https://github.com/lanl-future-campaign/c2-libzdb2.git
cd c2-libzdb2
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release \
-DCMAKE_PREFIX_PATH=/opt/zfs \
-DSPL_INCLUDE=/opt/zfs/include/libspl \
-DZFS_INCLUDE=/opt/zfs/include/libzfs \
-DZFS_INTERNAL_INCLUDE=/opt/zfs/src/zfs-2.1.99/include ..
make
```
