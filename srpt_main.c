#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <nlopt.h>


typedef struct {
	int ndat, idxmin, pardim;
	double *e_ab;
	char **param_names;
	char **param_atoms;
} DATAFUNC;


double opt_me(unsigned pardim, const double *x, double *grad, void *func_data)
{
	DATAFUNC *fd = (DATAFUNC *) func_data;
	int ndat = fd->ndat;
	int idxmin = fd->idxmin;
	int run;
	double e_srp[ndat];
	double e_ab[ndat];
	double energy;
	double sumsq = 0.0;
	char callmop[0x100];
	char param_names[pardim][10];
	char param_atoms[pardim][10];

	for (unsigned i = 0; i < pardim; ++i) {
		strcpy(param_names[i], fd->param_names[i]);
		strcpy(param_atoms[i], fd->param_atoms[i]);
	}

	for (int i = 0; i < ndat; ++i) {
		e_ab[i] = fd->e_ab[i];
	}

	FILE * fs;
	fs = fopen("mopac_parameter", "w");
	if (fs == NULL) exit(EXIT_FAILURE);
	for (unsigned i = 0; i < pardim; ++i) {
		fprintf(fs, "%s %s %lf\n", param_names[i], param_atoms[i], x[i]);
	}
	fclose(fs);

	for (int i = 0; i < ndat; ++i) {
		snprintf(callmop, sizeof(callmop),
		         "/home/ramon/bin/MOPACMINE/MOPAC2016.exe  \
                 ./inp_semp/geo_%d.mop", i);
	}

	for (int i = 0; i < ndat; ++i) {
		energy = NAN;
		char line[] = "TOTAL ENERGY";
		char tmp[500];
		char outfile[500];
		FILE * ft;
		snprintf(outfile, sizeof(outfile), "./inp_semp/geo_%d.out", i);
		ft = fopen(outfile, "r");
		if (ft == NULL) exit(EXIT_FAILURE);
		while (fgets(tmp, 500, ft) != NULL) {
			if ((strstr(tmp, line)) != NULL) {
				sscanf(tmp, "%*s %*s %lf", &energy);
			}
		}
		fclose(ft);
		e_srp[i] = energy;
	}

	for (int i = 0; i < ndat; ++i) {
		e_srp[i] = e_srp[i] - e_srp[idxmin];
		sumsq += (e_srp[i] - e_ab[i]) * (e_srp[i] - e_ab[i]);
	}

	return sqrt(sumsq/ndat);
}

