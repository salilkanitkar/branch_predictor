#!/bin/sh

make clean ; make 

for ((i=7 ; i<=12 ; i++)) ; do
	for ((j=2 ; j<=$i ; j=j+2)) ; do
		rm -f op.$i.$j
	done;
done;

for file in gcc jpeg perl ; do
	rm -f gshare.points.$file
done;

for file in gcc jpeg perl ; do
	for ((i=7 ; i<=12 ; i++)) ; do 
		for ((j=2 ; j<=$i ; j=j+2)) ; do
			trace_file="traces/"$file"_trace.txt"
			./sim gshare $i $j $trace_file > op.$i.$j
			mispred_rate=`cat op.$i.$j | grep "misprediction rate" | awk '{print $3}' | cut -d '%' -f 1`
			echo "$i $j $mispred_rate" >> gshare.points.$file
			# echo "$i $j $trace_file"
		done;
	done;
done;

for ((i=7 ; i<=12 ; i++)) ; do
	for ((j=2 ; j<=$i ; j=j+2)) ; do
		rm -f op.$i.$j
	done;
done;

make clean ;

