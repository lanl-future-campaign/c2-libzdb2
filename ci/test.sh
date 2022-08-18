#!/usr/bin/env bash

set -e

ZFS="$(realpath $1)"
LIBZDB="$(realpath $2)"

# can probably be input args
ashift=12
zpool_name="local_zpool"
zpool_root="/${zpool_name}"
backing_count=6
filebase="file"
filename="${zpool_root}/${filebase}"
filesize=128K

# backing files instead of drives
backing=()
for n in $(eval echo {1..${backing_count}})
do
    name="/tmp/file${n}"
    truncate -s 5GB "${name}"
    backing+=("${name}")
done

# find paths in ZFS source if it was retrieved from cache
if [[ -d "${ZFS}" ]]
then
    for cmd in $(find "${ZFS}/cmd" -maxdepth 1 -type d)
    do
        export PATH="${cmd}:${PATH}"
    done
fi

zpool destroy "${zpool_name}" || true

set -x

# single vdev zpool
for zpool_type in "" mirror raidz{1..3}
do
    zpool create -f -o ashift="${ashift}" "${zpool_name}" ${zpool_type} "${backing[@]}"

    if [[ ! -f "/etc/zfs/zpool.cache" ]]
    then
        zpool set cachefile=/etc/zfs/zpool.cache "${zpool_name}"
    fi

    zpool status

    dd if=/dev/urandom of="${filename}" bs="${filesize}" count=1

    "${LIBZDB}" "${zpool_name}" "${filebase}"

    zpool destroy "${zpool_name}"
done
