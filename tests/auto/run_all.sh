#!/bin/bash

for i in tests/*.py;
do
	./runtest.py "${i}" $@
	if [ "$?" != "0" ]; then
		exit 1
	fi
	echo 
done
