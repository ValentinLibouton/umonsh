#! /bin/bash

if ! [[ -x umonsh ]]; then
    echo "umonsh executable does not exist"
    exit 1
fi

./run-tests.sh -c -v $*
