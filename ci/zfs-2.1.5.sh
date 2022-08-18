#/usr/bin/env bash

ZFS_ROOT="$1"

# https://openzfs.github.io/openzfs-docs/Developer%20Resources/Building%20ZFS.html#

sudo apt install -y build-essential autoconf automake libtool gawk alien fakeroot dkms libblkid-dev uuid-dev libudev-dev libssl-dev zlib1g-dev libaio-dev libattr1-dev libelf-dev linux-headers-generic python3 python3-dev python3-setuptools python3-cffi libffi-dev python3-packaging git libcurl4-openssl-dev

git clone -b zfs-2.1.5 --depth 1 https://github.com/openzfs/zfs "${ZFS_ROOT}"

cd "${ZFS_ROOT}"
./autogen.sh
./configure
make -j $(nproc)
