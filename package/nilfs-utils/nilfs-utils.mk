################################################################################
#
# nilfs-utils
#
################################################################################

NILFS_UTILS_VERSION = 2.3.0-dev-1f3f8f903e72c41624e2f7e48d7ca125805c134d
NILFS_UTILS_SOURCE = nilfs-utils-$(NILFS_UTILS_VERSION).tar.bz2
NILFS_UTILS_SITE = https://github.com/bachm44/nilfs-utils/releases/download/$(NILFS_UTILS_VERSION)
NILFS_UTILS_LICENSE = GPL-2.0+ (programs), LGPL-2.1+ (libraries)
NILFS_UTILS_LICENSE_FILES = COPYING

# need libuuid, libblkid, libmount
NILFS_UTILS_DEPENDENCIES = util-linux

# We're patching sbin/cleanerd/Makefile.am
NILFS_UTILS_AUTORECONF = YES

ifeq ($(BR2_PACKAGE_LIBSELINUX),y)
NILFS_UTILS_CONF_OPTS += --with-selinux
NILFS_UTILS_DEPENDENCIES += libselinux
else
NILFS_UTILS_CONF_OPTS += --without-selinux
endif

TARGET_CFLAGS += -g -O0
$(eval $(autotools-package))
