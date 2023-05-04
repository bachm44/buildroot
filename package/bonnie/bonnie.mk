################################################################################
#
# bonnie
#
################################################################################

BONNIE_VERSION = 2.00e
BONNIE_SOURCE = $(BONNIE_VERSION).tar.gz
BONNIE_SITE = https://github.com/bachm44/bonnie-plus-plus/archive/refs/tags
BONNIE_LICENSE = GPL-2.0
BONNIE_LICENSE_FILES = copyright.txt

define BONNIE_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 755 $(@D)/bonnie++ $(TARGET_DIR)/usr/sbin/bonnie++
	$(INSTALL) -D -m 755 $(@D)/zcav $(TARGET_DIR)/usr/sbin/zcav
endef

$(eval $(autotools-package))
