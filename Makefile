VERSION :=	$(shell cat VERSION)
ZIP_NAME ?=	csr-spi-ftdi-$(VERSION)
ZIP_FILES +=	spilpt-win32-api1.4/spilpt.dll spilpt-win32-api1.3/spilpt.dll \
	spilpt-wine-linux-api1.4/spilpt.dll.so spilpt-wine-linux-api1.3/spilpt.dll.so \
	README.md hardware misc utils

all: win32 wine

win32::
	$(MAKE) -f Makefile.mingw all

wine::
	$(MAKE) -f Makefile.wine all

zip: all
	rm -rf $(ZIP_NAME).zip $(ZIP_NAME)
	mkdir -p $(ZIP_NAME)
	for p in $(ZIP_FILES); do \
		mkdir -p $(ZIP_NAME)/`dirname $$p`; \
		cp -Rp $$p $(ZIP_NAME)/`dirname $$p`; \
	done
	zip -9r $(ZIP_NAME).zip $(ZIP_NAME)

clean:
	$(MAKE) -f Makefile.mingw clean
	$(MAKE) -f Makefile.wine clean
	rm -rf $(ZIP_NAME) $(ZIP_NAME).zip
