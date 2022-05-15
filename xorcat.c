#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#define byte uint8_t
#define xorblock uint64_t
#define BLOCKSIZE 524288	//2^19
#if BLOCKSIZE%8!=0
	#error BLOCKSIZE must be multiple of 8
#endif
#define out 1

int main(int argc, char *argv[]) {
	if(argc==4 && strcmp(argv[3],"_")!=0) {
		fprintf(stderr, "Syntax error: unexpected parameter \"%s\"\n", argv[3]);
		goto help;
	}
	if(argc<=1 || argc>4) {
		if(argc>4) fputs("Syntax error\n", stderr);
		help:
		fprintf(stderr, "Usage:\n");
		fprintf(stderr, "\t%s input1 input2           #in this case, if input2 file name is `_` use ./_\n", argv[0]);
		fprintf(stderr, "\t%s input1 input2 _         #stop at the shorter input file length\n", argv[0]);
		fprintf(stderr, "\t%s input1                  #read input2 from stdin\n", argv[0]);
		fprintf(stderr, "\t%s input1 _                #read input2 from stdin, stop at the shorter input file length\n", argv[0]);
		fprintf(stderr, "output is always written to stdout\n");
		return -1;
	}
	//	------- FILES OPENING -------
	int in1, in2;
	in1=open(argv[1], O_RDONLY);
	if (in1<0) {
		fprintf(stderr, "Eror opening file \"%s", argv[1]);
		perror("\"");
		return EXIT_FAILURE;
	}
	if(argc==4 || argc==3 && strcmp(argv[2],"_")!=0) {
		in2=open(argv[2], O_RDONLY);
		if (in2<0) {
			fprintf(stderr, "Eror opening file \"%s", argv[2]);
			perror("\"");
			return EXIT_FAILURE;
		}
	}
	else
		in2=0;
	if(in1==in2 && in1<=2) {
		fprintf(stderr, "Error: attempt to read both inputs from std%s\n", in1==0?"in":(in1==1?"out":"err"));
		return -1;
	}
	
	//	------- READING DATA -------
	xorblock *const bl1=malloc(BLOCKSIZE), *const bl2=malloc(BLOCKSIZE);
	if(bl1==NULL || bl2==NULL) {
		fputs("malloc: error allocating memory\n", stderr);
		return EXIT_FAILURE;
	}
	ssize_t rs1, rs2;	//actual read size
	while((rs1=read(in1, bl1, BLOCKSIZE))==BLOCKSIZE && (rs2=read(in2, bl2, BLOCKSIZE))==BLOCKSIZE) {
		for(unsigned i=0; i<BLOCKSIZE/sizeof(xorblock); i++)
			bl1[i]^=bl2[i];
		if(write(out, bl1, BLOCKSIZE)!=BLOCKSIZE) {
			perror("write error");
			return EXIT_FAILURE;
		}
	}
	if(rs1<0) { perror(argv[1]); return EXIT_FAILURE; }
	if(rs1<BLOCKSIZE)
		rs2=read(in2, bl2, BLOCKSIZE);
	if(rs2<0) { perror(in2==0?argv[2]:"stdin"); return EXIT_FAILURE; }
	byte blout;
	unsigned i;
	for(i=0; i<(rs1<=rs2?rs1:rs2); i++) {
		blout = ((byte*)bl1)[i] ^ ((byte*)bl2)[i];
		if(write(out, &blout, sizeof(blout))!=sizeof(blout)) {
			perror("write error");
			return EXIT_FAILURE;
		}
	}
	if(!(argc==3 && strcmp(argv[2],"_")==0 || argc==4 && strcmp(argv[3],"_")==0)) {
		/*for(; i<(rs1>rs2?rs1:rs2); i++) {
			blout = rs1>rs2 ? ((byte*)bl1)[i] : ((byte*)bl2)[i];
			if(write(out, &blout, sizeof(blout))!=sizeof(blout)) {
				perror("write error");
				return EXIT_FAILURE;
			}
		}*/
		if(rs1!=rs2)
			if(write(out, &(((byte*)(rs1>rs2?bl1:bl2))[i]), rs1>rs2?rs1-rs2:rs2-rs1) != (rs1>rs2?rs1-rs2:rs2-rs1)) {
				perror("write error");
				return EXIT_FAILURE;
			}
		if(rs1==BLOCKSIZE || rs2==BLOCKSIZE) {
			in1 = rs1>rs2?in1:in2;
			while((rs1=read(in1, bl1, BLOCKSIZE))>0) {
				if(write(out, bl1, rs1)!=rs1) {
					perror("write error");
					return EXIT_FAILURE;
				}
			}
			if(rs1<0) { perror(in1==in2?(argc>=3?argv[2]:"stdin"):argv[1]); return EXIT_FAILURE; }
		}
	}
	free(bl1);
	free(bl2);
	if(in1>2) close(in1);
	if(in2>2) close(in2);
	
	return EXIT_SUCCESS;
}
