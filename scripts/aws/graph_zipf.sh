tar czvf csv.tgz *csv
: ${janus:=/home/novemser/janus}
echo "janus dir is: $janus"
$janus/scripts/aggregate_run_output.py --prefix $1 *yml
$janus/scripts/make_graphs '*csv' . $janus
