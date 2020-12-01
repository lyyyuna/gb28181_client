#include "h264_parser.h"
#include "spdlog/spdlog.h"
#include <stdlib.h>
#include <string.h>

typedef enum {
	NALU_PRIPORITY_DISPOSABLE = 0,
	NALU_PRIORITY_LOW = 1,
	NALU_PRIORITY_HIGH = 2,
	NALU_PRIORITY_HIGHTEST = 3, a
} NaluPriority;

typedef struct {
	int             startcodeprefix_len;        //! 4 for parameter sets and first slice in picture, 3 for everything else (suggest)
	unsigned        len;                        //! Length of the NAL unit (Excluding the start code, which does not belong to the NALU
	int             max_size;                   //! Nalu Unit Buffer size
	int             forbidden_bit;              //! should be always FALSE
	int             nal_reference_idc;          //! NALU_PRIPORITY_xxxx
	int             nal_unit_type;              //! NALU_TYPE_xxxx
	char*           buf;                        //! contains the first byte followed by the EBSP
} NALU_t;

FILE *h264bitstream = NULL;                     //! the bit stream file

int info2 = 0, info3 = 0;

static int FindStartCode2(unsigned char *Buf) {
	if (Buf[0] != 0 || Buf[1] != 0 || Buf[2] != 1) return 0;    //0x00 0001 ？
	else return 1;
}

static int FindStartCode3(unsigned char *Buf) {
	if (Buf[0] != 0 || Buf[1] != 0 || Buf[2] != 0 || Buf[3] != 1) return 0;     //0x00 000001?
	else return 1;
}

int GetAnnexbNALU(NALU_t *nalu) {
	int pos = 0;
	int startCodeFound, rewind;
	unsigned char *Buf;


	if ((Buf = (unsigned char*)calloc(nalu->max_size, sizeof(char))) == NULL)
		printf("GetAnnexbNALU: Could not allocate Buf memory\n");

	nalu->startcodeprefix_len = 3;

	if (3 != fread(Buf, 1, 3, h264bitstream)) {
		free(Buf);
		return 0;
	}

	info2 = FindStartCode2(Buf);
	if (info2 != 1) {   //不是0x000001
		if (1 != fread(Buf + 3, 1, 1, h264bitstream)) {
			free(Buf);
			return -1;
		}
		info3 = FindStartCode3(Buf);
		if (info3 != 1) {       //不是0x00 000001?
			free(Buf);
			return -1;
		}
		else {
			pos = 4;
			nalu->startcodeprefix_len = 4;
		}
	}
	else {
		pos = 3;
		nalu->startcodeprefix_len = 3;
	}

	startCodeFound = 0;
	info2 = 0;
	info3 = 0;

	while (!startCodeFound) {
		if (feof(h264bitstream)) {
			nalu->len = (pos - 1) - nalu->startcodeprefix_len;
			memcpy(nalu->buf, &Buf[nalu->startcodeprefix_len], nalu->len);
			nalu->forbidden_bit = nalu->buf[0] & 0x80;       //1 bit
			nalu->nal_reference_idc = nalu->buf[0] & 0x60;   //2 bit
			nalu->nal_unit_type = (nalu->buf[0]) & 0x1f;     //5 bit
			free(Buf);
			return pos - 1;
		}
		Buf[pos++] = fgetc(h264bitstream);
		info3 = FindStartCode3(&Buf[pos - 4]);
		if (info3 != 1)
			info2 = FindStartCode2(&Buf[pos - 3]);
		startCodeFound = (info2 == 1 || info3 == 1);
	}

	// Here we have found another start code (and read length of startcode bytes more than we should
	// have, hence ,go back in the file)
	rewind = info3 == 1 ? -4 : -3;
	if (0 != fseek(h264bitstream, rewind, SEEK_CUR)) {
		free(Buf);
		printf("GetAnnexbNALU:Cannot fseek in the bit stream file");
	}


	// Here the Start code, the complete NALU, and the next start code is in the Buf
	// The size of Buf is pos , pos+rewind are the number of bytes excluding the next
	// start code, and (pos+rewind)-startcodeprefix_len is the size of the NALU excluding the start code
	nalu->len = (pos + rewind) - nalu->startcodeprefix_len;
	memcpy(nalu->buf, Buf, nalu->len+ nalu->startcodeprefix_len);
	nalu->forbidden_bit = nalu->buf[nalu->startcodeprefix_len] & 0x80;       // 1 bit
	nalu->nal_reference_idc = nalu->buf[nalu->startcodeprefix_len] & 0x60;   // 2 bit
	nalu->nal_unit_type = nalu->buf[nalu->startcodeprefix_len] & 0x1f;       // 5 bit
	
	//nalu->len = (pos + rewind) - nalu->startcodeprefix_len;
	//memcpy(nalu->buf, &Buf[nalu->startcodeprefix_len], nalu->len);
	//nalu->forbidden_bit = nalu->buf[0] & 0x80;       // 1 bit
	//nalu->nal_reference_idc = nalu->buf[0] & 0x60;   // 2 bit
	//nalu->nal_unit_type = nalu->buf[0] & 0x1f;       // 5 bit
	free(Buf);

	return (pos + rewind);
}

