#!/bin/sh
set -euo pipefail # fail on error
set -x # logging
IFS=$'\n\t'

MODULE_NAME=ext2-inc
WORKDIR=/mnt/work
SOURCE_DIR="${WORKDIR}/workflow/linux"
FS_MNT_DIR=/mnt/ext2
FS_FILE_SIZE=40M
FS_BIN_FILE=/ext2.bin

function mount_workdir {
	if grep -qs "${WORKDIR} " /proc/mounts; then
		echo "Workdir already mounted"
	else
		echo "Mounting ${WORKDIR}"
		mount -t 9p -o trans=virtio host0 $WORKDIR
	fi
}

function install_modules {
	if lsmod | grep "ext2_inc" &> /dev/null ; then
		echo "$MODULE_NAME is loaded! Unloading it"
		rmmod -f $MODULE_NAME
	fi

	cd $SOURCE_DIR && \
		make modules_install && \
		ln -s -f $SOURCE_DIR/fs/$MODULE_NAME/$MODULE_NAME.ko /lib/modules/`uname -r` && \
		depmod -a && \
		modprobe $MODULE_NAME
}

function mount_fs {
	echo "Checking if ${FS_MNT_DIR} already mounted"
	if grep -qs "${FS_MNT_DIR} " /proc/mounts; then
		echo "Unmounting ${FS_MNT_DIR}"
		umount $FS_MNT_DIR
	fi

	echo "Allocating fs file with size ${FS_FILE_SIZE}"
	rm -vf $FS_BIN_FILE
	fallocate -l $FS_FILE_SIZE $FS_BIN_FILE

	echo "Mounting bin file on loop device"
	losetup -P /dev/loop0 $FS_BIN_FILE || true

	echo "Formatting loop device to ext2 partition"
	mkfs.ext2 /dev/loop0

	echo "Mounting loop on ${FS_MNT_DIR} with ${MODULE_NAME} module"
	mkdir -pv $FS_MNT_DIR
	mount -t $MODULE_NAME /dev/loop0 $FS_MNT_DIR
}

function main {
	mount_workdir
	install_modules
	mount_fs
}

main