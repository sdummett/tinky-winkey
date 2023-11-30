NAME = winkey.exe

DIR_SRCS = srcs
DIR_OBJS = objs
DIR_INCLUDES = incs

SRCS =	$(DIR_SRCS)\main.cpp \
		$(DIR_SRCS)\hooks.cpp \
		$(DIR_SRCS)\log.cpp \

OBJS = $(SRCS:.cpp=.obj)
OBJS = $(OBJS:srcs=objs)

CFLAGS = /Wall /WX /EHs

all: create_dirs make_exe

# Full clean + rebuilding
re: fclean all

# Build winkey.exe 
make_exe: $(NAME)

# TODO: Add $(CFLAGS)
# Compile the sources
{$(DIR_SRCS)}.cpp{$(DIR_OBJS)}.obj :
		@echo Compiling...
		cl /nologo $(CFLAGS) /c /Fo$(DIR_OBJS)\ /I$(DIR_INCLUDES)\ $<

# Link the files into an executable
$(NAME): $(OBJS)
		@echo Linking $(NAME)...
		link /nologo /out:$(NAME) $(OBJS) user32.lib
		@echo Executable $(NAME) created.

# Create output directories
create_dirs:
		@if not exist $(DIR_OBJS) mkdir $(DIR_OBJS)

# Delete output directories
clean:
		@if exist $(DIR_OBJS) rmdir /s /q $(DIR_OBJS)

# Delete output dirs + executable
fclean: clean
		@if exist $(NAME) del /f /s /q $(NAME) 1>nul
