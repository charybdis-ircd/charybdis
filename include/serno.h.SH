#!/bin/sh

revh=`git log -1 --date=format:%Y%m%d --pretty=format:%cd-%h`
datecode=`git log -1 --pretty=format:%ct`
if [ -n "$$revh" ]; then
	echo "#define SERNO \"$revh\"" > $1
	echo "#define DATECODE ${datecode}UL" >> $1
else
	echo '#define SERNO "unknown"' > $1
	echo '#define DATECODE 0UL' >> $1
fi
