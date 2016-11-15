
#include <limits.h>
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "host.h"
#include "misc.h"
#include "machine.h"
#include "regs.h"
#include "memory.h"
#include "loader.h"
#include "syscall.h"
#include "dlite.h"
#include "options.h"
#include "stats.h"
#include "sim.h"
#include "decode.def"

#include "instr.h"

/* PARAMETERS OF THE TOMASULO'S ALGORITHM */

#define INSTR_QUEUE_SIZE         10

#define RESERV_INT_SIZE    4
#define RESERV_FP_SIZE     2
#define FU_INT_SIZE        2
#define FU_FP_SIZE         1

#define FU_INT_LATENCY     4
#define FU_FP_LATENCY      9

/* IDENTIFYING INSTRUCTIONS */

//unconditional branch, jump or call
#define IS_UNCOND_CTRL(op) (MD_OP_FLAGS(op) & F_CALL || \
                         MD_OP_FLAGS(op) & F_UNCOND)

//conditional branch instruction
#define IS_COND_CTRL(op) (MD_OP_FLAGS(op) & F_COND)

//floating-point computation
#define IS_FCOMP(op) (MD_OP_FLAGS(op) & F_FCOMP)

//integer computation
#define IS_ICOMP(op) (MD_OP_FLAGS(op) & F_ICOMP)

//load instruction
#define IS_LOAD(op)  (MD_OP_FLAGS(op) & F_LOAD)

//store instruction
#define IS_STORE(op) (MD_OP_FLAGS(op) & F_STORE)

//trap instruction
#define IS_TRAP(op) (MD_OP_FLAGS(op) & F_TRAP) 

#define USES_INT_FU(op) (IS_ICOMP(op) || IS_LOAD(op) || IS_STORE(op))
#define USES_FP_FU(op) (IS_FCOMP(op))

#define WRITES_CDB(op) (IS_ICOMP(op) || IS_LOAD(op) || IS_FCOMP(op))

/* FOR DEBUGGING */

//prints info about an instruction
#define PRINT_INST(out,instr,str,cycle)	\
  myfprintf(out, "%d: %s", cycle, str);		\
  md_print_insn(instr->inst, instr->pc, out); \
  myfprintf(stdout, "(%d)\n",instr->index);

#define PRINT_REG(out,reg,str,instr) \
  myfprintf(out, "reg#%d %s ", reg, str);	\
  md_print_insn(instr->inst, instr->pc, out); \
  myfprintf(stdout, "(%d)\n",instr->index);

/* VARIABLES */

//instruction queue for tomasulo
static instruction_t* instr_queue[INSTR_QUEUE_SIZE];
//number of instructions in the instruction queue
static int instr_queue_size = 0;

//reservation stations (each reservation station entry contains a pointer to an instruction)
static instruction_t* reservINT[RESERV_INT_SIZE];
static instruction_t* reservFP[RESERV_FP_SIZE];

//functional units
static instruction_t* fuINT[FU_INT_SIZE];
static instruction_t* fuFP[FU_FP_SIZE];

//common data bus
static instruction_t* commonDataBus = NULL;

//The map table keeps track of which instruction produces the value for each register
static instruction_t* map_table[MD_TOTAL_REGS];

//the index of the last instruction fetched
static int fetch_index = 0;

/* FUNCTIONAL UNITS */
static int dont_fetch = 0;

/* RESERVATION STATIONS */


/* 
 * Description: 
 * 	Checks if simulation is done by finishing the very last instruction
 *      Remember that simulation is done only if the entire pipeline is empty
 * Inputs:
 * 	sim_insn: the total number of instructions simulated
 * Returns:
 * 	True: if simulation is finished
 */
static bool is_simulation_done(counter_t sim_insn) {
    /* ECE552: YOUR CODE GOES HERE */
    int i;

    /*  Check if all the instructions have been fetched */
    if (fetch_index >= sim_insn) {
        dont_fetch = 1;

        /*  Loop through all the reservation stations, functional units and the CDB
            in order to verify that they are all empty */
        for (i = 0; i < INSTR_QUEUE_SIZE; i++)
            if (instr_queue[i]) return 0;

        for (i = 0; i < RESERV_INT_SIZE; i++)
            if (reservINT[i]) return 0;

        for (i = 0; i < RESERV_FP_SIZE; i++)
            if (reservFP[i]) return 0;

        for (i = 0; i < FU_INT_SIZE; i++)
            if (fuINT[i]) return 0;

        for (i = 0; i < FU_FP_SIZE; i++)
            if (fuFP[i]) return 0;

        if (commonDataBus) return 0;

        return 1;
    }

    return 0; //ECE552: you can change this as needed; we've added this so the code provided to you compiles
}

