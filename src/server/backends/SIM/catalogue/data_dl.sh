#!/bin/bash
# extract HI spectral data from https://www.astro.uni-bonn.de/hisurvey/AllSky_profiles/
# 0.5 deg steps, 0.5 deg beam

if [ ! -d dl/ ]; then
  mkdir dl/;
fi

b=$(echo "-90.0");

while [ $(echo "90.0 >= $b" |bc) -gt 0 ]; do

        l=$(echo "0.0");
        while [  $(echo "360.0 >= $l" |bc) -gt 0 ]; do

                echo $l $b
                wget "https://www.astro.uni-bonn.de/hisurvey/AllSky_profiles/download.php?ral=${l}&decb=${b}&csys=0&beam=0.500" -O dl/${l}_${b}.txt
		# insert random timeout to reduce server load
                sleep $[ ( $RANDOM % 10 )  + 1 ]s
                l=$(echo "$l + 0.5" | bc)
        done

        b=$(echo "$b + 0.5" | bc)
done
