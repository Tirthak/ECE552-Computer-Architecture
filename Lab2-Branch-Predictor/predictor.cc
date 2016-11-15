#include "predictor.h"
int bitsat[4096];

int papbht[512];
int pappht[8][64];

/////////////////////////////////////////////////////////////
// 2bitsat
/////////////////////////////////////////////////////////////
void InitPredictor_2bitsat() {
    // Initialize the 2 bit saturated counter to weakly not taken
    int i;
    for (i = 0; i < 4096; i++)
        bitsat[i] = 1;    
}

bool GetPrediction_2bitsat(UINT32 PC) {
	// Isolate last 12 bits to use as index so PC & (2^13 - 1)//
	unsigned int num = 4095;
	unsigned int index = PC & num;
	if(bitsat[index] == 0 || bitsat[index] == 1)
		return NOT_TAKEN;
	else
		return TAKEN;
}

void UpdatePredictor_2bitsat(UINT32 PC, bool resolveDir, bool predDir, UINT32 branchTarget) {
	unsigned int index = PC & 4095;
	int state = bitsat[index];
	if(resolveDir == NOT_TAKEN)
	{
		if(state != 0)
			bitsat[index]--;
	}
	else
	{
		if(state != 3)
			bitsat[index]++;
	}
}

/////////////////////////////////////////////////////////////
// 2level
/////////////////////////////////////////////////////////////

void InitPredictor_2level() {
   int i, j;
   for(i = 0; i < 8; i++)
      for(j = 0; j < 64; j++)
         pappht[i][j] = 1;
   
   for(i = 0; i < 512; i++)
        papbht[i] = 0;
}

bool GetPrediction_2level(UINT32 PC) {
	unsigned phtIndex = PC & 7;
	unsigned bhtIndex = (PC >> 3) & 511;
	unsigned pattern = papbht[bhtIndex];
	unsigned state = pappht[phtIndex][pattern];
	if(state == 0 || state == 1)
		return NOT_TAKEN;
	else
		return TAKEN;
}

void UpdatePredictor_2level(UINT32 PC, bool resolveDir, bool predDir, UINT32 branchTarget) {
	unsigned phtIndex = PC & 7;
	unsigned bhtIndex = (PC >> 3) & 511;
	unsigned oldPattern = papbht[bhtIndex];

	//Update the bi-modal index of the old history
	if(resolveDir == NOT_TAKEN)
	{
		if(pappht[phtIndex][oldPattern] != 0)
			pappht[phtIndex][oldPattern]--;
	}
	else
	{
	  	if(pappht[phtIndex][oldPattern] != 3)
		 	pappht[phtIndex][oldPattern]++;
	}

	// Left shift once (or mul by 2), bitwise AND with 63 (for last 6 bits)
	// and append the resolveDir to the last bit
	unsigned newPattern = ((oldPattern << 1) & 63) | (unsigned int)resolveDir;
	papbht[bhtIndex] = newPattern;  
}

/////////////////////////////////////////////////////////////
// openend
/////////////////////////////////////////////////////////////

/* REFERENCES

[1]	A. Seznec and P. Michaud, "A case for (partially) TAgged GEometric history length branch prediction," Journal of Insturction Level Parallelism, Jan. 2016. [Online].
	Available: http://www.irisa.fr/caps/people/seznec/JILP-COTTAGE.pdf. Accessed: Oct. 20, 2016.

[2] A. Seznec, "A New Case for the TAGE Branch Predictor," in The 44th Annual IEEE/ACM International Symposium on Microarchitecture, Porto Allegre, Brazil, 2011. [Online].
	Available: https://hal.inria.fr/hal-00639193/file/MICRO44_Andre_Seznec.pdf. Accessed: Oct. 20, 2016.

[3] P. Michaud, "A PPM-like, tag-based branch predictor," Journal of Instruction-Level Parallelism, vol. 7, pp. 1–10, Apr. 2005. [Online].
	Available: http://www.jilp.org/vol7/v7paper10.pdf. Accessed: Oct. 20, 2016.

*/

