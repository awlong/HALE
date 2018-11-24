#!/bin/sh
hale_script=./HALE
data_dir=data
log_save_dir=$data_dir/logs
pik_save_dir=$data_dir/piks
num_samples=100

mkdir -p $log_save_dir
mkdir -p $pik_save_dir

for (( i=0; i < $num_samples; ++i ))
do
    date
    echo "Running Simulation $i"
    $hale_script > $log_save_dir/$i.txt
    echo "Augmenting Dataset"
    python parser.py $log_save_dir/$i.txt $pik_save_dir
    echo ""
done
