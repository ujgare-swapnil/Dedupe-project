#include "de_dupe_engin.h"

void usage() {
	printf("Incorrect usage\n");
	printf("Usage:\t"
		"de_dupe_util -w for writing a file\n"
		"-r for restoring the file"
		" -i <input file name>\n"
		"-o <output file name>\n");
	return;
}

int
main(int argc, char* argv[]) {
	int	sockfd, opt, flag_write = 0, flag_read = 0, in_fname = 0, out_fname = 0;
	char 	in_filename[1024] = {'\0'}, out_filename[1024] = {'\0'}, cmd[1024];
	char	ret_val[8], hostname[HOST_NAME_MAX + 1], *chrptr = NULL;

	if (argc < 4) {
		usage();
		return -1;
	}

	while((opt = getopt(argc, argv, "wri:o:")) != -1) {
		switch(opt) {
			case 'w':
				flag_write = 1;
				break;
			case 'r':
				flag_read = 1;
				break;
			case 'i':
				in_fname = 1;
				sprintf(in_filename, "%s", optarg);
				break;
			case 'o':
				out_fname = 1;
				sprintf(out_filename, "%s", optarg);
				break;
			default:
				usage();
				return -1;

		}
	}

	if ((in_fname == 0) || (flag_write == 1 && flag_read == 1)) {
		usage();
		return -1;
	}

	if (flag_write == 1) {
		sprintf(cmd, "%d %d %s",
			MAGIC, 1, in_filename);
	} else if (flag_read == 1) {
		if (out_fname == 0) {
			chrptr = strrchr(in_filename, '/');
			if (chrptr) {
				chrptr = chrptr + 1;
			} else {
				chrptr = in_filename;
			}

			sprintf(out_filename, "%s_%s", chrptr, "restore");
		}
		sprintf(cmd, "%d %d %s %s",
			MAGIC, 0, in_filename, out_filename);
	}

	sockfd = socket(AF_INET,SOCK_STREAM, 0);
	
	if (sockfd < 0) {
		printf ("failed to create a socket\n");
		return -1;
	}

	gethostname(hostname, HOST_NAME_MAX + 1);
	struct hostent* hptr = gethostbyname(hostname);

	if (!hptr) {
		printf("failed to get hostent\n");
		return -1;
	}

	if (hptr->h_addrtype != AF_INET) {
		return -1;
	}

	struct sockaddr_in saddr;
	memset(&saddr, 0, sizeof(saddr));
	saddr.sin_family = AF_INET;
	saddr.sin_addr.s_addr =
	   ((struct in_addr*) hptr->h_addr_list[0])->s_addr;
	saddr.sin_port = htons(PORTNUMBER);

	if (connect(sockfd, (struct sockaddr*) &saddr, sizeof(saddr)) < 0) {
		printf("failed to connect\n");
		return -1;
	}

	write(sockfd, (void *)cmd, strlen(cmd) + 1);

	while (read(sockfd, ret_val, sizeof(ret_val)) <= 0) {
		printf("waiting for servers response\n");
		sleep(1);
	} 
	
	printf("retval from server %d", ret_val);
	return 0;
}
