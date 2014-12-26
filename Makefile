# Determine package name and version from DESCRIPTION file
PKG_VERSION=$(shell grep -i ^version DESCRIPTION | cut -d : -d \  -f 2)
PKG_NAME=$(shell grep -i ^package DESCRIPTION | cut -d : -d \  -f 2)

# Roxygen version to check before generating documentation
ROXYGEN_VERSION=4.1.0

# Name of built package
PKG_TAR=$(PKG_NAME)_$(PKG_VERSION).tar.gz

all: readme
readme: $(patsubst %.Rmd, %.md, $(wildcard *.Rmd))

%md: %Rmd
	Rscript -e "library(knitr); knit('README.Rmd', quiet = TRUE)"
	sed   's/```r/```coffee/' README.md > README2.md
	rm README.md
	mv README2.md README.md

# Install package
install:
	cd .. && R CMD INSTALL $(PKG_NAME)

# Build documentation with roxygen
# 1) Check version of roxygen2 before building documentation
# 2) Remove old doc
# 3) Rebuild DESCRIPTION
# 4) Generate documentation
roxygen:
	Rscript -e "library(roxygen2); stopifnot(packageVersion('roxygen2') == '$(ROXYGEN_VERSION)')"
	rm -f man/*.Rd
	Rscript scripts/build_DESCRIPTION.r
	cd .. && Rscript -e "library(roxygen2); roxygenize('$(PKG_NAME)')"

# Generate PDF output from the Rd sources
# 1) Rebuild documentation with roxygen
# 2) Generate pdf, overwrites output file if it exists
pdf: roxygen
	cd .. && R CMD Rd2pdf --force $(PKG_NAME)

# Build and check package
check: clean
	cd .. && R CMD build --no-build-vignettes $(PKG_NAME)
	cd .. && R CMD check --no-manual --no-vignettes --no-build-vignettes $(PKG_TAR)

# Build and check package with valgrind
check_valgrind: clean
	cd .. && R CMD build --no-build-vignettes $(PKG_NAME)
	cd .. && R CMD check --as-cran --no-manual --no-vignettes --no-build-vignettes --use-valgrind $(PKG_TAR)

# Run all tests with valgrind
test_objects = $(wildcard tests/*.R)
valgrind:
	$(foreach var,$(test_objects),R -d "valgrind --tool=memcheck --leak-check=full" --vanilla < $(var);)

# Sync git2r with changes in the libgit2 C-library
#
# 1) clone or pull libgit2 to parent directory from
# https://github.com/libgit/libgit.git
#
# 2) run 'make sync_libgit2'. It first removes files and then copy
# files from libgit2 directory. Next it runs an R script to build
# Makevars.in and Makevars.win based on source files. Finally it runs
# a patch command to change some lines in the source code to pass
# 'R CMD check git2r'
#
# 3) Build and check updated package 'make check'
sync_libgit2:
	-rm -f src/http-parser/*
	-rm -f src/regex/*
	-rm -f src/libgit2/include/*.h
	-rm -f src/libgit2/include/git2/*.h
	-rm -f src/libgit2/include/git2/sys/*.h
	-rm -f src/libgit2/*.c
	-rm -f src/libgit2/*.h
	-rm -f src/libgit2/hash/*.c
	-rm -f src/libgit2/hash/*.h
	-rm -f src/libgit2/transports/*.c
	-rm -f src/libgit2/transports/*.h
	-rm -f src/libgit2/unix/*.c
	-rm -f src/libgit2/unix/*.h
	-rm -f src/libgit2/win32/*.c
	-rm -f src/libgit2/win32/*.h
	-rm -f src/libgit2/xdiff/*.c
	-rm -f src/libgit2/xdiff/*.h
	-rm -f scripts/AUTHORS_libgit2
	-rm -f inst/NOTICE
	-cp -f ../libgit2/deps/http-parser/* src/http-parser
	-cp -f ../libgit2/deps/regex/* src/regex
	-cp -f ../libgit2/include/*.h src/libgit2/include
	-cp -f ../libgit2/include/git2/*.h src/libgit2/include/git2
	-cp -f ../libgit2/include/git2/sys/*.h src/libgit2/include/git2/sys
	-cp -f ../libgit2/src/*.c src/libgit2
	-cp -f ../libgit2/src/*.h src/libgit2
	-cp -f ../libgit2/src/hash/*.c src/libgit2/hash
	-cp -f ../libgit2/src/hash/*.h src/libgit2/hash
	-cp -f ../libgit2/src/transports/*.c src/libgit2/transports
	-cp -f ../libgit2/src/transports/*.h src/libgit2/transports
	-cp -f ../libgit2/src/unix/*.c src/libgit2/unix
	-cp -f ../libgit2/src/unix/*.h src/libgit2/unix
	-cp -f ../libgit2/src/win32/*.c src/libgit2/win32
	-cp -f ../libgit2/src/win32/*.h src/libgit2/win32
	-rm -f src/libgit2/win32/mingw-compat.h
	-rm -f src/libgit2/win32/msvc-compat.h
	-cp -f ../libgit2/src/xdiff/*.c src/libgit2/xdiff
	-cp -f ../libgit2/src/xdiff/*.h src/libgit2/xdiff
	-cp -f ../libgit2/AUTHORS scripts/AUTHORS_libgit2
	-cp -f ../libgit2/COPYING inst/NOTICE
	cd src/libgit2 && patch -i ../../patches/cache-pass-R-CMD-check-git2r.patch
	cd src/libgit2 && patch -i ../../patches/checkout-pass-R-CMD-check-git2r.patch
	cd src/libgit2 && patch -i ../../patches/common-remove-includes.patch
	cd src/libgit2 && patch -p0 < ../../patches/diff_print-pass-R-CMD-check-git2r.patch
	cd src/libgit2 && patch -i ../../patches/rebase-pass-R-CMD-check-git2r.patch
	cd src/libgit2 && patch -i ../../patches/transaction-pass-R-CMD-check-git2r.patch
	cd src/libgit2 && patch -i ../../patches/util-pass-R-CMD-check-git2r.patch
	cd src/regex && patch -i ../../patches/regcomp-pass-R-CMD-check-git2r.patch
	cd src/libgit2/win32 && patch -i ../../../patches/posix-pass-R-CMD-check-git2r.patch
	cd src/libgit2/transports && patch -i ../../../patches/winhttp-build-with-mingw-w64.patch
	Rscript scripts/build_Makevars.r

Makevars:
	Rscript scripts/build_Makevars.r

configure: configure.ac
	autoconf ./configure.ac > ./configure
	chmod +x ./configure

clean:
	-rm -f config.log
	-rm -f config.status
	-rm -rf autom4te.cache
	-rm -f src/Makevars
	-rm -f src/*.o
	-rm -f src/*.so
	-rm -rf src-x64
	-rm -rf src-i386
	-rm -f src/winhttp/libwinhttp-x64.a
	-rm -f src/winhttp/libwinhttp.a
	-rm -f src/libgit2/*.o
	-rm -f src/libgit2/hash/*.o
	-rm -f src/libgit2/transports/*.o
	-rm -f src/libgit2/unix/*.o
	-rm -f src/libgit2/win32/*.o
	-rm -f src/libgit2/xdiff/*.o
	-rm -f src/http-parser/*.o
	-rm -f src/regex/*.o
	-rm -f src/zlib/*.o

.PHONY: all readme install roxygen sync_libgit2 Makevars check clean
