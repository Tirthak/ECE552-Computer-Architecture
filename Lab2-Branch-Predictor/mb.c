//#include <stdio.h>
/*
	This microbenchmark tests the implementation of our 2level PAP predictor.
	Since the number history bits in our private history register (phr) is 6, we notice that for 
	branches that have a fixed pattern that can be fit in 6 bits, the 2level predictor is able
	to adapt and understand and predict all the future Taken/Not Taken values. 

	But if the branch has a fixed history pattern that can only be fit in > 6 bits, the 2level
	predictor will struggle to adapt and cause a lot of mispredictions.

	In the following program, we have around 1810 mispredictions that are pre-generated and are
	extraneous to our purpose.

	a.	The first test, branches every 7 iterations of the loop. Therefore, the steady state pattern will
		look as such
	
		N, N, N, N, N, N, T
	
		Subsequently, all the requisite patterns can be fit within a 6 bit history. 	

		This provides the following results:
			NUM_MISPREDICTIONS	: 1819
			MISPRED_PER_1K_INST	: 0.656

	b.	The first test, branches every 8 iterations of the loop. Therefore, the steady state pattern will
		look as such
	
		N, N, N, N, N, N, N, T
	
		But this steady state pattern cannot be fit within a 6 bit history. 
		As N, N, N, N, N, N can lead to N in one case and T in the other case.
	
		This provides the following results:
			NUM_MISPREDICTIONS	: 14316
			MISPRED_PER_1K_INST	: 10.426


	These results can be obtained by commenting out the other test and running each individually.	

*/


int main ()
{
   int i,x;

   for (i = 0; i < 100000; i++)
   {
	//  TEST 1 The Assembly code is as follows
	// 	testl	%eax, %eax
	//	je	.L3
	//	movl $10, -8(%rbp)

	if (i % 7 != 0)
		x = 10;

	// TEST 2 The Assembly code is as follows
	// testl	%eax, %eax
	// je	.L4
	// movl	$5, -8(%rbp)
	if (i % 8 != 0)
		x = 5;
   }

   return 0;
}