/* 
 * Description: 
 * 	Retires the instruction from writing to the Common Data Bus
 * Inputs:
 * 	current_cycle: the cycle we are at
 * Returns:
 * 	None
 */
void CDB_To_retire(int current_cycle) {

    /* ECE552: YOUR CODE GOES HERE */
    int i, j;

    /* Check if the CDB is occupied. */
    if (commonDataBus != NULL) {

        /*  Loop through all the INT and FP reservation stations and check if any instruction is waiting
            for the instruction on the CDB to finish. If so, make that particular Q value NULL. */
        for (i = 0; i < RESERV_INT_SIZE; i++) {
            if (reservINT[i]) {
                for (j = 0; j < 3; j++) {
                    if (reservINT[i]->Q[j] == commonDataBus)
                        reservINT[i]->Q[j] = NULL;
                }
            }
        }
        for (i = 0; i < RESERV_FP_SIZE; i++) {
            if (reservFP[i]) {
                for (j = 0; j < 3; j++) {
                    if (reservFP[i]->Q[j] == commonDataBus)
                        reservFP[i]->Q[j] = NULL;
                }
            }
        }

        /*  Set the map table entry of the CDB instruction to NULL. */
        for (i = 0; i < MD_TOTAL_REGS; i++) {
            if (map_table[i] == commonDataBus) {
                        map_table[i] = NULL;
            }
        }
    }
    
    commonDataBus = NULL;
}


/* 
 * Description: 
 * 	Moves an instruction from the execution stage to common data bus (if possible)
 * Inputs:
 * 	current_cycle: the cycle we are at
 * Returns:
 * 	None
 */
void execute_To_CDB(int current_cycle) {

    /* ECE552: YOUR CODE GOES HERE */
    int i, j;
    int index = -1;
    int int_or_fp = -1;
    int station = -1;

    /*  Loop through all the INT and FP functional units and check if any instruction has finished.
        If so, put the oldest one of them on the CDB. */
    for (i = 0; i < FU_INT_SIZE; i++) {
        if (!fuINT[i]) continue;
        if (current_cycle >= (fuINT[i]->tom_execute_cycle + 4)) {
            /*  Only put the instruction on the CDB if it writes to a register. */
            if (!WRITES_CDB(fuINT[i]->op)) {
                for (j = 0; j < RESERV_INT_SIZE; j++)
                    if (reservINT[j] == fuINT[i]) reservINT[j] = NULL;
                fuINT[i] = NULL;
                continue;
            }
            if ((fuINT[i]->index < index) || (index == -1)) 
            {
                index = fuINT[i]->index;
                int_or_fp = 0;
                station = i;
            }
        }
    }
    for (i = 0; i < FU_FP_SIZE; i++) {
        if (!fuFP[i]) continue;
        if (current_cycle >= (fuFP[i]->tom_execute_cycle + 9)) {
            if ((fuFP[i]->index < index) || (index == -1)) 
            {
                index = fuFP[i]->index;
                int_or_fp = 1;
                station = i;
            }
        }
    }

    /*  Put the oldest instruction on the CDB, empty the corresponding functional unit
        and reservation station, and set tom_cdb_cycle to the current cycle. */
    if (index != -1)
    {
        if (int_or_fp == 1)
        {
            fuFP[station]->tom_cdb_cycle = current_cycle;
            commonDataBus = fuFP[station];
            fuFP[station] = NULL;
            for (i = 0; i < RESERV_FP_SIZE; i++)
                if (reservFP[i] == commonDataBus) reservFP[i] = NULL;
        }
        else
        {
            fuINT[station]->tom_cdb_cycle = current_cycle;
            commonDataBus = fuINT[station];
            fuINT[station] = NULL;
            for (i = 0; i < RESERV_INT_SIZE; i++)
                if (reservINT[i] == commonDataBus) reservINT[i] = NULL;
        }
    }
}

