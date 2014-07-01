VERSION!=	date '+%Y%m%d'

all: spilpt-1.4-win32/spilpt.dll spilpt-1.4-wine/spilpt.dll.so

zip: all
	version=`date '+%Y%m%d'`; \
	mkdir -p spilpt-$$version; \
	for p in spilpt-1.4-win32/spilpt.dll spilpt-1.3-win32/spilpt.dll \
		spilpt-1.4-wine/spilpt.dll.so spilpt-1.3-wine/spilpt.dll.so; \
	do \
		mkdir -p spilpt-$$version/`dirname $$p`; \
		cp -p $$p spilpt-$$version/`dirname $$p`; \
	done; \
	zip -9r spilpt-$$version.zip spilpt-$$version

spilpt-1.4-win32/spilpt.dll::
	make -f Makefile.mingw all

spilpt-1.4-wine/spilpt.dll.so::
	make -f Makefile.wine all

clean:
	make -f Makefile.mingw clean
	make -f Makefile.wine clean
