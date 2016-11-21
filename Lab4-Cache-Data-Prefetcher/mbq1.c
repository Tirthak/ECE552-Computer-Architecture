/*
With next-line prefetching:
dl1.accesses                6003793 # total number of accesses
dl1.hits                    6001772 # total number of hits
dl1.misses                     2021 # total number of misses
dl1.replacements             503043 # total number of replacements
dl1.writebacks                 1046 # total number of writebacks
dl1.invalidations                 0 # total number of invalidations
dl1.miss_rate                0.0003 # miss rate (i.e., misses/ref)

If I turn off prefetching:
dl1.accesses                6003793 # total number of accesses
dl1.hits                    5501425 # total number of hits
dl1.misses                   502368 # total number of misses
dl1.replacements             500320 # total number of replacements
dl1.writebacks                  551 # total number of writebacks
dl1.invalidations                 0 # total number of invalidations
dl1.miss_rate                0.0837 # miss rate (i.e., misses/ref)
dl1.repl_rate                0.0833 # replacement rate (i.e., repls/ref)

The cache line size is 8 Bytes or 2 Integers. With no prefetching, every 2 accesses result in 1 miss whereas with a next-line prefetcher, every element access
brings the next line into the cache and should theoretically reduce the number of cache misses
*/
int main ()
{
   int i,x;
   int a[1000004];
   for (i = 0; i < 1000000; i++)
   {
      x = a[i];
   }

   return 0;
}