/* 
 * Description: 
 * 	Moves instruction(s) from the issue to the execute stage (if possible). We prioritize old instructions
 *      (in program order) over new ones, if they both contend for the same functional unit.
 *      All RAW dependences need to have been resolved with stalls before an instruction enters execute.
 * Inputs:
 * 	current_cycle: the cycle we are at
 * Returns:
 * 	None
 */
void issue_To_execute(int current_cycle) {

    /* ECE552: YOUR CODE GOES HERE */
    int i, j;

    /*  Loop through all the INT and FP functional units and check is any is available. If so, loop through the
        reservation stations and check if any instruction's dependencies have been resolved. Put the the oldest
        'ready' instruction on the FU. Can put multiple instructions on the FU if units are available. */
    for (i = 0; i < FU_INT_SIZE; i++) {
        if (!fuINT[i]) {
            int index = -1;
            int station = -1;
            for (j = 0; j < RESERV_INT_SIZE; j++) {
                if (reservINT[j]) {
                    if (!reservINT[j]->Q[0] && !reservINT[j]->Q[1] && !reservINT[j]->Q[2]) {
                        if ((reservINT[j]->index < index) || (index == -1)) {
                            index = reservINT[j]->index;
                            station = j;
                        }
                    }
                }
            }
            /*  Put the oldest instruction on the functional unit and set tom_execute_cycle to the current cycle. */
            if (index != -1) {
                fuINT[i] = reservINT[station];
                fuINT[i]->tom_execute_cycle = current_cycle;
	    }
        }
    }

    for (i = 0; i < FU_FP_SIZE; i++) {
        if (!fuFP[i]) {
            int index = -1;
            int station = -1;
            for (j = 0; j < RESERV_FP_SIZE; j++) {
                if (reservFP[j]) {
                    if (!reservFP[j]->Q[0] && !reservFP[j]->Q[1] && !reservFP[j]->Q[2]) {
                        if ((reservFP[j]->index < index) || (index == -1)) {
                            index = reservFP[j]->index;
                            station = j;
                        }
                    }
                }
            }
            /*  Put the oldest instruction on the functional unit and set tom_execute_cycle to the current cycle. */
            if (index != -1) {
                fuFP[i] = reservFP[station];
                fuFP[i]->tom_execute_cycle = current_cycle;
            }
        }
    }    
}

/* 
 * Description: 
 * 	Moves instruction(s) from the dispatch stage to the issue stage
 * Inputs:
 * 	current_cycle: the cycle we are at
 * Returns:
 * 	None
 */
void dispatch_To_issue(int current_cycle) {

    /* ECE552: YOUR CODE GOES HERE */
    /* The basic premise of this fn is 
       'dispatch an instruction to the Reservation Station */
    int i, j;
    
    /* Check if the fetch queue is empty */
    instruction_t * head = instr_queue[0];
    if (!head) return;

    /* If the instr needs an integer reservation station,
       check through all integer RS and find an empty one */
    if (USES_INT_FU(head->op)) {
        for (i = 0; i < RESERV_INT_SIZE; i++)
            if (!reservINT[i]) {
                head->tom_issue_cycle = current_cycle;
                reservINT[i] = head;
                for (j = 0; j < 3; j++) {
                    if (head->r_in[j] != DNA)
                        head->Q[j] = map_table[head->r_in[j]];
                    else
                        head->Q[j] = NULL;
                }
                if (head->r_out[0] != DNA)
                    map_table[head->r_out[0]] = head;
                if (head->r_out[1] != DNA)
                    map_table[head->r_out[1]] = head;
                for (j = 0; j < INSTR_QUEUE_SIZE-1; j++)
                    instr_queue[j] = instr_queue[j+1];
                instr_queue[INSTR_QUEUE_SIZE-1] = NULL;
                instr_queue_size--;
                return;
            }        
    }
    
    /* If the instr needs an FP reservation station,
       check through all FP RS and find an empty one */
    else if (USES_FP_FU(head->op)) {
        for (i = 0; i < RESERV_FP_SIZE; i++) {
            if (!reservFP[i]) {
                head->tom_issue_cycle = current_cycle;
                reservFP[i] = head;
                for (j = 0; j < 3; j++) {
                    if (head->r_in[j] != DNA)
                        head->Q[j] = map_table[head->r_in[j]];
                    else
                        head->Q[j] = NULL;
                }
                if (head->r_out[0] != DNA)
                    map_table[head->r_out[0]] = head;
                if (head->r_out[1] != DNA)
                    map_table[head->r_out[1]] = head;
                for (j = 0; j < INSTR_QUEUE_SIZE-1; j++)
                    instr_queue[j] = instr_queue[j+1];  
                instr_queue[INSTR_QUEUE_SIZE-1] = NULL;
                instr_queue_size--;  
                return;
            }
        }
    }

    /* Instr is neither an FP or an INT type (possibly
       conditional/unconditional branches, so evict the instr
       from the fetch queue */
    else {
        for (i = 0; i < INSTR_QUEUE_SIZE-1; i++)
            instr_queue[i] = instr_queue[i+1];
        instr_queue[INSTR_QUEUE_SIZE-1] = NULL;
        instr_queue_size--;
        return;
    }
}

