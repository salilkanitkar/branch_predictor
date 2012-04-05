#define TRUE 0
#define FALSE 1

#define MAX_CMDTEXT_LEN 100
#define MAX_SIMTYPE_LEN 100
#define MAX_FILENAME_LEN 100

#define MAX_TRACESTR_LEN 16

typedef struct _pc_t {
	char branch_outcome;
	unsigned int addr;
}pc_t;

extern pc_t pc;

