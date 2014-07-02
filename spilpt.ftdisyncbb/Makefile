NAME:=	spilpt-ftdi-$(shell date '+%Y%m%d')

all: spilpt-1.4-win32/spilpt.dll spilpt-1.4-wine/spilpt.dll.so

zip: all
	rm -rf $(NAME).zip $(NAME)
	mkdir -p $(NAME)
	for p in spilpt-1.4-win32/spilpt.dll spilpt-1.3-win32/spilpt.dll \
		spilpt-1.4-wine/spilpt.dll.so spilpt-1.3-wine/spilpt.dll.so; \
	do \
		mkdir -p $(NAME)/`dirname $$p`; \
		cp -p $$p $(NAME)/`dirname $$p`; \
	done
	zip -9r $(NAME).zip $(NAME)

spilpt-1.4-win32/spilpt.dll::
	make -f Makefile.mingw all

spilpt-1.4-wine/spilpt.dll.so::
	make -f Makefile.wine all

clean:
	make -f Makefile.mingw clean
	make -f Makefile.wine clean
	rm -rf $(NAME) $(NAME).zip
