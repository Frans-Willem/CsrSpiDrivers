VERSION:=	$(shell date '+%Y%m%d')
ZIP_NAME=	csr-spi-ftdi-$(VERSION)
ZIM_FILES=	spilpt-1.4-win32/spilpt.dll spilpt-1.3-win32/spilpt.dll \
	spilpt-1.4-wine/spilpt.dll.so spilpt-1.3-wine/spilpt.dll.so \
	README.md hardware/csr-spi-ftdi.sch hardware/csr-spi-ftdi.svg \
	hardware/components.lib

all: spilpt-1.4-win32/spilpt.dll spilpt-1.4-wine/spilpt.dll.so

zip: all
	rm -rf $(ZIP_NAME).zip $(ZIP_NAME)
	mkdir -p $(ZIP_NAME)
	for p in $(ZIM_FILES); do \
		mkdir -p $(ZIP_NAME)/`dirname $$p`; \
		cp -p $$p $(ZIP_NAME)/`dirname $$p`; \
	done
	zip -9r $(ZIP_NAME).zip $(ZIP_NAME)

spilpt-1.4-win32/spilpt.dll::
	make -f Makefile.mingw all

spilpt-1.4-wine/spilpt.dll.so::
	make -f Makefile.wine all

clean:
	make -f Makefile.mingw clean
	make -f Makefile.wine clean
	rm -rf $(ZIP_NAME) $(ZIP_NAME).zip