/* DESCRIPTION

The openended branch predictor is implemented using a TAGE predictor. The default component T0 is PaP
predictor similar to the one implemented in part 2 of this lab. It, however, has 1024 entries in its 1st level
and uses 3 bits for prediction. The rest of the TAGE tables have 1024 entires and use 11 bits for tag.

Design Decisions:

	1.	Use an 8 table TAGE, as opposed to 5, because it gives lower MPKI as mentioned in [1] and also empirically determined.
	2.	Use histories of lengths {8, 16, 32, 64, 128, 256, 512} as this combination gives the best performance.
	3.	Use 1 useful bit as suggested in [2] and it also gives the best experimental performance. This requires maintaining a
		useful counter to count the number of success and failures. All the useful bits are reset when the counter saturates.
	4.	Use a dynamic implementation of Circular Shift Registers (CSR) to fold the global history for all 7 TAGE tables. These
		CSRs are used to calculate the tags and indices of the entries. This implementation has been from [3] as recommended in [1].
	5.	Use altPred as the prediction when the predictor component has a weak prediction or is not useful. This method is used in [2].

*/

/* STORAGE ANALYSIS

T0 is a PaP predictor

1st level: 1024 entries each storing 9 bits:			1024*9		= 9216

2nd level: 8 tables each with 512 entries.
	Each entry has 3 bits for prediction:				8*512*3		= 12288

T1-T7 TAGE tables

7 tables each with 1024 entries.
	Each entry has 15 bits in total
	Tag is 11 bits, prediction is 3 and useful is 1		7*1024*15	= 107520

Useful Counter is 8 bits wide:							8			= 8

Global History Register is 512 bits:					512			= 512

Each table has two CSRs, one 11 bits and one 10 bits:	7*(21)		= 147
--------------------------------------------------------------------------------------------
														Total		= 129691 bits < 128Kbits
*/


// Macro definitions
#define NUM_TOTAL_TABLES 8
#define NUM_TAGE_TABLES 7
#define NUM_TAGE_COLUMNS 3
#define NUM_TAGE_ENTRIES 1024
#define NUM_PRED_MODES 8
#define NUM_GHR_BITS 512

#define TAG_WIDTH 11
#define T1_HIS_BITS 8
#define T2_HIS_BITS 16
#define T3_HIS_BITS 32
#define T4_HIS_BITS 64
#define T5_HIS_BITS 128
#define T6_HIS_BITS 256
#define T7_HIS_BITS 512

#define T0_PHRT_ENTRIES 1024
#define T0_PHT_TABLES 8
#define T0_PHT_ENTRIES 512
#define T0_PHT_MODES 8
#define T0_PHT_PC_BITS 3

// Global variables declarations
int T0Phrt[T0_PHRT_ENTRIES];
int T0Pht[T0_PHT_TABLES][T0_PHT_ENTRIES];

int GHR[NUM_GHR_BITS];

int T1[NUM_TAGE_ENTRIES][NUM_TAGE_COLUMNS];
int T2[NUM_TAGE_ENTRIES][NUM_TAGE_COLUMNS];
int T3[NUM_TAGE_ENTRIES][NUM_TAGE_COLUMNS];
int T4[NUM_TAGE_ENTRIES][NUM_TAGE_COLUMNS];
int T5[NUM_TAGE_ENTRIES][NUM_TAGE_COLUMNS];
int T6[NUM_TAGE_ENTRIES][NUM_TAGE_COLUMNS];
int T7[NUM_TAGE_ENTRIES][NUM_TAGE_COLUMNS];

int providerComp;
int providerPred;
int providerIndex;
int altPred;
int probability;
int useCounter;

int CSR[NUM_TAGE_TABLES][2][TAG_WIDTH];

// Functions used by PaP
// START T0 //////////////////////////////////////////////////////////
void initT0 () {
	int i, j;
	for(i = 0; i < T0_PHRT_ENTRIES; i++)
		T0Phrt[i] = 0;
	for(i = 0; i < T0_PHT_TABLES; i++)
		for(j = 0; j < T0_PHT_ENTRIES; j++)
			T0Pht[i][j] = (T0_PHT_MODES/2)-1;
}

unsigned getT0 (UINT32 PC) {
	unsigned phrtIndex = (PC >> T0_PHT_PC_BITS) & (T0_PHRT_ENTRIES-1);
	unsigned phtIndex = PC & (T0_PHT_TABLES-1);
	unsigned mode = T0Pht[phtIndex][T0Phrt[phrtIndex]];
	if (mode <= ((T0_PHT_MODES/2)-1)) return NOT_TAKEN;
	else return TAKEN;
}

