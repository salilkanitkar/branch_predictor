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
		predictor->config.M2 = 0;
		predictor->config.M1 = M1;
		predictor->config.N = N;
		predictor->config.K = 0;
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
	} else if (strcmp(predictor->config.sim_type, "gshare") == 0) {

		predictor->config.num_entries = pow(2, predictor->config.M1);
		predictor->config.bits_per_entry = COUNTER_BITS;
		predictor->pred_table.size_in_bytes = predictor->config.num_entries * predictor->config.bits_per_entry / BITS_PER_BYTE;
		predictor->config.entries_per_byte = BITS_PER_BYTE / predictor->config.bits_per_entry;

		predictor->pred_table.table = (unsigned char *)malloc(sizeof(unsigned char) * predictor->pred_table.size_in_bytes);
		if (predictor->pred_table.table == NULL) {
			printf("Error while allocating Memory!!\n");
			printf("Exiting...\n");
			exit(1);
		}

		/* Initialize all the counters in Prediction Table to 2 (weakly taken) */
		for (i=0 ; i < predictor->pred_table.size_in_bytes ; i++) {
			predictor->pred_table.table[i] = 0xAA;
		}

		/* Initialize Branch History Register to all Zeros*/
		predictor->bhr = 0;
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
	int m_pc_bits, n_pc_bits, lower_pc_bits, msb_bhr;
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

	} else if (strcmp(predictor->config.sim_type, "gshare") == 0) {

#ifdef DEBUG_OP
		printf("=%ld\t%x %c\n", predictor->config.num_predictions, pc->addr, pc->branch_outcome);
#endif

		/* Extracting index:
			1) First we extract 2 to M1+1 bits of pc - number of bits extracted will be M1.
			2) Then from these M1 bits, we extract upper N bits.
			3) XOR these upper N bits with N-bits long BHR (Branch History Register).
			4) Append the lower bits (n-m) of pc extracted bits to result of XOR - forming the index.
		*/
		m_pc_bits = extract_bits(pc->addr, 2, predictor->config.M1 + 1);
		lower_pc_bits = extract_bits(m_pc_bits, 0, predictor->config.M1 - predictor->config.N - 1);
		n_pc_bits = extract_bits(m_pc_bits, predictor->config.M1 - predictor->config.N, predictor->config.M1 - 1);

		index = n_pc_bits ^ predictor->bhr;
		index = index << (predictor->config.M1 - predictor->config.N);
		index = index ^ lower_pc_bits;

		/* The index will provide a unique prediction counter.
		   However, since each counter is a 2 bit entity, index is first split into base and offset.
		   Base points to the byte which contains the required indexed count. Offset points to the first bit of the two-bit
		   counter within the byte indexed by the base.
		*/
		base = index / predictor->config.entries_per_byte;
		offset = index % predictor->config.entries_per_byte;
		offset = offset * 2;
		byte = predictor->pred_table.table[base];
		count = extract_bits(byte, offset, offset + predictor->config.bits_per_entry - 1);

#ifdef DEBUG_OP
		printf("\tGP: %d %d\n", index, count);
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

		msb_bhr = 0;
		if (pc->branch_outcome == 'n')
			msb_bhr = 0;
		else if (pc->branch_outcome == 't')
			msb_bhr = 1;

		/* Updating BHR (Branch History Register)
		   BHR is shifted Right by one bit.
		   The actual outcome of the branch is then put as the Most Significant Bit of the BHR (i.e. N-1th bit of BHR).
		   This updated BHR will then be used in the Prediction stage while XORing with PC bits. (See Above).
		*/
		predictor->bhr = predictor->bhr >> 1;
		msb_bhr = msb_bhr << (predictor->config.N - 1);
		predictor->bhr = predictor->bhr ^ msb_bhr;

#ifdef DEBUG_OP
		printf("\tGU: %d %d\n", index, count);
#endif

	}
}

