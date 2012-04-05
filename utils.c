#include<stdio.h>
#include<stdlib.h>
#include<string.h>

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

