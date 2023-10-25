#!/bin/bash

cd /mlserver/lyrics
git add .
git commit -m "Auto add lyrics on $(date +%F)"
git pull origin master --rebase
git push origin master
