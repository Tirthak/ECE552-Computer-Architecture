Overview:
============

Uses the Championship Branch Predictor to simulate the the number of mispredictions  
per thousand instructions (MPKI) of the following predictors:  
* An 8K Bimodal Predictor with a 2-bit saturating counter
* A 4K PAp Predictor with 512 Branch History Entries and 8 Private History Tables
* A 128K TAGE Predictor that uses PAp as it's base predictor (T0) 

The above implementations can be viewed in greater detail in <code>predictor.cc</code>

To compile:  
============

type make


To run:
===========

./predictor <TRACE_FILE_PATH>

Results:
===========

Across the 8 benchmarks tested, the average MPKI observed was as follows:
* BiModal: 14.49
* PAp: 10.65
* TAGE: 5.81
Detailed results can be viewed in <code>report.pdf</code>

Area, Delay and Power Measurements
===========

Using CACTI, we modeled each of our Branch predictors as Cache tags to observe the 
effects of our structure on metrics like area, delay, power consumption, leakage and so on  
These can be observed in <code>{file}.cfg</code> and the results are in <code>report.pdf</code>