int simplest_h264_parser(const char *url, void(*out_nalu)(char * buffer,int size, NaluType type))
//int simplest_h264_parser(const char *url)
{
	NALU_t *n;
	int buffersize = 1000000;

	//FILE *myout=fopen("output_log.txt","wb+");
	//C语言中的 stdout 是一个定义在<stdio.h>的宏（macro），它展开到一个 FILE* （“指向 FILE 的指针”）类型的表达式（不一定是常量），这个表达式指向一个与标准输出流（standard output stream）相关连的 FILE 对象。
	FILE *myout = stdout;


	h264bitstream = fopen(url, "rb+");
	if (h264bitstream == NULL) {
		spdlog::critical("Open file error");
		exit(1);
	}

	n = (NALU_t *)calloc(1, sizeof(NALU_t));
	if (n == NULL) {
		spdlog::critical("Alloc NALU Error");
		exit(1);
	}

	n->max_size = buffersize;
	n->buf = (char *)calloc(buffersize, sizeof(char));
	if (n->buf == NULL) {
		free(n);
		spdlog::critical("AllocNALU:n->buf");
		exit(1);
	}

	int data_offset = 0;
	int nal_num = 0;
	printf("-----+-------- NALU Table ------+---------+\n");
	printf(" NUM |    POS  |    IDC |  TYPE |   LEN   |\n");
	printf("-----+---------+--------+-------+---------+\n");

	while (!feof(h264bitstream)) {
		int data_lenth;
		data_lenth = GetAnnexbNALU(n);

		char type_str[20] = { 0 };
		switch (n->nal_unit_type) {
		case NALU_TYPE_SLICE:       sprintf(type_str, "SLICE");      break;
		case NALU_TYPE_DPA:         sprintf(type_str, "DPA");        break;
		case NALU_TYPE_DPB:         sprintf(type_str, "DPB");        break;
		case NALU_TYPE_DPC:         sprintf(type_str, "DPC");        break;
		case NALU_TYPE_IDR:         sprintf(type_str, "IDR");	     break;
		case NALU_TYPE_SEI:         sprintf(type_str, "SEI");        break;
		case NALU_TYPE_SPS:         sprintf(type_str, "SPS");        break;
		case NALU_TYPE_PPS:         sprintf(type_str, "PPS");		 break;
		case NALU_TYPE_AUD:         sprintf(type_str, "AUD");        break;
		case NALU_TYPE_EOSEQ:       sprintf(type_str, "EOSEQ");      break;
		case NALU_TYPE_EOSTREAM:    sprintf(type_str, "EOSTREAM");   break;
		case NALU_TYPE_FILL:        sprintf(type_str, "FILL");       break;
		}

		char idc_str[20] = { 0 };
		switch (n->nal_reference_idc >> 5) {
		case NALU_PRIPORITY_DISPOSABLE: sprintf(idc_str, "DISPOS");     break;
		case NALU_PRIORITY_LOW:         sprintf(idc_str, "LOW");        break;
		case NALU_PRIORITY_HIGH:        sprintf(idc_str, "HIGH");       break;
		case NALU_PRIORITY_HIGHTEST:    sprintf(idc_str, "HIGHTEST");   break;
		}
		fprintf(myout, "%5d| %8d| %7s| %6s| %8d|\n", nal_num, data_offset, idc_str, type_str, n->len);

		if (out_nalu != NULL && n->nal_unit_type != NALU_TYPE_SEI) {
			out_nalu(n->buf, data_lenth, static_cast<NaluType>(n->nal_unit_type));
		}

		data_offset = data_offset + data_lenth;
		nal_num++;
	}

	//Free
	if (n) {
		if (n->buf) {
			free(n->buf);
			n->buf = NULL;
		}
		free(n);
	}
	return 0;
}
