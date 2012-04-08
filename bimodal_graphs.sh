#!/bin/sh

make clean ; make 

for ((i=7 ; i<=12 ; i++)) ; do
	rm -f op.$i
done;

for j in gcc jpeg perl ; do
	rm -f bimodal.points.$j
done;

for j in gcc jpeg perl ; do
	for ((i=7 ; i<=12 ; i++)) ; do
		trace_file="traces/"$j"_trace.txt"
		./sim bimodal $i $trace_file > op.$i
		mispred_rate=`cat op.$i | grep "misprediction rate" | awk '{print $3}' | cut -d '%' -f 1`
		echo "$i $mispred_rate" >> bimodal.points.$j
	done;
done;

for ((i=7 ; i<=12 ; i++)) ; do
	rm -f op.$i
done;

make clean ; 

