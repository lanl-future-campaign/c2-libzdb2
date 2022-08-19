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

On CentOS 8, this will install gcc 8.5.0, make 4.2.1, cmake 3.20.2, and kernel-devel 4.18.0-408.

# Build ZFS

Next, download the latest ZFS source tree, build, and install it. In this demo, we will use `/opt/zfs` as our install
target. Installing ZFS is not required to compile LibZDB2 (but is required to run tests).

```bash
cd ${HOME}
git clone -b zfs-2.1.5 https://github.com/openzfs/zfs
cd zfs
./autogen.sh
./configure --prefix=/opt/zfs
make
sudo make install
```

# Build LibZDB

There are two options to build LibZDB. The first is to build LibZDB against the ZFS source tree we just built.

```bash
git clone -b zfs-2.1.5 https://github.com/lanl-future-campaign/c2-libzdb2.git
cd c2-libzdb2
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release \
-DZFS_SOURCE_HOME=${HOME}/zfs ..
make
```

The other is to build LibZDB against the ZFS we just installed.

```bash
git clone -b zfs-2.1.5 https://github.com/lanl-future-campaign/c2-libzdb2.git
cd c2-libzdb2
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release \
-DCMAKE_PREFIX_PATH=/opt/zfs \
-DSPL_INCLUDE=/opt/zfs/include/libspl \
-DZFS_INCLUDE=/opt/zfs/include/libzfs \
-DZFS_INTERNAL_INCLUDE=/opt/zfs/src/zfs-2.1.5/include ..
make
```

# Example test case

First, create a simple `raidz1` zpool backed by five plain files.

```bash
for i in 1 2 3 4 5; do
	dd if=/dev/urandom of=file$i bs=1M count=64
done

sudo modprobe zfs
sudo /opt/zfs/sbin/zpool create mypool raidz1 `pwd`/file1 `pwd`/file2 \
`pwd`/file3 `pwd`/file4 `pwd`/file5
sudo /opt/zfs/sbin/zpool status
  pool: mypool
 state: ONLINE
config:

	NAME                       STATE     READ WRITE CKSUM
	mypool                     ONLINE       0     0     0
	  raidz1-0                 ONLINE       0     0     0
	    /home/qingzheng/file1  ONLINE       0     0     0
	    /home/qingzheng/file2  ONLINE       0     0     0
	    /home/qingzheng/file3  ONLINE       0     0     0
	    /home/qingzheng/file4  ONLINE       0     0     0
	    /home/qingzheng/file5  ONLINE       0     0     0

errors: No known data errors
```

Next, insert a 128K file into the newly created zpool.

```bash
dd if=/dev/urandom of=myfile bs=128K count=1
sudo cp myfile /mypool
```

Then, use `zdb` to show the DVAs of the file we just inserted.

```bash
sudo /opt/zfs/sbin/zdb -vvvvv -O -bb mypool myfile
obj=2 dataset=mypool path=/myfile type=19 bonustype=44

    Object  lvl   iblk   dblk  dsize  dnsize  lsize   %full  type
         2    1   128K   128K   128K     512   128K  100.00  ZFS plain file (K=inherit) (Z=inherit=uncompressed)
                                               184   bonus  System attributes
	dnode flags: USED_BYTES USERUSED_ACCOUNTED USEROBJUSED_ACCOUNTED
	dnode maxblkid: 0
	uid     0
	gid     0
	atime	Fri Aug 19 00:07:47 2022
	mtime	Fri Aug 19 00:07:47 2022
	ctime	Fri Aug 19 00:07:47 2022
	crtime	Fri Aug 19 00:07:47 2022
	gen	15
	mode	100644
	size	131072
	parent	34
	links	1
	pflags	840800000004
	xattr	3
Indirect blocks:
               0 L0 DVA[0]=<0:1c400:28000> [L0 ZFS plain file] fletcher4 uncompressed unencrypted LE contiguous unique single size=20000L/20000P birth=15L/15P fill=1 cksum=401a079a3d42:10003bc6e83bb43b:e0f600df0713fa2:5d57ed649a9e3068

		segment [0000000000000000, 0000000000020000) size  128K
```

Finally, use LibZDB2 to locate disk LBAs of the file.

```bash
${HOME}/c2-libzdb2/build/src/zdb mypool myfile
file size: 131072(1 blocks)
file_offset=0 vdev=0 io_offset=115712 record_size=131072
col=01 devidx=02 dev=/home/qingzheng/file3 offset=4217344 size=32768
col=02 devidx=03 dev=/home/qingzheng/file4 offset=4217344 size=32768
col=03 devidx=04 dev=/home/qingzheng/file5 offset=4217344 size=32768
col=04 devidx=00 dev=/home/qingzheng/file1 offset=4217856 size=32768
```
