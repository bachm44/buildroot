#!/bin/sh
set -euo pipefail
IFS=$'\n\t'

MODULE_NAME=ext2-inc
SOURCE_DIR=/mnt/work/workflow/linux

cd $SOURCE_DIR && \
	make modules_install && \
	ln --symbolic --force $SOURCE_DIR/fs/$MODULE_NAME/$MODULE_NAME.ko /lib/modules/`uname -r` && \
	depmod -a && \
	modprobe $MODULE_NAME