void init_hybrid_pred_params(hybrid_predictor_t *hybrid_predictor)
{
	/* Initialize Hybrid Branch Predictor Parameters. */
	strcpy(hybrid_predictor->config.sim_type, sim_type);
	hybrid_predictor->config.K = K;
	hybrid_predictor->config.M1 = M1;
	hybrid_predictor->config.N = N;
	hybrid_predictor->config.M2 = M2;

	/* Initialize Bimodal Branch Predictor Parameters. */
	strcpy(hybrid_predictor->bimodal.config.sim_type, "bimodal");
	hybrid_predictor->bimodal.config.M2 = M2;
	hybrid_predictor->bimodal.config.M1 = 0;
	hybrid_predictor->bimodal.config.N = 0;
	hybrid_predictor->bimodal.config.K = 0;

	/* initialize GShare Branch Predictor Parameters. */
	strcpy(hybrid_predictor->gshare.config.sim_type, "gshare");
	hybrid_predictor->gshare.config.M2 = 0;
	hybrid_predictor->gshare.config.M1 = M1;
	hybrid_predictor->gshare.config.N = N;
	hybrid_predictor->gshare.config.K = 0;
}

void allocate_and_init_hybrid_pred_tab(hybrid_predictor_t *hybrid_predictor)
{
	int i;

	/* Allocate and Assign Hybrid Branch Predictor Parameters. */
	hybrid_predictor->config.num_entries = pow(2, hybrid_predictor->config.K);
	hybrid_predictor->config.bits_per_entry = COUNTER_BITS;
	hybrid_predictor->chooser_table.size_in_bytes = hybrid_predictor->config.num_entries * hybrid_predictor->config.bits_per_entry / BITS_PER_BYTE;
	hybrid_predictor->config.entries_per_byte = BITS_PER_BYTE / hybrid_predictor->config.bits_per_entry;

	hybrid_predictor->chooser_table.table = (unsigned char *)malloc(sizeof(unsigned char) * hybrid_predictor->chooser_table.size_in_bytes);
	if (hybrid_predictor->chooser_table.table == NULL) {
		printf("Error while allocating Memory!!\n");
		printf("Exiting...\n");
		exit(1);
	}

	for (i=0 ; i < hybrid_predictor->chooser_table.size_in_bytes ; i++) {
		hybrid_predictor->chooser_table.table[i] = 0x55;
	}

	/* Allocate and Assign Bimodal Branch Predictor Parameters. */
	hybrid_predictor->bimodal.config.num_entries = pow(2, hybrid_predictor->bimodal.config.M2);
	hybrid_predictor->bimodal.config.bits_per_entry = COUNTER_BITS;
	hybrid_predictor->bimodal.pred_table.size_in_bytes = hybrid_predictor->bimodal.config.num_entries * hybrid_predictor->bimodal.config.bits_per_entry / BITS_PER_BYTE;
	hybrid_predictor->bimodal.config.entries_per_byte = BITS_PER_BYTE / hybrid_predictor->bimodal.config.bits_per_entry;

	hybrid_predictor->bimodal.pred_table.table = (unsigned char *)malloc(sizeof(unsigned char) * hybrid_predictor->bimodal.pred_table.size_in_bytes);
	if (hybrid_predictor->bimodal.pred_table.table == NULL) {
		printf("Error while allocating Memory!!\n");
		printf("Exiting...\n");
		exit(1);
	}

	for (i=0 ; i < hybrid_predictor->bimodal.pred_table.size_in_bytes ; i++) {
		hybrid_predictor->bimodal.pred_table.table[i] = 0xAA;
	}

	/* Allocate and Assign GShare Branch Predictor Parameters. */
	hybrid_predictor->gshare.config.num_entries = pow(2, hybrid_predictor->gshare.config.M1);
	hybrid_predictor->gshare.config.bits_per_entry = COUNTER_BITS;
	hybrid_predictor->gshare.pred_table.size_in_bytes = hybrid_predictor->gshare.config.num_entries * hybrid_predictor->gshare.config.bits_per_entry / BITS_PER_BYTE;
	hybrid_predictor->gshare.config.entries_per_byte = BITS_PER_BYTE / hybrid_predictor->gshare.config.bits_per_entry;

	hybrid_predictor->gshare.pred_table.table = (unsigned char *)malloc(sizeof(unsigned char) * hybrid_predictor->gshare.pred_table.size_in_bytes);
	if (hybrid_predictor->gshare.pred_table.table == NULL) {
		printf("Error while allocating Memory!!\n");
		printf("Exiting...\n");
		exit(1);
	}

	for (i=0 ; i < hybrid_predictor->gshare.pred_table.size_in_bytes ; i++) {
		hybrid_predictor->gshare.pred_table.table[i] = 0xAA;
	}

	hybrid_predictor->gshare.bhr = 0;

}

