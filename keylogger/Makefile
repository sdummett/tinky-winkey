CC = cl
CFLAGS = /nologo /W3 /EHsc /Wall /WX /I.

SRC_DIR = srcs
OBJ_DIR = objs

TARGET = winkey.exe

SRC_FILES = winkey.c

OBJ_FILES = $(OBJ_DIR)\winkey.obj

all: $(TARGET)

$(TARGET): $(OBJ_FILES)
	$(CC) /Fe$@ $(OBJ_FILES)

$(OBJ_DIR)\winkey.obj: $(SRC_DIR)\winkey.c
	@if not exist "$(OBJ_DIR)" mkdir "$(OBJ_DIR)"
	$(CC) $(CFLAGS) /c $(SRC_DIR)\winkey.c /Fo$(OBJ_DIR)\winkey.obj

clean:
	@if exist "$(OBJ_DIR)" del /Q $(OBJ_DIR)\*.obj 2>nul && rmdir /Q /S $(OBJ_DIR)

fclean: clean
	@if exist "$(TARGET)" del /Q $(TARGET)

re: fclean all
