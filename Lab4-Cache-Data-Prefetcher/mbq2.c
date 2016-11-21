/*
Using stride prefetch policy:
dl1.accesses                3003793 # total number of accesses
dl1.hits                    3002949 # total number of hits
dl1.misses                      844 # total number of misses
dl1.replacements             500330 # total number of replacements
dl1.writebacks                  552 # total number of writebacks
dl1.invalidations                 0 # total number of invalidations
dl1.miss_rate                0.0003 # miss rate (i.e., misses/ref)
dl1.repl_rate                0.1666 # replacement rate (i.e., repls/ref)

Using no prefetch policy:
dl1.accesses                3003793 # total number of accesses
dl1.hits                    2501913 # total number of hits
dl1.misses                   501880 # total number of misses
dl1.replacements             499832 # total number of replacements
dl1.writebacks                  307 # total number of writebacks
dl1.invalidations                 0 # total number of invalidations
dl1.miss_rate                0.1671 # miss rate (i.e., misses/ref)
dl1.repl_rate                0.1664 # replacement rate (i.e., repls/ref)

With no prefetcing, we expect every access with a stride of 2 to result in a miss (as only 2 consecutive elements fit in a cache line). With the stride predictor, we expect the predictor to record the pattern and prefetch the next element that is 2 elements ahead of the current onE 
*/
int main ()
{
   int i,x;
   int a[1000004];
   for (i = 0; i < 1000000; i+=2)
   {
      x = a[i];
   }

   return 0;
}
