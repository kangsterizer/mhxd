/* write a Mac Binary file given a data fork and resource fork */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <netinet/in.h>

#define L32NTOH(_word, _addr) \
	do { unsigned int _x; memcpy(&_x, (_addr), 4); _word = ntohl(_x); } while (0)
#define S32HTON(_word, _addr) \
	do { unsigned int _x; _x = htonl(_word); memcpy((_addr), &_x, 4); } while (0)

#define kTimeOffset 	2082844800

void
create_macbin (FILE *outfp, char *datafile, char *rsrcfile, char *type, char *creator)
{
	FILE *fp;
	int r;
	unsigned int data_len = 0, rsrc_len = 0, padlen;
	unsigned char buf[16384], header[128], pad[128] = {0};
	time_t mac_date;

	memset(header, 0, sizeof header);
	memset(pad, 0, sizeof pad);

	fp = fopen(datafile, "rb");
	if (!fp) {
		perror(datafile);
		return;
	}
	for (;;) {
		r = fread(buf, 1, sizeof buf, fp);
		if (r <= 0)
			break;
		data_len += r;
	}
	fclose(fp);

	fp = fopen(rsrcfile, "rb");
	if (!fp) {
		perror(rsrcfile);
		return;
	}
	for (;;) {
		r = fread(buf, 1, sizeof buf, fp);
		if (r <= 0)
			break;
		rsrc_len += r;
	}
	fclose(fp);

	header[0] = 0;	/* macbinary version */
	header[1] = (unsigned char)strlen(datafile);
	memcpy(&header[2], datafile, strlen(datafile));
	memcpy(&header[65], type, 4);
	memcpy(&header[69], creator, 4);
	S32HTON(data_len, &header[83]);		/* length of data fork */
	S32HTON(rsrc_len, &header[87]);		/* length of rsrc fork */
	mac_date = time(0) + kTimeOffset;
	S32HTON(mac_date, &header[91]);		/* create seconds since mac epoch */
	S32HTON(mac_date, &header[95]);		/* modify seconds since mac epoch */
	if (fwrite(header, 1, 128, outfp) != 128)
		perror("writing header");

	fp = fopen(datafile, "rb");
	if (!fp) {
		perror(datafile);
		return;
	}
	padlen = 128 - (data_len % 128);
	for (; data_len;) {
		r = fread(buf, 1, sizeof buf < data_len ? sizeof buf : data_len, fp);
		if (r <= 0)
			break;
		if (fwrite(buf, 1, r, outfp) != r) {
			perror("writing data");
			break;
		}
		data_len -= r;
	}
	fclose(fp);
	if (padlen != 128)
		if (fwrite(pad, 1, padlen, outfp) != padlen)
			perror("writing data pad");

	fp = fopen(rsrcfile, "rb");
	if (!fp) {
		perror(rsrcfile);
		return;
	}
	padlen = 128 - (rsrc_len % 128);
	for (; rsrc_len;) {
		r = fread(buf, 1, sizeof buf < rsrc_len ? sizeof buf : rsrc_len, fp);
		if (r <= 0)
			break;
		if (fwrite(buf, 1, r, outfp) != r) {
			perror("writing rsrc");
			break;
		}
		rsrc_len -= r;
	}
	fclose(fp);
	if (padlen != 128)
		if (fwrite(pad, 1, padlen, outfp) != padlen)
			perror("writing rsrc pad");

	fflush(outfp);
}

void
decode_macbin (char *file)
{
	FILE *fp, *dfp, *rfp;
	int r;
	unsigned char header[128], buf[16384];
	unsigned int data_len, rsrc_len, padlen;
	char datafile[256], rsrcfile[256];

	fp = fopen(file, "rb");
	if (!fp) {
		perror(file);
		return;
	}
	if (fread(header, 1, 128, fp) != 128) {
		perror("fread");
		return;
	}
	L32NTOH(data_len, &header[83]);		/* length of data fork */
	L32NTOH(rsrc_len, &header[87]);		/* length of rsrc fork */
	sprintf(datafile, "%.*s", header[1], &header[2]);
	sprintf(rsrcfile, "%.*s.rsrc", header[1], &header[2]);
	dfp = fopen(datafile, "wb");
	if (!dfp) {
		perror(datafile);
		return;
	}
	padlen = 128 - (data_len % 128);
	for (; data_len;) {
		r = fread(buf, 1, sizeof buf < data_len ? sizeof buf : data_len, fp);
		if (r <= 0)
			break;
		if (fwrite(buf, 1, r, dfp) != r) {
			perror("writing data");
			break;
		}
		data_len -= r;
	}
	fclose(dfp);
	if (padlen != 128)
		if (fread(buf, 1, padlen, fp) != padlen)
			perror("reading data pad");
	rfp = fopen(rsrcfile, "wb");
	if (!rfp) {
		perror(rsrcfile);
		return;
	}
	padlen = 128 - (rsrc_len % 128);
	for (; rsrc_len;) {
		r = fread(buf, 1, sizeof buf < rsrc_len ? sizeof buf : rsrc_len, fp);
		if (r <= 0)
			break;
		if (fwrite(buf, 1, r, rfp) != r) {
			perror("writing rsrc");
			break;
		}
		rsrc_len -= r;
	}
	fclose(rfp);
	if (padlen != 128)
		if (fread(buf, 1, padlen, fp) != padlen)
			perror("reading rsrc pad");
	fclose(fp);
}

int
main (int argc, char **argv)
{
	char *type, *creator;

	if (argc < 3) {
		if (argv[1]) {
			decode_macbin(argv[1]);
			return 0;
		}	
		fprintf(stderr, "usage: %s <macbin file> | <data file> <rsrc file> [type] [creator]\n", argv[0]);
		fprintf(stderr, "  writes macbinary file to stdout\n");
		exit(1);
	}

	if (argc > 3)
		type = argv[3];
	else
		type = "????";
	if (argc > 4)
		creator = argv[4];
	else
		creator = "????";
	create_macbin(stdout, argv[1], argv[2], type, creator);

	return 0;
}
