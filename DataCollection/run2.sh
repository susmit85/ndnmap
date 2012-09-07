#!/bin/bash


while true
do
  ./getAllLinkData2.py >& getAllLinkData2.log 
  DATE=`date +%Y.%b.%d.%H.%M`
  mv getAllLinkData2.log getAllLinkData2.log.$DATE
  gzip getAllLinkData2.log.$DATE
done
