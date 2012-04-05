#include<stdio.h>
#include<stdlib.h>
#include<string.h>

#include<fcntl.h>

#include "main.h"

char cmd_text[MAX_CMDTEXT_LEN];
char sim_type[MAX_SIMTYPE_LEN];
char trace_file[MAX_FILENAME_LEN];
FILE *fp_trace=0;
char trace_str[MAX_TRACESTR_LEN];

int M1=0;
int M2=0;
int N=0;
int K=0;

pc_t pc;

predictor_t predictor;

void print_usage();
int validate_and_set_params(int, char *[]);
void print_params();
void print_pred_table();

int main(int argc, char *argv[])
{

	predictor.config.num_predictions = 0;
	predictor.config.num_mispredictions = 0;

	if (!(argc == 4 || argc == 5 || argc == 7)) {
		printf("Invalid Numebr of Arguments!! \n\n");
		print_usage();
		printf("Exiting...\n");
		exit(1);
	}

	if (validate_and_set_params(argc, argv)) {
		printf("Exiting...\n");
		exit(1);
	}

	fp_trace = fopen(trace_file, "r");
	if (fp_trace == NULL) {
		printf("Error while opening the tracefile - %s\n", trace_file);
		printf("Exiting...\n");
		exit(1);
	}

	initialize_pred_params(&predictor);
	allocate_and_init_pred_tab(&predictor);
#ifdef DEBUG_FLAG
	print_pred_table();
#endif

	while (fgets(trace_str, MAX_TRACESTR_LEN, fp_trace)) {

		sscanf(trace_str, "%x %c\n", &pc.addr, &pc.branch_outcome);

#ifdef DEBUG_FLAG
		printf("%x %c\n", pc.addr, pc.branch_outcome);
#endif

		handle_branch_prediction(&predictor, &pc);

		predictor.config.num_predictions += 1;
	}

	print_params();

	return 0;
}

void print_usage()
{
	printf("The Branch Predictor Simulator can be run in one of the following configurations -> \n\n");
	printf("1) Bimodal Branch Predictor Simulator:\n");
	printf("$ ./sim bimodal <M2> <tracefile>\n");
	printf("Where,\n");
	printf("M2: Number of PC bits used to index the Bimodal Prediction Table\n\n");

	printf("2) GShare Branch Predictor Simulator:\n");
	printf("$ ./sim gshare <M1> <N> <tracefile>\n");
	printf("Where,\n");
	printf("M1: Number of PC bits used to index the GShare Prediction Table\n");
	printf("N: Number of Branch History Register (BHR) bits used to index the GShare Prediction Table\n\n");


	printf("3) Hybrid Branch Predictor Simulator:\n");
	printf("$ ./sim gshare <K> <M1> <N> <M2> <tracefile>\n");
	printf("Where,\n");
	printf("K: Number of PC bits used to index the Chooser Table\n");
	printf("M1: Number of PC bits used to index the GShare Prediction Table\n");
	printf("N: Number of Branch History Register (BHR) bits used to index the GShare Prediction Table\n");
	printf("M2: Number of PC bits used to index the Bimodal Prediction Table\n\n");
}

int validate_and_set_params(int num_params, char *params[])
{
	strcpy(cmd_text, params[0]);

	strcpy(sim_type, params[1]);

	if (!(strcmp(sim_type, "bimodal") == 0 || strcmp(sim_type, "gshare") == 0 || strcmp(sim_type, "hybrid") == 0)) {
		printf("The first parameter has to be the type of branch predictor simulator! \n");
		printf("Supported types are - bimodal, gshare or hybrid\n");
		return FALSE;
	}

	if (strcmp(sim_type, "bimodal") == 0 && num_params != 4) {
		printf("The Bimodal Branch Predictor Simulator should have 3 command line parameters!!\n\n");
		print_usage();
		return FALSE;
	} else if (strcmp(sim_type, "bimodal") == 0 && num_params == 4) {
		M2 = atoi(params[2]);
		strcpy(trace_file, params[3]);
		return TRUE;
	}

	if (strcmp(sim_type, "gshare") == 0 && num_params != 5) {
		printf("The GShare Branch Predictor Simulator should have 4 command line parameters!!\n\n");
		print_usage();
		return FALSE;
	} else if (strcmp(sim_type, "gshare") == 0 && num_params == 5) {
		M1 = atoi(params[2]);
		N = atoi(params[3]);
		strcpy(trace_file, params[4]);
		return TRUE;
	}

	if (strcmp(sim_type, "hybrid") == 0 && num_params != 7) {
		printf("The GShare Hybrid Predictor Simulator should have 6 command line parameters!!\n\n");
		print_usage();
		return FALSE;
	} else if (strcmp(sim_type, "hybrid") == 0 && num_params == 7) {
		K = atoi(params[2]);
		M1 = atoi(params[3]);
		N = atoi(params[4]);
		M2 = atoi(params[5]);
		strcpy(trace_file, params[6]);
		return TRUE;
	}

	return FALSE;
}

void print_params()
{
	printf("COMMAND\n");
	if (strcmp(sim_type, "bimodal") == 0) {
		printf(" %s %s %d %s\n", cmd_text, sim_type, M2, trace_file);
	} else if (strcmp(sim_type, "gshare") == 0) {
		printf(" %s %s %d %d %s\n", cmd_text, sim_type, M1, N, trace_file);
	} else if (strcmp(sim_type, "hybrid") == 0) {
		printf(" %s %s %d %d %d %d %s\n", cmd_text, sim_type, K, M1, N, M2, trace_file);
	}

	predictor.config.misprediction_rate = (float)predictor.config.num_mispredictions / (float)predictor.config.num_predictions;
	predictor.config.misprediction_rate = predictor.config.misprediction_rate * 100;

	printf("OUTPUT\n");
	printf(" number of predictions:    %ld\n", predictor.config.num_predictions);
	printf(" number of mispredictions: %ld\n", predictor.config.num_mispredictions);
	printf(" misprediction rate:       %.2f%%\n", predictor.config.misprediction_rate);

	if (strcmp(sim_type, "bimodal") == 0) {
		printf("FINAL BIMODAL CONTENTS\n");
	} else if (strcmp(sim_type, "gshare") == 0) {
		printf("FINAL GSHARE CONTENTS\n");
	} else if (strcmp(sim_type, "hybrid") == 0) {
		printf("FINAL HYBRID CONTENTS\n");
	}
	print_pred_table();
}

void print_pred_table()
{
	int base, offset, byte, count;
	int i;

	for (i=0 ; i < predictor.config.num_entries ; i++) {
		base = i / predictor.config.entries_per_byte;
		offset = i % predictor.config.entries_per_byte;
		offset = offset * 2;
		byte = predictor.pred_table.table[base];
		count = extract_bits(byte, offset, offset + predictor.config.bits_per_entry - 1);
		printf(" %d\t%d\n", i, count);
	}
}

