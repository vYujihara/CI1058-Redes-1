CC = gcc
C_FLAGS = -Wall -g
EXECUTAVEL = client server
SOURCE_DIR = src
OBJECT_DIR = obj

SOURCE_FILES = $(wildcard $(SOURCE_DIR)/*.c)
HEADER_FILES = $(wildcard $(SOURCE_DIR)/*.h)
OBJECT_FILES = $(patsubst $(SOURCE_DIR)/%,$(OBJECT_DIR)/%,$(SOURCE_FILES:.c=.o))

CLIENT_OBJS = $(filter-out $(OBJECT_DIR)/server.o, $(OBJECT_FILES))
SERVER_OBJS = $(filter-out $(OBJECT_DIR)/client.o, $(OBJECT_FILES))

# regra padrao
all: makeobjdir $(EXECUTAVEL)

# debug
debug: C_FLAGS += -DDEBUG
debug: all

# regras de ligacao
client: $(OBJECT_DIR)/client.o $(CLIENT_OBJS)
	$(CC) $(C_FLAGS) -o $@ $^

server: $(OBJECT_DIR)/server.o $(SERVER_OBJS)
	$(CC) $(C_FLAGS) -o $@ $^

# regras de compilacao (.o colocados em um diretorio separado)
$(OBJECT_DIR)/client.o: $(SOURCE_DIR)/client.c $(HEADER_FILES)
	$(CC) $(C_FLAGS) -o $@ -c $<

$(OBJECT_DIR)/server.o: $(SOURCE_DIR)/server.c $(HEADER_FILES)
	$(CC) $(C_FLAGS) -o $@ -c $<

$(OBJECT_DIR)/%.o: $(SOURCE_DIR)/%.c $(SOURCE_DIR)/%.h
	$(CC) $(C_FLAGS) -o $@ -c $<

# utilidades
makeobjdir:
	@ mkdir -p $(OBJECT_DIR)

run: makeobjdir $(EXECUTAVEL)
	@ ./$(EXECUTAVEL)

clean:
	rm -f $(OBJECT_FILES)
	rmdir $(OBJECT_DIR)

purge: clean
	rm -f $(EXECUTAVEL)
	rm -f videos-client/*

# $@ target of the current rule
# $< terget dependency of the current rule
# $^ all dependences of the current target
# @  suppress (don't display) the command
