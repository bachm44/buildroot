################################################################################
#
# genfile
#
################################################################################

GENFILE_VERSION = 1.0.5-dev-09dfc220ea449a02f71f59dfd803e676a2db7905
GENFILE_SOURCE = genfile-$(GENFILE_VERSION).tar.gz
GENFILE_SITE = https://github.com/bachm44/genfile/releases/download/$(GENFILE_VERSION)
GENFILE_LICENSE = GPL-3.0+
GENFILE_LICENSE_FILES = COPYING
GENFILE_DEPENDENCIES = argp-standalone
GENFILE_CONF_OPTS += --with-argp-standalone

$(eval $(autotools-package))
