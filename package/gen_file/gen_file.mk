################################################################################
#
# gen_file
#
################################################################################

GEN_FILE_VERSION = 1.0.5-dev-fdc92431531f063827927ecad5b8cf1658316173
GEN_FILE_SOURCE = gen_file-$(GEN_FILE_VERSION).tar.gz
GEN_FILE_SITE = https://github.com/bachm44/gen_file/releases/download/$(GEN_FILE_VERSION)
GEN_FILE_LICENSE = GPL-3.0+
GEN_FILE_LICENSE_FILES = COPYING
GEN_FILE_DEPENDENCIES = argp-standalone
GEN_FILE_CONF_OPTS += --with-argp-standalone

$(eval $(autotools-package))
