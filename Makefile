#
# Makefile for building plugin where pkg-config is available.
#
# On Debian, you will need to do "apt-get install pidgin-dev" first.
#

TARGET = xmpp_muc_presence_plugin.so

##
## INCLUDE PATHS
##
CFLAGS += `pkg-config --cflags pidgin` -fPIC

##
##  SOURCES, OBJECTS
##
C_SRC =	xmpp_muc_presence_plugin.c

OBJECTS = $(C_SRC:%.c=%.o)

##
## LIBRARIES
##
LIBS =			`pkg-config --libs pidgin`

##
## TARGET DEFINITIONS
##
.PHONY: all install clean

all: $(TARGET)

install: all $(PIDGIN_INSTALL_PLUGINS_DIR)
	cp $(TARGET) $(PIDGIN_INSTALL_PLUGINS_DIR)

$(TARGET): $(OBJECTS)
	$(CC) -shared $(OBJECTS) $(LIB_PATHS) $(LIBS) -o $(TARGET)

##
## CLEAN RULES
##
clean:
	rm -rf $(OBJECTS)
	rm -rf $(TARGET)

include $(PIDGIN_COMMON_TARGETS)
