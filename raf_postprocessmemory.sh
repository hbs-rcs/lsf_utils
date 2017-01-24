#!/bin/bash


if [[ ! -z $1 ]]
then
   PERIOD=$1
   echo "Running over logs $PERIOD*.log"
else
   echo "Running over all logs"
fi


echo  "Username    N_jobs   Total_Runtime[hr]   WAvgUnusMem[GB]  SUM(RTime[hr]*UnMem[GB]^2)/1K " |awk '{printf "\n \n %10s  %10s  %20s  %20s  %40s\n", $1,$2,$3,$4,$5}'

for uu in `cat $PERIOD*.log|grep -v Username|awk '{print $1}'|sort -u`; do echo $uu|awk '{printf "%10s ",$1}';cat $PERIOD*.log|grep -v Username|grep $uu|awk '{jobs+=$2; if ($3 == 0) $3=0.01; Truntime+=$3; AvgMEM+=$4*$3; SIdx+=$5} END {printf "%10s  %20s  %20s  %40s\n",jobs, int(Truntime), int(AvgMEM/Truntime), SIdx}';done|sort -n -r -k5

#echo $PERIOD
