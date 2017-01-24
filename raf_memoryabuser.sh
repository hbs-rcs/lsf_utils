#!/bin/bash

#check we are on boobam

if [[ $HOSTNAME != "boobam" ]];
then
  echo "This script must be executed on boobam"
  exit 1
fi

# mysql query dump information for previous day on TEMPLOG
#echo "extracting info from cacti"
mysql -u ritg -h localhost --password=[PASSWORD] --database='cacti' --execute="select user, run_time,greatest(mem_used,max_memory)/1024,num_cpus*greatest(mem_requested,mem_reserved)/1024 from grid_jobs_finished where end_time > '`date -d "- 1 day" +%Y-%m-%d`' and end_time < '`date +%Y-%m-%d`' and exitStatus = '0'" > /groups/ritg/watch_memory/TEMPLOG


#echo "processing info from cacti"
echo  "Username    N_jobs   Total_Runtime[hr]   AvgUnusMem[GB]  SUM(RTime[hr]*UnMem[GB]^2)/1K " |awk '{printf "\n \n %10s  %10s  %20s  %20s  %40s\n", $1,$2,$3,$4,$5}'

#process the TEMPLOG file
for user in `cat /groups/ritg/watch_memory/TEMPLOG |grep -v greatest|awk '{if (5*$3 < $4 && $4 > 30000 && $2 > 60) print $0}' |awk '{print $1}'|sort -u`; do Offend=`cat /groups/ritg/watch_memory/TEMPLOG |grep -v greatest|grep $user|awk '{if (5*$3 < $4 && $4 > 30000 && $2 > 60) print $0}' |wc -l`; TotRun=`cat /groups/ritg/watch_memory/TEMPLOG |grep -v greatest|grep $user|awk '{if (5*$3 < $4 && $4 > 30000 && $2 > 60) print $0}' |awk '{sumruntime+=$2; Wavgextramem+=($4-$3)*$2; badidx+=$2/3600*(($4-$3)/1024)^2 } END {print sumruntime/3600 "  " Wavgextramem/(sumruntime*1024) "   "badidx/1000  }'`; echo "$user $Offend  $TotRun"|awk '{printf "%10s  %10s  %20s  %20s  %40s\n", $1,$2,int($3),int($4),int($5)}';done|sort -n -k5
