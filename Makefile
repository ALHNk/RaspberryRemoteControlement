#################################################
# PROJECT: Motor TCP Server
##################################################

TARGET      = motor_server

DIR_DXL    = /home/alikhan/DynamixelSDK/c
DIR_OBJS   = .objects

CC          = gcc
CX          = g++
CCFLAGS     = -O2 -DLINUX -D_GNU_SOURCE -Wall $(INCLUDES) -g
CXFLAGS     = -O2 -DLINUX -D_GNU_SOURCE -Wall $(INCLUDES) -g
LNKCC       = $(CX)
LNKFLAGS    = $(CXFLAGS)

INCLUDES   += -I$(DIR_DXL)/include/dynamixel_sdk -I./MoorControl
LIBRARIES  += -L$(DIR_DXL)/build/linux_sbc -ldxl_sbc_c -lpthread -lrt

# файлы
SOURCES = TCP/tcp.c MoorControl/motor.c
OBJECTS = $(addsuffix .o,$(addprefix $(DIR_OBJS)/,$(basename $(notdir $(SOURCES)))))

# правила
$(TARGET): make_directory $(OBJECTS)
	$(LNKCC) $(LNKFLAGS) $(OBJECTS) -o $(TARGET) $(LIBRARIES)

all: $(TARGET)

clean:
	rm -rf $(TARGET) $(DIR_OBJS) core *~ *.a *.so *.lo

make_directory:
	mkdir -p $(DIR_OBJS)/

$(DIR_OBJS)/%.o: TCP/%.c
	$(CC) $(CCFLAGS) -c $< -o $@

$(DIR_OBJS)/%.o: MoorControl/%.c
	$(CC) $(CCFLAGS) -c $< -o $@
