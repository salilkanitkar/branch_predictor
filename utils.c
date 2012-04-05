#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<math.h>

#include "main.h"

void initialize_pred_params(predictor_t *predictor)
{
	if (strcmp(sim_type, "bimodal") == 0) {
		strcpy(predictor->config.sim_type, sim_type);
		predictor->config.M2 = M2;
		predictor->config.M1 = 0;
		predictor->config.N = 0;
		predictor->config.K = 0;
	} else if (strcmp(sim_type, "gshare") == 0) {
		strcpy(predictor->config.sim_type, sim_type);
		predictor->config.M2 = M2;
		predictor->config.M1 = M1;
		predictor->config.N = N;
		predictor->config.K = 0;
	} else if (strcmp(sim_type, "hybrid") == 0) {
		strcpy(predictor->config.sim_type, sim_type);
		predictor->config.M2 = M2;
		predictor->config.M1 = M1;
		predictor->config.N = N;
		predictor->config.K = K;
	}
}

void allocate_and_init_pred_tab(predictor_t *predictor)
{
	int i;

	if (strcmp(predictor->config.sim_type, "bimodal") == 0) {

		predictor->config.num_entries = pow(2, predictor->config.M2);
		predictor->config.bits_per_entry = COUNTER_BITS;
		predictor->pred_table.size_in_bytes = predictor->config.num_entries * predictor->config.bits_per_entry / BITS_PER_BYTE;
		predictor->config.entries_per_byte = BITS_PER_BYTE / predictor->config.bits_per_entry;

		predictor->pred_table.table = (unsigned char *)malloc(sizeof(unsigned char) * predictor->pred_table.size_in_bytes);
		if (predictor->pred_table.table == NULL) {
			printf("Error while allocating Memory!!\n");
			printf("Exiting...\n");
			exit(1);
		}

		for (i=0 ; i < predictor->pred_table.size_in_bytes ; i++) {
			predictor->pred_table.table[i] = 0xAA;
		}
	}
}

int extract_bits(int byte, int start, int end)
{
       int res = ((byte >> start) & ~(~0 << (end-start+1)));
       return res;
}

int update_bits(int byte, int start, int end, int new_val)
{
        int old_val = extract_bits(byte, start, end);
        old_val = old_val << start;
        byte = byte ^ old_val;
        new_val = new_val << start;
        byte = byte ^ new_val;
        return byte;
}

void handle_branch_prediction(predictor_t *predictor, pc_t *pc)
{
	int index;
	int base, offset, byte, count;
	int this_prediction, is_misprediction=0;

	if (strcmp(predictor->config.sim_type, "bimodal") == 0) {

#ifdef DEBUG_OP
		printf("=%ld\t%x %c\n", predictor->config.num_predictions, pc->addr, pc->branch_outcome);
#endif

		index = extract_bits(pc->addr, 2, predictor->config.M2 + 1);
                base = index / predictor->config.entries_per_byte;
                offset = index % predictor->config.entries_per_byte;
                offset = offset * 2;
                byte = predictor->pred_table.table[base];
                count = extract_bits(byte, offset, offset + predictor->config.bits_per_entry - 1);
#ifdef DEBUG_OP
		printf("\tBP: %d %d\n", index, count);
#endif

		if (count == 0 || count == 1) {
			this_prediction = NOT_TAKEN;
		} else if (count == 2 || count == 3) {
			this_prediction = TAKEN;
		}

		is_misprediction = 0;
		if (pc->branch_outcome == 'n' && this_prediction == TAKEN) {
			is_misprediction = 1;
			predictor->config.num_mispredictions += 1;
		} else if (pc->branch_outcome == 't' && this_prediction == NOT_TAKEN) {
			is_misprediction = 1;
			predictor->config.num_mispredictions += 1;
		}

		if (pc->branch_outcome == 't') {
			if (count < 3)
				count += 1;
		} else if (pc->branch_outcome == 'n') {
			if (count > 0)
				count -= 1;
		}

		predictor->pred_table.table[base] = update_bits(byte, offset, offset + predictor->config.bits_per_entry - 1, count);

#ifdef DEBUG_OP
		printf("\tBU: %d %d\n", index, count);
#endif

	}
}