/* 
 * Description: 
 * 	Grabs an instruction from the instruction trace (if possible)
 * Inputs:
 *      trace: instruction trace with all the instructions executed
 * Returns:
 * 	None
 */
void fetch(instruction_trace_t* trace) {

    /* ECE552: YOUR CODE GOES HERE */
    
    /*  Not sure what to do in this stage because according to FAQ #5 fetch and dispatch stages are merged
        and dispatch stage is when the instruction is fetched into the IFQ. */
}

/* 
 * Description: 
 * 	Calls fetch and dispatches an instruction at the same cycle (if possible)
 * Inputs:
 *      trace: instruction trace with all the instructions executed
 * 	current_cycle: the cycle we are at
 * Returns:
 * 	None
 */
void fetch_To_dispatch(instruction_trace_t* trace, int current_cycle) {

    fetch(trace);
    
    /* ECE552: YOUR CODE GOES HERE */

    /*  Check if the IFQ is full and if all the instructions have already been fetched. */
    if ((instr_queue_size != INSTR_QUEUE_SIZE) && (!dont_fetch)) {
        instruction_t * instr = get_instr(trace, fetch_index++);
        /*  If the instruction is not a TRAP instruction, then add it to the IFQ
            and set tom_dispatch_cycle to the current cycle. */
        if (!IS_TRAP(instr->op))
            instr_queue[instr_queue_size++] = instr;
            instr->tom_dispatch_cycle = current_cycle;
    }
}

/* 
 * Description: 
 * 	Performs a cycle-by-cycle simulation of the 4-stage pipeline
 * Inputs:
 *      trace: instruction trace with all the instructions executed
 * Returns:
 * 	The total number of cycles it takes to execute the instructions.
 * Extra Notes:
 * 	sim_num_insn: the number of instructions in the trace
 */
counter_t runTomasulo(instruction_trace_t* trace)
{
    //initialize instruction queue
    int i;
    for (i = 0; i < INSTR_QUEUE_SIZE; i++) {
        instr_queue[i] = NULL;
    }

    //initialize reservation stations
    for (i = 0; i < RESERV_INT_SIZE; i++) {
        reservINT[i] = NULL;
    }

    for(i = 0; i < RESERV_FP_SIZE; i++) {
        reservFP[i] = NULL;
    }

    //initialize functional units
    for (i = 0; i < FU_INT_SIZE; i++) {
        fuINT[i] = NULL;
    }

    for (i = 0; i < FU_FP_SIZE; i++) {
        fuFP[i] = NULL;
    }

    //initialize map_table to no producers
    int reg;
    for (reg = 0; reg < MD_TOTAL_REGS; reg++) {
        map_table[reg] = NULL;
    }

    int cycle = 1;
    while (true) {

        /* ECE552: YOUR CODE GOES HERE */

        /*  Run the stages in the reverse order in order to simulate the fact that
            they run in parallel. */
        CDB_To_retire(cycle);
        execute_To_CDB(cycle);
        issue_To_execute(cycle);

        /* The reason we do fetch_to_dispatch before dispatch_to_issue
           is because in some cases (like the first valid instruction), we
           want to fetch and dispatch into an RS in the same cycle. We note a 1
           cycle improvement with this order */
        fetch_To_dispatch(trace, cycle);
        dispatch_To_issue(cycle);

        /* Increment the cycle after all stages are run */
        cycle++;

        if (is_simulation_done(sim_num_insn))
            break;
    }
  
    return cycle;
}
