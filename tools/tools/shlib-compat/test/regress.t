#!/bin/sh

cd `dirname $0`

m4 regress.m4 regress.sh | sh
