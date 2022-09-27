#!/usr/bin/env bash

set -e

ZFS="$(realpath $1)"
DUMP_DVAS="$(realpath $2)"
RECONSTRUCT="$(realpath $3)"

# can probably be input args
ashift=12
zpool_name="mypool"
zpool_root="/${zpool_name}"
backing_count=6
filebase="myfile"
filename="${zpool_root}/${filebase}"
bs=1048577
count=2
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
rm -rf "${reconstructed}"
sync

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

    dd if=/dev/urandom of="${filename}" bs="${bs}" count="${count}" status=none

    sync -f "${filename}"

    # get physical locations of data and reconstruct the file
    "${DUMP_DVAS}" "${zpool_name}" "${filebase}" | tee /dev/stderr | "${RECONSTRUCT}" "${reconstructed}"

    cmp "${filename}" "${reconstructed}"

    rm "${reconstructed}"

    zpool destroy "${zpool_name}"
done