void updateT0 (UINT32 PC, bool resolveDir) {
	unsigned phrtIndex = (PC >> T0_PHT_PC_BITS) & (T0_PHRT_ENTRIES-1);
	unsigned phtIndex = PC & (T0_PHT_TABLES-1);
	if ((resolveDir == NOT_TAKEN) && (T0Pht[phtIndex][T0Phrt[phrtIndex]] != 0)) T0Pht[phtIndex][T0Phrt[phrtIndex]]--;
	if ((resolveDir == TAKEN) && (T0Pht[phtIndex][T0Phrt[phrtIndex]] != (T0_PHT_MODES-1))) T0Pht[phtIndex][T0Phrt[phrtIndex]]++;
	T0Phrt[phrtIndex] = ((T0Phrt[phrtIndex] << 1) & (T0_PHT_ENTRIES-1)) | (unsigned)resolveDir;
}
// END T0 ////////////////////////////////////////////////////////////

// Functions used for updating and obtaining the CSR entries
// START CSR //////////////////////////////////////////////////////////
void updateCSR (int table, int numHisBits) {
	int CSR0MSB = CSR[table][0][TAG_WIDTH-1];
	int CSR1MSB = CSR[table][1][TAG_WIDTH-2];

	int i;
	for (i = TAG_WIDTH - 1; i > 0; i--) {
		CSR[table][0][i] = CSR[table][0][i-1];
		if (i < (TAG_WIDTH - 1))
			CSR[table][1][i] = CSR[table][1][i-1];
	}

    CSR[table][0][0] = GHR[0] ^ CSR0MSB;
    CSR[table][0][numHisBits%TAG_WIDTH] = GHR[numHisBits] ^ CSR[table][0][numHisBits%TAG_WIDTH];

    CSR[table][1][0] = GHR[0] ^ CSR1MSB;
    CSR[table][1][numHisBits%(TAG_WIDTH-1)] = GHR[numHisBits] ^ CSR[table][1][numHisBits%(TAG_WIDTH-1)];
}

int getCSR (int table, int num) {
	int i;
	int end = num?TAG_WIDTH:(TAG_WIDTH-1);
	unsigned int pow = 1;
	unsigned int sum = 0;
	for (i = 0; i < end; i++) {
		sum += pow * CSR[table][num][i];	
		pow *= 10;
	}
	return sum;
}
// END CSR ////////////////////////////////////////////////////////////

void InitPredictor_openend() {

	int i, j;

	// Initialize T0 PaP
	initT0();

	// Initialize the GHR and CSRs
	for (i = 0; i < NUM_GHR_BITS; i++) {
		GHR[i] = 0;
	}
	for (i = 0; i < NUM_TAGE_TABLES; i++) {
		for (j = 0; j < TAG_WIDTH; j++) {
			CSR[i][0][j] = 0;
			CSR[i][1][j] = 0;
		}
	}
	
	// Initialize tables T0-T7
	for (i = 0; i < NUM_TAGE_ENTRIES; i++) {
		T1[i][0] = (NUM_PRED_MODES/2)-1;
		T1[i][1] = 0;
		T1[i][2] = 0;
		T2[i][0] = (NUM_PRED_MODES/2)-1;
		T2[i][1] = 0;
		T2[i][2] = 0;
		T3[i][0] = (NUM_PRED_MODES/2)-1;
		T3[i][1] = 0;
		T3[i][2] = 0;
		T4[i][0] = (NUM_PRED_MODES/2)-1;
		T4[i][1] = 0;
		T4[i][2] = 0;
		T5[i][0] = (NUM_PRED_MODES/2)-1;
		T5[i][1] = 0;
		T5[i][2] = 0;
		T6[i][0] = (NUM_PRED_MODES/2)-1;
		T6[i][1] = 0;
		T6[i][2] = 0;
		T7[i][0] = (NUM_PRED_MODES/2)-1;
		T7[i][1] = 0;
		T7[i][2] = 0;
	}

	// Initialize other global variables
	providerComp = NULL;
	providerPred = NULL;
	providerIndex = NULL;
	altPred = NULL;
	probability = 0;
	useCounter = 128;
}

