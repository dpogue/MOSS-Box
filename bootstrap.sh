#!/bin/sh

# glibtoolize if present, else, libtoolize
glibtoolize --version 2> /dev/null | grep GNU > /dev/null && mylibtoolize=glibtoolize || mylibtoolize=libtoolize

${ACLOCAL-aclocal} -I aclocal && ${AUTOCONF-autoconf} && ${AUTOHEADER-autoheader} && ${LIBTOOLIZE-$mylibtoolize} --force && ${AUTOMAKE-automake} -a --foreign
