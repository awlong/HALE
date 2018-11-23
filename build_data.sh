#!/bin/sh
hale_script=./HALE
data_path=data
json_path=$data_path/jsons
num_samples=2

for (( i=0; i < $num_samples; ++i ))
do
    date
    echo "Running Simulation $i"
    $hale_script > $data_path/$i.txt
    echo "Augmenting Dataset"
    python logparser.py $data_path/$i.txt $json_path
    echo ""
done
