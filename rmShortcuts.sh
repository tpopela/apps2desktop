#!/bin/bash

SAVEIFS=$IFS
IFS=$(echo -en "\n\b")

for f in ~/.local/share/applications/*.desktop
do
	out=`grep X-App-Version $f`
	if [ ! -z "$out" ]; then
		rm $f
	fi
done

IFS=$SAVEIFS
