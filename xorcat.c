/*  XORcat, a tool to bitwise-XOR files/streams.
Copyright © 2022  Samuel Albani <https://gitlab.com/viablu>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU Affero General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Affero General Public License for more details.

You should have received a copy of the GNU Affero General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#define byte uint8_t
#define xorblock uint64_t
#define BLOCKSIZE 524288	//2^19
#if BLOCKSIZE%8!=0
	#error BLOCKSIZE must be multiple of 8
#endif
#define out STDOUT_FILENO

static inline ssize_t readblock(int fd, void *buf, size_t nbytes) {
	ssize_t nread, totread=0;
	do {
		if((nread=read(fd, buf+totread, nbytes-totread))<=0)
			return nread==0?totread:nread;
	} while((totread+=nread)<nbytes);
	return totread;
}

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
		return errno;
	}
	if(argc==4 || argc==3 && strcmp(argv[2],"_")!=0) {
		in2=open(argv[2], O_RDONLY);
		if (in2<0) {
			fprintf(stderr, "Eror opening file \"%s", argv[2]);
			perror("\"");
			return errno;
		}
	}
	else
		in2=STDIN_FILENO;
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
	while((rs1=readblock(in1, bl1, BLOCKSIZE))==BLOCKSIZE && (rs2=readblock(in2, bl2, BLOCKSIZE))==BLOCKSIZE) {
		for(unsigned i=0; i<BLOCKSIZE/sizeof(xorblock); i++)
			bl1[i]^=bl2[i];
		if(write(out, bl1, BLOCKSIZE)!=BLOCKSIZE) {
			perror("write error");
			return errno;
		}
	}
	if(rs1<0) { perror(argv[1]); return errno; }
	if(rs1<BLOCKSIZE)
		rs2=readblock(in2, bl2, BLOCKSIZE);
	if(rs2<0) { perror(in2==0?argv[2]:"stdin"); return errno; }
	unsigned i;
	for(i=0; i<(rs1<=rs2?rs1:rs2); i++) {
		((byte*)bl1)[i] ^= ((byte*)bl2)[i];
	}
	if(write(out, bl1, rs1<=rs2?rs1:rs2)!=(rs1<=rs2?rs1:rs2)) {
		perror("write error");
		return errno;
	}
	if(!(argc==3 && strcmp(argv[2],"_")==0 || argc==4 && strcmp(argv[3],"_")==0)) {
		if(rs1!=rs2)
			if(write(out, &(((byte*)(rs1>rs2?bl1:bl2))[i]), rs1>rs2?rs1-rs2:rs2-rs1) != (rs1>rs2?rs1-rs2:rs2-rs1)) {
				perror("write error");
				return errno;
			}
		if((rs1<=rs2?in1:in2)>2) close(rs1<=rs2?in1:in2);
		in1 = rs1>rs2?in1:in2;
		if(rs1==BLOCKSIZE || rs2==BLOCKSIZE) {
			while((rs1=read(in1, bl1, BLOCKSIZE))>0) {
				if(write(out, bl1, rs1)!=rs1) {
					perror("write error");
					return errno;
				}
			}
			if(rs1<0) { perror(in1==in2?(argc>=3?argv[2]:"stdin"):argv[1]); return errno; }
		}
	}
	else
		if(in2>2) close(in2);
	if(in1>2) close(in1);
#ifndef out
	if(out>2) close(out);
#endif
	free(bl1);
	free(bl2);
	
	return EXIT_SUCCESS;
}
