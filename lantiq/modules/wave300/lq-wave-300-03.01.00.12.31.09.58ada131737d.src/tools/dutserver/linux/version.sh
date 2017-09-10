#!/bin/sh

if [ -d .svn -a `which svnversion` != "" ]; then
  (cd ../../..;svnversion .) > vv
fi

if ! [ -f vv ]; then
  if [ -f version ]; then
    cat version
    exit
  else
    echo ""
    exit
  fi
fi

if [ -f version ] && cmp -s vv version; then
  cat version
  rm vv
  exit
fi

cat vv > version
cat version
rm vv