bool GetPrediction_openend(UINT32 PC) {

	int i, j;

	// Set T0 as the default predictor component
	providerComp = 0;
	providerPred = getT0(PC);
	providerIndex = -1;
	altPred = getT0(PC);

	int pred[NUM_TOTAL_TABLES] = {providerPred, -1, -1, -1, -1, -1, -1, -1};
	int index[NUM_TOTAL_TABLES] = {NULL, -1, -1, -1, -1, -1, -1, -1};
	int useful[NUM_TOTAL_TABLES] = {NULL, 0, 0, 0, 0, 0, 0, 0};

	// Calculate tags
	unsigned int tagT1, tagT2, tagT3, tagT4, tagT5, tagT6, tagT7;
	tagT1 = (PC ^ getCSR(0,0) ^ (getCSR(0,1) << 1)) & (NUM_TAGE_ENTRIES-1);
	tagT2 = (PC ^ getCSR(1,0) ^ (getCSR(1,1) << 1)) & (NUM_TAGE_ENTRIES-1);
	tagT3 = (PC ^ getCSR(2,0) ^ (getCSR(2,1) << 1)) & (NUM_TAGE_ENTRIES-1);
	tagT4 = (PC ^ getCSR(3,0) ^ (getCSR(3,1) << 1)) & (NUM_TAGE_ENTRIES-1);
	tagT5 = (PC ^ getCSR(4,0) ^ (getCSR(4,1) << 1)) & (NUM_TAGE_ENTRIES-1);
	tagT6 = (PC ^ getCSR(5,0) ^ (getCSR(5,1) << 1)) & (NUM_TAGE_ENTRIES-1);
	tagT7 = (PC ^ getCSR(6,0) ^ (getCSR(6,1) << 1)) & (NUM_TAGE_ENTRIES-1);
	
	// Calculate indices
	unsigned int indT1, indT2, indT3, indT4, indT5, indT6, indT7;
	indT1 = (PC ^ (PC >> TAG_WIDTH) ^ getCSR(0,1)) & (NUM_TAGE_ENTRIES-1);
	indT2 = (PC ^ (PC >> TAG_WIDTH) ^ getCSR(1,1)) & (NUM_TAGE_ENTRIES-1);
	indT3 = (PC ^ (PC >> TAG_WIDTH) ^ getCSR(2,1)) & (NUM_TAGE_ENTRIES-1);
	indT4 = (PC ^ (PC >> TAG_WIDTH) ^ getCSR(3,1)) & (NUM_TAGE_ENTRIES-1);
	indT5 = (PC ^ (PC >> TAG_WIDTH) ^ getCSR(4,1)) & (NUM_TAGE_ENTRIES-1);
	indT6 = (PC ^ (PC >> TAG_WIDTH) ^ getCSR(5,1)) & (NUM_TAGE_ENTRIES-1);
	indT7 = (PC ^ (PC >> TAG_WIDTH) ^ getCSR(6,1)) & (NUM_TAGE_ENTRIES-1);

	// Check if the entry exists in any of the tables
	if (T1[indT1][1] == tagT1) {
		pred[1] = T1[indT1][0];
		index[1] = indT1;
		useful[1] = T1[indT1][2];
	}
	if (T2[indT2][1] == tagT2) {
		pred[2] = T2[indT2][0];
		index[2] = indT2;
		useful[2] = T2[indT2][2];
	}
	if (T3[indT3][1] == tagT3) {
		pred[3] = T3[indT3][0];
		index[3] = indT3;
		useful[3] = T3[indT3][2];
	}
	if (T4[indT4][1] == tagT4) {
		pred[4] = T4[indT4][0];
		index[4] = indT4;
		useful[4] = T4[indT4][2];
	}
	if (T5[indT5][1] == tagT5) {
		pred[5] = T5[indT5][0];
		index[5] = indT5;
		useful[5] = T5[indT5][2];
	}
	if (T6[indT6][1] == tagT6) {
		pred[6] = T6[indT6][0];
		index[6] = indT6;
		useful[6] = T6[indT6][2];
	}
	if (T7[indT7][1] == tagT7) {
		pred[7] = T7[indT7][0];
		index[7] = indT7;
		useful[7] = T7[indT7][2];
	}

	// Provide prediction from the highest component. If weak, provide altPred.
	bool weak = false;
	for(i = NUM_TAGE_TABLES; i > 0; i--) {
		if (pred[i] != -1) {
			providerComp = i;
			providerPred = (pred[i]<(NUM_PRED_MODES/2))?NOT_TAKEN:TAKEN;
			providerIndex = index[i];
			if (((pred[i] > 2) && (pred[i] < 5)) || (useful[i] == 0)) weak = true;
			for(j = i - 1; j > 0; j--) {
				if (pred[j] != -1) {
					altPred = (pred[i]<(NUM_PRED_MODES/2))?NOT_TAKEN:TAKEN;
					break;
				}
			}
			break;
		}
	}

	return weak?altPred:providerPred;
}

