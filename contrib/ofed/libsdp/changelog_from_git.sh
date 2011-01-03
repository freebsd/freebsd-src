#!/bin/sh

git log --pretty=format:"%ai %an <%ae>%n         * %h %s%n"
