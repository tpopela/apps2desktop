#!/bin/bash

# Compile the NPAPI plugin
cd plugin
make
if [ $? -ne 0 ]; then
	echo "Compilation failed"
	exit;
fi
cd ..

# Copy plugin to directory that contains Chrome extensions stuff
cp plugin/apps2desktop.so apps2desktop

if [ ! -f extension.pem ]; then
	echo "Generating certificate"
	openssl genrsa -out extension.pem 1024
	if [ $? -ne 0 ]; then
		echo "Something's wrong. You have to generate extension.pem file and store it here"
		exit;
	fi
fi

# Generate .crx

./crxmake.sh apps2desktop extension.pem
