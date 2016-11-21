/*

Using DCPT prefetch type:

dl1.accesses                  91293 # total number of accesses
dl1.hits                      91153 # total number of hits
dl1.misses                      140 # total number of misses
dl1.replacements              24835 # total number of replacements
dl1.writebacks                   60 # total number of writebacks
dl1.invalidations                 0 # total number of invalidations
dl1.miss_rate                0.0015 # miss rate (i.e., misses/ref)

Using stride prefetch type:

dl1.accesses                  91293 # total number of accesses
dl1.hits                      78679 # total number of hits
dl1.misses                    12614 # total number of misses
dl1.replacements              12462 # total number of replacements
dl1.writebacks                   60 # total number of writebacks
dl1.invalidations                 0 # total number of invalidations
dl1.miss_rate                0.1382 # miss rate (i.e., misses/ref)

The following code should access i'th elements of a, where i has values of:
i       0, 11, 16, 27, 32.........
deltas  0, 11, 5,  11, 5.........

DCPT should give nearly perfect values here as it is designed to store and record complex history patterns. The stride predictor will incorrectly swap between predciting a fixed stride of 5 and a fixed stride of 11 thereby resulting a large number of misses 
*/
int main ()
{
   int i,x,y,z;
   int a[1000004];
   for (i = 0; i < 100000; )
   {
      x = a[i];
      if (i&1)
        i+=5;
      else
        i+=11;
   }

   return 0;
}
