CC = cl
CFLAGS = /nologo /W3 /EHsc /Wall /WX /I.

SRC_DIR = srcs
OBJ_DIR = objs

TARGET = winkey.exe

SRC_FILES = main.c hooks.c logger.c utils.c

OBJ_FILES = $(OBJ_DIR)\main.obj \
            $(OBJ_DIR)\hooks.obj \
            $(OBJ_DIR)\logger.obj \
            $(OBJ_DIR)\utils.obj
all: $(TARGET)

$(TARGET): $(OBJ_FILES)
	$(CC) /Fe$@ $(OBJ_FILES)

$(OBJ_DIR)\main.obj: $(SRC_DIR)\main.c
	@if not exist "$(OBJ_DIR)" mkdir "$(OBJ_DIR)"
	$(CC) $(CFLAGS) /c $(SRC_DIR)\main.c /Fo$(OBJ_DIR)\main.obj

$(OBJ_DIR)\hooks.obj: $(SRC_DIR)\hooks.c
	@if not exist "$(OBJ_DIR)" mkdir "$(OBJ_DIR)"
	$(CC) $(CFLAGS) /c $(SRC_DIR)\hooks.c /Fo$(OBJ_DIR)\hooks.obj

$(OBJ_DIR)\logger.obj: $(SRC_DIR)\logger.c
	@if not exist "$(OBJ_DIR)" mkdir "$(OBJ_DIR)"
	$(CC) $(CFLAGS) /c $(SRC_DIR)\logger.c /Fo$(OBJ_DIR)\logger.obj

$(OBJ_DIR)\utils.obj: $(SRC_DIR)\utils.c
	@if not exist "$(OBJ_DIR)" mkdir "$(OBJ_DIR)"
	$(CC) $(CFLAGS) /c $(SRC_DIR)\utils.c /Fo$(OBJ_DIR)\utils.obj

clean:
	@if exist "$(OBJ_DIR)" del /Q $(OBJ_DIR)\*.obj 2>nul && rmdir /Q /S $(OBJ_DIR)

fclean: clean
	@if exist "$(TARGET)" del /Q $(TARGET)

re: fclean all
