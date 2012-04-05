#define TRUE 0
#define FALSE 1

#define MAX_CMDTEXT_LEN 100
#define MAX_SIMTYPE_LEN 100
#define MAX_FILENAME_LEN 100

#define MAX_TRACESTR_LEN 16

#define BITS_PER_BYTE 8
#define COUNTER_BITS 2

#define TAKEN 0
#define NOT_TAKEN 1

extern char sim_type[MAX_SIMTYPE_LEN];
extern char trace_str[MAX_TRACESTR_LEN];

extern int M1;
extern int M2;
extern int N;
extern int K;

typedef struct _pc_t {
	char branch_outcome;
	unsigned int addr;
}pc_t;

extern pc_t pc;

typedef struct _predictor_config_t {
	char sim_type[MAX_SIMTYPE_LEN];

	int K;
	int N;
	int M1;
	int M2;

	int num_entries;
	int bits_per_entry;
	int entries_per_byte;

	unsigned long num_predictions;
	unsigned long num_mispredictions;
	float misprediction_rate;
}predictor_config_t;

typedef struct _prediction_table_t {
	int size_in_bytes;
	unsigned char *table;
}prediction_table_t;

typedef struct _predictor_t {
	predictor_config_t config;
	prediction_table_t pred_table;
	unsigned int bhr;
}predictor_t;

extern predictor_t predictor;

extern void initialize_pred_params(predictor_t *);
extern void allocate_and_init_pred_tab(predictor_t *);


extern int extract_bits(int, int, int);
extern int update_bits(int, int, int, int);

extern void handle_branch_prediction(predictor_t *, pc_t *);
