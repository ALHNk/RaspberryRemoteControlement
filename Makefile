#################################################
# PROJECT: Motor TCP Server
##################################################

TARGET      = motor_server

DIR_DXL    = /home/robotics/Dependencies/DynamixelSDK/c
DIR_OBJS   = .objects

CC          = gcc
CX          = g++
CCFLAGS     = -O2 -DLINUX -D_GNU_SOURCE -Wall -g $(INCLUDES)
CXFLAGS     = -O2 -DLINUX -D_GNU_SOURCE -Wall -g $(INCLUDES)
LNKCC       = $(CX)
LNKFLAGS    = $(CXFLAGS)

INCLUDES   += -I$(DIR_DXL)/include -I./MoorControl -I./Logger
LIBRARIES  += -L$(DIR_DXL)/build/linux_sbc -ldxl_sbc_c -lpthread -lrt

# файлы
SOURCES = \
	TCP/tcp.c \
	MoorControl/motor.c \
	Logger/logger.c

OBJECTS = $(addsuffix .o,$(addprefix $(DIR_OBJS)/,$(basename $(notdir $(SOURCES)))))

# правила
all: $(TARGET)

$(TARGET): make_directory $(OBJECTS)
	$(LNKCC) $(LNKFLAGS) $(OBJECTS) -o $(TARGET) $(LIBRARIES)

clean:
	rm -rf $(TARGET) $(DIR_OBJS) core *~ *.a *.so *.lo

make_directory:
	mkdir -p $(DIR_OBJS)

$(DIR_OBJS)/%.o: TCP/%.c
	$(CC) $(CCFLAGS) -c $< -o $@

$(DIR_OBJS)/%.o: MoorControl/%.c
	$(CC) $(CCFLAGS) -c $< -o $@

$(DIR_OBJS)/%.o: Logger/%.c
	$(CC) $(CCFLAGS) -c $< -o $@
