#! /bin/csh

# Set an alias for florating point operations
alias FLOAT 'set \!:1 = `echo "\!:3-$" | bc -l`' 

# Remove existing reports
if (-f results.rpt) then
	rm results.rpt
endif
if (-f summary.rpt) then
	rm summary.rpt
endif

# Run the benchmarks
set files = /cad2/ece552f/cbp4_benchmarks/*.gz
foreach file ($files)
	predictor $file >> results.rpt
end

# Read the results
set lines1 = `grep "openend: NUM_MISPREDICTIONS" results.rpt | cut -d ":" -f3`
set lines2 = `grep "openend: MISPRED_PER_1K_INST" results.rpt | cut -d ":" -f3`

# Output the results
set index = 1
set total = 0
echo "************* RESULTS SUMMARY **************" >> summary.rpt
echo "" >> summary.rpt
foreach file ($files)
	echo "Benchmark:	$file" >> summary.rpt
	set num1 = `echo $lines1 | cut -d " " -f $index`
	echo "Num Mispred:	$num1" >> summary.rpt
	set num2 = `echo $lines2 | cut -d " " -f $index`
	echo "MPKI:		$num2" >> summary.rpt
	echo "" >> summary.rpt
	set index = `expr "$index" + 1`
	FLOAT total = $total + $num2
end

# Calculate and output the avrage MPKI
FLOAT avg = $total / 8
echo "AVERAGE = $avg" >> summary.rpt
