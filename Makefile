TARGET_SVC = svc.exe
TARGET_WINKEY = winkey.exe

SRC_SVC = svc.c
SRC_WINKEY = winkey.c

OBJ_SVC = svc.obj
OBJ_WINKEY = winkey.obj

CC = cl.exe

CFLAGS = /Wall /WX

all: $(TARGET_SVC) $(TARGET_WINKEY)

# svc.exe
$(TARGET_SVC): $(OBJ_SVC)
	$(CC) $(LDFLAGS) /Fe$@ $(OBJ_SVC)

# winkey.exe
$(TARGET_WINKEY): $(OBJ_WINKEY)
	$(CC) $(LDFLAGS) /Fe$@ $(OBJ_WINKEY)

$(OBJ_SVC): $(SRC_SVC)
	$(CC) $(CFLAGS) /c $(SRC_SVC)

$(OBJ_WINKEY): $(SRC_WINKEY)
	$(CC) $(CFLAGS) /c $(SRC_WINKEY)

clean:
	del /Q $(OBJ_SVC) $(OBJ_WINKEY)

fclean: clean
	del /Q $(TARGET_SVC) $(TARGET_WINKEY)

re: fclean all
