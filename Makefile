# Final program name
TARGET = svc.exe

# Source file
SRC = svc.c

# Generated object file
OBJ = svc.obj

# Compiler
CC = cl.exe

# Compiler options
#CFLAGS = /Wall /WX /EHsc
CFLAGS =

# Default rule
all: $(TARGET)

# Rule to generate the executable
$(TARGET): $(OBJ)
	$(CC) $(LDFLAGS) /Fe$@ $(OBJ)

# Rule to generate the object file from the source file
$(OBJ): $(SRC)
	$(CC) $(CFLAGS) /c $(SRC)

# Clean up generated files
clean:
	del /Q $(OBJ) $(TARGET)
