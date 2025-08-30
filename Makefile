#################################################
# PROJECT: App (udp + motor)
##################################################

TARGET      = app

# пути
DIR_DXL     = /home/alikhan/DynamixelSDK/c
DIR_OBJS    = .objects

# компилятор / флаги
CC          = gcc
CFLAGS      = -O2 -DLINUX -D_GNU_SOURCE -Wall -g $(INCLUDES) -MMD -MP
LDFLAGS     = $(CFLAGS)

SRC_DIRS = UdpServer MoorController

# инклуды и либы
INCLUDES   += -I$(DIR_DXL)/include/dynamixel_sdk
LIBRARIES  += -L$(DIR_DXL)/build/linux_sbc -ldxl_sbc_c -lpthread -lrt \
              -Wl,-rpath,$(DIR_DXL)/build/linux_sbc

# исходники
SOURCES     = $(wildcard $(addsuffix /*.c,$(SRC_DIRS)))
OBJECTS     = $(SOURCES:.c=.o)
DEPS        = $(OBJECTS:.o=.d)

# правила
all: $(TARGET)

$(TARGET): make_directory $(OBJECTS)
	$(CC) $(OBJECTS) -o $(TARGET) $(LIBRARIES)

make_directory:
	mkdir -p $(DIR_OBJS)/

$(DIR_OBJS)/%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(TARGET) $(DIR_OBJS) core *~ *.a *.so *.lo

-include $(DEPS)

.PHONY: all clean make_directory