int main(void)
{
	// Input files processing and variable initialization

	DATAFUNC func_data = {.ndat = 0, .pardim = 0};

	int i = 0, ch = 0;
	int dim = 3;
	long length;
	double pdev = 0.7;
	double ** data;
	double mineab = HUGE_VAL;
	char * buffer = 0;

	FILE * fn;
	fn = fopen("./inp_ab.txt", "r");
	if (fn == NULL)	exit(EXIT_FAILURE);

	while ( (ch = fgetc(fn)) != EOF) {
		if (ch == '\n') {
			func_data.ndat++;
		}
	}

	rewind(fn);

	func_data.e_ab = malloc(func_data.ndat * sizeof(func_data.e_ab));
	data = (double **)  malloc(func_data.ndat * sizeof(double));
	for (int i = 0; i < func_data.ndat; i++) {
		data[i] = (double *) malloc(dim * sizeof(double));
	}

	if (data == NULL) {
		printf("Error: memory not allocated\n");
		exit(0);
	}

	while (i < func_data.ndat) {
		fscanf(fn, "%lf %lf %lf %lf", &data[i][0], &data[i][1], &data[i][2],
		       &func_data.e_ab[i]);
		if (func_data.e_ab[i] < mineab) {
			mineab = func_data.e_ab[i];
			func_data.idxmin = i;
		}
		++i;
	}

	fclose(fn);

	for (i = 0; i < func_data.ndat; i++) {
		func_data.e_ab[i] -= mineab;
	}

	FILE * fp = fopen("./naf_geo.xyz", "r");
	if (fp == NULL) exit(EXIT_FAILURE);
	fseek(fp, 0L, SEEK_END);
	length = ftell(fp);
	rewind(fp);
	buffer = (char *) malloc((length+1) * sizeof(char));
	if (buffer) {
		fread(buffer, sizeof(char), length, fp);
	}
	fclose(fp);

	for (i = 0; i < func_data.ndat; ++i) {
		char buf[0x100];
		snprintf(buf, sizeof(buf), "./inp_semp/geo_%d.mop", i);
		FILE * fq = fopen(buf, "w");
		fprintf(fq, "pm7 charge=0 1scf EXTERNAL=mopac_parameter\n");
		fprintf(fq, "Dumb title rule\n");
		fprintf(fq, " \n");
		fprintf(fq, "Ar %f %f %f\n", data[i][0], data[i][1], data[i][2]);
		fputs(buffer, fq);
		fprintf(fq, " ");
		fclose(fq);
	}

	FILE * fr;
	fr = fopen("./parameter_pm7", "r");
	if (fr == NULL)	exit(EXIT_FAILURE);

	while ( (ch = fgetc(fn)) != EOF) {
		if (ch == '\n') {
			func_data.pardim++;
		}
	}

	rewind(fr);

	func_data.param_names = (char **)  malloc(func_data.pardim
	                                          * sizeof(char *));
	for (int i = 0; i < func_data.pardim; i++) {
		func_data.param_names[i] = (char *) malloc(10 * sizeof(char));
	}
	func_data.param_atoms = (char **)  malloc(func_data.pardim
	                                          * sizeof(char *));
	for (int i = 0; i < func_data.pardim; i++) {
		func_data.param_atoms[i] = (char *) malloc(10 * sizeof(char));
	}

	double param_values[func_data.pardim];
	double value_upper[func_data.pardim];
	double value_lower[func_data.pardim];

	i = 0;
	while (i < func_data.pardim) {
		fscanf(fr, "%s %s %lf", func_data.param_names[i],
		       func_data.param_atoms[i], &param_values[i]);
		++i;
		if (param_values[i] >= 0) {
			value_upper[i] = param_values[i] * (1.0 + pdev);
			value_lower[i] = param_values[i] * (1.0 - pdev);
		} else {
			value_upper[i] = param_values[i] * (1.0 - pdev);
			value_lower[i] = param_values[i] * (1.0 + pdev);
		}
	}

	fclose(fr);

	// Optimization process

	int maxeval = 1;
	double minrms = 0.01;
	double tol = 0.001;
	double minf = 0.0;

	nlopt_opt opt = nlopt_create(NLOPT_G_MLSL_LDS, func_data.pardim);
	nlopt_set_local_optimizer(opt, nlopt_create(NLOPT_LN_BOBYQA,
	                                            func_data.pardim));

	nlopt_set_lower_bounds(opt, value_lower);
	nlopt_set_upper_bounds(opt, value_upper);

	nlopt_set_min_objective(opt, opt_me, &func_data);
	nlopt_set_maxeval(opt, maxeval);
	nlopt_set_stopval(opt, minrms);
	nlopt_set_ftol_abs(opt, tol);

	int dbg = nlopt_optimize(opt, param_values, &minf);

	if (dbg < 0) {
		fprintf(stderr, "%s:%d %s -> Nlopt C function failed: %d expected: %d\n"
		        ,__FILE__ , __LINE__, __FUNCTION__, dbg, NLOPT_SUCCESS);
	} else {
		printf("minimum: f(%lf, %lf) = %lf\n",
		       param_values[0], param_values[1], minf);
	}

	// Cleaning up stuff

	nlopt_destroy(opt);

	for (i = 0; i < func_data.ndat; i++) {
		free(data[i]);
	}

	free(buffer);
	free(data);
	free(func_data.e_ab);
	free(func_data.param_names);
	free(func_data.param_atoms);

	return 0;
}

