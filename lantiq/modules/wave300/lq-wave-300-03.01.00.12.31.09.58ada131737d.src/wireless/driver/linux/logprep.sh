#!/bin/sh

# Get last argument supplied:
# for last; do :; done
# Optimized version (warning: deletes commandline args)
eval "set X "\$@"; shift $#; last=\$1"

# WARNING: O(rigin)ID = 0 means Driver. 
# Other components (MAC, apps etc.) must be compiled with other OIDs.
echo "  LOGPREP" $last
$(dirname "$0")/../../../tools/rtlogger/logprep/logprep --silent --sid-no-reuse --oid 0 --workdir "$(dirname "$0")" $last
# --silent -d2 --preserve-dt