void UpdatePredictor_openend(UINT32 PC, bool resolveDir, bool predDir, UINT32 branchTarget) {
	
	int i;

	// Update T0 PaP
	updateT0(PC, resolveDir);

	int useful = 0, pred = 0;
	if (providerComp != 0) {
		// Always update prediction bits for the provider component
		if (resolveDir == TAKEN) {
			if ((providerComp == 1) && (T1[providerIndex][0] != 7)) { T1[providerIndex][0]++; useful = T1[providerIndex][2]; pred = T1[providerIndex][0]; }
			if ((providerComp == 2) && (T2[providerIndex][0] != 7)) { T2[providerIndex][0]++; useful = T2[providerIndex][2]; pred = T2[providerIndex][0]; }
			if ((providerComp == 3) && (T3[providerIndex][0] != 7)) { T3[providerIndex][0]++; useful = T3[providerIndex][2]; pred = T3[providerIndex][0]; }
			if ((providerComp == 4) && (T4[providerIndex][0] != 7)) { T4[providerIndex][0]++; useful = T4[providerIndex][2]; pred = T4[providerIndex][0]; }
			if ((providerComp == 5) && (T5[providerIndex][0] != 7)) { T5[providerIndex][0]++; useful = T5[providerIndex][2]; pred = T5[providerIndex][0]; }
			if ((providerComp == 6) && (T6[providerIndex][0] != 7)) { T6[providerIndex][0]++; useful = T6[providerIndex][2]; pred = T6[providerIndex][0]; }
			if ((providerComp == 7) && (T7[providerIndex][0] != 7)) { T7[providerIndex][0]++; useful = T7[providerIndex][2]; pred = T7[providerIndex][0]; }
		} else {
			if ((providerComp == 1) && (T1[providerIndex][0] != 0)) { T1[providerIndex][0]--; useful = T1[providerIndex][2]; pred = T1[providerIndex][0]; }
			if ((providerComp == 2) && (T2[providerIndex][0] != 0)) { T2[providerIndex][0]--; useful = T2[providerIndex][2]; pred = T2[providerIndex][0]; }
			if ((providerComp == 3) && (T3[providerIndex][0] != 0)) { T3[providerIndex][0]--; useful = T3[providerIndex][2]; pred = T3[providerIndex][0]; }
			if ((providerComp == 4) && (T4[providerIndex][0] != 0)) { T4[providerIndex][0]--; useful = T4[providerIndex][2]; pred = T4[providerIndex][0]; }
			if ((providerComp == 5) && (T5[providerIndex][0] != 0)) { T5[providerIndex][0]--; useful = T5[providerIndex][2]; pred = T5[providerIndex][0]; }
			if ((providerComp == 6) && (T6[providerIndex][0] != 0)) { T6[providerIndex][0]--; useful = T6[providerIndex][2]; pred = T6[providerIndex][0]; }
			if ((providerComp == 7) && (T7[providerIndex][0] != 0)) { T7[providerIndex][0]--; useful = T7[providerIndex][2]; pred = T7[providerIndex][0]; }
		}

		// Update useful bit according to the method describes in [2]
		if ((providerPred == resolveDir) && (providerPred != altPred)) {
			if (providerComp == 1) T1[providerIndex][2]= 1;
			if (providerComp == 2) T2[providerIndex][2]= 1;
			if (providerComp == 3) T3[providerIndex][2]= 1;
			if (providerComp == 4) T4[providerIndex][2]= 1;
			if (providerComp == 5) T5[providerIndex][2]= 1;
			if (providerComp == 6) T6[providerIndex][2]= 1;
			if (providerComp == 7) T7[providerIndex][2]= 1;
			if (useCounter != 255) useCounter++;
		} else if ((providerPred != resolveDir) && (providerPred != altPred)) {
			if (providerComp == 1) T1[providerIndex][2]= 0;
			if (providerComp == 2) T2[providerIndex][2]= 0;
			if (providerComp == 3) T3[providerIndex][2]= 0;
			if (providerComp == 4) T4[providerIndex][2]= 0;
			if (providerComp == 5) T5[providerIndex][2]= 0;
			if (providerComp == 6) T6[providerIndex][2]= 0;
			if (providerComp == 7) T7[providerIndex][2]= 0;
			if (useCounter != 0) useCounter--;
		}
	}
	
	if ((providerComp != NUM_TAGE_TABLES) && ((resolveDir != providerPred) && !((useful == 0) && (pred > 2) && (pred < 5)))) {

		int initialPred = resolveDir?4:3;
		bool found[NUM_TAGE_TABLES] = {false, false, false, false, false, false, false};

		// Calculate tags
		unsigned int tagT1, tagT2, tagT3, tagT4, tagT5, tagT6, tagT7;
		tagT1 = (PC ^ getCSR(0,0) ^ (getCSR(0,1) << 1)) & (NUM_TAGE_ENTRIES-1);
		tagT2 = (PC ^ getCSR(1,0) ^ (getCSR(1,1) << 1)) & (NUM_TAGE_ENTRIES-1);
		tagT3 = (PC ^ getCSR(2,0) ^ (getCSR(2,1) << 1)) & (NUM_TAGE_ENTRIES-1);
		tagT4 = (PC ^ getCSR(3,0) ^ (getCSR(3,1) << 1)) & (NUM_TAGE_ENTRIES-1);
		tagT5 = (PC ^ getCSR(4,0) ^ (getCSR(4,1) << 1)) & (NUM_TAGE_ENTRIES-1);
		tagT6 = (PC ^ getCSR(5,0) ^ (getCSR(5,1) << 1)) & (NUM_TAGE_ENTRIES-1);
		tagT7 = (PC ^ getCSR(6,0) ^ (getCSR(6,1) << 1)) & (NUM_TAGE_ENTRIES-1);

		// Calculate indices
		unsigned int indT1, indT2, indT3, indT4, indT5, indT6, indT7;
		indT1 = (PC ^ (PC >> TAG_WIDTH) ^ getCSR(0,1)) & (NUM_TAGE_ENTRIES-1);
		indT2 = (PC ^ (PC >> TAG_WIDTH) ^ getCSR(1,1)) & (NUM_TAGE_ENTRIES-1);
		indT3 = (PC ^ (PC >> TAG_WIDTH) ^ getCSR(2,1)) & (NUM_TAGE_ENTRIES-1);
		indT4 = (PC ^ (PC >> TAG_WIDTH) ^ getCSR(3,1)) & (NUM_TAGE_ENTRIES-1);
		indT5 = (PC ^ (PC >> TAG_WIDTH) ^ getCSR(4,1)) & (NUM_TAGE_ENTRIES-1);
		indT6 = (PC ^ (PC >> TAG_WIDTH) ^ getCSR(5,1)) & (NUM_TAGE_ENTRIES-1);
		indT7 = (PC ^ (PC >> TAG_WIDTH) ^ getCSR(6,1)) & (NUM_TAGE_ENTRIES-1);

		// Check how many available entries are found
		int numFound = 0;
		if ((providerComp <= 0) && (T1[indT1][2] == 0)) { found[0] = true; numFound++; }
		if ((providerComp <= 1) && (T2[indT2][2] == 0)) { found[1] = true; numFound++; }
		if ((providerComp <= 2) && (T3[indT3][2] == 0) && (numFound < 2)) { found[2] = true; numFound++; }
		if ((providerComp <= 3) && (T4[indT4][2] == 0) && (numFound < 2)) { found[3] = true; numFound++; }
		if ((providerComp <= 4) && (T5[indT5][2] == 0) && (numFound < 2)) { found[4] = true; numFound++; }
		if ((providerComp <= 5) && (T6[indT6][2] == 0) && (numFound < 2)) { found[5] = true; numFound++; }
		if ((providerComp <= 6) && (T7[indT7][2] == 0) && (numFound < 2)) { found[6] = true; numFound++; }

		// If an empty entry is not found, reset all the useful bits
		// If only one entry is found, allocate it
		// If multiple entries are found, use 1/2 probability to allocate a higher table
		// and 1/2 probability to occupy the lower-numbered table as suggested it gives better results.
		if (numFound == 0) {
			if (providerComp <= 0)
				for (i = 0; i < NUM_TAGE_ENTRIES; i++)
					T1[i][2] = 0;
			if (providerComp <= 1)
				for (i = 0; i < NUM_TAGE_ENTRIES; i++)
					T2[i][2] = 0;
			if (providerComp <= 2)
				for (i = 0; i < NUM_TAGE_ENTRIES; i++)
					T3[i][2] = 0;
			if (providerComp <= 3)
				for (i = 0; i < NUM_TAGE_ENTRIES; i++)
					T4[i][2] = 0;
			if (providerComp <= 4)
				for (i = 0; i < NUM_TAGE_ENTRIES; i++)
					T5[i][2] = 0;
			if (providerComp <= 5)
				for (i = 0; i < NUM_TAGE_ENTRIES; i++)
					T6[i][2] = 0;
			if (providerComp <= 6)
				for (i = 0; i < NUM_TAGE_ENTRIES; i++)
					T7[i][2] = 0;
		} else if ((numFound == 1) || ((numFound > 1) && (probability < 2))) {
			if (found[0] == true) {
				T1[indT1][0] = initialPred;
				T1[indT1][1] = tagT1;
				T1[indT1][2] = 0;
			} else if (found[1] == true) {
				T2[indT2][0] = initialPred;
				T2[indT2][1] = tagT2;
				T2[indT2][2] = 0;
			} else if (found[2] == true) {
				T3[indT3][0] = initialPred;
				T3[indT3][1] = tagT3;
				T3[indT3][2] = 0;
			} else if (found[3] == true) {
				T4[indT4][0] = initialPred;
				T4[indT4][1] = tagT4;
				T4[indT4][2] = 0;
			} else if (found[4] == true) {
				T5[indT5][0] = initialPred;
				T5[indT5][1] = tagT5;
				T5[indT5][2] = 0;
			} else if (found[5] == true) {
				T6[indT6][0] = initialPred;
				T6[indT6][1] = tagT6;
				T6[indT6][2] = 0;
			} else if (found[6] == true) {
				T7[indT7][0] = initialPred;
				T7[indT7][1] = tagT7;
				T7[indT7][2] = 0;
			}
			probability++;
		} else if ((numFound > 1) && (probability >= 2)) {
			probability = 1;
			if (found[6] == true) {
				T7[indT7][0] = initialPred;
				T7[indT7][1] = tagT7;
				T7[indT7][2] = 0;
			} else if (found[5] == true) {
				T6[indT6][0] = initialPred;
				T6[indT6][1] = tagT6;
				T6[indT6][2] = 0;
			} else if (found[4] == true) {
				T5[indT5][0] = initialPred;
				T5[indT5][1] = tagT5;
				T5[indT5][2] = 0;
			} else if (found[3] == true) {
				T4[indT4][0] = initialPred;
				T4[indT4][1] = tagT4;
				T4[indT4][2] = 0;
			} else if (found[2] == true) {
				T3[indT3][0] = initialPred;
				T3[indT3][1] = tagT3;
				T3[indT3][2] = 0;
			} else if (found[1] == true) {
				T2[indT2][0] = initialPred;
				T2[indT2][1] = tagT2;
				T2[indT2][2] = 0;
			} else if (found[0] == true) {
				T1[indT1][0] = initialPred;
				T1[indT1][1] = tagT1;
				T1[indT1][2] = 0;
			} 
		}
	}

	// Reset all the useful bits when the useful counter is sturated
	if (useCounter == 0) {
		for (i = 0; i < NUM_TAGE_ENTRIES; i++) {
			T1[i][2] = 0;
			T2[i][2] = 0;
			T3[i][2] = 0;
			T4[i][2] = 0;
			T5[i][2] = 0;
			T6[i][2] = 0;
			T7[i][2] = 0;
		}
		useCounter = 128;
	}

	// Update the GHR and the CSRs for each table
	for (i = NUM_GHR_BITS - 1; i > 0; i--) {
		GHR[i] = GHR[i-1];
	}
	GHR[0] = resolveDir;

	updateCSR(0, T1_HIS_BITS-1);
	updateCSR(1, T2_HIS_BITS-1);
	updateCSR(2, T3_HIS_BITS-1);
	updateCSR(3, T4_HIS_BITS-1);
	updateCSR(4, T5_HIS_BITS-1);
	updateCSR(5, T6_HIS_BITS-1);
	updateCSR(6, T7_HIS_BITS-1);
}

