#!/bin/bash


while true
do
  ./getAllLinkData1.py >& getAllLinkData1.log 
  DATE=`date +%Y.%b.%d.%H.%M`
  mv getAllLinkData1.log getAllLinkData1.log.$DATE
  gzip getAllLinkData1.log.$DATE
done
