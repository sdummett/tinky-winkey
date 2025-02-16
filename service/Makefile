CC = cl
CFLAGS = /nologo /W3 /EHsc /Wall /WX /I.

SRC_DIR = srcs
OBJ_DIR = objs

TARGET = service.exe

SRC_FILES = main.c service.c service_manager.c utils.c

OBJ_FILES = $(OBJ_DIR)\main.obj \
            $(OBJ_DIR)\service.obj \
            $(OBJ_DIR)\service_manager.obj \
            $(OBJ_DIR)\utils.obj

all: $(TARGET)

$(TARGET): $(OBJ_FILES)
	$(CC) /Fe$@ $(OBJ_FILES)

$(OBJ_DIR)\main.obj: $(SRC_DIR)\main.c
	@if not exist "$(OBJ_DIR)" mkdir "$(OBJ_DIR)"
	$(CC) $(CFLAGS) /c $(SRC_DIR)\main.c /Fo$(OBJ_DIR)\main.obj

$(OBJ_DIR)\service.obj: $(SRC_DIR)\service.c
	@if not exist "$(OBJ_DIR)" mkdir "$(OBJ_DIR)"
	$(CC) $(CFLAGS) /c $(SRC_DIR)\service.c /Fo$(OBJ_DIR)\service.obj

$(OBJ_DIR)\service_manager.obj: $(SRC_DIR)\service_manager.c
	@if not exist "$(OBJ_DIR)" mkdir "$(OBJ_DIR)"
	$(CC) $(CFLAGS) /c $(SRC_DIR)\service_manager.c /Fo$(OBJ_DIR)\service_manager.obj

$(OBJ_DIR)\utils.obj: $(SRC_DIR)\utils.c
	@if not exist "$(OBJ_DIR)" mkdir "$(OBJ_DIR)"
	$(CC) $(CFLAGS) /c $(SRC_DIR)\utils.c /Fo$(OBJ_DIR)\utils.obj

clean:
	@if exist "$(OBJ_DIR)" del /Q $(OBJ_DIR)\*.obj 2>nul && rmdir /Q /S $(OBJ_DIR)

fclean: clean
	@if exist "$(TARGET)" del /Q $(TARGET)

re: fclean all
