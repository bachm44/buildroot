#!/bin/sh

set -euo pipefail # fail on error
set -x # logging
IFS=$'\n\t'

FS_MNT_DIR=/mnt/nilfs2
FS_FILE_SIZE=500M
FS_BIN_FILE=/nilfs2.bin
LOOP_INTERFACE=/dev/loop0

rm -f $FS_BIN_FILE
fallocate -l $FS_FILE_SIZE $FS_BIN_FILE
losetup -P $LOOP_INTERFACE $FS_BIN_FILE
mkfs.nilfs2 $LOOP_INTERFACE -B 16
nilfs-tune -i 1 /dev/loop0
mkdir -p $FS_MNT_DIR
mount -t nilfs2 $LOOP_INTERFACE $FS_MNT_DIR
