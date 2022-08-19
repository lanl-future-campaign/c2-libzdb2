#!/usr/bin/env bash

set -e

ZFS="$(realpath $1)"
LIBZDB="$(realpath $2)"
RECONSTRUCT="$(realpath $3)"

# can probably be input args
ashift=12
zpool_name="local_zpool"
zpool_root="/${zpool_name}"
backing_count=6
record_size=1M
record_size_bytes=1048576
filebase="file"
filename="${zpool_root}/${filebase}"
bs=128K
count=1
reconstructed="/tmp/reconstructed"

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
sync

set -x

# single vdev zpool
for zpool_type in "" mirror raidz{1..3}
do
    zpool create -f -o ashift="${ashift}" "${zpool_name}" ${zpool_type} "${backing[@]}"
    zfs set recordsize="${record_size}" "${zpool_name}"

    if [[ ! -f "/etc/zfs/zpool.cache" ]]
    then
        zpool set cachefile=/etc/zfs/zpool.cache "${zpool_name}"
    fi

    zpool status

    dd if=/dev/urandom of="${filename}" bs="${bs}" count="${count}"

    sync -f "${filename}"

    # reconstruct the file
    "${LIBZDB}" "${zpool_name}" "${filebase}" | "${RECONSTRUCT}" "${reconstructed}" "${record_size_bytes}"

    diff "${filename}" "${reconstructed}"

    rm "${reconstructed}"

    zpool destroy "${zpool_name}"
done
