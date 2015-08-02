#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include "getopts.h"
#include "picInterface.h"

void parse_args(int argc, char *argv[]);

typedef struct {
	short x;
	short y;
} ComplexShort;

#define getIceSampNum(i,tn) ((long)(getSampleNumber(i) / getDecim(i, tn)))
int g_shutdown = 0;

int card_number = 0;
int module_number = 0;
int tuner_number = 0;
double tuner_freq = 0;
char* des_name = NULL;

FILE *output = NULL;
unsigned long total_output_bytes = 0;

unsigned long total_processed_bytes = 0;

void alarm_handler(int signum)
{
	fprintf(stderr, "%lu MB/s\n", total_processed_bytes/1000000L);
	total_processed_bytes = 0;
	(void) alarm(1);
}

void sig_handler(int signum)
{
	switch(signum) {
		case SIGALRM:
			alarm_handler(SIGALRM);
			break;
		case SIGHUP:
		case SIGINT:
		case SIGTERM:
		case SIGQUIT:
			g_shutdown = 1;
			break;
	}
}

inline void savaData(void *ptr, size_t size, size_t nmemb)
{
	if(output) {
		fwrite(ptr, size, nmemb, output);
		total_output_bytes += (size * nmemb);
	}
}

void processData(char *dataPtr, long picSize, long last, long now)
{
	static int do_next = 0;
	unsigned long sampDiff = now - last;
	unsigned long diff = sampDiff*sizeof(ComplexShort);
	unsigned long lasti = (last*sizeof(ComplexShort))%picSize;
	unsigned long nowi = (now*sizeof(ComplexShort))%picSize;
	unsigned long pos, handled=0;
	
	total_processed_bytes += diff;

	if((output) && (total_output_bytes > 4L*1073741824L)) {
		fflush(output);
		fclose(output);
		output = NULL;
		exit(9);
	}
	
	if(lasti + diff > picSize) {
		long tail = picSize - lasti;
		long front = diff - tail;

		printf("%lu [%lu + %lu = %lu] %lu\n", lasti, tail, front, diff, nowi);
		do_next = 1;

		savaData(&dataPtr[lasti], 1, tail);
		savaData(&dataPtr[0], 1, front);
	} else {
		if(do_next) {
			printf("need to handle %lu @ %lu\n", diff, lasti);
			do_next = 0;
		}
		
		savaData(&dataPtr[lasti], 1, diff);
	}

	fflush(stdout);
	
	return;

#ifdef TIMINGDEBUG
	if(diff > 16384) {
		printf("diff: %lu %lu %lu\n", diff, diff/4, diff%4);
		fflush(stdout);
	}
#endif

	printf("need to handle %lu ComplexSamples\n", diff/4);
	pos = last;
	while(pos < now) {
		printf("%lu %lu\n", pos/4, pos%picSize);
		handled++;
		pos += sizeof(ComplexShort);
	}
	
	printf("handled %lu ComplexSamples\n", handled);
	
}

int main(int argc, char *argv[])
{
	double myfreq;
	//long csNum;
	long picBase, picSize;
	long last, now;
	PicInfoStruct *info;
	void *dataPtr;

	parse_args(argc, argv);

	info = picConnect(card_number, module_number);
	if (!info) {
		fprintf(stderr, "picConnect() failed!\n");
		exit(1);
	}

	myfreq = (tuner_freq * 1e6) / (double)getRate(info);
	printf("Rate is %d %f \n", getRate(info), myfreq);
	setTuneVal(info, tuner_number, myfreq);

	last = getIceSampNum(info,tuner_number);
	usleep(1000000);
	now = getIceSampNum(info,tuner_number);

	if (now <= last) {
		fprintf(stderr, "Pic is not advancing?\n");
		exit(2);
	}

	picBase = getPicBufferBase(info, tuner_number);
	picSize = getPicBufferSize(info);
	dataPtr = picBuffer(picBase, picSize);
	//csNum = picSize/(sizeof(ComplexShort));

	if(!dataPtr) {
		fprintf(stderr, "picBuffer() failed!\n");
		exit(3);
	}
	
	output = fopen("test.dat", "w");
	if(!output) {
		fprintf(stderr, "fopen(%s) failed!\n", "test.dat");
		exit(4);
	}

	signal(SIGINT,	sig_handler);
	signal(SIGTERM, sig_handler);
	signal(SIGQUIT,	sig_handler);
	signal(SIGHUP,	sig_handler);
	signal(SIGALRM, sig_handler);
	(void) alarm(1);

	last = now = getIceSampNum(info,tuner_number);
	while(!g_shutdown) {
		if(now > last) {
			processData(dataPtr, picSize, last, now);
			last = now;
		}

		now = getIceSampNum(info,tuner_number);
	};
	
	if(output) fclose(output);
	picDetach(info);
	dataPtr = NULL;

	return 0;
}

struct options opts[] = {
	{  2, "card",		"IcePic Card Number (zero base)",		"c",	1 },
	{  3, "tuner",		"IcePic Tuner Number (one base)",		"t",	1 },
	{  4, "freq",		"IcePic CenterFreq in MHz",				"f",	1 },
	{  5, "name", 		"Shared Name to attach to", 			"n", 	1 },
	{  6, "module", 	"IcePic Module Number (zero base)", 	"m", 	1 },
	{  0, NULL,			NULL,									NULL,	0 }
};

void parse_args(int argc, char *argv[])
{
	int c;
	char *args;
	FILE* fp;
	char buffer[16384];

	memset(&buffer[0], 0, sizeof(buffer));

	while ((c = getopts(argc, argv, opts, &args)) != 0) {
		switch(c) {
			case -1:
				fprintf(stderr, "Unable to allocate memory for getopts.\n");
				exit(-1);
				break;
			case 2:
				card_number = atoi(args);
				printf("Using IcePic card %d \n", card_number);
				break;
			case 3:
				tuner_number = atoi(args) - 1;
				if (tuner_number < 0) {
					printf("Tuner number is one based\n");
					exit(1);
				}
				printf("Using IcePic Tuner %d\n", (tuner_number+1));
				break;
			case 4:
				tuner_freq = atof(args);
				printf("Using Tuner Freq %f MHz\n", tuner_freq);
				break;
			case 5:
				des_name = strndup(args,1024);
				printf("Using descriptor %s\n", des_name);
				break;
			case 6:
				module_number = atoi(args);
				printf("Using IcePic module %d\n", module_number);
				break;
			default:
				fprintf(stderr, "Unknown command line argument %i\n", c);
		}
		free(args);
	}
	if (!des_name) {
		if (module_number) {
			sprintf(buffer, "icepicdata%d_m%d", card_number, module_number);
		} else {
			// have to keep it this way to not change deployed api
			sprintf(buffer, "icepicdata%d", card_number);
		}
		des_name = strdup(buffer);
	}

	sprintf(buffer, "/var/run/%s", des_name);

	fp = fopen(buffer, "w");
	if (!fp) {
		exit(2);
	}
	fclose(fp);

}
