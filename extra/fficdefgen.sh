#!/bin/sh

# 1. cleanup irrelevant preprocessor instructions first
# 2. then leave only between \cond ffi .. \endcond ffi 
# 3. and final preprocessor to remove comments
sed -e '/^#/d' -e '/^$/d' | \
sed -n '/^\/\*\* \\cond ffi \*\/$/,/^\/\*\* \\endcond ffi \*\/$/P' | \
cc -E -P -
