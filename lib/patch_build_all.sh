#!/bin/sh

sh patch_build_libev.sh

which mysql_config || echo "can't find mysql_config" 
which mysql_config || exit 1
sh patch_build_libzdb.sh


echo "done!"


