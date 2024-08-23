# Final program names
TARGET_SVC = svc.exe
TARGET_WINKEY = winkey.exe

# Source files
SRC_SVC = svc.c
SRC_WINKEY = winkey.c

# Generated object files
OBJ_SVC = svc.obj
OBJ_WINKEY = winkey.obj

# Compiler
CC = cl.exe

# Compiler options
CFLAGS = /Wall /WX

# Default rule - compile both programs
all: $(TARGET_SVC) $(TARGET_WINKEY)

# Rule to generate svc.exe
$(TARGET_SVC): $(OBJ_SVC)
	$(CC) $(LDFLAGS) /Fe$@ $(OBJ_SVC)

# Rule to generate winkey.exe
$(TARGET_WINKEY): $(OBJ_WINKEY)
	$(CC) $(LDFLAGS) /Fe$@ $(OBJ_WINKEY)

# Rule to generate svc.obj from svc.c
$(OBJ_SVC): $(SRC_SVC)
	$(CC) $(CFLAGS) /c $(SRC_SVC)

# Rule to generate winkey.obj from winkey.c
$(OBJ_WINKEY): $(SRC_WINKEY)
	$(CC) $(CFLAGS) /c $(SRC_WINKEY)

# Clean up generated files
clean:
	del /Q $(OBJ_SVC) $(TARGET_SVC) $(OBJ_WINKEY) $(TARGET_WINKEY)
