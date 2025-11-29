# ============================================
# Compiler and Flags
# ============================================
CC = gcc
CFLAGS = -Wall -Wextra -std=c11 `pkg-config --cflags gtk4`
LIBS = `pkg-config --libs gtk4` -lm

# ============================================
# Backend Logic Files (Shared)
# ============================================
BACKEND_SRCS = \
    minigit.c \
    search_engine.c \
    ranking.c \
    autocomplete.c \
    trie_index.c

BACKEND_OBJS = $(BACKEND_SRCS:.c=.o)

# ============================================
# CLI Target (Console App)
# ============================================
CLI_SRC = cli.c
CLI_OBJ = $(CLI_SRC:.c=.o)
TARGET_CLI = minigitsearch

$(TARGET_CLI): $(CLI_OBJ) $(BACKEND_OBJS)
	$(CC) -o $(TARGET_CLI) $(CLI_OBJ) $(BACKEND_OBJS) -lm

# ============================================
# GUI Target (GTK4 Application)
# ============================================
GUI_SRC = gui.c
GUI_OBJ = $(GUI_SRC:.c=.o)
TARGET_GUI = minigitgui

# IMPORTANT:
# We DO NOT link cli.o here because gui.c already has its own main()
# and linking cli.o would cause duplicate symbol '_main'
$(TARGET_GUI): $(GUI_OBJ) $(BACKEND_OBJS)
	$(CC) $(CFLAGS) -o $(TARGET_GUI) $(GUI_OBJ) $(BACKEND_OBJS) $(LIBS)

# ============================================
# Compilation Rule
# ============================================
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# ============================================
# Build Both by Default
# ============================================
all: $(TARGET_CLI) $(TARGET_GUI)

# ============================================
# Cleanup
# ============================================
clean:
	rm -f $(BACKEND_OBJS) $(CLI_OBJ) $(GUI_OBJ) $(TARGET_CLI) $(TARGET_GUI)

.PHONY: all clean