int get_bimodal_prediction(predictor_t *predictor, pc_t *pc)
{
	int index, base, offset, byte, count;
	int this_prediction=0;

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

	return (this_prediction);
}

void update_bimodal_predictor(predictor_t *predictor, int prediction, pc_t *pc)
{
	int is_misprediction = 0;
	int base, offset, byte, count, index;

	index = extract_bits(pc->addr, 2, predictor->config.M2 + 1);

	base = index / predictor->config.entries_per_byte;
	offset = index % predictor->config.entries_per_byte;
	offset = offset * 2;
	byte = predictor->pred_table.table[base];
	count = extract_bits(byte, offset, offset + predictor->config.bits_per_entry - 1);

	if (pc->branch_outcome == 'n' && prediction == TAKEN) {
		is_misprediction = 1;
		predictor->config.num_mispredictions += 1;
	} else if (pc->branch_outcome == 't' && prediction == NOT_TAKEN) {
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

int get_gshare_prediction(predictor_t *predictor, pc_t *pc)
{
	int index, base, offset, byte, count;
	int m_pc_bits, lower_pc_bits, n_pc_bits;
	int this_prediction=0;

	/* Extracting index:
		1) First we extract 2 to M1+1 bits of pc - number of bits extracted will be M1.
		2) Then from these M1 bits, we extract upper N bits.
		3) XOR these upper N bits with N-bits long BHR (Branch History Register).
		4) Append the lower bits (n-m) of pc extracted bits to result of XOR - forming the index.
	*/
	m_pc_bits = extract_bits(pc->addr, 2, predictor->config.M1 + 1);
	lower_pc_bits = extract_bits(m_pc_bits, 0, predictor->config.M1 - predictor->config.N - 1);
	n_pc_bits = extract_bits(m_pc_bits, predictor->config.M1 - predictor->config.N, predictor->config.M1 - 1);

	index = n_pc_bits ^ predictor->bhr;
	index = index << (predictor->config.M1 - predictor->config.N);
	index = index ^ lower_pc_bits;

	/* The index will provide a unique prediction counter.
	   However, since each counter is a 2 bit entity, index is first split into base and offset.
	   Base points to the byte which contains the required indexed count. Offset points to the first bit of the two-bit
	   counter within the byte indexed by the base.
	*/
	base = index / predictor->config.entries_per_byte;
	offset = index % predictor->config.entries_per_byte;
	offset = offset * 2;
	byte = predictor->pred_table.table[base];
	count = extract_bits(byte, offset, offset + predictor->config.bits_per_entry - 1);

#ifdef DEBUG_OP
	printf("\tGP: %d %d\n", index, count);
#endif

	if (count == 0 || count == 1) {
		this_prediction = NOT_TAKEN;
	} else if (count == 2 || count == 3) {
		this_prediction = TAKEN;
	}

	return (this_prediction);
}

void update_gshare_predictor(predictor_t *predictor, int prediction, pc_t *pc)
{
	int base, offset, byte, count, index;
	int m_pc_bits, lower_pc_bits, n_pc_bits;
	int is_misprediction;

	m_pc_bits = extract_bits(pc->addr, 2, predictor->config.M1 + 1);
	lower_pc_bits = extract_bits(m_pc_bits, 0, predictor->config.M1 - predictor->config.N - 1);
	n_pc_bits = extract_bits(m_pc_bits, predictor->config.M1 - predictor->config.N, predictor->config.M1 - 1);

	index = n_pc_bits ^ predictor->bhr;
	index = index << (predictor->config.M1 - predictor->config.N);
	index = index ^ lower_pc_bits;

	base = index / predictor->config.entries_per_byte;
	offset = index % predictor->config.entries_per_byte;
	offset = offset * 2;
	byte = predictor->pred_table.table[base];
	count = extract_bits(byte, offset, offset + predictor->config.bits_per_entry - 1);

	is_misprediction = 0;
	if (pc->branch_outcome == 'n' && prediction == TAKEN) {
		is_misprediction = 1;
		predictor->config.num_mispredictions += 1;
	} else if (pc->branch_outcome == 't' && prediction == NOT_TAKEN) {
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

void update_gshare_bhr(predictor_t *predictor, pc_t *pc)
{
	int msb_bhr;

	msb_bhr = 0;
	if (pc->branch_outcome == 'n')
		msb_bhr = 0;
	else if (pc->branch_outcome == 't')
		msb_bhr = 1;

	/* Updating BHR (Branch History Register)
	   BHR is shifted Right by one bit.
	   The actual outcome of the branch is then put as the Most Significant Bit of the BHR (i.e. N-1th bit of BHR).
	*/
	predictor->bhr = predictor->bhr >> 1;
	msb_bhr = msb_bhr << (predictor->config.N - 1);
	predictor->bhr = predictor->bhr ^ msb_bhr;
}

void handle_hybrid_branch_prediction(hybrid_predictor_t *hybrid_predictor, pc_t *pc)
{

	int bimodal_prediction, gshare_prediction;
	int index, offset, base, byte, chooser_count;
	int this_prediction=0, pred_type, actual_outcome=0;

#ifdef DEBUG_OP
	printf("=%ld\t%x %c\n", hybrid_predictor->config.num_predictions, pc->addr, pc->branch_outcome);
#endif

	gshare_prediction = get_gshare_prediction(&hybrid_predictor->gshare, pc);
	bimodal_prediction = get_bimodal_prediction(&hybrid_predictor->bimodal, pc);

	index = extract_bits(pc->addr, 2, hybrid_predictor->config.K + 1);
	base = index / hybrid_predictor->config.entries_per_byte;
	offset = index % hybrid_predictor->config.entries_per_byte;
	offset = offset * 2;
	byte = hybrid_predictor->chooser_table.table[base];
	chooser_count = extract_bits(byte, offset, offset + hybrid_predictor->config.bits_per_entry - 1);

#ifdef DEBUG_OP
	printf("\tCP: %d %d\n", index, chooser_count);
#endif

	if (chooser_count == 0 || chooser_count == 1) {
		this_prediction = bimodal_prediction;
		pred_type = BIMODAL;
		update_bimodal_predictor(&hybrid_predictor->bimodal, bimodal_prediction, pc);
	} else if (chooser_count == 2 || chooser_count == 3) {
		this_prediction = gshare_prediction;
		pred_type = GSHARE;
		update_gshare_predictor(&hybrid_predictor->gshare, gshare_prediction, pc);
	}

	update_gshare_bhr(&hybrid_predictor->gshare, pc);

	if (pc->branch_outcome == 'n')
		actual_outcome = NOT_TAKEN;
	else if (pc->branch_outcome == 't')
		actual_outcome = TAKEN;

	if (gshare_prediction == actual_outcome && bimodal_prediction != actual_outcome) {
		if (chooser_count < 3)
			chooser_count += 1;
		hybrid_predictor->chooser_table.table[base] = update_bits(byte, offset, offset + hybrid_predictor->config.bits_per_entry - 1, chooser_count);
#ifdef DEBUG_OP
		printf("\tCU: %d %d\n", index, chooser_count);
#endif
	} else if (gshare_prediction != actual_outcome && bimodal_prediction == actual_outcome) {
		if (chooser_count > 0)
			chooser_count -= 1;
		hybrid_predictor->chooser_table.table[base] = update_bits(byte, offset, offset + hybrid_predictor->config.bits_per_entry - 1, chooser_count);
#ifdef DEBUG_OP
		printf("\tCU: %d %d\n", index, chooser_count);
#endif
	}

	if (this_prediction != actual_outcome)
		hybrid_predictor->config.num_mispredictions += 1;

}

