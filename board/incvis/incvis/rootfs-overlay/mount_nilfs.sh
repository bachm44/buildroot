#!/bin/sh

set -euo pipefail # fail on error
set -x # logging
IFS=$'\n\t'

FS_MNT_DIR=/mnt/nilfs2
FS_FILE_SIZE=1052672
FS_BIN_FILE=/nilfs2.bin

rm -f $FS_BIN_FILE
fallocate -l $FS_FILE_SIZE $FS_BIN_FILE
losetup -P /dev/loop0 $FS_BIN_FILE
mkfs.nilfs2 /dev/loop0 -B 16
mkdir -p $FS_MNT_DIR
mount -t nilfs2 /dev/loop0 $FS_MNT_DIR